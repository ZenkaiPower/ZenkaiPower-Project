NativeTitleLogo android_icon(View texture,int index) {
    static const unsigned block[8][4]={{0,2,8,10},{1,3,9,11},{4,6,12,14},{5,7,13,15},{16,18,24,26},{17,19,25,27},{20,22,28,30},{21,23,29,31}};
    static const unsigned columns[4][32]={
        {0,8,32,40,64,72,96,104,2,10,34,42,66,74,98,106,4,12,36,44,68,76,100,108,6,14,38,46,70,78,102,110},
        {16,24,48,56,80,88,112,120,18,26,50,58,82,90,114,122,20,28,52,60,84,92,116,124,22,30,54,62,86,94,118,126},
        {65,73,97,105,1,9,33,41,67,75,99,107,3,11,35,43,69,77,101,109,5,13,37,45,71,79,103,111,7,15,39,47},
        {81,89,113,121,17,25,49,57,83,91,115,123,19,27,51,59,85,93,117,125,21,29,53,61,87,95,119,127,23,31,55,63}};
    View e=texture.sub((size_t)texture.word(4)*4+index*64,64);
    Uint64 tex0=(Uint64)e.word(48)|((Uint64)e.word(52)<<32);
    unsigned width=1u<<((tex0>>26)&15),height=1u<<((tex0>>30)&15);
    require(width<=512 && height<=512 && ((tex0>>20)&63)==20,"Unsupported controller texture.");
    size_t pixels=(size_t)e.word(0)*4+96,palette=(size_t)e.word(4)*4+96;
    require(pixels>=48 && e.word(24)>>8==0,"Invalid controller upload.");
    unsigned uw=texture.word(pixels-48)&4095,uh=texture.word(pixels-44)&4095;
    require(uw<=512 && uh<=512,"Invalid controller upload dimensions.");
    unsigned dbw=e.word(24)&63,tbw=(tex0>>14)&63;
    std::vector<int> gs(512*512*4,-1);
    for(unsigned y=0;y<uh;y++) for(unsigned x=0;x<uw;x++) {
        unsigned a=(((y&~31u)*dbw+((x>>1)&~31u)+kBlock32[(y>>3)&3][(x>>3)&7])<<6)+kCol32[y&7][x&7];
        require(a<gs.size()/4,"Invalid controller upload address.");
        for(unsigned c=0;c<4;c++) gs[a*4+c]=(y*uw+x)*4+c;
    }
    NativeTitleLogo out; out.width=width; out.height=height; out.pixels.resize(width*height*4);
    for(unsigned y=0;y<height;y++) for(unsigned x=0;x<width;x++) {
        unsigned col=(y&15)>>2,sx=x^((col&1)?4u:0u);
        unsigned a=((((y>>2)&~31u)*(tbw>>1)+((x>>2)&~31u)+block[(y>>4)&7][(x>>5)&3])<<9)+columns[y&3][sx&31]+col*128;
        require((a>>1)<gs.size() && gs[a>>1]>=0,"Invalid controller pixel address.");
        unsigned color=(texture.byte(pixels+gs[a>>1])>>((a&1)*4))&15;
        View rgba=texture.sub(palette+color*4,4); auto *p=&out.pixels[(y*width+x)*4];
        memcpy(p,rgba.p,4); p[3]=p[3]>=128 ? 255 : p[3]*2;
    }
    return out;
}
void android_buttons(const std::string &root) {
    step("Touch controller artwork");
    DiskReader file(root+"gamedata/pzs3us1.afs");
    AfsReader afs([&](Uint64 at,void *p,size_t n){file.read(at,p,n);},file.size);
    Bytes boot=afs.entry(4,8<<20);
    View data(boot); size_t offset=data.word(8*4)&~3u;
    View icons=data.sub(offset,data.n-offset); Assets assets;
    const char *names[]={"select","start","circle","square","triangle","cross","l1","l2","l3","r1","r2","r3","up","down"};
    const int indices[]={8,9,20,21,22,23,14,15,16,17,18,19,4,5};
    for (int i=0;i<14;i++) {
        NativeTitleLogo image=android_icon(icons,indices[i]);
        for (int column=0;column<(indices[i]>=20 && indices[i]<=23 ? 2 : 1);column++) {
            require(image.width>column*32,"Truncated controller texture.");
            Bytes rgba(32*32*4,0);
            for (int y=0;y<std::min(32,image.height);y++) {
                int width=std::min(32,image.width-column*32);
                memcpy(rgba.data()+y*128,image.pixels.data()+(y*image.width+column*32)*4,width*4);
            }
            asset(assets,std::string(names[i])+(column ? "-pressed.png" : ".png"),png_rgba(32,32,rgba));
        }
    }
    publish_assets(root+"gamedata/touch-buttons.pak",assets);
    say("@ok Touch controller artwork installed");
}
