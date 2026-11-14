// SDL's Android filename backend must not reopen a document-provider descriptor.
struct AndroidIsoFile { int fd; int64_t length,position; };
SDL_IOStream *android_iso_stream(const std::string &path) {
    const std::string prefix="/proc/self/fd/";
    if (path.rfind(prefix,0)!=0) return SDL_IOFromFile(path.c_str(),"rb");
    char *end=nullptr; errno=0; const char *number=path.c_str()+prefix.size();
    long original=strtol(number,&end,10);
    if (end==number || !end || *end || errno || original<0 || original>INT_MAX) return nullptr;
    int fd=fcntl((int)original,F_DUPFD_CLOEXEC,0); if (fd<0) return nullptr;
    struct stat info{};
    if (fstat(fd,&info)!=0 || info.st_size<=0) { close(fd); return nullptr; }
    auto *file=new AndroidIsoFile{fd,(int64_t)info.st_size,0};
    SDL_IOStreamInterface callbacks{}; SDL_INIT_INTERFACE(&callbacks);
    callbacks.size=[](void *p)->Sint64 { return static_cast<AndroidIsoFile *>(p)->length; };
    callbacks.seek=[](void *p,Sint64 offset,SDL_IOWhence whence)->Sint64 {
        auto &f=*static_cast<AndroidIsoFile *>(p);
        Sint64 base=whence==SDL_IO_SEEK_SET ? 0 : whence==SDL_IO_SEEK_CUR ? f.position : whence==SDL_IO_SEEK_END ? f.length : -1;
        if (base<0 || (offset>0 && base>INT64_MAX-offset) || (offset<0 && (offset==INT64_MIN || base< -offset))) return -1;
        f.position=base+offset; return f.position;
    };
    callbacks.read=[](void *p,void *out,size_t size,SDL_IOStatus *status)->size_t {
        auto &f=*static_cast<AndroidIsoFile *>(p); ssize_t count;
        do { count=pread(f.fd,out,size,(off_t)f.position); } while (count<0 && errno==EINTR);
        if (count<0) { *status=SDL_IO_STATUS_ERROR; return 0; }
        if (!count) *status=SDL_IO_STATUS_EOF;
        f.position+=count; return (size_t)count;
    };
    callbacks.close=[](void *p)->bool { auto *f=static_cast<AndroidIsoFile *>(p); bool ok=close(f->fd)==0; delete f; return ok; };
    SDL_IOStream *io=SDL_OpenIO(&callbacks,file);
    if (!io) callbacks.close(file);
    return io;
}
