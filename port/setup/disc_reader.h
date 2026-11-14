// Shared Wii disc reader, backed by the pinned nod C API.
struct NodClose { void operator()(NodHandle *h) const { nod_free(h); } };
using NodPtr=std::unique_ptr<NodHandle,NodClose>;
void nod_ok(NodResult result) {
    if (result!=NOD_RESULT_OK) {
        const char *error=nod_error_message();
        throw Stop{std::string("Cannot read Wii disc: ")+(error ? error : "invalid disc image")};
    }
}
#ifdef ZENKAIPOWER_ANDROID_INSTALLER
struct AndroidNodStream { int fd; int64_t length; };
#endif
NodPtr open_wii(const std::string &input) {
#ifndef ZENKAIPOWER_ANDROID_INSTALLER
    NodHandle *raw=nullptr;
    nod_ok(nod_disc_open(input.c_str(),nullptr,&raw));
    return NodPtr(raw);
#else
    NodHandle *raw=nullptr;
    const std::string prefix="/proc/self/fd/";
    if (input.rfind(prefix,0)!=0) {
        nod_ok(nod_disc_open(input.c_str(),nullptr,&raw));
        return NodPtr(raw);
    }
    // A descriptor is a stream, not a split-image filename. Never probe its neighbors.
    char *end=nullptr; errno=0;
    const char *number=input.c_str()+prefix.size();
    long original=strtol(number,&end,10);
    require(number!=end && end && !*end && !errno && original>=0 && original<=INT_MAX,"Invalid selected disc descriptor.");
    int fd=fcntl((int)original,F_DUPFD_CLOEXEC,0);
    require(fd>=0,"The selected disc file is no longer open. Select the image again.");
    struct stat info{};
    int64_t length=fstat(fd,&info)==0 ? (int64_t)info.st_size : -1;
    if (length<=0) length=(int64_t)lseek(fd,0,SEEK_END);
    if (length<=0) { close(fd); throw Stop{"The selected disc image must support random access."}; }
    NodDiscStream stream{};
    stream.user_data=new AndroidNodStream{fd,length};
    stream.read_at=[](void *user,uint64_t offset,void *out,size_t size)->int64_t {
        if (offset>(uint64_t)INT64_MAX) return -1;
        ssize_t count;
        do { count=pread(static_cast<AndroidNodStream *>(user)->fd,out,size,(off_t)offset); } while (count<0 && errno==EINTR);
        return count;
    };
    stream.stream_len=[](void *user)->int64_t { return static_cast<AndroidNodStream *>(user)->length; };
    stream.close=[](void *user) {
        auto *source=static_cast<AndroidNodStream *>(user); close(source->fd); delete source;
    };
    // nod owns the callbacks on both success and failure, including reader clones.
    nod_ok(nod_disc_open_stream(&stream,nullptr,&raw));
    return NodPtr(raw);
#endif
}
std::string disc_run(const std::string &,const std::vector<std::string> &args,bool cancellable=true) {
    std::string input,output,selected; bool listing=false;
    for (size_t i=0;i<args.size();++i) {
        if (args[i]=="-i" && i+1<args.size()) input=args[++i];
        else if (args[i]=="-o" && i+1<args.size()) output=args[++i];
        else if (args[i]=="-s" && i+1<args.size()) selected=args[++i];
        else if (args[i]=="-l") listing=true;
    }
    NodHandle *raw=nullptr; NodPtr disc=open_wii(input);
    NodDiscHeader header{}; nod_ok(nod_disc_header(disc.get(),&header));
    require(!memcmp(header.game_id,"RDSPAF",6),"Expected the European Wii disc RDSPAF.");
    nod_ok(nod_disc_open_partition_kind(disc.get(),NOD_PARTITION_KIND_DATA,nullptr,&raw)); NodPtr partition(raw);
    struct Entries { std::string listing, wanted; std::vector<uint32_t> matches; } entries;
    entries.wanted=selected;
    nod_partition_iterate_fst(partition.get(),[](uint32_t index,NodNodeKind kind,const char *name,uint32_t,void *user) {
        auto &e=*static_cast<Entries *>(user);
        if (kind==NOD_NODE_KIND_FILE) {
            std::string leaf=lower(std::filesystem::path(name).filename().string());
            e.listing+=leaf+"\n";
            if (leaf==e.wanted) e.matches.push_back(index);
        }
        return index+1;
    },&entries);
    if (listing) return entries.listing;
    require(entries.matches.size()==1,"The Wii disc is missing the requested archive or contains duplicates: "+selected);
    require(!output.empty() && selected.find('/')==std::string::npos && selected.find("..") == std::string::npos,"Invalid Wii extraction path.");
    nod_ok(nod_partition_open_file(partition.get(),entries.matches[0],&raw)); NodPtr file(raw);
    std::string destination=output+"/"+selected;
#ifdef _WIN32
    FILE *stream=_wfopen(std::filesystem::u8path(destination).c_str(),L"wb");
#else
    FILE *stream=fopen(destination.c_str(),"wb");
#endif
    std::unique_ptr<FILE,decltype(&fclose)> out(stream,fclose);
    require(out!=nullptr,"Cannot write "+destination);
    Bytes buffer(1<<20);
    for (;;) {
        if (cancellable) check_cancel();
        int64_t n=nod_read(file.get(),buffer.data(),buffer.size());
        require(n>=0,"Failed reading Wii archive "+selected);
        if (!n) break;
        require(fwrite(buffer.data(),1,(size_t)n,out.get())==(size_t)n,"Not enough space to extract "+selected);
    }
    require(fclose(out.release())==0,"Cannot finish "+destination);
    return "";
}
