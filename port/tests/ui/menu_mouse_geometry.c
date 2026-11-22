/* gcc -std=gnu99 -g -fno-builtin -ffunction-sections -fdata-sections \
 *   -Iinclude -Iport/include -include port_compat.h \
 *   port/tests/ui/menu_mouse_geometry.c -Wl,--gc-sections -lm -o /tmp/menu_mouse_geometry_test
 * /tmp/menu_mouse_geometry_test */
extern float ceilf(float);
#include "../../../src/sys/gfxm_c.c"
extern void abort(void);
extern char *strcpy(char *, const char *);
#define CHECK(condition) do { if (!(condition)) abort(); } while (0)
static int art43, page43, aspect = 1778;
static int hovered;
void PortMenuMouse_SetHover(int on) { hovered=on; }
int gPortMenuMode = 1;
int Port_TitleMainChars43(void) { return art43; }
int Port_Menus43(void) { return page43; }
int Port_PageAspectMilli(void) { return aspect; }
int Port_IsWide(void) { return aspect > 1340; }

int main(void) {
    /* Disabled Options rows share the Dragon Net Battle RGB tint, without
       changing their geometry or destroying the authored animation alpha. */
    {
        FlashProp prop;
        Flash_CxformIdentity(&prop.cx);
        prop.mtx.m[2]=123;prop.alpha=0.75f;prop.flags=FLASH_OV_ALPHA;
        Flash_OptionsDisabledProp(&prop);
        CHECK(prop.cx.mul[0]==88.0f/128.0f && prop.cx.mul[1]==88.0f/128.0f &&
              prop.cx.mul[2]==88.0f/128.0f && prop.cx.mul[3]==1.0f);
        CHECK(prop.mtx.m[2]==123 && prop.alpha==0.75f && prop.flags==FLASH_OV_ALPHA);
    }

    /* Dynamic/native title sheets must have identical source-to-screen mapping. */
    {
        FlashQuad title;s32 i;f32 scales[3]={.5f,1,2};
        for(i=0;i<3;i++) {
            f32 scale=scales[i];
            Flash_OptionsHeadingQuad(&title,12,5,12+512*scale,5+32*scale,0,0,512,32);
            CHECK(__builtin_fabsf(title.pt[0][0]-12)<.001f);
            CHECK(__builtin_fabsf(title.pt[0][1]-5)<.001f);
            CHECK(__builtin_fabsf((title.pt[3][1]-title.pt[0][1])/64-scale)<.001f);
            /* Original 21px capitals and 29px baseline retain the exact sprite scale. */
            CHECK(__builtin_fabsf(title.pt[0][1]+29*scale-(5+29*scale))<.001f);
        }
        /* A partial UV crop still maps each source texel to the native position. */
        Flash_OptionsHeadingQuad(&title,20,10,276,42,128,8,384,40);
        CHECK(title.pt[0][0]+128==20 && title.pt[0][1]+8==10);
        CHECK(title.pt[3][0]-title.pt[0][0]==512);
    }
    /* Replacement ink follows the original caption shape's centre and live motion. */
    {
        FlashProp text={0};FlashMtx shape={0};FlashCxform color={0};
        FlashQuad caption,original={0};s32 ofs[2]={7,9},step,i;
        f32 scales[3]={0.85f,1.0f,1.2f};
        for(i=0;i<4;i++) color.mul[i]=1;
        for(step=0;step<3;step++) {
            f32 scale=scales[step];f32 inkX=63,inkY=14;
            shape.m[0]=scale;shape.m[4]=scale;shape.m[2]=step*3;shape.m[5]=-step*2;
            text.mtx.m[0]=text.mtx.m[4]=1;text.mtx.m[2]=222;text.mtx.m[5]=230;
            for(i=0;i<4;i++) text.cx.mul[i]=1;
            Flash_OptionsCaptionQuad(&caption,-128,-16,128,16,inkX,inkY);
            original=caption;
            for(i=0;i<4;i++) { original.pt[i][0]=0;original.pt[i][1]=0; }
            FlashDraw_TransformQuad(&original,&shape,&color,&text,ofs);
            FlashDraw_TransformQuad(&caption,&shape,&color,&text,ofs);
            CHECK(__builtin_fabsf(caption.pt[0][0]+inkX*scale-original.pt[0][0])<.001f);
            CHECK(__builtin_fabsf(caption.pt[0][1]+inkY*scale-original.pt[0][1])<.001f);
            CHECK(__builtin_fabsf(caption.pt[3][0]-caption.pt[0][0]-512*scale)<.001f);
            CHECK(__builtin_fabsf(original.pt[0][0]-(229+step*3))<.001f);
        }
    }
    /* The hit transform must follow inherited draw anchors, not assume center. */
    {
        FlashQuad q={0};PortMousePlate hit;
        f32 scale=1333.333f/(f32)aspect;
        q.pt[0][0]=140;q.pt[1][0]=310;q.pt[2][0]=140;
        q.pt[0][1]=q.pt[1][1]=240;q.pt[2][1]=272;q.rgba[3]=128;
        art43=1;
        sFlashMouseMarker=0x1A;Flash_MouseDrawnPiece(&hit,&q);
        CHECK(__builtin_fabsf(hit.pt[0][0]-140*scale)<.001f);
        sFlashMouseMarker=0x1B;Flash_MouseDrawnPiece(&hit,&q);
        CHECK(__builtin_fabsf(hit.pt[0][0]-(512+(140-512)*scale))<.001f);
        sFlashMouseMarker=0x19;Flash_MouseDrawnPiece(&hit,&q);
        CHECK(__builtin_fabsf(hit.pt[0][0]-(256+(140-256)*scale))<.001f);
        sFlashMouseMarker=0x13;Flash_MouseDrawnPiece(&hit,&q);
        CHECK(hit.pt[0][0]==140);
        aspect=1333;sFlashMouseMarker=0x1B;Flash_MouseDrawnPiece(&hit,&q);
        CHECK(hit.pt[0][0]==140);
        aspect=1778;art43=0;
    }
    /* Card entrance/exit staging stays hidden before widescreen narrowing.
       Visible and partially entering cards must still draw in either direction. */
    {
        FlashQuad card = {0};
        s32 corner;
        for (corner=0; corner<4; corner++) card.pt[corner][0]=600+(corner & 1)*64;
        CHECK(Flash_MenuQuadOffPage(&card));
        for (corner=0; corner<4; corner++) card.pt[corner][0]-=100;
        CHECK(!Flash_MenuQuadOffPage(&card));
        for (corner=0; corner<4; corner++) card.pt[corner][0]-=300;
        CHECK(!Flash_MenuQuadOffPage(&card));
        for (corner=0; corner<4; corner++) card.pt[corner][0]-=300;
        CHECK(Flash_MenuQuadOffPage(&card));
    }
    /* Each player's inspection header and name bars use opposite page anchors. */
    sMenuArtScreen=13;
    sTitleMenuEntriesVisible=0;
    CHECK(FlashClip_MenuArtMarker("mc_select_plate")==0x1B);
    CHECK(FlashClip_MenuArtMarker("mc_name_plate")==0x1A);
    sTitleMenuEntriesVisible=1;
    CHECK(FlashClip_MenuArtMarker("mc_select_plate")==0x1A);
    CHECK(FlashClip_MenuArtMarker("mc_ability_limit_big_num")==0x1A);
    CHECK(FlashClip_MenuArtMarker("mc_name_plate")==0x1B);
    CHECK(FlashClip_MenuArtMarker("mc_form_text")==0x1B);
    CHECK(FlashClip_MenuArtMarker("mc_list_plate_1")==0x19);
    sMenuArtScreen=0;
    sTitleMenuEntriesVisible=0;
    Flash flash = {0};
    FlashProp prop = {0};
    prop.mtx.m[0] = prop.mtx.m[4] = 0.5f;
    prop.mtx.m[2] = 100; prop.mtx.m[5] = 80;
    prop.cx.mul[0] = prop.cx.mul[1] = prop.cx.mul[2] = prop.cx.mul[3] = 1;
    flash.ofs[0] = 10; flash.ofs[1] = 20;
    /* Selector hit regions use the composed draw transform and are scoped
       by player/movie. Hidden clips and stale frames cannot receive clicks. */
    Port_FlashMenuMouseBegin();
    sSelectionMouseCount=1;
    memcpy(sSelectionMouseClips[0].name,"mc_chara_chip_000",18);
    sSelectionMouseClips[0].scope=11;
    sSelectionMouseClips[0].prop=prop;
    sSelectionMouseClips[0].flash=&flash;
    CHECK(Port_FlashMenuMouseHit(11,"mc_chara_chip_000",32,32,0,115,105));
    CHECK(hovered);
    CHECK(Port_FlashMenuMouseHit(11,"mc_chara_chip_000",32,32,0,100,90));
    CHECK(!Port_FlashMenuMouseHit(11,"mc_chara_chip_000",32,32,0,94,90));
    CHECK(!Port_FlashMenuMouseHit(12,"mc_chara_chip_000",32,32,0,115,105));
    CHECK(!Port_FlashMenuMouseHit(11,"mc_chara_chip_000",32,32,0,140,105));
    sSelectionMouseClips[0].prop.cx.mul[3]=0;
    CHECK(!Port_FlashMenuMouseHit(11,"mc_chara_chip_000",32,32,0,115,105));
    Port_FlashMenuMouseBegin();
    CHECK(!Port_FlashMenuMouseHit(11,"mc_chara_chip_000",32,32,0,115,105));
    /* Stage cells are centred like portraits. Music occupies the next cell
       to the right and must follow the same animation/scaling transform. */
    sSelectionMouseCount=1;
    strcpy(sSelectionMouseClips[0].name,"mc_map_chip_05");
    sSelectionMouseClips[0].scope=10;
    sSelectionMouseClips[0].prop=prop;
    sSelectionMouseClips[0].flash=&flash;
    CHECK(Port_FlashMenuMouseHit(10,"mc_map_chip_05",64,64,256,100,90));
    CHECK(Port_FlashMenuMouseHit(10,"mc_map_chip_05",64,64,256,125,110));
    CHECK(!Port_FlashMenuMouseHit(10,"mc_map_chip_05",64,64,256,130,105));
    CHECK(Port_FlashMenuMouseHit(10,"mc_stage_music",64,64,256,130,105));
    CHECK(!Port_FlashMenuMouseHit(10,"mc_stage_music",64,64,256,115,105));
    CHECK(!Port_FlashMenuMouseHit(10,"mc_stage_music",64,64,256,160,105));
    CHECK(!Port_FlashMenuMouseHit(11,"mc_stage_music",64,64,256,130,105));
    art43=1;
    CHECK(Port_FlashMenuMouseHit(10,"mc_stage_music",64,64,256,162,105));
    CHECK(!Port_FlashMenuMouseHit(10,"mc_map_chip_05",64,64,256,162,105));
    art43=0;
    /* Costume buttons are 64 pixels wide, not full custom-set rows. */
    sSelectionMouseCount=1;
    strcpy(sSelectionMouseClips[0].name,"mc_color_plate_1");
    sSelectionMouseClips[0].scope=11;
    sSelectionMouseClips[0].prop=prop;
    CHECK(Port_FlashMenuMouseHit(11,"mc_color_plate_1",64,32,0,130,105));
    CHECK(!Port_FlashMenuMouseHit(11,"mc_color_plate_1",64,32,0,150,105));
    /* Item panels have separate captures for each player. */
    strcpy(sSelectionMouseClips[0].name,"mc_list_plate_1");
    sSelectionMouseClips[0].scope=70;
    CHECK(Port_FlashMenuMouseHit(70,"mc_list_plate_1",256,24,256,130,105));
    CHECK(!Port_FlashMenuMouseHit(71,"mc_list_plate_1",256,24,256,130,105));
    /* The 64px shop plate quads overlap; adjacent targets follow the 35px row pitch. */
    {
        PortSelectionMouseClip saved = sSelectionMouseClips[0];
        int scope, wide;
        sSelectionMouseCount=2;
        for (scope=55; scope<=56; scope++) {
            for (int row=0; row<2; row++) {
                PortSelectionMouseClip *clip=&sSelectionMouseClips[row];
                *clip=saved;
                strcpy(clip->name,row ? "mc_list_plate_2" : "mc_list_plate_1");
                clip->scope=scope;
                clip->prop.mtx.m[0]=clip->prop.mtx.m[4]=1;
                clip->prop.mtx.m[2]=26;
                clip->prop.mtx.m[5]=139+35*row;
            }
            for (wide=0; wide<=1; wide++) {
                art43=wide;
                CHECK(Port_FlashMenuMouseHit(scope,"mc_list_plate_1",256,32,256,267,208));
                CHECK(!Port_FlashMenuMouseHit(scope,"mc_list_plate_1",256,32,256,267,210));
                CHECK(!Port_FlashMenuMouseHit(scope,"mc_list_plate_2",256,32,256,267,208));
                CHECK(Port_FlashMenuMouseHit(scope,"mc_list_plate_2",256,32,256,267,210));
            }
        }
        for (int row=0; row<2; row++) {
            PortSelectionMouseClip *clip=&sSelectionMouseClips[row];
            clip->scope=53;
            clip->prop.mtx.m[5]=127+35*row;
            clip->drawn=(PortMousePlate){{{130,136+35*row},{403,136+35*row},{130,200+35*row}},1};
        }
        CHECK(Port_FlashMenuMouseHit(53,"mc_list_plate_1",256,32,256,400,158));
        CHECK(!Port_FlashMenuMouseHit(53,"mc_list_plate_1",256,32,256,400,179));
        CHECK(!Port_FlashMenuMouseHit(53,"mc_list_plate_2",256,32,256,400,158));
        CHECK(Port_FlashMenuMouseHit(53,"mc_list_plate_2",256,32,256,400,179));
        art43=0;
        sSelectionMouseCount=1;
        sSelectionMouseClips[0]=saved;
    }
    /* Battle rule choices are 256-wide and right anchored on wide pages. */
    strcpy(sSelectionMouseClips[0].name,"mc_set_1_1");
    sSelectionMouseClips[0].scope=9;
    art43=1;
    CHECK(Port_FlashMenuMouseHit(9,"mc_set_1_1",256,32,512,300,105));
    CHECK(!Port_FlashMenuMouseHit(9,"mc_set_1_1",256,32,512,200,105));
    art43=0;
    /* The equipped-item close chevron inherits its header's RIGHT anchor. */
    art43=1;
    strcpy(sSelectionMouseClips[0].name,"mc_yajirusi_left");
    sSelectionMouseClips[0].scope=70;
    CHECK(Port_FlashMenuMouseHit(70,"mc_yajirusi_left",32,32,512,215,105));
    CHECK(!Port_FlashMenuMouseHit(70,"mc_yajirusi_left",32,32,256,215,105));
    art43=0;
    /* History controls follow their left anchor, including during animation. */
    art43=1;
    strcpy(sSelectionMouseClips[0].name,"mc_menu_plate_1");
    sSelectionMouseClips[0].scope=6;
    CHECK(Port_FlashMenuMouseHit(6,"mc_menu_plate_1",80,80,0,100,110));
    CHECK(!Port_FlashMenuMouseHit(6,"mc_menu_plate_1",80,80,0,120,110));
    sSelectionMouseClips[0].scope=5;
    strcpy(sSelectionMouseClips[0].name,"mc_level_plate_1");
    CHECK(Port_FlashMenuMouseHit(5,"mc_level_plate_1",512,32,0,100,110));
    art43=0;
    /* Ultimate Battle controls use centre, left and right foreground anchors. */
    {
        const char *names[] = {"mc_menu_plate_1", "mc_window_plate01", "mc_sim_plate01",
                               "mc_sim_botan00", "mc_menu_plate_1", "mc_ranking_plate00",
                               "mc_card_anime00", "mc_popo00"};
        s32 scopes[] = {17, 19, 21, 24, 30, 47, 25, 26};
        f32 pivots[] = {256, 0, 512, 512, 256, 0, 256, 256};
        s32 n;
        art43=1;
        for (n=0; n<8; n++) {
            f32 narrow=1333.333f/aspect;
            f32 x=pivots[n]+(110-pivots[n])*narrow+20;
            Port_FlashMenuMouseBegin();
            sSelectionMouseCount=1;
            strcpy(sSelectionMouseClips[0].name,names[n]);
            sSelectionMouseClips[0].scope=scopes[n];
            sSelectionMouseClips[0].prop=prop;
            sSelectionMouseClips[0].flash=&flash;
            CHECK(Port_FlashMenuMouseHit(scopes[n],names[n],128,32,pivots[n],x,105));
            CHECK(!Port_FlashMenuMouseHit(scopes[n],names[n],128,32,pivots[n],x+60,105));
            CHECK(!Port_FlashMenuMouseHit(99,names[n],128,32,pivots[n],x,105));
            sSelectionMouseClips[0].prop.cx.mul[3]=0;
            CHECK(!Port_FlashMenuMouseHit(scopes[n],names[n],128,32,pivots[n],x,105));
        }
        art43=0;
        sSelectionMouseClips[0].prop=prop;
    }
    /* Reset confirmation choices use their own dialog scope. */
    strcpy(sSelectionMouseClips[0].name,"mc_menu_plate_1");
    sSelectionMouseClips[0].scope=4;
    memset(&sSelectionMouseClips[0].drawn,0,sizeof(PortMousePlate));
    Flash_MouseRegion(&sSelectionMouseClips[0].drawn,0,128,32,256,&prop,&flash);
    CHECK(Port_FlashMenuMouseHit(4,"mc_menu_plate_1",128,32,256,130,105));
    CHECK(!Port_FlashMenuMouseHit(9,"mc_menu_plate_1",128,32,256,130,105));
    CHECK(!Port_FlashMenuMouseHit(4,"mc_menu_plate_1",128,64,256,130,140)); /* outside visible button */
    sSelectionMouseClips[0].drawn.valid=0;
    CHECK(!Port_FlashMenuMouseHit(4,"mc_menu_plate_1",128,64,256,130,105));
    /* Live Stereo/Mono quads contain right/bottom transparent padding.
       Use the measured body extent, retaining scale at all page aspects. */
    {
        FlashQuad q={0};PortMousePlate hit;
        int ratios[]={1333,1778,2333},n;
        q.pt[0][0]=318;q.pt[1][0]=574;q.pt[2][0]=318;
        q.pt[0][1]=q.pt[1][1]=232;q.pt[2][1]=296;
        q.uv.u1=256;q.uv.v1=64;q.rgba[3]=128;
        art43=1;sFlashMouseMarker=0x19;
        for(n=0;n<3;n++) {
            float scale=ratios[n]>1333 ? 1333.333f/ratios[n]:1;
            aspect=ratios[n];Flash_MouseOptionsSelectorPiece(&hit,&q);
            CHECK(hit.valid);
            CHECK(__builtin_fabsf(hit.pt[0][0]-(256+(318-256)*scale))<.001f);
            CHECK(__builtin_fabsf(hit.pt[1][0]-(256+(486-256)*scale))<.001f);
            CHECK(hit.pt[2][1]==272);
            CHECK(Flash_MouseRow(&hit,1,256+(480-256)*scale,250)==0);
            CHECK(Flash_MouseRow(&hit,1,256+(500-256)*scale,250)==-1);
            CHECK(Flash_MouseRow(&hit,1,256+(400-256)*scale,280)==-1);
        }
        /* Animated panel scaling is preserved; caption/glow cannot enlarge it. */
        q.pt[1][0]=446;Flash_MouseOptionsSelectorPiece(&hit,&q);
        CHECK(__builtin_fabsf((hit.pt[1][0]-hit.pt[0][0])-84*(1333.333f/aspect))<.001f);
        q.uv.v1=128;Flash_MouseOptionsSelectorPiece(&hit,&q);CHECK(!hit.valid);
        q.uv.v0=128;q.uv.v1=160;Flash_MouseOptionsSelectorPiece(&hit,&q);CHECK(!hit.valid);
        aspect=1778;art43=0;
    }
    /* Three-slice buttons must include both caps, not only the large body. */
    {
        PortMousePlate body={{{160,240},{210,240},{160,268}},1};
        PortMousePlate left={{{140,240},{160,240},{140,268}},1};
        PortMousePlate right={{{210,240},{230,240},{210,268}},1};
        Flash_MouseUnion(&body,&left);Flash_MouseUnion(&body,&right);
        CHECK(Flash_MouseRow(&body,1,141,250)==0);
        CHECK(Flash_MouseRow(&body,1,185,250)==0);
        CHECK(Flash_MouseRow(&body,1,229,250)==0);
        CHECK(Flash_MouseRow(&body,1,231,250)==-1);
    }
    /* Two-choice Options buttons use rendered bounds, independent of the
       caller's estimated 256px region. Gaps and adjacent buttons stay clear. */
    sSelectionMouseCount=2;
    strcpy(sSelectionMouseClips[0].name,"mc_select_plate_1");
    strcpy(sSelectionMouseClips[1].name,"mc_select_plate_2");
    sSelectionMouseClips[0].scope=sSelectionMouseClips[1].scope=68;
    sSelectionMouseClips[0].drawn=(PortMousePlate){{{140,240},{230,240},{140,268}},1};
    sSelectionMouseClips[1].drawn=(PortMousePlate){{{240,240},{330,240},{240,268}},1};
    CHECK(Port_FlashMenuMouseHit(68,"mc_select_plate_1",256,32,256,225,250));
    CHECK(!Port_FlashMenuMouseHit(68,"mc_select_plate_1",256,32,256,235,250));
    CHECK(!Port_FlashMenuMouseHit(68,"mc_select_plate_1",256,32,256,245,250));
    CHECK(Port_FlashMenuMouseHit(68,"mc_select_plate_2",256,32,256,245,250));
    CHECK(!Port_FlashMenuMouseHit(68,"mc_select_plate_2",256,32,256,245,270));
    /* Tournament difficulty spheres use the rendered atlas bounds, including
       child-shape offsets, rather than a guessed top-left 64px rectangle. */
    sSelectionMouseCount=3;
    for(int i=0;i<3;i++) {
        PortSelectionMouseClip *clip=&sSelectionMouseClips[i];
        strcpy(clip->name,i==0 ? "mc_dragonball_off_01":i==1 ? "mc_dragonball_off_02":"mc_dragonball_off_03");clip->scope=31;
        clip->drawn=(PortMousePlate){{{100+80*i,120},{160+80*i,120},{100+80*i,180}},1};
        CHECK(Port_FlashMenuMouseHit(31,clip->name,64,64,512,130+80*i,150));
        CHECK(!Port_FlashMenuMouseHit(31,clip->name,64,64,512,165+80*i,150));
    }
    Port_FlashMenuMouseBegin();
    Flash_MainMousePlate(0, &prop, &flash);
    CHECK(Port_FlashMainMouseRow(121,104)==0);
    CHECK(Port_FlashMainMouseRow(266,115)==0);
    CHECK(Port_FlashMainMouseRow(119,110)==-1);
    CHECK(Port_FlashMainMouseRow(268,110)==-1);
    CHECK(Port_FlashMainMouseRow(150,102)==-1);
    CHECK(Port_FlashMainMouseRow(150,116)==-1);
    prop.mtx.m[5]+=18; Flash_MainMousePlate(1,&prop,&flash);
    CHECK(Port_FlashMainMouseRow(150,120)==-1); /* no overlap through padding */
    CHECK(Port_FlashMainMouseRow(150,123)==1);
    prop.mtx.m[5]-=18;
    /* Skew and scale apply to the crop, not to an axis-aligned estimate. */
    memset(sMainMousePlates,0,sizeof(sMainMousePlates));prop.mtx.m[3]=.25f;
    Flash_MainMousePlate(0,&prop,&flash);
    CHECK(Port_FlashMainMouseRow(121,115)==-1);
    CHECK(Port_FlashMainMouseRow(150,110)==0);
    prop.mtx.m[3]=0;
    memset(sMainMousePlates,0,sizeof(sMainMousePlates));art43=1;
    Flash_MainMousePlate(0,&prop,&flash);
    CHECK(Port_FlashMainMouseRow(100,110)==0);
    CHECK(Port_FlashMainMouseRow(210,110)==-1);
    memset(sMainMousePlates,0,sizeof(sMainMousePlates));page43=1;
    Flash_MainMousePlate(0,&prop,&flash);
    CHECK(Port_FlashMainMouseRow(210,110)==0);
    memset(sMainMousePlates,0,sizeof(sMainMousePlates));prop.cx.mul[3]=0;
    Flash_MainMousePlate(0,&prop,&flash);
    CHECK(Port_FlashMainMouseRow(150,110)==-1);
    prop.cx.mul[3]=1;prop.mtx.m[4]=0;
    Flash_MainMousePlate(0,&prop,&flash);
    CHECK(Port_FlashMainMouseRow(150,103)==-1);
    /* Team cards include their upper/left halves and reject the old offset area. */
    prop.mtx.m[4]=.5f;page43=1;sSelectionMouseCount=1;
    for(int side=0;side<2;side++) {
        PortSelectionMouseClip *clip=&sSelectionMouseClips[0];
        strcpy(clip->name,"mc_team_0");clip->scope=side ? 12:11;
        clip->prop=prop;clip->flash=&flash;
        CHECK(Port_FlashMenuMouseHit(clip->scope,clip->name,64,32,side ? 512:0,95,90));
        CHECK(Port_FlashMenuMouseHit(clip->scope,clip->name,64,32,side ? 512:0,125,117));
        CHECK(!Port_FlashMenuMouseHit(clip->scope,clip->name,64,32,side ? 512:0,130,120));
        CHECK(!Port_FlashMenuMouseHit(clip->scope,clip->name,64,32,side ? 512:0,100,80));
    }
    /* Title plates are 32 pixels tall and pivot around the page centre. */
    prop.mtx.m[0] = prop.mtx.m[4] = 1;
    prop.mtx.m[2] = 0; prop.mtx.m[5] = 284;
    flash.ofs[0] = flash.ofs[1] = 0;
    page43 = 0;
    Flash_MousePlate(sTitleMousePlates, 0, 32, 256, &prop, &flash);
    CHECK(Port_FlashTitleMouseRow(100, 300) == 0);
    CHECK(Port_FlashTitleMouseRow(50, 300) == -1); /* centred wide transform */
    CHECK(Port_FlashTitleMouseRow(100, 316) == -1);
    prop.mtx.m[5] = 332;
    Flash_MousePlate(sTitleMousePlates, 1, 32, 256, &prop, &flash);
    CHECK(Port_FlashTitleMouseRow(100, 340) == 1);
    CHECK(Port_FlashTitleMouseRow(100, 324) == -1);
    page43 = 1;
    Flash_MousePlate(sTitleMousePlates, 1, 32, 256, &prop, &flash);
    CHECK(Port_FlashTitleMouseRow(50, 340) == 1);
    /* Main-menu chevrons use their own 32 x 32 animated clip, left anchored. */
    prop.mtx.m[2] = 180; prop.mtx.m[5] = 70;
    Flash_MouseRegion(sMainMouseArrows, 0, 32, 32, 0, &prop, &flash);
    CHECK(Port_FlashMainMouseArrow(190, 80) == -1);
    CHECK(Port_FlashMainMouseArrow(220, 80) == 0);
    prop.mtx.m[5] = 300;
    Flash_MouseRegion(sMainMouseArrows, 1, 32, 32, 0, &prop, &flash);
    CHECK(Port_FlashMainMouseArrow(190, 310) == 1);
    /* Exit prompt regions are scoped to its own dialog and centred in fights. */
    Port_FlashQuitMouseCapture(1);
    prop.mtx.m[2] = 88; prop.mtx.m[5] = 280;
    gPortMenuMode = 0; art43 = 0;
    Flash_MouseRegion(sQuitMousePlates, 0, 128, 64, 256, &prop, &flash);
    prop.mtx.m[2] = 308;
    Flash_MouseRegion(sQuitMousePlates, 1, 128, 64, 256, &prop, &flash);
    Port_FlashQuitMouseCapture(0);
    CHECK(Port_FlashQuitMouseRow(150 * 16, 300 * 16) == 0);
    CHECK(Port_FlashQuitMouseRow(350 * 16, 300 * 16) == 1);
    CHECK(Port_FlashQuitMouseRow(256 * 16, 300 * 16) == -1);
    Port_FlashQuitMouseCapture(1);
    CHECK(Port_FlashQuitMouseRow(150 * 16, 300 * 16) == -1);
    Port_FlashQuitMouseCapture(0);
    /* Evolution Z arrows use their visible quads, not a digit/container's
       nominal 32px box. Parent-qualified digit arrows stay independent. */
    {
        const char *names[]={"mc_menu_yajirusi_up","mc_menu_yajirusi_down",
            "mc_num_zp_2/mc_yajirusi_num_up","mc_num_zp_2/mc_yajirusi_num_dwn",
            "mc_custom_yajirusi/mc_yajirusi_left","mc_custom_yajirusi/mc_yajirusi_right",
            "mc_menu_yajirusi_up","mc_menu_yajirusi_down"};
        s32 scopes[]={55,56,51,51,53,53,53,53};s32 n;
        sSelectionMouseCount=1;
        for(n=0;n<8;n++) {
            PortSelectionMouseClip *clip=&sSelectionMouseClips[0];
            strcpy(clip->name,names[n]);clip->scope=scopes[n];
            clip->drawn=(PortMousePlate){{{140,80},{164,80},{140,104}},1};
            hovered=0;
            CHECK(Flash_EvoMouseArrow(scopes[n],names[n]));
            CHECK(Port_FlashMenuMouseHit(scopes[n],names[n],32,32,0,152,92) && hovered);
            CHECK(!Port_FlashMenuMouseHit(scopes[n],names[n],32,32,0,200,92));
            clip->drawn.valid=0;
            CHECK(!Port_FlashMenuMouseHit(scopes[n],names[n],32,32,0,152,92));
        }
        CHECK(!Flash_EvoMouseArrow(51,"mc_yajirusi_icon_up"));
        CHECK(!Flash_EvoMouseArrow(53,"mc_custom_yajirusi"));
    }
    /* Reference biography/costume arrows and the draggable thumb use the
       actual visible shape, including its current scale and marker. */
    {
        PortSelectionMouseClip *clip=&sSelectionMouseClips[0];
        f32 u,v;
        sSelectionMouseCount=1;clip->scope=67;
        strcpy(clip->name,"mc_scroll_bar_point");
        clip->drawn=(PortMousePlate){{{450,100},{466,100},{450,120}},1};
        CHECK(Port_FlashMenuMousePosition(67,clip->name,16,32,512,458,110,&u,&v));
        CHECK(u==0.5f && v==0.5f);
        CHECK(Port_FlashMenuMousePosition(67,clip->name,16,32,512,458,130,&u,&v) && v==1.5f);
        strcpy(clip->name,"mc_yajirusi_icon_down");
        CHECK(Port_FlashMenuMouseHit(67,clip->name,32,32,512,458,110));
        CHECK(!Port_FlashMenuMouseHit(67,clip->name,32,32,512,430,110));
        clip->drawn.valid=0;
        CHECK(!Port_FlashMenuMouseHit(67,clip->name,32,32,512,458,110));
    }
    return 0;
}
