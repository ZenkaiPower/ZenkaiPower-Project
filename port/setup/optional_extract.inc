// Native optional-disc readers. Included inside native.cpp's installer namespace.
// The Python tools remain reference/developer utilities, not release dependencies.
using Bytes = std::vector<Uint8>;
struct View {
    const Uint8 *p; size_t n;
    View(const Bytes &b): p(b.data()), n(b.size()) {}
    View(const Uint8 *data, size_t size): p(data), n(size) {}
    View sub(size_t at, size_t size) const {
        if (at > n || size > n-at) throw Stop{SetupText("error.an_optional_disc_resource_is_truncated")};
        return View(p+at,size);
    }
    Uint8 byte(size_t at) const { return sub(at,1).p[0]; }
    Uint32 word(size_t at, int count=4, bool big=false) const {
        View v=sub(at,count); Uint32 out=0;
        for (int i=0;i<count;i++) out |= (Uint32)v.p[i] << (8*(big ? count-i-1 : i));
        return out;
    }
    bool starts(const char *s,size_t count) const { return n>=count && !memcmp(p,s,count); }
    Bytes bytes() const { return Bytes(p,p+n); }
};
void require(bool condition,const std::string &why) { if (!condition) throw Stop{why}; }
void put(Bytes &out,Uint64 value,int count,bool big=false) {
    for (int i=0;i<count;i++) out.push_back((Uint8)(value>>(8*(big ? count-i-1 : i))));
}
void append(Bytes &out,View data) { out.insert(out.end(),data.p,data.p+data.n); }
Bytes literal(const char *s) { return Bytes(s,s+strlen(s)); }
std::string numbered(const char *format,int a,int b=0,int c=0) {
    char text[192]; snprintf(text,sizeof(text),format,a,b,c); return text;
}
struct TemporaryDirectory {
    std::string path;
    explicit TemporaryDirectory(const std::string &parent) {
        require(SDL_CreateDirectory(parent.c_str()),SetupText("error.cannot_create")+parent);
        for (int i=0;i<100;i++) {
            std::string next=parent+"/.zenkaipower-extract-"+std::to_string(SDL_GetPerformanceCounter())+"-"+std::to_string(i);
            std::error_code ec;
            if (std::filesystem::create_directory(std::filesystem::u8path(next),ec)) { path=next; return; }
        }
        throw Stop{SetupText("error.cannot_create_temporary_extraction_directory_in")+parent};
    }
    ~TemporaryDirectory() { std::error_code ec; std::filesystem::remove_all(std::filesystem::u8path(path),ec); }
};
struct NamedAsset {
    Bytes data;
    Iso *iso=nullptr; Uint64 offset=0,size=0;
    explicit NamedAsset(Bytes b): data(std::move(b)),size(data.size()) {}
    NamedAsset(Iso &disc,const IsoFile &file): iso(&disc),offset(file.offset),size(file.size) {}
};
using Assets=std::map<std::string,NamedAsset>;
void asset(Assets &out,const std::string &name,Bytes bytes) { out.insert_or_assign(name,NamedAsset(std::move(bytes))); }
// Publish only after all region/content validation succeeds. On failure the old
// package remains intact; large movie files stream directly from the PS2 image.
void publish_assets(const std::string &target,const Assets &assets) {
    require(!assets.empty() && assets.size()<=100000,SetupText("error.invalid_optional_resource_count"));
    Uint64 offset=12;
    for (const auto &e:assets) { require(!e.first.empty() && e.first.size()<1200,SetupText("error.invalid_resource_name")); offset+=20+e.first.size(); }
    Uint64 total=offset;
    for (const auto &e:assets) total+=e.second.size;
    require(total<=0x7fffffff,SetupText("error.optional_resource_package_is_too_large"));
    std::string parent=target.substr(0,target.rfind('/'));
#ifdef ZENKAIPOWER_ANDROID_INSTALLER
    // SAF can atomically replace a sibling document, but cannot move and rename
    // a document from a temporary subdirectory in the same operation.
    std::string path=target+".part-"+std::to_string(SDL_GetPerformanceCounter());
    struct RemoveTemporaryPackage {
        std::string path;
        ~RemoveTemporaryPackage() { SDL_RemovePath(path.c_str()); }
    } temp{path};
#else
    TemporaryDirectory temp(parent);
    std::string path=temp.path+"/resources.pak";
#endif
    SDL_IOStream *raw=SDL_IOFromFile(path.c_str(),"wb");
    require(raw!=nullptr,SetupText("error.cannot_write")+path);
    struct Close { void operator()(SDL_IOStream *io) const { SDL_CloseIO(io); } };
    std::unique_ptr<SDL_IOStream,Close> out(raw);
    auto write=[&](View data) { require(SDL_WriteIO(out.get(),data.p,data.n)==data.n,SetupText("error.cannot_write_optional_resource_package")); };
    Bytes index=literal("BT3TEXT1"); put(index,assets.size(),4);
    for (const auto &e:assets) {
        put(index,e.first.size(),4); put(index,offset,8); put(index,e.second.size,8);
        index.insert(index.end(),e.first.begin(),e.first.end()); offset+=e.second.size;
    }
    write(index); Bytes buffer(1<<20);
    for (const auto &e:assets) {
        const NamedAsset &a=e.second;
        if (!a.iso) write(a.data);
        else for (Uint64 at=0;at<a.size;) {
            size_t n=(size_t)std::min<Uint64>(buffer.size(),a.size-at);
            a.iso->read(a.offset+at,buffer.data(),n); write(View(buffer.data(),n)); at+=n; check_cancel();
        }
        check_cancel();
    }
    require(SDL_CloseIO(out.release()),SetupText("error.cannot_finish_optional_resource_package"));
    check_cancel();
    require(SDL_RenamePath(path.c_str(),target.c_str()),SetupText("error.cannot_publish")+target);
}
struct AfsReader {
    std::function<void(Uint64,void *,size_t)> read;
    Uint64 length; Uint32 count; Bytes records;
    AfsReader(std::function<void(Uint64,void *,size_t)> reader,Uint64 size):read(reader),length(size) {
        require(length>=8,SetupText("error.truncated_afs_archive"));
        Bytes head(8); read(0,head.data(),8); View h(head);
        count=h.word(4);
        require(h.starts("AFS\0",4) && count>0 && (Uint64)count*8+8<=length && count<1000000,SetupText("error.invalid_afs_archive"));
        records.resize((size_t)count*8); read(8,records.data(),records.size());
    }
    Bytes entry(Uint32 id,size_t limit=64u<<20) {
        require(id<count,SetupText("error.missing_optional_afs_entry"));
        View table(records); Uint64 at=table.word(id*8),size=table.word(id*8+4);
        require(at>=8+(Uint64)count*8 && at<=length && size<=length-at && size<=limit,
                SetupText("error.invalid_optional_afs_entry_bounds_entry") + std::to_string(id) +
                SetupText("error.offset") + std::to_string(at) + SetupText("error.size") + std::to_string(size) +
                SetupText("error.archive_size") + std::to_string(length) + SetupText("error.size_limit") + std::to_string(limit) + ".");
        Bytes out((size_t)size); read(at,out.data(),out.size()); check_cancel(); return out;
    }
    std::vector<Uint32> dragon_entries() {
        Uint64 at=8+(Uint64)count*8; require(at+8<=length,SetupText("error.missing_afs_filename_table"));
        Bytes info(8); read(at,info.data(),8); View h(info);
        at=h.word(0); Uint64 size=h.word(4);
        require(size>=(Uint64)count*48 && at<=length && size<=length-at,SetupText("error.invalid_afs_filename_table"));
        std::vector<Uint32> matches; Bytes name(48);
        for (Uint32 i=0;i<count;i++) { read(at+(Uint64)i*48,name.data(),48); if (!memcmp(name.data(),"DragonNe",8)) matches.push_back(i); }
        return matches;
    }
};
Bytes bpe_expand(View src,bool big) {
    Uint32 size=src.word(0,4,big),packed=src.word(4,4,big);
    require(size<=64u<<20 && packed<=src.n-8,SetupText("error.invalid_bpe_resource_size"));
    View data=src.sub(8,packed); size_t at=0; Bytes out; out.reserve(size);
    Uint64 budget=(Uint64)size*16+65536;
    auto take=[&]() { return data.byte(at++); };
    while (at<data.n) {
        Uint8 left[256],right[256]={}; for (int i=0;i<256;i++) left[i]=(Uint8)i;
        int symbol=0;
        while (symbol<256) {
            int count=take(); if (count>127) { symbol+=count-127; count=0; }
            require(symbol<=256,SetupText("error.invalid_bpe_symbol_table"));
            if (symbol==256) break;
            for (int i=0;i<=count;i++) {
                require(symbol<256,SetupText("error.invalid_bpe_symbol_table"));
                left[symbol]=take(); if (left[symbol]!=symbol) right[symbol]=take(); symbol++;
            }
        }
        Uint32 block=data.word(at,2,true); at+=2; View codes=data.sub(at,block); at+=block;
        for (size_t i=0;i<codes.n;i++) {
            Bytes stack(1,codes.p[i]);
            while (!stack.empty()) {
                require(budget-->0 && stack.size()<=4096,SetupText("error.cyclic_or_excessive_bpe_expansion"));
                Uint8 value=stack.back(); stack.pop_back();
                if (left[value]==value) { require(out.size()<size,SetupText("error.bpe_expansion_exceeds_expected_size")); out.push_back(value); }
                else { stack.push_back(right[value]); stack.push_back(left[value]); }
            }
        }
        check_cancel();
    }
    require(out.size()==size,SetupText("error.bpe_resource_size_mismatch")); return out;
}
std::vector<View> ps2_pack(View data) {
    Uint32 count=data.word(0); require(count>0 && count<=4096 && 4ull*(count+2)<=data.n,SetupText("error.invalid_japanese_menu_pack"));
    std::set<size_t> starts; starts.insert(data.n);
    for (Uint32 i=0;i<=count;i++) { size_t at=data.word(4+i*4)&~3u; if (at<data.n) starts.insert(at); }
    std::vector<View> parts;
    for (Uint32 i=0;i<count;i++) {
        Uint32 a=data.word(4+i*4),b=data.word(8+i*4); size_t at=a&~3u;
        if (a==b) parts.emplace_back(data.p,0);
        else { require(at<data.n,SetupText("error.missing_japanese_menu_member")); parts.push_back(data.sub(at,*starts.upper_bound(at)-at)); }
    }
    return parts;
}
void extract_japanese(const std::string &root,const std::string &path) {
    Iso iso; iso.open(path); require(iso.find("SLPS_258.15")!=nullptr,SetupText("error.expected_the_japanese_ps2_disc_slps_25815"));
    auto archive=[&](const char *name) {
        const IsoFile *file=iso.find(name); require(file!=nullptr,std::string(SetupText("error.missing"))+name);
        Uint64 base=file->offset;
        return AfsReader([&iso,base](Uint64 at,void *p,size_t n){iso.read(base+at,p,n);},file->size);
    };
    AfsReader menus=archive("DATA/PZS3JP1.AFS"),sounds=archive("DATA/PZS3JP2.AFS");
    Assets assets;
    auto screen=[&](Uint32 id,size_t member) {
        Bytes packed=menus.entry(id,4<<20); auto parts=ps2_pack(packed);
        require(member<parts.size(),SetupText("error.missing_japanese_menu_screen"));
        return bpe_expand(parts[member],false);
    };
    Bytes title=screen(448,0); auto parts=ps2_pack(title);
    require(parts.size()==14 && parts[4].n==132480 && parts[4].word(0)==1,SetupText("error.unexpected_japanese_title_artwork"));
    asset(assets,"title/sparking-meteor.bin",parts[4].bytes());
    Bytes select=screen(447,1); parts=ps2_pack(select);
    require(parts.size()>=4,SetupText("error.missing_japanese_music_name_artwork"));
    for (size_t i=2;i<=3;i++) {
        require(parts[i].n==431424 && parts[i].word(0)==4,SetupText("error.unexpected_japanese_music_names"));
        asset(assets,i==2 ? "soundtrack/meteor/names-selected.bin" : "soundtrack/meteor/names-normal.bin",parts[i].bytes());
    }
    for (int slot=0;slot<24;slot++) {
        Bytes track=sounds.entry(63602+slot,8<<20); View v(track);
        require(v.n>=2048 && v.starts("\x80\0\x07\xfc\x03\x12\x04\x02",8) && v.word(8,4,true)==24000 && v.word(36,4,true)==1,SetupText("error.unexpected_japanese_bgm_format"));
        asset(assets,numbered("soundtrack/meteor/%02d.adx",slot),std::move(track));
    }
    for (const char *name:{"ZS3JPED.PSS","ZS3JPED.ADX","ZS3JPOP.PSS","ZS3JPOP.ADX"}) {
        std::string relative=std::string("DATA/")+name;
        const IsoFile *file=iso.find(relative.c_str()); require(file!=nullptr,SetupText("error.missing_japanese_movie")+relative);
        bool picture=relative.substr(relative.size()-3)=="PSS";
        require(file->size>=64 && file->size<=(picture ? 160u<<20 : 8u<<20),SetupText("error.invalid_japanese_movie_size"));
        Bytes head(4); iso.read(file->offset,head.data(),head.size());
        require(picture ? View(head).starts("\0\0\1\xba",4) : View(head).starts("\x80\0",2),SetupText("error.invalid_japanese_movie_header"));
        // Check the last byte as well, before touching an existing package.
        iso.read(file->offset+file->size-1,head.data(),1);
        assets.emplace("disc/"+relative,NamedAsset(iso,*file));
    }
    publish_assets(root+"gamedata/sparking-meteor.pak",assets);
}
std::vector<View> wii_table(View data) {
    Uint32 count=data.word(0,4,true);
    require(count>0 && count<=4096 && 4ull*(count+2)<=data.n,SetupText("error.invalid_wii_offset_table"));
    std::vector<View> out; size_t previous=data.word(4,4,true);
    require(previous>=4ull*(count+2) && previous<=data.n,SetupText("error.invalid_wii_resource_offset"));
    for (Uint32 i=0;i<count;i++) {
        size_t next=data.word(8+i*4,4,true); require(next>=previous && next<=data.n,SetupText("error.invalid_wii_resource_bounds"));
        out.push_back(data.sub(previous,next-previous)); previous=next;
    }
    return out;
}
Uint32 crc32(View bytes) {
    Uint32 crc=~0u;
    for (size_t i=0;i<bytes.n;i++) { crc^=bytes.p[i]; for (int bit=0;bit<8;bit++) crc=(crc>>1)^((crc&1)?0xedb88320u:0); }
    return ~crc;
}
Bytes png_rgba(Uint32 width,Uint32 height,const Bytes &rgba) {
    Bytes out={0x89,'P','N','G','\r','\n',0x1a,'\n'};
    auto chunk=[&](const char *type,const Bytes &bytes) {
        put(out,bytes.size(),4,true); size_t start=out.size(); out.insert(out.end(),type,type+4); append(out,bytes);
        put(out,crc32(View(out.data()+start,out.size()-start)),4,true);
    };
    Bytes head; put(head,width,4,true); put(head,height,4,true); append(head,Bytes{8,6,0,0,0}); chunk("IHDR",head);
    Bytes rows; rows.reserve((width*4+1)*height);
    for (Uint32 y=0;y<height;y++) { rows.push_back(0); append(rows,View(rgba).sub((size_t)y*width*4,width*4)); }
    // Stored DEFLATE blocks avoid an additional zlib DLL in the installer.
    Bytes deflate={0x78,0x01}; Uint32 a=1,b=0;
    for (Uint8 value:rows) { a=(a+value)%65521; b=(b+a)%65521; }
    for (size_t at=0;at<rows.size();) {
        size_t n=std::min<size_t>(65535,rows.size()-at); deflate.push_back(at+n==rows.size()?1:0);
        put(deflate,n,2); put(deflate,(~n)&65535,2); append(deflate,View(rows).sub(at,n)); at+=n;
    }
    put(deflate,(b<<16)|a,4,true); chunk("IDAT",deflate); chunk("IEND",Bytes{}); return out;
}
std::set<Uint32> wii_textures(View data,Assets &assets,const std::string &folder,const std::set<Uint32> *wanted=nullptr) {
    std::set<Uint32> found;
    if (data.n<8) return found;
    Uint32 count=data.word(0,4,true); Uint64 start=(Uint64)data.word(4,4,true)*4;
    if (!count || count>128 || start<8 || start+count*96>data.n) return found;
    for (Uint32 i=0;i<count;i++) {
        if (wanted && !wanted->count(i)) continue;
        size_t entry=(size_t)start+i*96; Uint64 pix=(Uint64)data.word(entry,4,true)*4,pal=(Uint64)data.word(entry+4,4,true)*4;
        Uint32 pixSize=data.word(entry+8,4,true),palSize=data.word(entry+12,4,true);
        Uint8 lh=data.byte(entry+40),lw=data.byte(entry+41),pf=data.byte(entry+42),format=data.byte(entry+43);
        if ((format!=0 && format!=8 && format!=9) || lw>10 || lh>10 || (format && pf)) continue;
        Uint32 width=1u<<lw,height=1u<<lh,colors=format==9?256:format==8?16:0;
        Uint32 tw=8,th=format==9?4:8;
        if (pix>data.n || pixSize>data.n-pix || pal>data.n || palSize>data.n-pal || palSize<colors*4 ||
            pixSize<((width+tw-1)/tw)*((height+th-1)/th)*32) continue;
        Bytes rgba((size_t)width*height*4); size_t tile=0;
        for (Uint32 by=0;by<height;by+=th) for (Uint32 bx=0;bx<width;bx+=tw,tile+=32) {
            for (Uint32 y=0;y<th;y++) for (Uint32 x=0;x<tw;x++) {
                Uint32 p=y*tw+x,raw=data.byte((size_t)pix+tile+(format==9?p:p/2));
                Uint32 value=format==9?raw:((p&1)?raw&15:raw>>4);
                if (bx+x>=width || by+y>=height) continue;
                size_t at=((size_t)(by+y)*width+bx+x)*4;
                if (!format) { rgba[at]=rgba[at+1]=rgba[at+2]=(Uint8)(value*17); rgba[at+3]=255; }
                else {
                    rgba[at]=data.byte((size_t)pal+value*2+1); rgba[at+1]=data.byte((size_t)pal+value*2);
                    rgba[at+2]=data.byte((size_t)pal+colors*2+value*2+1);
                    rgba[at+3]=(Uint8)std::min(255,(int)data.byte((size_t)pal+colors*2+value*2)*2);
                }
            }
        }
        asset(assets,folder+numbered("/texture-%03d.png",i),png_rgba(width,height,rgba)); found.insert(i); check_cancel();
    }
    return found;
}
struct FodFrame { std::string name; View data; };
std::vector<View> fod_records(View data,Uint8 code) {
    require(data.starts("FOD\x11" "BIG\0",8),SetupText("error.invalid_wii_movie_header"));
    for (size_t pos=8;pos+8<=data.n;) {
        Uint8 tag=data.byte(pos); Uint32 count=data.word(pos+2,2,true),size=data.word(pos+4,4,true);
        View payload=data.sub(pos+8,size);
        if (tag==code) {
            std::vector<View> out; size_t at=0;
            for (Uint32 i=0;i<count;i++) { size_t n=8+(size_t)payload.word(at+4,4,true); out.push_back(payload.sub(at,n)); at+=n; }
            return out;
        }
        if (!tag) break;
        pos+=8+(size_t)size;
    }
    throw Stop{SetupText("error.a_required_wii_movie_tag_is_missing")};
}
std::vector<FodFrame> fod_frames(View record) {
    std::vector<FodFrame> out; size_t pos=8;
    while (pos<record.n) {
        size_t end=pos; while (record.byte(end)!=0) end++;
        size_t n=record.word(end+1,2,true); if (!n) break;
        out.push_back({std::string((const char *)record.p+pos,end-pos),record.sub(end+3,n)}); pos=end+3+n;
    }
    return out;
}
struct Bits {
    View data; size_t bit=0;
    Sint32 get(unsigned width,bool sign=false) {
        require(width<=31,SetupText("error.invalid_wii_movie_bit_width")); Uint32 out=0;
        for (unsigned i=0;i<width;i++,bit++) out=(out<<1)|((data.byte(bit/8)>>(7-bit%8))&1);
        if (sign && width && (out&(1u<<(width-1)))) return (Sint32)((Sint64)out-(1ll<<width));
        return (Sint32)out;
    }
};
struct Matrix { double value[4]={1,1,0,0}; };
Matrix fod_matrix(View data,size_t &pos) {
    Uint8 flags=data.byte(pos++); Bits bits{data.sub(pos,data.n-pos)}; Matrix m;
    if (flags&0x11) {
        unsigned width=(unsigned)bits.get(5);
        if (flags&1) m.value[0]=bits.get(width,true)/65536.0;
        if (flags&0x10) m.value[1]=bits.get(width,true)/65536.0;
    }
    if (flags&0x0a) { unsigned width=(unsigned)bits.get(5); if (flags&2) bits.get(width,true); if (flags&8) bits.get(width,true); }
    if (flags&0x24) {
        unsigned width=(unsigned)bits.get(5);
        if (flags&4) m.value[2]=bits.get(width,true)*0.05;
        if (flags&0x20) m.value[3]=bits.get(width,true)*0.05;
    }
    pos+=(bits.bit+7)/8; return m;
}
double fod_alpha(View data,size_t &pos) {
    Uint8 flags=data.byte(pos++); Bits bits{data.sub(pos,data.n-pos)}; double alpha=1;
    if (flags) { unsigned width=(unsigned)bits.get(4); for (int i=0;i<8;i++) if (flags&(1<<i)) { Sint32 value=bits.get(width,true); if (i==3) alpha=value/256.0; } }
    pos+=(bits.bit+7)/8; return alpha;
}
struct Placement { Matrix matrix; double alpha=1; bool hasMatrix=false,hasAlpha=false; };
Placement fod_place(View frame,Uint32 depth) {
    size_t pos=0; Placement found;
    auto skip=[&](size_t n) { frame.sub(pos,n); pos+=n; };
    auto string=[&]() { while (frame.byte(pos++)) {} };
    while (pos<frame.n) {
        Uint32 code=frame.word(pos,2,true); pos+=2;
        if (code==1) break;
        if (code==4 || code==26) {
            Uint32 flags=0,d,kind=0;
            if (code==4) { d=frame.word(pos,2,true); kind=frame.byte(pos+2); skip(5); flags=4|(kind==6?8:0); }
            else { flags=frame.byte(pos); d=frame.word(pos+1,2,true); skip(3); if (flags&2) skip(3); }
            if (flags&4) { Matrix m=fod_matrix(frame,pos); if (d==depth) { found.matrix=m; found.hasMatrix=true; } }
            if (flags&8) { double a=fod_alpha(frame,pos); if (d==depth) { found.alpha=a; found.hasAlpha=true; } }
            if (code==26 && (flags&0x20)) string();
        } else if (code==5) skip(5);
        else if (code==28) skip(2);
        else if (code==12) {
            for (;;) {
                Uint8 action=frame.byte(pos++); if (!action) break;
                if (action==0x81) skip(2);
                else if (action==0x83) { string(); string(); }
                else if (action==0x8a) skip(3);
                else if (action==0x8b || action==0x8c) string();
                else require(action>=4 && action<=7,SetupText("error.unsupported_wii_movie_action"));
            }
        } else throw Stop{SetupText("error.unsupported_wii_movie_frame_tag")};
    }
    return found;
}
Bytes wii_motion(View data) {
    auto roots=fod_records(data,7),sprites=fod_records(data,6);
    require(!roots.empty() && sprites.size()>11,SetupText("error.wii_home_movie_is_missing_animation_tracks"));
    auto root=fod_frames(roots[0]),pan=fod_frames(sprites[6]),radar=fod_frames(sprites[11]),giru=fod_frames(sprites[3]),sparkles=fod_frames(sprites[1]);
    require(root.size()>1 && !pan.empty() && !radar.empty() && root[1].name=="fl_btn_in_1" && pan[0].name=="fl_loop_start" && radar[0].name=="fl_on_loop",SetupText("error.unexpected_wii_home_animation_labels"));
    std::vector<std::vector<float>> tracks;
    auto track=[&](const std::vector<FodFrame> &frames,Uint32 depth,int field,size_t count,double base,bool alpha=false) {
        require(frames.size()>=count,SetupText("error.wii_animation_track_is_too_short")); std::vector<float> values;
        for (size_t i=0;i<count;i++) {
            Placement p=fod_place(frames[i].data,depth);
            if (alpha && p.hasAlpha) base=p.alpha;
            if (!alpha && p.hasMatrix) base=p.matrix.value[field];
            values.push_back((float)base);
        }
        tracks.push_back(std::move(values));
    };
    track(root,20,2,18,0); track(root,10,2,18,0); track(pan,1,3,500,256); track(radar,1,0,50,1);
    track(giru,1,2,30,0); track(giru,1,3,30,0); track(sparkles,1,0,60,1,true); track(sparkles,2,0,60,1,true);
    Bytes out=literal("WMO2"); for (const auto &t:tracks) put(out,t.size(),2);
    for (const auto &t:tracks) for (float value:t) { Uint32 bits; memcpy(&bits,&value,4); put(out,bits,4); }
    return out;
}
std::vector<std::string> wii_text(View text,size_t count,size_t limit,bool singleLine=false) {
    auto lines=wii_table(text); require(lines.size()>=count,SetupText("error.missing_wii_text_lines"));
    std::vector<std::string> out;
    for (size_t i=0;i<count;i++) {
        View line=lines[i]; require(line.starts("\xff\xfe",2) && !(line.n&1),SetupText("error.invalid_wii_utf_16_text"));
        for (size_t unit=2;unit<line.n;unit+=2) {
            Uint32 value=line.word(unit,2);
            if (value>=0xd800 && value<=0xdbff) {
                require(unit+4<=line.n && line.word(unit+2,2)>=0xdc00 && line.word(unit+2,2)<=0xdfff,SetupText("error.invalid_wii_utf_16_surrogate"));
                unit+=2;
            } else require(value<0xdc00 || value>0xdfff,SetupText("error.invalid_wii_utf_16_surrogate"));
        }
        size_t end=2; while (end+2<=line.n && line.word(end,2)) end+=2;
        char *converted=SDL_iconv_string("UTF-8","UTF-16LE",(const char *)line.p+2,end-2);
        require(converted!=nullptr,SetupText("error.invalid_wii_utf_16_text")); std::string value(converted); SDL_free(converted);
        if (singleLine) std::replace(value.begin(),value.end(),'\n',' ');
        require(!value.empty() && value.size()<limit,SetupText("error.invalid_wii_text_line_length")); out.push_back(std::move(value));
    }
    return out;
}
Bytes text_bank(const char *magic,const std::vector<std::string> &lines) {
    Bytes out=literal(magic); put(out,lines.size(),2); put(out,0,2);
    for (const auto &line:lines) { put(out,line.size(),2); out.insert(out.end(),line.begin(),line.end()); } return out;
}
void wii_dialog(View text,View lips,AfsReader &voices,Assets &assets) {
    auto phrases=wii_text(text,44,256); auto tracks=wii_table(lips);
    require(wii_table(text).size()==44 && tracks.size()==88 && voices.count>=32203,SetupText("error.unexpected_wii_dialogue_voice_archive_layout"));
    Bytes out=literal("WDL2"); put(out,44,2); put(out,2,2);
    for (int line=0;line<44;line++) {
        std::vector<std::pair<Uint32,Uint32>> events[2];
        for (int language=0;language<2;language++) {
            View track=tracks[line*2+language]; require(track.starts("LIPS",4) && track.word(4)==2,SetupText("error.invalid_wii_lip_track"));
            Uint32 count=track.word(8); require(count>=2 && count<=65535 && 20ull+count*4<=track.n,SetupText("error.truncated_wii_lip_track"));
            for (Uint32 i=0;i<count;i++) {
                Uint32 frame=track.word(20+i*4,2),state=track.word(22+i*4,2); if (!frame && !state) continue;
                require(state<=1 && (events[language].empty() || frame>=events[language].back().first),SetupText("error.invalid_wii_lip_events"));
                events[language].push_back({frame,state});
            }
            require(!events[language].empty(),SetupText("error.empty_wii_lip_track"));
            Bytes voice=voices.entry((language?32159:30787)+line,2<<20); View v(voice);
            require(v.n>=0x40 && v.starts("\x80\0\0\x24\x03\x12\x04\x01",8),SetupText("error.unexpected_wii_adx_voice_format"));
            Uint32 rate=v.word(8,4,true),samples=v.word(12,4,true); require(rate>0,SetupText("error.invalid_wii_voice_sample_rate"));
            double drift=(double)samples*60/rate-events[language].back().first;
            require(drift>=-2 && drift<=(language?12:45),SetupText("error.wii_voice_does_not_match_its_lip_track"));
            asset(assets,std::string("voices/")+(language?"en/":"ja/")+numbered("%03d.adx",line),std::move(voice));
        }
        put(out,phrases[line].size(),2); put(out,events[0].size(),2); put(out,events[1].size(),2);
        put(out,events[0].back().first,2); put(out,events[1].back().first,2); put(out,0,2);
        out.insert(out.end(),phrases[line].begin(),phrases[line].end());
        for (int language=0;language<2;language++) for (auto event:events[language]) { put(out,event.first,2); put(out,event.second,2); }
    }
    asset(assets,"home-dialog.bin",std::move(out));
}
std::string json_quote(const std::string &text) {
    std::string out="\"";
    for (unsigned char c:text) {
        if (c=='"' || c=='\\') { out+='\\'; out+=(char)c; }
        else if (c<32) { char escaped[8]; snprintf(escaped,sizeof(escaped),"\\u%04x",c); out+=escaped; }
        else out+=(char)c;
    }
    return out+'"';
}
// Keep JSON values as raw slices so edited labels and unknown catalog fields are
// retained verbatim when refreshing only untouched Wii translations.
struct JsonReader {
    const std::string &s; size_t at=0;
    void space() { while (at<s.size() && isspace((unsigned char)s[at])) at++; }
    char peek() { space(); require(at<s.size(),SetupText("error.truncated_ui_translation_catalog")); return s[at]; }
    void expect(char c) { require(peek()==c,SetupText("error.invalid_ui_translation_catalog")); at++; }
    std::string string() {
        expect('"'); std::string out;
        while (at<s.size()) {
            unsigned char c=s[at++]; if (c=='"') return out;
            require(c>=32,SetupText("error.invalid_json_string"));
            if (c!='\\') { out+=(char)c; continue; }
            require(at<s.size(),SetupText("error.truncated_json_escape")); c=s[at++];
            if (c=='u') {
                auto hex=[&]() { require(at+4<=s.size(),SetupText("error.truncated_unicode_escape")); unsigned value=0; for (int i=0;i<4;i++) { char h=s[at++]; require(isxdigit((unsigned char)h)!=0,SetupText("error.invalid_unicode_escape")); value=value*16+(h<='9'?h-'0':tolower(h)-'a'+10); } return value; };
                unsigned code=hex();
                if (code>=0xd800 && code<=0xdbff) { require(at+2<=s.size() && s.substr(at,2)=="\\u",SetupText("error.invalid_unicode_surrogate")); at+=2; unsigned low=hex(); require(low>=0xdc00 && low<=0xdfff,SetupText("error.invalid_unicode_surrogate")); code=0x10000+((code-0xd800)<<10)+(low-0xdc00); }
                require(code<0xd800 || code>0xdfff,SetupText("error.invalid_unicode_surrogate"));
                if (code<0x80) out+=(char)code;
                else if (code<0x800) { out+=(char)(0xc0|(code>>6)); out+=(char)(0x80|(code&63)); }
                else if (code<0x10000) { out+=(char)(0xe0|(code>>12)); out+=(char)(0x80|((code>>6)&63)); out+=(char)(0x80|(code&63)); }
                else { out+=(char)(0xf0|(code>>18)); out+=(char)(0x80|((code>>12)&63)); out+=(char)(0x80|((code>>6)&63)); out+=(char)(0x80|(code&63)); }
            } else {
                const std::string codes="\"\\/bfnrt",values="\"\\/\b\f\n\r\t"; size_t i=codes.find((char)c); require(i!=std::string::npos,SetupText("error.invalid_json_escape")); out+=values[i];
            }
        }
        throw Stop{SetupText("error.unterminated_ui_translation_string")};
    }
    void value(unsigned depth=0) {
        require(depth<64,SetupText("error.ui_translation_catalog_nesting_is_excessive")); char c=peek();
        if (c=='"') { string(); return; }
        if (c=='{' || c=='[') {
            char end=c=='{'?'}':']'; at++;
            if (peek()==end) { at++; return; }
            for (;;) { if (c=='{') { string(); expect(':'); } value(depth+1); if (peek()==end) { at++; return; } expect(','); }
        }
        size_t begin=at; while (at<s.size() && s[at]!=',' && s[at]!='}' && s[at]!=']' && !isspace((unsigned char)s[at])) at++;
        require(at>begin,SetupText("error.invalid_json_value"));
    }
    std::map<std::string,std::string> object() {
        expect('{'); std::map<std::string,std::string> out;
        if (peek()=='}') { at++; return out; }
        for (;;) { std::string key=string(); expect(':'); space(); size_t begin=at; value(); out[key]=s.substr(begin,at-begin); if (peek()=='}') { at++; return out; } expect(','); }
    }
};
std::string json_object(const std::map<std::string,std::string> &values) {
    std::string out="{"; bool comma=false;
    for (const auto &e:values) { if (comma) out+=','; out+='\n'+json_quote(e.first)+": "+e.second; comma=true; }
    return out+"\n}";
}
std::map<std::string,std::string> catalog_rows(const std::vector<std::string> &originalDialog,const std::vector<std::string> &originalMenu,
                                             const std::vector<std::string> &dialog,const std::vector<std::string> &menu) {
    std::map<std::string,std::string> rows;
    for (int group=0;group<2;group++) {
        const auto &original=group?originalMenu:originalDialog; const auto &localized=group?menu:dialog;
        for (size_t i=0;i<original.size();i++) rows[std::string("wii-dragon-net/")+(group?"menu-text/":"dialog/")+std::to_string(i)]=
            SetupText("error.original")+json_quote(original[i])+SetupText("error.text")+json_quote(localized[i])+"}";
    }
    return rows;
}
std::string fragment(const std::map<std::string,std::string> &rows) {
    std::string out; for (const auto &e:rows) out+=",\n    "+json_quote(e.first)+": "+e.second; return out;
}
using CatalogUpdates=std::map<std::string,std::string>;
void update_catalog(const std::string &root,const std::string &locale,const std::map<std::string,std::string> &rows,CatalogUpdates &updates) {
    std::string path=root+"gamedata/game-text/translations/ui/"+locale+".json";
    FILE *fp=Port_PackFopen(path.c_str(),"rb"); if (!fp) return;
    std::unique_ptr<FILE,ClosePackageFile> file(fp); require(!fseek(fp,0,SEEK_END),SetupText("error.cannot_read_ui_catalog")); long size=ftell(fp);
    require(size>=0 && size<=16<<20 && !fseek(fp,0,SEEK_SET),SetupText("error.invalid_ui_catalog_size"));
    std::string text((size_t)size,'\0'); require(fread(&text[0],1,text.size(),fp)==text.size(),SetupText("error.cannot_read_ui_catalog"));
    auto document=JsonReader{text}.object(); require(document.count("locale") && JsonReader{document.at("locale")}.string()==locale && document.count("labels"),SetupText("error.invalid_ui_translation_catalog_for")+locale);
    auto labels=JsonReader{document.at("labels")}.object();
    for (auto i=labels.begin();i!=labels.end();) { if (i->first.rfind("wii-dragon-net/battle-help/",0)==0) i=labels.erase(i); else ++i; }
    for (const auto &row:rows) {
        auto old=labels.find(row.first); bool replace=old==labels.end();
        if (!replace) { auto value=JsonReader{old->second}.object(); replace=value.count("text") && value.count("original") && JsonReader{value.at("text")}.string()==JsonReader{value.at("original")}.string(); }
        if (replace) labels[row.first]=row.second;
    }
    document["labels"]=json_object(labels); document["labels_count"]=std::to_string(labels.size()); updates[path]=json_object(document)+"\n";
}
struct DiskReader {
    SDL_IOStream *io; Uint64 size;
    explicit DiskReader(const std::string &path):io(SDL_IOFromFile(path.c_str(),"rb")),size(0) {
        require(io!=nullptr,SetupText("error.cannot_open")+path); Sint64 n=SDL_GetIOSize(io);
        if (n<0) { SDL_CloseIO(io); io=nullptr; throw Stop{SetupText("error.cannot_size")+path}; } size=(Uint64)n;
    }
    ~DiskReader() { if (io) SDL_CloseIO(io); }
    void read(Uint64 at,void *out,size_t n) {
        require(at<=size && n<=size-at && SDL_SeekIO(io,(Sint64)at,SDL_IO_SEEK_SET)>=0 && SDL_ReadIO(io,out,n)==n,SetupText("error.truncated_optional_disc_archive"));
    }
};
#if defined(ZENKAIPOWER_ANDROID_INSTALLER) || defined(ZENKAIPOWER_NOD_INSTALLER)
#include "disc_reader.h"
#else
// Small developer harnesses that do not read Wii images can omit the nod library.
std::string disc_run(const std::string &,const std::vector<std::string> &,bool=true) {
    throw Stop{"Wii disc reading requires the linked nod library; build with port/setup/build.sh."};
}
#endif
std::string find_extracted(const std::string &dir,const std::string &name) {
    std::string found; std::error_code ec;
    std::filesystem::recursive_directory_iterator it(std::filesystem::u8path(dir),ec),end;
    require(!ec,SetupText("error.cannot_inspect_wii_extraction_directory"));
    for (;it!=end;it.increment(ec)) {
        require(!ec,SetupText("error.cannot_inspect_wii_extraction_directory"));
        if (it->is_regular_file(ec) && lower(it->path().filename().u8string())==name) { require(found.empty(),SetupText("error.wii_extraction_contains_duplicate_archives")); found=it->path().u8string(); }
    }
    require(!found.empty(),SetupText("error.wii_archive_was_not_extracted")+name); return found;
}
void preserve_custom_assets(const std::string &package,Assets &assets) {
    SDL_PathInfo info; if (!SDL_GetPathInfo(package.c_str(),&info)) return;
    DiskReader reader(package); Bytes header(12); reader.read(0,header.data(),12); View h(header);
    require(h.starts("BT3TEXT1",8) && h.word(8)<=100000,SetupText("error.invalid_existing_dragon_net_battle_package"));
    Uint64 at=12;
    for (Uint32 i=0;i<h.word(8);i++) {
        Bytes record(20); reader.read(at,record.data(),20); View r(record); at+=20; Uint32 length=r.word(0);
        require(length>0 && length<1200,SetupText("error.invalid_existing_resource_name"));
        Bytes nameBytes(length); reader.read(at,nameBytes.data(),length); at+=length;
        std::string name((const char *)nameBytes.data(),nameBytes.size());
        bool generated=name.rfind("section-",0)==0 || name.rfind("entry-",0)==0 || name.rfind("languages/",0)==0 ||
            name.rfind("home-",0)==0 || name.rfind("battle-help-",0)==0 || name.rfind("menu-text-",0)==0 ||
            name.rfind("ui-fragment-",0)==0 || name.rfind("voices/",0)==0 || name=="manifest.json" || name=="index.html";
        if (generated || assets.count(name)) continue;
        require(name.find("..") == std::string::npos && name.find('\\')==std::string::npos && name[0]!='/' && name.find('\0')==std::string::npos,SetupText("error.invalid_existing_resource_path"));
        Uint64 offset=(Uint64)r.word(4)|((Uint64)r.word(8)<<32),size=(Uint64)r.word(12)|((Uint64)r.word(16)<<32);
        require(size<=64u<<20,SetupText("error.existing_custom_resource_is_too_large")); Bytes data((size_t)size); reader.read(offset,data.data(),data.size()); asset(assets,name,std::move(data));
    }
}
void publish_catalogs(const CatalogUpdates &catalogs) {
    for (const auto &entry:catalogs) {
        std::string parent=entry.first.substr(0,entry.first.rfind('/'));
        require(SDL_CreateDirectory(parent.c_str()),SetupText("error.cannot_create_ui_catalog_directory"));
        // Rename must stay on the destination filesystem (the disc scratch
        // directory may be on /tmp or another drive). Keep replacement atomic.
        TemporaryDirectory staging(parent);
        std::string temp=staging.path+"/catalog.json"; SDL_IOStream *out=SDL_IOFromFile(temp.c_str(),"wb");
        require(out!=nullptr,SetupText("error.cannot_update_ui_catalog")); bool ok=SDL_WriteIO(out,entry.second.data(),entry.second.size())==entry.second.size();
        bool closed=SDL_CloseIO(out); require(ok && closed && SDL_RenamePath(temp.c_str(),entry.first.c_str()),SetupText("error.cannot_publish_ui_catalog"));
    }
}
void extract_wii(const std::string &root,const std::string &source) {
    std::string dataRoot=root+"gamedata";
    // The two Wii archives need several GB of scratch space, independently of the output package.
    TemporaryDirectory temporary(std::filesystem::temp_directory_path().u8string());
    std::string path=std::filesystem::absolute(std::filesystem::u8path(source)).u8string(),menuPath,voicePath;
    if (lower(std::filesystem::u8path(path).extension().u8string())==".afs") {
        require(lower(std::filesystem::u8path(path).filename().u8string())=="wzs3eu1.afs",SetupText("error.expected_the_european_wii_menu_archive"));
        menuPath=path; voicePath=(std::filesystem::u8path(path).parent_path()/"wzs3eu2.afs").u8string();
    } else {
        say((std::string("@note ") + SetupText("progress.reading_the_wii_disc")));
        std::string listing=lower(disc_run(root,{"extract","-i",path,"-g","-l"}));
        require(listing.find("wzs3eu1.afs")!=std::string::npos && listing.find("wzs3us1.afs")==std::string::npos && listing.find("wzs3eu2.afs")!=std::string::npos,SetupText("error.expected_the_european_wii_disc_rdspaf_with_menu_and_voice_archives"));
        for (const char *name:{"wzs3eu1.afs","wzs3eu2.afs"}) {
            check_cancel(); disc_run(root,{"extract","-i",path,"-o",temporary.path,"-g","-s",name,"-q"});
        }
        menuPath=find_extracted(temporary.path,"wzs3eu1.afs"); voicePath=find_extracted(temporary.path,"wzs3eu2.afs");
    }
    DiskReader menuFile(menuPath),voiceFile(voicePath);
    AfsReader menu([&](Uint64 at,void *p,size_t n){menuFile.read(at,p,n);},menuFile.size);
    AfsReader voices([&](Uint64 at,void *p,size_t n){voiceFile.read(at,p,n);},voiceFile.size);
    auto matches=menu.dragon_entries(); require(matches.size()==5,SetupText("error.the_european_wii_archive_must_contain_five_dragonne_language_entries"));
    Assets assets; CatalogUpdates catalogs;
    std::vector<std::string> originalDialog,originalMenu;
    static const char *locales[]={"en","es","de","fr","it"};
    for (size_t localeIndex=0;localeIndex<5;localeIndex++) {
        std::string locale=locales[localeIndex]; say((std::string("@note ") + SetupText("progress.decoding_wii_artwork_animation_and_text"))+locale);
        Bytes pack=menu.entry(matches[localeIndex]); auto sections=wii_table(pack);
        require(sections.size()>=2,SetupText("error.wii_online_pack_is_missing_a_battle_section"));
        if (!localeIndex) {
            asset(assets,numbered("entry-%05d.bin",matches[0]),pack);
            for (size_t si=0;si<sections.size();si++) {
                Bytes decoded=bpe_expand(sections[si],true); auto resources=wii_table(decoded);
                std::string folder=numbered("section-%02d",(int)si); asset(assets,folder+"/decoded.bin",decoded);
                for (size_t ri=0;ri<resources.size();ri++) {
                    View resource=resources[ri]; std::string name=folder+numbered("/resource-%03d",(int)ri);
                    asset(assets,name+".bin",resource.bytes()); wii_textures(resource,assets,name+"-textures");
                    if (!si && !ri) asset(assets,"home-motion.bin",wii_motion(resource));
                    // A texture resource can also be a pack of smaller texture sets.
                    std::vector<View> parts; try { parts=wii_table(resource); } catch (const Stop &) {}
                    for (size_t pi=0;pi<parts.size();pi++) wii_textures(parts[pi],assets,name+numbered("-part-%02d-textures",(int)pi));
                }
            }
        }
        Bytes home=bpe_expand(sections[0],true),battle=bpe_expand(sections[1],true);
        auto resources=wii_table(home),battleResources=wii_table(battle);
        require(resources.size()>13 && battleResources.size()>4,SetupText("error.unexpected_wii_online_resource_layout"));
        auto dialog=wii_text(resources[7],44,256),menuLines=wii_text(resources[11],66,512);
        require(wii_table(resources[7]).size()==44,SetupText("error.unexpected_wii_dialogue_line_count"));
        if (!localeIndex) { originalDialog=dialog; originalMenu=menuLines; wii_dialog(resources[7],resources[6],voices,assets); }
        asset(assets,"home-subtitles-"+locale+".bin",text_bank("WST1",dialog));
        auto singleLines=menuLines; for (auto &line:singleLines) std::replace(line.begin(),line.end(),'\n',' ');
        asset(assets,"menu-text-"+locale+".bin",text_bank("WMT1",singleLines));
        auto rows=catalog_rows(originalDialog,originalMenu,dialog,menuLines);
        std::string json=fragment(rows); asset(assets,"ui-fragment-"+locale+".json",Bytes(json.begin(),json.end()));
        update_catalog(root,locale,rows,catalogs);
        for (int ri:{3,4,5}) wii_textures(resources[ri],assets,"languages/"+locale+numbered("/resource-%03d-textures",ri));
        auto nickname=wii_table(resources[13]); require(nickname.size()>2,SetupText("error.missing_wii_name_keyboard"));
        wii_textures(nickname[2],assets,"languages/"+locale+"/resource-013-part-02-textures");
        for (int ri:{3,4}) {
            const std::set<Uint32> wanted=ri==3?std::set<Uint32>{29,30,41,42}:std::set<Uint32>{1,2,3};
            auto found=wii_textures(battleResources[ri],assets,"languages/"+locale+numbered("/section-01/resource-%03d-textures",ri),&wanted);
            require(found==wanted,SetupText("error.wii_battle_artwork_is_missing_required_textures"));
        }
        check_cancel();
    }
    // Refresh existing non-PAL UI catalogs with English Wii IDs, retaining edits.
    std::set<std::string> installed;
    std::string directory=root+"gamedata/game-text/translations/ui";
    for (int i=0;i<1000;i++) { char name[1200]; if (!Port_TextPackageList(directory.c_str(),i,name,sizeof(name))) break; std::string n=name; if (n.size()>5 && n.substr(n.size()-5)==".json") installed.insert(n.substr(0,n.size()-5)); }
    std::error_code ec; std::filesystem::directory_iterator it(std::filesystem::u8path(directory),ec),end;
    if (!ec) for (;it!=end;it.increment(ec)) {
        require(!ec,SetupText("error.cannot_inspect_ui_translations")); std::string name=it->path().filename().u8string();
        if (name.size()>5 && name.substr(name.size()-5)==".json") installed.insert(name.substr(0,name.size()-5));
    }
    auto english=catalog_rows(originalDialog,originalMenu,originalDialog,originalMenu);
    for (const auto &locale:installed) if (std::find(std::begin(locales),std::end(locales),locale)==std::end(locales)) {
        std::string json=fragment(english); asset(assets,"ui-fragment-"+locale+".json",Bytes(json.begin(),json.end())); update_catalog(root,locale,english,catalogs);
    }
    preserve_custom_assets(dataRoot+"/dragon-net-battle.pak",assets);
    publish_assets(dataRoot+"/dragon-net-battle.pak",assets);
    publish_catalogs(catalogs);
}
