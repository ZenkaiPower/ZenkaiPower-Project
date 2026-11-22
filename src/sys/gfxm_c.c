#include "common.h"
/*
 * The Flash-like movie player (0x10AD58..0x10EC18). The data is a converted SWF: see include/sys/gfxm_c.h and
 * docs. Continues src/sys/gfxm_b_c.c (the low-level readers), very likely the same original object.
 * Proper name: flash.c.
 */
#include "sys/gfxm_c.h"
#include "sys/heap.h"
#include "sys/dma.h"
#include "sys/gfx.h"
#include "sys/gfxm_b_b.h"

extern void *memcpy(void *dst, const void *src, u32 size);
extern void *memset(void *dst, s32 c, u32 n);
extern s32 strcmp(const char *a, const char *b);
extern u32 strlen(const char *s);
#ifdef PORT
#include "menu/port_menu_mouse.h"
extern s32 strncmp(const char *a, const char *b, u32 n);
#include "menu/port_options_art.h"
#include "menu/port_video_options.h"
extern void Port_GsMarker(s32 effect);
extern int MenuTranslation_PortTextCopy(const char *id,const char *fallback,char *out,s32 capacity);
/* Retain the effective draw anchor, including inherited parent anchors and
   per-image overrides, so mouse geometry follows the exact same transform. */
static s32 sFlashMouseMarker;
static void Flash_MenuMarker(s32 effect) {
    sFlashMouseMarker=effect;
    Port_GsMarker(effect);
}
extern s32 Port_TitleMainChars43(void);
extern s32 Port_Menus43(void);
extern s32 gPortMenuMode;
extern s32 Port_PageAspectMilli(void);
static s32 sMenuArtDrawDepth;
static s32 sMenuArtScreen; /* 1: title; 2: main; 3: header; 4: dialog; 5: saga; 6: episodes; 7: story result; 8: GT scenery; 9: Duel menu */
static s32 sOptionsPlateCopy;
static s32 sOptionsViewport,sOptionsViewportDrawn;
void Port_FlashOptionsViewport(s32 enabled) {
    sOptionsViewport=enabled;
    if(enabled) sOptionsViewportDrawn=0;
}
static s32 Flash_OptionsViewportRow(const char *name) {
    if(!sOptionsViewport || sMenuArtScreen!=68 || strncmp(name,"mc_menu_plate_",14)!=0) return 0;
    /* Mode 2 inserts the added Controller row, preserving native rows. */
    s32 mode=sOptionsViewport;
    if(!sOptionsViewportDrawn) {
        extern void Port_OptionsTopDraw(void);
        sOptionsViewportDrawn=1;sOptionsViewport=0;
        Port_OptionsTopDraw();
        sOptionsViewport=mode;
    }
    return mode==1;
}
static const char *sOptionsPlateLabel;
static char sOptionsTranslatedLabel[1024];
static s32 sOptionsCaptionLayer=-1;
static u32 sOptionsCaptionTint[2];
static s32 sOptionsCaptionAlpha[2];
static f32 sOptionsCaptionX[2],sOptionsCaptionY[2];
static s32 sOptionsVideoFooter;
static s32 sOptionsExitCaption;
static s32 sOptionsVideoPicker=-1;
static s32 sOptionsPickerCaption;
static s32 sOptionsPickerBoundsValid;
static f32 sOptionsPickerBounds[4];
static const char *sOptionsHeading;
void Port_FlashOptionsHeading(const char *text) { sOptionsHeading=text; }
void Port_FlashOptionsVideoPicker(s32 row) { sOptionsVideoPicker=row; }
void Port_FlashOptionsVideoFooter(s32 enabled) { sOptionsVideoFooter=enabled; }
static s32 sTrainingGotenTree;
static s32 sTrainingCloudTree;
static s32 sTrainingCloudFraction;
static s32 sTrainingSlowFlight;
static f32 sTrainingFlightPhase;
static s32 sTrainingFlightTick;
static struct {
    FlashShape *shape;
    s32 id;
    FlashMtx matrix;
} sTrainingFlightPrevious[16];
static s32 sTrainingFlightPreviousCount;
static s32 sHistoryGuestDrawDepth;
static s32 sTitleMenuEntriesVisible;

/* Use the authored unselected plate (512 x 64), not the union of its
   children: the selected glow is twice as tall and overlaps adjacent rows. */
static s32 sMainMouseDraw, sTitleMouseDraw, sQuitMouseDraw;
typedef struct PortMousePlate { f32 pt[3][2]; s32 valid; } PortMousePlate;
static PortMousePlate sMainMousePlates[6], sTitleMousePlates[2], sMainMouseArrows[2], sQuitMousePlates[2];

static s32 Flash_MouseRow(PortMousePlate *plates, s32 count, f32 x, f32 y) {
    s32 row, best = -1;
    f32 nearest = 0.0f;
    for (row = 0; row < count; row++) {
        f32 ax, ay, bx, by, dx, dy, det, u, v, distance;
        if (!plates[row].valid) continue;
        ax = plates[row].pt[1][0] - plates[row].pt[0][0];
        ay = plates[row].pt[1][1] - plates[row].pt[0][1];
        bx = plates[row].pt[2][0] - plates[row].pt[0][0];
        by = plates[row].pt[2][1] - plates[row].pt[0][1];
        dx = x - plates[row].pt[0][0];
        dy = y - plates[row].pt[0][1];
        det = ax * by - ay * bx;
        if (det > -0.0001f && det < 0.0001f) continue;
        u = (dx * by - dy * bx) / det;
        v = (ax * dy - ay * dx) / det;
        if (u < 0.0f || u >= 1.0f || v < 0.0f || v >= 1.0f) continue;
        /* Animation can bring two plates together: prefer the nearest row
           centre instead of whichever rectangle was tested first. */
        distance = (v - 0.5f) * (v - 0.5f);
        if (best < 0 || distance < nearest) {
            best = row;
            nearest = distance;
        }
    }
    return best;
}

/* Button artwork can contain multiple independently transformed slices. */
static void Flash_MouseUnion(PortMousePlate *bounds,const PortMousePlate *piece) {
    f32 x0,y0,x1,y1;
    s32 i;
    if(!piece->valid) return;
    if(!bounds->valid) { *bounds=*piece;return; }
    x0=x1=bounds->pt[0][0];y0=y1=bounds->pt[0][1];
    for(i=0;i<8;i++) {
        const PortMousePlate *p=i<4 ? bounds:piece;
        s32 n=i%4;
        f32 x=n<3 ? p->pt[n][0]:p->pt[1][0]+p->pt[2][0]-p->pt[0][0];
        f32 y=n<3 ? p->pt[n][1]:p->pt[1][1]+p->pt[2][1]-p->pt[0][1];
        if(x<x0) x0=x;if(x>x1) x1=x;
        if(y<y0) y0=y;if(y>y1) y1=y;
    }
    bounds->pt[0][0]=bounds->pt[2][0]=x0;
    bounds->pt[1][0]=x1;
    bounds->pt[0][1]=bounds->pt[1][1]=y0;
    bounds->pt[2][1]=y1;
}

static void Flash_MouseDrawnPiece(PortMousePlate *piece,const FlashQuad *quad) {
    s32 corner;
    f32 narrow=1.0f;
    f32 pivot=sFlashMouseMarker==0x1A ? 0.0f : sFlashMouseMarker==0x1B ? 512.0f : 256.0f;
    if(sFlashMouseMarker>=0x19 && sFlashMouseMarker<=0x1B && Port_PageAspectMilli()>1340 &&
       (!gPortMenuMode || (Port_TitleMainChars43() && !Port_Menus43())))
        narrow=1333.333f/(f32)Port_PageAspectMilli();
    for(corner=0;corner<3;corner++) {
        piece->pt[corner][0]=pivot+(quad->pt[corner][0]-pivot)*narrow;
        piece->pt[corner][1]=quad->pt[corner][1];
    }
    piece->valid=quad->rgba[3]!=0;
}

/* The native picker body is 168x40 visible pixels in a padded 256x64
   texture. Its caption and 256x128 glow are separate quads, not hit targets.
   Crop before the renderer's anchor transform, preserving animation scale. */
static void Flash_MouseOptionsSelectorPiece(PortMousePlate *piece,const FlashQuad *quad) {
    FlashQuad body;
    s32 axis;
    memset(piece,0,sizeof(*piece));
    if(quad->uv.u0!=0 || quad->uv.v0!=0 || quad->uv.u1!=256 || quad->uv.v1!=64) return;
    body=*quad;
    for(axis=0;axis<2;axis++) {
        body.pt[1][axis]=quad->pt[0][axis]+(quad->pt[1][axis]-quad->pt[0][axis])*(168.0f/256.0f);
        body.pt[2][axis]=quad->pt[0][axis]+(quad->pt[2][axis]-quad->pt[0][axis])*(40.0f/64.0f);
    }
    Flash_MouseDrawnPiece(piece,&body);
}

s32 Port_FlashMainMouseRow(f32 x, f32 y) {
    s32 row = Flash_MouseRow(sMainMousePlates, 6, x, y);
    if (row >= 0) PortMenuMouse_SetHover(1);
    return row;
}

s32 Port_FlashTitleMouseRow(f32 x, f32 y) {
    s32 row = Flash_MouseRow(sTitleMousePlates, 2, x, y);
    if (row >= 0) PortMenuMouse_SetHover(1);
    return row;
}

s32 Port_FlashMainMouseArrow(f32 x, f32 y) {
    s32 row = Flash_MouseRow(sMainMouseArrows, 2, x, y);
    if (row >= 0) PortMenuMouse_SetHover(1);
    return row < 0 ? 0 : row == 0 ? -1 : 1;
}

void Port_FlashQuitMouseCapture(s32 on) {
    sQuitMouseDraw = on;
    if (on) memset(sQuitMousePlates, 0, sizeof(sQuitMousePlates));
}

s32 Port_FlashQuitMouseRow(s32 x16, s32 y16) {
    return Flash_MouseRow(sQuitMousePlates, 2, (f32)x16 / 16.0f, (f32)y16 / 16.0f);
}

void FlashDraw_TransformQuad(FlashQuad *q, FlashMtx *mtx, FlashCxform *cx, FlashProp *prop, s32 *ofs);

static void Flash_MouseLocalRegion(PortMousePlate *plates, s32 row, f32 left, f32 top, f32 width, f32 height, f32 pivot, FlashProp *prop, Flash *flash) {
    FlashQuad q;
    FlashMtx identity = {{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}};
    FlashCxform color = {{1.0f, 1.0f, 1.0f, 1.0f}, {0, 0, 0, 0}};
    s32 i;
    f32 narrow = 1.0f;
    memset(&q, 0, sizeof(q));
    q.pt[0][0]=q.pt[2][0]=left;q.pt[1][0]=left+width;
    q.pt[0][1]=q.pt[1][1]=top;q.pt[2][1]=top+height;
    q.rgba[3] = 128;
    FlashDraw_TransformQuad(&q, &identity, &color, prop, flash->ofs);
    if (q.rgba[3] == 0) return;
    if (Port_PageAspectMilli() > 1340 && (!gPortMenuMode || (Port_TitleMainChars43() && !Port_Menus43())))
        narrow = 1333.333f / (f32)Port_PageAspectMilli();
    for (i = 0; i < 3; i++) {
        plates[row].pt[i][0] = pivot + (q.pt[i][0] - pivot) * narrow;
        plates[row].pt[i][1] = q.pt[i][1];
    }
    plates[row].valid = 1;
}

static void Flash_MouseRegion(PortMousePlate *plates, s32 row, f32 width, f32 height, f32 pivot, FlashProp *prop, Flash *flash) {
    Flash_MouseLocalRegion(plates,row,0,0,width,height,pivot,prop,flash);
}

typedef struct PortSelectionMouseClip {
    char name[64];
    s32 scope;
    FlashProp prop;
    Flash *flash;
    PortMousePlate drawn;
} PortSelectionMouseClip;
static PortSelectionMouseClip sSelectionMouseClips[512];
static s32 sSelectionMouseCount, sSelectionMouseScope;
static const char *sSelectionMouseParent;
static s32 sOptionMouseDrawClip = -1;
static s32 sItemMousePlateBody;
/* Arrow children repeat under each digit/editor group. Keep their parent in
   the hit name and capture visible quads, including the child's shape offset. */
static const char *Flash_MouseChildName(const char *name) {
    const char *child=name;
    while(*name) {if(*name=='/') child=name+1;name++;}
    return child;
}
static s32 Flash_EvoMouseArrow(s32 scope,const char *name) {
    const char *child=Flash_MouseChildName(name);
    if(scope==53 && (strcmp(child,"mc_menu_yajirusi_up")==0 || strcmp(child,"mc_menu_yajirusi_down")==0)) return 1;
    if(scope==51 || scope==53) return strcmp(child,"mc_yajirusi_left")==0 ||
        strcmp(child,"mc_yajirusi_right")==0 || strcmp(child,"mc_yajirusi_up")==0 ||
        strcmp(child,"mc_yajirusi_down")==0 || strcmp(child,"mc_yajirusi_num_up")==0 ||
        strcmp(child,"mc_yajirusi_num_dwn")==0;
    return (scope==55 || scope==56) &&
        (strcmp(child,"mc_menu_yajirusi_up")==0 || strcmp(child,"mc_menu_yajirusi_down")==0);
}
static s32 Flash_CharRefMouseControl(s32 scope,const char *name) {
    return scope==67 && (strcmp(name,"mc_scroll_bar_point")==0 ||
        strcmp(name,"mc_yajirusi_icon_up")==0 || strcmp(name,"mc_yajirusi_icon_down")==0 ||
        strcmp(name,"mc_yajirusi_num_up")==0 || strcmp(name,"mc_yajirusi_num_dwn")==0);
}
static s32 Flash_ItemMouseScope(s32 scope) {
    return scope==53 || scope==55 || scope==56;
}
static s32 Flash_ItemMouseThumb(s32 scope,const char *name) {
    return Flash_ItemMouseScope(scope) && strcmp(name,"mc_scroll_bar_point")==0;
}
static s32 Flash_ItemMouseRow(s32 scope,const char *name) {
    return Flash_ItemMouseScope(scope) && strncmp(name,"mc_list_plate_",14)==0 &&
        name[14]>='1' && name[14]<='8' && name[15]==0;
}
void Port_FlashMenuMouseBegin(void) { sSelectionMouseCount = 0; }
int Port_FlashMenuMousePosition(int scope,const char *name,float width,float height,
                               float pivot,float x,float y,float *u,float *v) {
    s32 i;
    for(i=sSelectionMouseCount-1;i>=0;i--) {
        PortSelectionMouseClip *clip=&sSelectionMouseClips[i];
        PortMousePlate plate;
        f32 ax,ay,bx,by,dx,dy,det;
        if(clip->scope!=scope || strcmp(clip->name,name)!=0) continue;
        memset(&plate,0,sizeof(plate));
        if(scope==68 || Flash_CharRefMouseControl(scope,name) || Flash_ItemMouseThumb(scope,name)) plate=clip->drawn;
        else Flash_MouseRegion(&plate,0,width,height,pivot,&clip->prop,clip->flash);
        if(!plate.valid) continue;
        ax=plate.pt[1][0]-plate.pt[0][0];ay=plate.pt[1][1]-plate.pt[0][1];
        bx=plate.pt[2][0]-plate.pt[0][0];by=plate.pt[2][1]-plate.pt[0][1];
        dx=x-plate.pt[0][0];dy=y-plate.pt[0][1];det=ax*by-ay*bx;
        if(det>-0.00001f && det<0.00001f) continue;
        *u=(dx*by-dy*bx)/det;*v=(ax*dy-ay*dx)/det;
        return 1;
    }
    return 0;
}

int Port_ItemListMouseScroll(PortMenuMouse *mouse, int scope, int rows, int count, int *top) {
    static int dragScope, startTop, touch;
    static float startX, startY;
    const char *thumb = "mc_scroll_bar_point";
    float u, v;
    int held = PortMenuMouse_LeftHeld();
    if (dragScope != scope || mouse->cancel || !(touch ? mouse->dragHeld || mouse->dragEnd : held))
        dragScope = 0;
    if (count <= rows ||
        !Port_FlashMenuMousePosition(scope, thumb, 16, 32, 256, mouse->x, mouse->y, &u, &v)) {
        dragScope = 0;
        return 0;
    }
    int over = u >= 0 && u <= 1 && v >= 0 && v <= 1;
    if (over) PortMenuMouse_SetHover(1);
    if (mouse->dragStart || (mouse->confirm && held && over)) {
        float x = mouse->dragStart ? mouse->dragX : mouse->x;
        float y = mouse->dragStart ? mouse->dragY : mouse->y;
        float grabU = u, grabV = v;
        float padding = 0, verticalPadding = 0;
        if (mouse->dragStart) {
            float unused, paddedV;
            dragScope = 0;
            Port_FlashMenuMousePosition(scope, thumb, 16, 32, 256, x, y, &grabU, &grabV);
            if (Port_FlashMenuMousePosition(scope, thumb, 16, 32, 256, x, y + 5, &unused, &paddedV))
                verticalPadding = paddedV - grabV;
            padding = 0.5f;
        }
        if (grabU >= -padding && grabU <= 1 + padding &&
            grabV >= -verticalPadding && grabV <= 1 + verticalPadding) {
            dragScope = scope;
            startTop = *top;
            startX = x;
            startY = y;
            touch = mouse->dragStart;
        }
    }
    if (dragScope == scope) {
        float grabU, grabV = v;
        Port_FlashMenuMousePosition(scope, thumb, 16, 32, 256, startX, startY, &grabU, &grabV);
        /* The thumb spans one page. Compare both points in the same draw
           transform so rounding the moving thumb's offset cannot cause drift. */
        float delta = (v - grabV) * rows;
        int next = startTop + (int)(delta + (delta >= 0 ? 0.5f : -0.5f));
        *top = next < 0 ? 0 : next > count - rows ? count - rows : next;
        if (touch && !mouse->dragHeld) dragScope = 0;
        PortMenuMouse_SetHover(1);
        return 1;
    }
    return over && mouse->confirm;
}

int Port_FlashMenuMouseHit(int scope, const char *name, float width, float height,
                          float pivot, float x, float y) {
    s32 i;
    for (i = sSelectionMouseCount - 1; i >= 0; --i) {
        PortSelectionMouseClip *clip = &sSelectionMouseClips[i];
        /* Active movies also expose their visible navigation arrows on hover;
           their click handlers still decide whether to navigate. */
        if (clip->scope == scope && scope >= 5 && scope <= 12 &&
            strncmp(clip->name, "mc_yajirusi", 11) == 0) {
            PortMousePlate arrow;
            memset(&arrow, 0, sizeof(arrow));
            Flash_MouseRegion(&arrow, 0, 32, 32, pivot, &clip->prop, clip->flash);
            if (Flash_MouseRow(&arrow, 1, x, y) == 0) PortMenuMouse_SetHover(1);
        }
        if (clip->scope == scope && (strcmp(clip->name, name) == 0 ||
            (scope == 10 && strcmp(name, "mc_stage_music") == 0 &&
             strcmp(clip->name, "mc_map_chip_05") == 0))) {
            PortMousePlate plate;
            memset(&plate, 0, sizeof(plate));
            FlashProp hitProp = clip->prop;
            f32 left=0,top=0;
            if(Flash_ItemMouseRow(scope,name) || Flash_CharRefMouseControl(scope,name) || Flash_EvoMouseArrow(scope,name) || (scope==33 && (strcmp(name,"mc_yajirusi_l")==0 || strcmp(name,"mc_yajirusi_r")==0)) ||
               (scope==31 && strncmp(name,"mc_dragonball_off_",18)==0) ||
               (scope==62 && strncmp(name,"mc_input_code_",14)==0) ||
               (scope==68 && Port_OptionsMouseControl(name)) ||
               (scope==4 && (strcmp(name,"mc_menu_plate_1")==0 || strcmp(name,"mc_menu_plate_2")==0))) {
                if(Flash_MouseRow(&clip->drawn,1,x,y)==0) { PortMenuMouse_SetHover(1);return 1; }
                continue;
            }
            /* Portraits are centred on their clip origin, not top-left.
               Their 64px art overlaps at 60px spacing; use the inner cell. */
            if (strncmp(name, "mc_chara_chip_", 14) == 0) {
                left=top=-30;
                width = height = 60.0f;
            } else if ((scope==11 || scope==12 || scope==32 || scope==57) &&
                       strncmp(name,"mc_team_",8)==0 && name[8]>='0' && name[8]<='4' && name[9]==0) {
                /* Team cards are centred on their clip: the backing child
                   starts at (-32,-36), not at the card's centre. */
                left=-32;top=-36;
                width=64.0f;height=72.0f;
            } else if (scope == 10 && strncmp(name, "mc_map_chip_", 12) == 0) {
                /* Stage thumbnails also use a centred origin. */
                hitProp.mtx.m[2] -= 32.0f * (hitProp.mtx.m[0] + hitProp.mtx.m[1]);
                hitProp.mtx.m[5] -= 32.0f * (hitProp.mtx.m[3] + hitProp.mtx.m[4]);
                width = height = 64.0f;
            } else if (scope == 10 && strcmp(name, "mc_stage_music") == 0) {
                /* The seventh grid cell is music: follow the rightmost stage's
                   composed transform, including reel motion and widescreen. */
                hitProp.mtx.m[2] += 32.0f * hitProp.mtx.m[0] - 32.0f * hitProp.mtx.m[1];
                hitProp.mtx.m[5] += 32.0f * hitProp.mtx.m[3] - 32.0f * hitProp.mtx.m[4];
                width = height = 64.0f;
            }
            Flash_MouseLocalRegion(&plate,0,left,top,width,height,pivot,&hitProp,clip->flash);
            if (Flash_MouseRow(&plate, 1, x, y) == 0) {
                PortMenuMouse_SetHover(1);
                return 1;
            }
        }
    }
    return 0;
}

/* Test native page coordinates before widescreen narrowing exposes parked art. */
static s32 Flash_MenuQuadOffPage(const FlashQuad *quad) {
    s32 left = 1, right = 1, corner;
    for (corner = 0; corner < 4; corner++) {
        if (quad->pt[corner][0] >= 0.0f) left = 0;
        if (quad->pt[corner][0] <= 512.0f) right = 0;
    }
    return left || right;
}

static void Flash_MousePlate(PortMousePlate *plates, s32 row, f32 height, f32 pivot, FlashProp *prop, Flash *flash) {
    Flash_MouseRegion(plates, row, 512.0f, height, pivot, prop, flash);
}

static void Flash_MainMousePlate(s32 row, FlashProp *prop, Flash *flash) {
    /* Exclude the transparent padding and selected glow of the 512x64 art. */
    Flash_MouseLocalRegion(sMainMousePlates,row,20,6,295,26,0,prop,flash);
}

/* Buy-list background shape at root depth 2 animates from x=-520 to zero.
   Its centred controls need the same extra page travel as the wide backing. */
static f32 FlashShop_BuyTravel(Flash *flash) {
    if (flash->root->depths >= 2 && flash->root->slots[1].shape != NULL) {
        f32 travel = flash->root->slots[1].shape->mtx.m[2] / 520.0f;
        if (travel < -1.0f) travel = -1.0f;
        if (travel > 0.0f) travel = 0.0f;
        return 256.0f * ((f32)Port_PageAspectMilli() / 1333.333f - 1.0f) * travel;
    }
    return 0.0f;
}

/* Return the anchor marker for each foreground clip tree. Keep a whole option
   plate together so its backing, icon and label receive the same transform. */
#ifdef PORT
#include "menu/port_training_art.h"
#include "menu/port_data_center_art.h"
#include "menu/port_character_reference_art.h"
#endif

static s32 FlashClip_MenuArtMarker(const char *name) {
    if (sMenuArtScreen == 69) return 0x19; /* complete Shenron foreground */
    if (sMenuArtScreen == 68) return Port_OptionsArtMarker(name);
    if (sMenuArtScreen == 67) return Port_CharacterReferenceArtMarker(name);
    if (sMenuArtScreen == 61) return Port_DataCenterArtMarker(name);
    if (sMenuArtScreen >= 62 && sMenuArtScreen <= 66)
        return Port_DataCenterSubmenuMarker(sMenuArtScreen, name);
    if (sMenuArtScreen == 60) return Port_TrainingArtMarker(name);
    if (sMenuArtScreen == 54) {
        if (strncmp(name, "menu_plate_", 11) == 0) return 0x1A;
        if (strncmp(name, "mc_pay_num_", 11) == 0 || strncmp(name, "mc_item_num_", 12) == 0)
            return 0x1B;
        return 0; /* scenery inside the shop window keeps its native scissor */
    }
    if (sMenuArtScreen == 55 || sMenuArtScreen == 56)
        return strncmp(name, "mc_", 3) == 0 ? 0x19 : 0;
    if (sMenuArtScreen == 53) {
        /* Editable counterpart of the battle selector's item inspection. */
        if (strcmp(name, "mc_custom_yajirusi") == 0) return 0x1A;
        if (strncmp(name, "mc_ability_limit_", 17) == 0) return 0x1B;
        return strncmp(name, "mc_", 3) == 0 ? 0x19 : 0;
    }
    if (sMenuArtScreen == 51) {
        /* Evolution Z customization shares the selector's left portrait.
           Keep the left ability diagram together; editing controls sit right. */
        /* The limit-up movie contains a wide dimmer/banner, centred lettering
           and a separate right-side capacity readout; mark its children. */
        if (strcmp(name, "mc_ability_limit_up") == 0) return 0;
        if (strcmp(name, "mc_single_chara_l") == 0 ||
            strncmp(name, "mc_name_", 8) == 0 || strncmp(name, "mc_form_", 8) == 0 ||
            strncmp(name, "mc_status_ability_", 18) == 0 ||
            strcmp(name, "mc_status_attribute") == 0) return 0x1A;
        return (strncmp(name, "mc_", 3) == 0 || strncmp(name, "menu_plate_", 11) == 0) ? 0x1B : 0;
    }
    if (sMenuArtScreen == 50) {
        /* Evolution Z landing screen: Krillin on the right, menu and
           explanation controls on the left; cloud/lantern scenery stays wide. */
        if (strncmp(name, "mc_bg_", 6) == 0) return 0;
        if (strncmp(name, "mc_guide_cririn", 15) == 0) return 0x1B;
        return strncmp(name, "mc_", 3) == 0 ? 0x1A : 0;
    }
    if (sMenuArtScreen == 46 || sMenuArtScreen == 47) {
        if (strncmp(name, "mc_bg_", 6) == 0) return 0;
        return strncmp(name, "mc_", 3) == 0 ? 0x1A : 0;
    }
    if (sMenuArtScreen == 49) {
        if (strcmp(name, "mc_single_chara_l") == 0) return 0x1A;
        if (strcmp(name, "mc_single_chara_r") == 0) return 0x1B;
        if (strcmp(name, "mc_vs") == 0) return 0x19;
        return 0;
    }
    if (sMenuArtScreen == 45) {
        /* Mission 100 results: Android 17 and the complete score/reward
           foreground share the left pivot, like the mission selector. */
        if (strncmp(name, "mc_bg_", 6) == 0) return 0;
        return strncmp(name, "mc_", 3) == 0 ? 0x1A : 0;
    }
    if (sMenuArtScreen == 44) {
        if (strcmp(name, "mc_single_chara_l") == 0) return 0x1A;
        if (strcmp(name, "mc_single_chara_r") == 0) return 0x1B;
        if (strcmp(name, "mc_vs") == 0 || strncmp(name, "mc_round_num", 12) == 0) return 0x19;
        return 0;
    }
    if (sMenuArtScreen == 41 && strcmp(name, "mc_taikaimoji_red") == 0)
        return 0x19; /* complete central tournament emblem animation */
    if (sMenuArtScreen == 40 && strcmp(name, "mc_menu_title_plate") == 0)
        return 0x1B; /* tournament name and round share the right screen edge */
    if (sMenuArtScreen == 39) {
        /* Tournament matchups follow Versus: player-side groups stay at their
           own screen edge for the entire native entry/exit animation. */
        if (strcmp(name, "mc_vs") == 0) return 0x19;
        if (strcmp(name, "mc_single_chara_l") == 0 ||
            strcmp(name, "mc_name_plate_l") == 0 || strcmp(name, "mc_form_plate_l") == 0 ||
            strcmp(name, "mc_name_text_l") == 0 || strcmp(name, "mc_form_text_l") == 0 ||
            strcmp(name, "mc_vs_number_l") == 0 || strcmp(name, "mc_victory_or_defeat_l") == 0)
            return 0x1A;
        if (strcmp(name, "mc_single_chara_r") == 0 ||
            strcmp(name, "mc_name_plate_r") == 0 || strcmp(name, "mc_form_plate_r") == 0 ||
            strcmp(name, "mc_name_text_r") == 0 || strcmp(name, "mc_form_text_r") == 0 ||
            strcmp(name, "mc_vs_number_r") == 0 || strcmp(name, "mc_victory_or_defeat_r") == 0)
            return 0x1B;
        return 0;
    }
    if (sMenuArtScreen == 31) {
        /* World Tour settings: Satan left; helicopter, controls and clock right. */
        if (strcmp(name, "mc_guide_satan") == 0) return 0x1A;
        if (strcmp(name, "mc_heri_body") == 0) return 0x1B;
        if (strcmp(name, "mc_timer") == 0) return 0x1B;
        if (strncmp(name, "mc_menu_", 8) == 0 || strncmp(name, "mc_taikai", 9) == 0 ||
            strncmp(name, "mc_plate_", 9) == 0 || strncmp(name, "mc_dragonball_", 14) == 0 ||
            strncmp(name, "mc_yajirusi", 11) == 0 || strcmp(name, "mc_timer") == 0 ||
            strcmp(name, "mc_suji_01") == 0) return 0x1B;
        return 0;
    }
    if (sMenuArtScreen == 35) {
        /* Entrant selector: match existing one-player selector component anchors. */
        if (strcmp(name, "mc_single_chara_l") == 0 ||
            strncmp(name, "mc_name_", 8) == 0 || strncmp(name, "mc_form_", 8) == 0)
            return 0x1A;
        if (strncmp(name, "mc_chara_entry_", 15) == 0 ||
            strncmp(name, "mc_entry_", 9) == 0) return 0x1B;
        return 0;
    }
    if (sMenuArtScreen == 24) {
        /* Sim Dragon board: monitor and its top readouts stay on the left;
           action buttons belong to the right edge, status remains left. */
        if (strcmp(name, "mc_sim_monita") == 0 ||
            strcmp(name, "mc_point_plate") == 0 ||
            strcmp(name, "mc_icon_timer") == 0 || strcmp(name, "mc_time_text") == 0)
            return 0x1A;
        if (strncmp(name, "mc_sim_botan", 12) == 0) return 0x1B;
    }
    if (sMenuArtScreen == 22) {
        /* Ultimate Battle's shared selector movie: one player on the left,
           Android guides on the right. Do not reuse Versus image/depth IDs. */
        if (strcmp(name, "mc_guide_17go") == 0 || strcmp(name, "mc_guide_18go") == 0)
            return 0x1B;
        if (strcmp(name, "mc_single_chara_l") == 0 ||
            strcmp(name, "mc_name_plate_l") == 0 || strcmp(name, "mc_form_plate_l") == 0 ||
            strcmp(name, "mc_name_text_l") == 0 || strcmp(name, "mc_form_text_l") == 0 ||
            strcmp(name, "mc_dp_window_l") == 0 || strcmp(name, "mc_dp_max_l") == 0 ||
            strncmp(name, "mc_dp_total_l_", 14) == 0) return 0x1A;
        return 0;
    }
    if (sMenuArtScreen == 21) {
        /* Sim Dragon entry: complete guide and control groups share the right edge. */
        if (strcmp(name, "mc_guide_18go") == 0 ||
            strncmp(name, "mc_sim_plate", 12) == 0 ||
            strncmp(name, "mc_senreki_", 11) == 0 ||
            strncmp(name, "mc_misson_", 10) == 0 ||
            strncmp(name, "mc_menu_yajirusi", 15) == 0 ||
            strncmp(name, "mc_pt_", 6) == 0 ||
            strncmp(name, "mc_jyuni_", 9) == 0 ||
            strncmp(name, "mc_episode_next", 15) == 0 ||
            strcmp(name, "mc_icon_play1") == 0 || strcmp(name, "mc_icon_star") == 0 ||
            strcmp(name, "mc_setumeikari") == 0 || strcmp(name, "mc_kazari01") == 0)
            return 0x1B;
        return 0;
    }
    if (sMenuArtScreen == 18 || sMenuArtScreen == 19 || sMenuArtScreen == 20) {
        /* Survival, Mission 100 and Disc Fusion share left-anchored foregrounds. */
        if (strcmp(name, "mc_guide_17go") == 0) return 0x1A;
        if (strncmp(name, "mc_bg_", 6) == 0) return 0;
        if (strncmp(name, "mc_disk_plate", 13) == 0 ||
            strcmp(name, "mc_icon_play1") == 0 ||
            strncmp(name, "mc_window_", 10) == 0 ||
            strncmp(name, "mc_mission", 10) == 0 ||
            strncmp(name, "mc_gekiha_", 10) == 0 ||
            strncmp(name, "mc_time_", 8) == 0 ||
            strncmp(name, "mc_score_", 9) == 0 ||
            strncmp(name, "mc_rank", 7) == 0 ||
            strncmp(name, "mc_pt_", 6) == 0 ||
            strncmp(name, "mc_yajirusi", 11) == 0 ||
            strcmp(name, "mc_scroll_yazirusi") == 0 ||
            strcmp(name, "mc_newrecord_text") == 0) return 0x1A;
        return 0;
    }
    if (sMenuArtScreen == 17) {
        /* Landing screen: guides retain their screen-edge spacing; entries centre. */
        if (strcmp(name, "mc_guide_17go") == 0) return 0x1A;
        if (strcmp(name, "mc_guide_18go") == 0) return 0x1B;
        if (strncmp(name, "mc_menu_plate_", 14) == 0 ||
            strcmp(name, "mc_icon_play1") == 0) return 0x19;
        return 0;
    }
    if (sMenuArtScreen == 15 || sMenuArtScreen == 16 || sMenuArtScreen == 24 || sMenuArtScreen == 28) {
        /* Ultimate Battle foreground only. Never select the mc_bg_* scenery.
           Whole parent trees keep masks, portraits and callback labels together. */
        static const char *const foreground[] = {
            "mc_menu_plate", "mc_menu_yajirusi", "mc_icon_play", "mc_mission_",
            "mc_misson_", "mc_window_plate", "mc_ranking_plate", "mc_senreki_",
            "mc_sim_plate", "mc_disk_plate", "mc_gekiha_", "mc_time_", "mc_score_",
            "mc_pt_", "mc_rank", "mc_newrecord", "mc_point_", "mc_bonus_",
            "mc_henkan_", "mc_zpoint_", "mc_win_text", "mc_lose_text", "mc_item_",
            "mc_clear_", "mc_day_text", "mc_sim_text", "mc_text_kari",
            "mc_yes_no_", "mc_yajirusi", "mc_scroll_yazirusi", "mc_episode_next",
            "mc_setumeikari", "mc_kazari", "mc_blue_yajirusi"
        };
        s32 i;
        if (strcmp(name, "mc_guide_17go") == 0 || strcmp(name, "mc_guide_18go") == 0)
            return 0x1B;
        for (i = 0; i < (s32)(sizeof(foreground) / sizeof(foreground[0])); i++)
            if (strncmp(name, foreground[i], strlen(foreground[i])) == 0)
                return sMenuArtScreen == 28 ? 0x1B : 0x1A;
        if (sMenuArtScreen == 16 || sMenuArtScreen == 24) {
            static const char *const board[] = {
                "mc_icon_timer", "mc_chara_pt_plate", "mc_at_plate", "mc_df_plate",
                "mc_card_anime", "mc_fukidasi_popo", "mc_popo", "mc_sim_botan",
                "mc_sim_monita", "mc_plate_potara", "mc_round_num", "mc_mark",
                "mc_star_anime", "mc_level_up", "mc_surprise_icon", "mc_shot"
            };
            for (i = 0; i < (s32)(sizeof(board) / sizeof(board[0])); i++)
                if (strncmp(name, board[i], strlen(board[i])) == 0) return 0x1A;
        }
    }
    if (sMenuArtScreen == 13) {
        s32 header = sTitleMenuEntriesVisible ? 0x1A : 0x1B;
        s32 character = sTitleMenuEntriesVisible ? 0x1B : 0x1A;
        if (strncmp(name, "mc_ability_limit_", 17) == 0) return header;
        if (strcmp(name, "mc_select_plate") == 0) return header;
        if (strncmp(name, "mc_list_plate_", 14) == 0) return 0x19;
        if (strcmp(name, "mc_name_plate") == 0 || strcmp(name, "mc_form_plate") == 0 ||
            strcmp(name, "mc_name_text") == 0 || strcmp(name, "mc_form_text") == 0) return character;
    }
    if (sMenuArtScreen == 1) {
        return (strncmp(name, "mc_menu_plate_", 14) == 0 ||
                strcmp(name, "mc_taikenban") == 0) ? 0x19 : 0;
    }
    if (sMenuArtScreen == 2) {
        if (strncmp(name, "mc_menu_plate_", 14) == 0 ||
            strcmp(name, "mc_menu_text_off") == 0 || strcmp(name, "mc_menu_text_on") == 0 ||
            strcmp(name, "mc_icon_off") == 0 || strcmp(name, "mc_icon_play") == 0 ||
            strncmp(name, "mc_menu_yajirusi", 16) == 0) {
            return 0x1A; /* options and arrows */
        }
        if (strncmp(name, "mc_ch_a_db_", 11) == 0) {
            return 0x1A; /* keep the Dragon Balls aligned with their left-anchored holder */
        }
        return (strncmp(name, "mc_guide_", 9) == 0 ||
                strcmp(name, "mc_uide_ghn_bdl_hair") == 0) ? 0x19 : 0;
    }
    if (sMenuArtScreen == 3) {
        if (strcmp(name, "mc_window_up_02") == 0) {
            return 0x1B; /* right-hand corner of the heading */
        }
        if (strcmp(name, "mc_window_up_01") == 0 ||
            strcmp(name, "mc_title") == 0 ||
            strcmp(name, "mc_title_icon") == 0 ||
            strcmp(name, "mc_icon") == 0) {
            return 0x1A;
        }
    }
    if (sMenuArtScreen == 5) {
        /* Left-entering groups must keep x=0 at the screen edge throughout
           their animation, including every backing, label and badge. */
        if (strncmp(name, "mc_menu_plate_", 14) == 0 ||
            strncmp(name, "mc_level_plate_", 15) == 0 ||
            strncmp(name, "mc_scenario_", 12) == 0 ||
            strcmp(name, "mc_icon_new") == 0 || strcmp(name, "mc_saru") == 0) return 0x1A;
        if (strncmp(name, "mc_tassei_num_", 14) == 0) return 0x1B;
        if (strcmp(name, "mc_guide_gokuu") == 0 || strncmp(name, "mc_guide_0", 10) == 0) {
            return 0x19;
        }
        if (strcmp(name, "mc_yajirusi_left") == 0) return 0x1A;
        if (strcmp(name, "mc_yajirusi_right") == 0) return 0x1A;
    }
    if (sMenuArtScreen == 6) {
        /* Preview mask, picture, frame, battle title and badges share the list's
           left edge. Keep guides entering from beyond the right edge. */
        if (strncmp(name, "mc_menu_plate_", 14) == 0 ||
            strcmp(name, "mc_battle_mask") == 0 || strcmp(name, "mc_image_l_1") == 0 ||
            strcmp(name, "mc_battle_text_1") == 0 || strcmp(name, "mc_clear") == 0 ||
            strncmp(name, "mc_yajirusi_", 12) == 0) return 0x1A;
        if (strncmp(name, "mc_guide_0", 10) == 0) return 0x1B;
        if (strcmp(name, "mc_scenario_text") == 0 || strcmp(name, "mc_scenario") == 0 ||
            strcmp(name, "mc_episode_next") == 0 ||
            strncmp(name, "mc_episode_text_", 16) == 0) return 0x19;
    }
    if (sMenuArtScreen == 7) {
        if (strncmp(name, "mc_chara_", 9) == 0) return 0x1B;
        if (strncmp(name, "mc_point_text_", 14) == 0 ||
            strncmp(name, "mc_get_point_", 13) == 0 ||
            strncmp(name, "mc_total_point_", 15) == 0) return 0x1A;
        if (strncmp(name, "mc_result_text_", 15) == 0 ||
            strcmp(name, "mc_battle_text") == 0) return 0x1A;
    }
    if (sMenuArtScreen == 9) {
        if (strcmp(name, "mc_guide_nappa") == 0 || strcmp(name, "mc_guide_bejita") == 0 ||
            strcmp(name, "set_4_1p2p") == 0) return 0x1B;
        if (strncmp(name, "mc_dp_", 6) == 0) return 0x1A;
        if (strncmp(name, "mc_battle_set_text_", 19) == 0) return 0x19;
        if (strncmp(name, "mc_set_", 7) == 0) {
            /* Value-choice panels enter from the right; primary values stay
               with the centred settings labels and backing. */
            return (name[8] == '_' || strncmp(name, "mc_set_4a_", 10) == 0 ||
                    strncmp(name, "mc_set_4b_", 10) == 0) ? 0x1B : 0x19;
        }
        if (strncmp(name, "mc_menu_plate_", 14) == 0 || strncmp(name, "mc_mode_", 8) == 0) return 0x1A;
    }
    if (sMenuArtScreen == 10) {
        if (strcmp(name, "mc_face_mask_l") == 0 || strcmp(name, "mc_single_chara_l") == 0) return 0x1A;
        if (strcmp(name, "mc_face_mask_r") == 0 || strcmp(name, "mc_single_chara_r") == 0) return 0x1B;
        if (strcmp(name, "mc_name_plate_l") == 0 || strcmp(name, "mc_form_plate_l") == 0 ||
            strcmp(name, "mc_name_text_l") == 0 || strcmp(name, "mc_form_text_l") == 0 ||
            strcmp(name, "mc_dp_window_l") == 0 || strcmp(name, "mc_dp_max_l") == 0 ||
            strncmp(name, "mc_dp_total_l_", 14) == 0) return 0x1A;
        if (strcmp(name, "mc_name_plate_r") == 0 || strcmp(name, "mc_form_plate_r") == 0 ||
            strcmp(name, "mc_name_text_r") == 0 || strcmp(name, "mc_form_text_r") == 0 ||
            strcmp(name, "mc_dp_window_r") == 0 || strcmp(name, "mc_dp_max_r") == 0 ||
            strncmp(name, "mc_dp_total_r_", 14) == 0) return 0x1B;
        if (strncmp(name, "mc_map_", 7) == 0 || strcmp(name, "mc_bgm_now") == 0 ||
            strncmp(name, "mc_player_1p_", 13) == 0 || strncmp(name, "mc_time_", 8) == 0 ||
            strcmp(name, "mc_vs") == 0 || strncmp(name, "mc_yajirusi_", 12) == 0) return 0x19;
    }
    if (sMenuArtScreen == 8 && strcmp(name, "mc_gil") == 0) return 0x1A;
    return 0;
}

static s32 FlashShape_MenuArtMarker(s32 image) {
    if (sMenuArtScreen == 69) return 0x19; /* loose wish frame and row backings */
    if (sMenuArtScreen == 68) return Port_OptionsImage(image);
    if (sMenuArtScreen == 67) return Port_CharacterReferenceImage(image);
    if (sMenuArtScreen >= 62 && sMenuArtScreen <= 66)
        return Port_DataCenterSubmenuImage(sMenuArtScreen, image);
    if (sMenuArtScreen == 60 && image > 0) return 0x1B;
    /* Shop window, clouds, Oolong and flower share the background window's
       proportional region; the surrounding room still fills the page. */
    if (sMenuArtScreen == 54 && image >= 0 && image <= 3) return 0x19;
    if (sMenuArtScreen == 54 && image >= 9 && image <= 14) return 0x1B;
    if (sMenuArtScreen == 55 && image >= 2 && image <= 20) return 0x19;
    if (sMenuArtScreen == 56 && image >= 3 && image <= 17) return 0x19;
    if (sMenuArtScreen == 53) {
        if (image == 0 || image == 1 || image == 9 || image == 10) return 0; /* expanded backings */
        if (image == 6) return 0x1A; /* equipment action caption */
        if (image >= 2 && image <= 5) return 0x1B; /* capacity */
        if (image >= 7 && image <= 25) return 0x19;
    }
    if (sMenuArtScreen == 51) {
        if (image == 0 || image == 39) return 0; /* dimmer and limit-up banner */
        if (image == 40) return 0x19; /* Ability Limit Up lettering */
        if ((image >= 1 && image <= 6) || (image >= 21 && image <= 24)) return 0x1A;
        if (image == 41) return 0x1A; /* portrait divider */
        if (image >= 7 && image <= 42) return 0x1B;
    }
    if (sMenuArtScreen == 50 && image >= 4 && image <= 6) return 0x1B;
    if (sMenuArtScreen == 50 && image >= 7 && image <= 12) return 0x1A;
    if (sMenuArtScreen == 46 && ((image >= 2 && image <= 31) || (image >= 35 && image <= 37))) return 0x1A;
    if (sMenuArtScreen == 47 && image == 30) return 0x19; /* challenger announcement */
    if (sMenuArtScreen == 47 && ((image >= 4 && image <= 23) || image >= 34)) return 0x1A;
    if (sMenuArtScreen == 49 && (image == 8 || image == 9)) return 0x19;
    /* Mission 100 results (451/3/1): cover standalone punctuation, backings
       and labels too; 0/1 are scenery, 39..41 belong to the guide tree. */
    if (sMenuArtScreen == 45 && ((image >= 2 && image <= 38) ||
                               (image >= 42 && image <= 45))) return 0x1A;
    if (sMenuArtScreen == 44 && image >= 9 && image <= 12) return 0x19;
    if (sMenuArtScreen == 41) {
        /* TourBg's optional emblem textures: Big Tournament has four extra
           scenery slots before the shared three-piece decoration. */
        s32 first = sTitleMenuEntriesVisible == 1 ? 7 : 3;
        if (image >= first && image < first + 3) return 0x19;
    }
    /* Tournament top header: its backing and standalone title/round copies
       must use the same right pivot as mc_menu_title_plate. */
    if (sMenuArtScreen == 40 && (image == 9 || image == 36 || image == 37)) return 0x1B;
    /* Tournament VS decoration has unparented layers as well as mc_vs.
       Both 256-square emblem layers must share its centre transform. */
    if (sMenuArtScreen == 39 && (image == 1 || image == 4)) return 0x19;
    /* World Tour welcome illustration uses two unparented image halves,
       separate from both the helicopter and the right-hand menu plates. */
    if (sMenuArtScreen == 31 && (image == 1 || image == 2)) return 0x19;
    /* World Tour main settings movie (450/1/15), foreground image slots only. */
    if (sMenuArtScreen == 31 && image >= 7 && image <= 30) return 0x1B;
    /* Entrant selector (450/2/3): entrant panel/badges right; portrait
       divider and name/form backings left. Image 5 is the portrait itself. */
    if (sMenuArtScreen == 35 && (image == 0 || image == 1 || image == 18)) return 0x1B;
    if (sMenuArtScreen == 35 && (image == 4 || (image >= 6 && image <= 9))) return 0x1A;
    /* Sim Dragon results, 451/3/3: standalone panel, score/time punctuation,
       captions and guide pieces share the same right anchor as result rows. */
    if (sMenuArtScreen == 28 && image >= 4 && image <= 48) return 0x1B;
    /* Sim Dragon board: root shape 6 is the dark action-column backing,
       outside mc_sim_botan*. Match the buttons' right-edge proportions. */
    if (sMenuArtScreen == 24 && image == 6) return 0x1B;
    /* Sim Dragon entry, 451/5/1: panel/entry/readout images. Guide pieces
       31-34 inherit their complete parent; scenery 0-3 remains wide. */
    if (sMenuArtScreen == 21 && ((image >= 4 && image <= 30) ||
                               (image >= 35 && image <= 39))) return 0x1B;
    /* Movie-local foreground slots: Mission 100 (451/2/1) and Disc Fusion
       (451/6/1). Their guide images and scenery are deliberately excluded. */
    if (sMenuArtScreen == 19 && ((image >= 4 && image <= 32) ||
                               (image >= 36 && image <= 38))) return 0x1A;
    if (sMenuArtScreen == 20 && ((image >= 4 && image <= 11) ||
                               (image >= 15 && image <= 17))) return 0x1A;
    /* Survival movie (451/2/2): frame, entry plates and readouts. Guide
       images 23-25 inherit the guide tree; scenery images 0-3 remain wide. */
    if (sMenuArtScreen == 18 && ((image >= 4 && image <= 22) ||
                               (image >= 26 && image <= 28))) return 0x1A;
    /* Landing movie, archive 451 section 1 resource 1: entry backing/text
       strips also occur outside the named option trees. Keep them centred. */
    if (sMenuArtScreen == 17 && image >= 2 && image <= 7) return 0x19;
    if (sMenuArtScreen == 13 && image >= 15 && image <= 18) {
        /* Original panel: caption 15, slash 16, large digits 17, small digits 18.
           All four follow the mirrored header, including unparented shapes. */
        return sTitleMenuEntriesVisible ? 0x1A : 0x1B;
    }
    /* Unparented shapes need the same anchor as their neighbouring clips.
       Main-menu image 42 is the Dragon Ball holder, beside the seven clips. */
    if (sMenuArtScreen == 1 &&
        (image == 4 || image == 6 || image == 9 || (image >= 10 && image <= 23))) {
        return 0x19;
    }
    if (sMenuArtScreen == 2 && image == 42) {
        return 0x1A;
    }
    if (sMenuArtScreen == 3 && image >= 0) {
        return 0x1A; /* unparented parts of the heading */
    }
    if (sMenuArtScreen == 5) {
        /* Shared saga/difficulty frame, saga label bar, difficulty heading. */
        if (image == 37 || image == 76 || image == 82) return 0x1A;
        /* Achievement caption and panel (including %) match the digit clips. */
        if (image == 77 || image == 78) return 0x1B;
    }
    if (sMenuArtScreen == 6) {
        /* Root shapes: saga title backing, thumbnail strip, preview frame;
           the mirrored halves of the scrolling story panel. */
        if (image == 6 || image == 7) return 0x1A;
        if (image == 5 || image == 38) return 0x19;
    }
    if (sMenuArtScreen == 7) {
        if (image == 3) return 0x1A; /* episode-title backing */
        if (image == 5 || image == 7) return 0x1A; /* point label/number backings */
    }
    if (sMenuArtScreen == 9) {
        if (image == 41 || image == 42 || image == 43) return 0x1A;
        if (image >= 44 && image <= 46) return 0x19;
        if (image == 47 || image == 48) return 0x1B;
    }
    if (sMenuArtScreen == 10 && (image == 38 || image == 47 || image == 48))
        return 0x19; /* stage-name backing and both pre-fight VS emblem layers */
    return 0;
}
#endif
extern s32 atoi(const char *s);

void FlashTl_TagPlace(FlashTl *tl, u8 *data, s32 *ofs);
void FlashTl_TagPlace2(FlashTl *tl, u8 *data, s32 *ofs);
void FlashTl_TagRemove(FlashTl *tl, u8 *data, s32 *ofs);
void FlashTl_TagRemove2(FlashTl *tl, u8 *data, s32 *ofs);
void FlashTl_TagActions(FlashTl *tl, u8 *data, s32 *ofs);
void FlashSlot_SetShape(FlashSlot *slot, u16 id, FlashMtx *mtx, FlashCxform *cx, Flash *flash);
void FlashSlot_SetClip(FlashSlot *slot, u16 id, char *name, FlashMtx *mtx, FlashCxform *cx, Flash *flash);
void FlashSlot_Clear(FlashSlot *slot);
void FlashSlot_Move(FlashSlot *slot, FlashMtx *mtx, FlashCxform *cx);
void FlashShape_Free(FlashShape *shape);
void FlashClip_Reset(FlashClip *clip, s32 arg);

/* Sets a colour transform to the identity. */
void Flash_CxformIdentity(FlashCxform *cx) {
    s32 i;

    for (i = 0; i < 4; i++) {
        cx->mul[i] = 1.0f;
        cx->add[i] = 0;
    }
}

/* Reads a bit-packed colour transform at data + *ofs: a flag byte (bits 0..3 multipliers present, 4..7 offsets
   present), a 4-bit field width, then the fields (multipliers in 8.8). Leaves *ofs on the next whole byte. */
void Flash_ReadCxform(u8 *data, FlashCxform *cx, s32 *ofs) {
    u32 bitPos = 0;
    u8 flags;
    u8 bits;
    u32 i;

    Flash_CxformIdentity(cx);
    flags = data[*ofs];
    *ofs += 1;
    if (flags != 0) {
        bits = Flash_ReadBits(data + *ofs, &bitPos, 4);
        if (flags & 0xF) {
            for (i = 0; i < 4; i++) {
                if ((flags >> i) & 1) {
                    cx->mul[i] = (s16)Flash_ReadSBits(data + *ofs, &bitPos, bits) * (1.0f / 256.0f);
                }
            }
        }
        if (flags & 0xF0) {
            for (i = 0; i < 4; i++) {
                if (flags & (0x10U << i)) {
                    cx->add[i] = Flash_ReadSBits(data + *ofs, &bitPos, bits);
                }
            }
        }
    }
    *ofs += bitPos >> 3;
    if (bitPos & 7) {
        *ofs += 1;
    }
}

/* Runs the tag list of one frame block (name, u16 size, then tags: u16 code + data, ended by code 1 = ShowFrame).
   A block of size 0 is the end of the timeline: stops and steps back onto the last frame. */
void FlashTl_RunFrame(FlashTl *tl, u8 *frame) {
    u16 size;
    u16 code;
    s32 ofs;

    ofs = 0;
    size = 0;
    code = 0;
    if (frame == NULL) {
        goto end;
    }
    while (frame[ofs++] != 0) {
    }
    memcpy(&size, frame + ofs, 2);
    ofs += 2;
    if (size == 0) {
    end:
        tl->state &= ~FLASH_TL_PLAY;
        if (tl->frame != 0) {
            tl->frame--;
        }
        return;
    }
    do {
        memcpy(&code, frame + ofs, 2);
        ofs += 2;
        switch (code) {
        case 4:
            FlashTl_TagPlace(tl, frame, &ofs);
            break;
        case 26:
            FlashTl_TagPlace2(tl, frame, &ofs);
            break;
        case 5:
            FlashTl_TagRemove(tl, frame, &ofs);
            break;
        case 28:
            FlashTl_TagRemove2(tl, frame, &ofs);
            break;
        case 12:
            FlashTl_TagActions(tl, frame, &ofs);
            break;
        case 1:
            break;
        }
    } while (code != 1);
}

/* Tag 4 (PlaceObject): u16 depth, u8 kind (4 shape, 6 sprite), u16 id, matrix, and for a sprite a colour transform. */
void FlashTl_TagPlace(FlashTl *tl, u8 *data, s32 *ofs) {
    FlashMtx mtx;
    FlashCxform cx;
    u16 depth;
    u16 id;
    u8 kind;

    memcpy(&depth, data + *ofs, 2);
    *ofs += 2;
    kind = data[*ofs];
    *ofs += 1;
    memcpy(&id, data + *ofs, 2);
    *ofs += 2;
    switch (kind) {
    case 4:
        Flash_ReadMtx(data, &mtx, ofs);
        Flash_CxformIdentity(&cx);
        FlashSlot_SetShape(&tl->slots[depth - 1], id, &mtx, &cx, tl->owner);
        break;
    case 6:
        Flash_ReadMtx(data, &mtx, ofs);
        Flash_ReadCxform(data, &cx, ofs);
        FlashSlot_SetClip(&tl->slots[depth - 1], id, NULL, &mtx, &cx, tl->owner);
        break;
    }
}

/* Tag 26 (PlaceObject2): u8 flags (2 new object, 4 matrix, 8 colour transform, 0x20 name), u16 depth; a new object
   adds u8 kind and u16 id. Without flag 2 the object already at that depth is moved. */
void FlashTl_TagPlace2(FlashTl *tl, u8 *data, s32 *ofs) {
    FlashMtx mtx;
    FlashCxform cx;
    char name[0x40];
    u16 depth;
    u16 id;
    u8 flags;
    u8 kind;
    FlashMtx *pm;
    FlashCxform *pc;

    Flash_MtxIdentity(&mtx);
    Flash_CxformIdentity(&cx);
    flags = data[*ofs];
    *ofs += 1;
    memcpy(&depth, data + *ofs, 2);
    *ofs += 2;
    if (flags & 2) {
        kind = data[*ofs];
        *ofs += 1;
        memcpy(&id, data + *ofs, 2);
        *ofs += 2;
        if (flags & 4) {
            Flash_ReadMtx(data, &mtx, ofs);
        }
        if (flags & 8) {
            Flash_ReadCxform(data, &cx, ofs);
        }
        if (flags & 0x20) {
            memset(name, 0, sizeof(name));
            Flash_ReadString(name, data, ofs);
        }
        switch (kind) {
        case 4:
            FlashSlot_SetShape(&tl->slots[depth - 1], id, &mtx, &cx, tl->owner);
            break;
        case 6:
            FlashSlot_SetClip(&tl->slots[depth - 1], id, name, &mtx, &cx, tl->owner);
            break;
        }
    } else {
        pm = NULL;
        pc = NULL;
        if (flags & 4) {
            Flash_ReadMtx(data, &mtx, ofs);
            pm = &mtx;
        }
        if (flags & 8) {
            Flash_ReadCxform(data, &cx, ofs);
            pc = &cx;
        }
        FlashSlot_Move(&tl->slots[depth - 1], pm, pc);
    }
}

/* Tag 5 (RemoveObject): u16 depth, u8 kind, u16 id: frees the shape or resets the sprite at that depth. */
void FlashTl_TagRemove(FlashTl *tl, u8 *data, s32 *ofs) {
    u16 depth;
    u16 id;
    u8 kind;
    FlashSlot *slot;

    memcpy(&depth, data + *ofs, 2);
    *ofs += 2;
    kind = data[*ofs];
    *ofs += 1;
    memcpy(&id, data + *ofs, 2);
    *ofs += 2;
    slot = &tl->slots[depth - 1];
    switch (kind) {
    case 4:
        if (slot->shape != NULL) {
            FlashShape_Free(slot->shape);
            slot->shape = NULL;
        }
        break;
    case 6:
        if (slot->clip != NULL) {
            FlashClip_Reset(slot->clip, 1);
            slot->clip = NULL;
        }
        break;
    }
}

/* Tag 28 (RemoveObject2): u16 depth: empties that depth. */
void FlashTl_TagRemove2(FlashTl *tl, u8 *data, s32 *ofs) {
    u16 depth;

    memcpy(&depth, data + *ofs, 2);
    *ofs += 2;
    FlashSlot_Clear(&tl->slots[depth - 1]);
}

/* Action list (tag 12) while a jump replays earlier frames: every action is skipped except GetURL "pad". */
void FlashTl_SkipActions(FlashTl *tl, u8 *data, s32 *ofs) {
    char url[0x40];
    char arg[0x40];
    char name[0x40];
    char label[0x100];
    u8 code;

    do {
        code = data[*ofs];
        *ofs += 1;
        switch (code) {
        case 0x81:
            *ofs += 2;
            break;
        case 0x83:
            memset(url, 0, sizeof(url));
            memset(arg, 0, sizeof(arg));
            Flash_ReadString(url, data, ofs);
            Flash_ReadString(arg, data, ofs);
            if (strcmp(url, "pad") == 0) {
                if (strcmp(arg, "true") == 0) {
                    tl->owner->flags |= FLASH_PAD;
                } else if (strcmp(arg, "false") == 0) {
                    tl->owner->flags &= ~FLASH_PAD;
                }
            }
            break;
        case 0x8A:
            *ofs += 3;
            break;
        case 0x8B:
            memset(name, 0, sizeof(name));
            Flash_ReadString(name, data, ofs);
            break;
        case 0x8C:
            memset(label, 0, 0xFF);
            Flash_ReadString(label, data, ofs);
            break;
        case 4:
        case 5:
        case 6:
        case 7:
            break;
        }
    } while (code != 0);
}

FlashClip *FlashClipList_FindByName(FlashClipList *list, char *name);

/* Tag 12 (DoAction): runs SWF actions until the end code 0. 0x81 GotoFrame, 4 NextFrame, 5 PrevFrame, 6 Play,
   7 Stop, 0x83 GetURL (the game's command channel), 0x8A WaitForFrame (skipped), 0x8B SetTarget, 0x8C GotoLabel. */
void FlashTl_TagActions(FlashTl *tl, u8 *data, s32 *ofs) {
    FlashTl *cur;
    u8 code;

    if (tl->state & FLASH_TL_SEEK) {
        FlashTl_SkipActions(tl, data, ofs);
        return;
    }
    cur = tl;
    do {
        code = data[*ofs];
        *ofs += 1;
        switch (code) {
        case 4:
            cur->frame = cur->frame + 1;
            cur->state |= FLASH_TL_HOLD;
            break;
        case 5:
            if (cur->frame != 0) {
                cur->frame--;
            }
            cur->state |= FLASH_TL_HOLD;
            break;
        case 6:
            cur->state |= FLASH_TL_PLAY;
            break;
        case 7:
            cur->state &= ~FLASH_TL_PLAY;
            break;
        case 0x81: {
            u16 frame;

            memcpy(&frame, data + *ofs, 2);
            *ofs += 2;
            cur->frame = frame - 1;
            cur->state |= FLASH_TL_GOTO;
            break;
        }
        case 0x83: {
            char url[0x40];
            char arg[0x40];

            memset(url, 0, sizeof(url));
            memset(arg, 0, sizeof(arg));
            Flash_ReadString(url, data, ofs);
            Flash_ReadString(arg, data, ofs);
            if (strcmp(url, "pad") == 0) {
                if (strcmp(arg, "true") == 0) {
                    tl->owner->flags |= FLASH_PAD;
                } else if (strcmp(arg, "false") == 0) {
                    tl->owner->flags &= ~FLASH_PAD;
                }
            } else if (strcmp(url, "trig") == 0) {
                tl->owner->trig |= 1 << atoi(arg);
            } else if (strcmp(url, "se") == 0) {
                tl->owner->se |= 1 << atoi(arg);
            } else if (strcmp(url, "trigger") == 0) {
                if (strcmp(arg, "end") == 0) {
                    tl->owner->flags |= FLASH_END;
                }
            }
            break;
        }
        case 0x8A:
            *ofs += 3;
            break;
        case 0x8B: {
            char name[0x40];
            FlashClip *clip;

            memset(name, 0, sizeof(name));
            Flash_ReadString(name, data, ofs);
            if (name[0] == 0) {
                cur = tl;
            } else {
                clip = FlashClipList_FindByName(tl->owner->clips, name);
                cur = clip != NULL ? &clip->tl : tl;
            }
            break;
        }
        case 0x8C: {
            char label[0x100];
            s32 index;

            memset(label, 0, 0xFF);
            Flash_ReadString(label, data, ofs);
            index = Flash_FindName(cur->frames, label);
            if (index >= 0) {
                cur->frame = index;
                cur->state |= FLASH_TL_GOTO;
            }
            break;
        }
        }
    } while (code != 0);
}

/* Sets a timeline up on a frame record (root: tag 7, sprite: a record of tag 6) and allocates its depth slots. */
void FlashTl_Init(FlashTl *tl, u8 *record, Flash *owner) {
    FlashTag hdr;

    memset(&hdr, 0, sizeof(hdr));
    memset(tl, 0, sizeof(FlashTl));
    memcpy(&hdr, record, sizeof(hdr));
    record += 8;
    tl->owner = owner;
    tl->depths = hdr.count;
    tl->frames = record;
    if (tl->slots == NULL) {
        tl->slots = Heap_Alloc(tl->depths * 8, 0x20, 0, 2);
        memset(tl->slots, 0, tl->depths * 8);
    }
}

/* Frees the depth slots and clears the timeline. */
void FlashTl_Term(FlashTl *tl) {
    if (tl->slots != NULL) {
        Heap_Free(tl->slots);
        tl->slots = NULL;
    }
    memset(tl, 0, sizeof(FlashTl));
}

/* Back to frame 0, stopped, every depth emptied. The second argument is not used. */
void FlashTl_Rewind(FlashTl *tl, s32 unused) {
    s32 i;

    tl->prev = 0;
    tl->frame = 0;
    tl->unk14 = 0;
    tl->state = 0;
    for (i = 0; i < tl->depths; i++) {
        FlashSlot_Clear(&tl->slots[i]);
    }
}

void FlashSlot_Advance(FlashSlot *slot, u32 count);

/* Runs `count` ticks: a pending jump first replays the frames up to the target (from 0, or from the frame the
   jump left when FLASH_TL_KEEP), then the current frame's tags run and the play head steps. `children` ticks the
   sprites placed on it after each frame (when 0 they are ticked once at the end); `force` does so even when this
   timeline is stopped. */
void FlashTl_Advance(FlashTl *tl, u32 count, u8 children, u8 force) {
    u32 n;
    u32 i;
    u32 target;
    u32 start;

    for (n = 0; n < count; n++) {
        if (tl->state & (FLASH_TL_PLAY | FLASH_TL_GOTO)) {
            if (tl->state & FLASH_TL_GOTO) {
                target = tl->frame;
                if (tl->state & FLASH_TL_KEEP) {
                    start = tl->prev;
                } else {
                    FlashTl_Rewind(tl, 1);
                    start = 0;
                }
                tl->state |= FLASH_TL_PLAY | FLASH_TL_SEEK;
                for (i = start; i < target; i++) {
                    FlashTl_RunFrame(tl, Flash_SkipNamed(tl->frames, i));
                }
                tl->frame = target;
                tl->state &= ~(FLASH_TL_GOTO | FLASH_TL_SEEK | FLASH_TL_KEEP);
            }
            FlashTl_RunFrame(tl, Flash_SkipNamed(tl->frames, tl->frame));
            if (!(tl->state & FLASH_TL_GOTO)) {
                if (!(tl->state & FLASH_TL_HOLD)) {
                    if ((u8)(tl->state & FLASH_TL_PLAY)) {
                        tl->frame++;
                    }
                }
            }
            if (children) {
                for (i = 0; i < tl->depths; i++) {
                    FlashSlot_Advance(&tl->slots[i], count);
                }
            }
        } else if (children && force) {
            for (i = 0; i < tl->depths; i++) {
                FlashSlot_Advance(&tl->slots[i], count);
            }
        }
    }
    if (!children) {
        for (i = 0; i < tl->depths; i++) {
            FlashSlot_Advance(&tl->slots[i], count);
        }
    }
}

void FlashSlot_Draw(FlashSlot *slot, FlashProp *prop, Flash *flash);

/* Draws every depth, lowest first. `prop` is the drawing state of the sprite this timeline belongs to (NULL: root). */
void FlashTl_Draw(FlashTl *tl, FlashProp *prop) {
    s32 i;

    for (i = 0; i < tl->depths; i++) {
        FlashSlot_Draw(&tl->slots[i], prop, tl->owner);
    }
}

/* Jumps to a frame label. `restart` rebuilds the display list from frame 0, else from the current frame. */
void FlashTl_GotoLabel(FlashTl *tl, char *label, u8 restart) {
    s32 index = Flash_FindName(tl->frames, label);

    if (index >= 0) {
        tl->prev = tl->frame;
        tl->frame = index;
        tl->state |= FLASH_TL_GOTO;
        if (!restart) {
            tl->state |= FLASH_TL_KEEP;
        }
    }
}

/* Moves the play head by `step` frames and plays; any step but +1 goes through the jump path. */
void FlashTl_StepFrames(FlashTl *tl, s32 step) {
    s32 frame = tl->frame + step;

    if (frame >= 0 && Flash_SkipNamed(tl->frames, frame) != NULL) {
        tl->frame = frame;
        tl->state |= FLASH_TL_PLAY;
        if (step != 1) {
            tl->state |= FLASH_TL_GOTO;
        }
    }
}

FlashShape *FlashShape_Create(FlashShapePool *pool, u16 id, FlashMtx *mtx, FlashCxform *cx);
FlashClip *FlashClip_Start(FlashClipList *list, u16 id, char *name, FlashMtx *mtx, FlashCxform *cx);
void FlashShape_Set(FlashShape *shape, FlashMtx *mtx, FlashCxform *cx);
void FlashClip_Set(FlashClip *clip, FlashMtx *mtx, FlashCxform *cx);

/* Puts a shape on a depth, replacing the shape that was there. */
void FlashSlot_SetShape(FlashSlot *slot, u16 id, FlashMtx *mtx, FlashCxform *cx, Flash *flash) {
    if (slot->shape != NULL) {
        FlashShape_Free(slot->shape);
        slot->shape = NULL;
    }
    slot->shape = FlashShape_Create(flash->shapes, id, mtx, cx);
}

/* Puts a sprite on a depth, replacing the sprite that was there. */
void FlashSlot_SetClip(FlashSlot *slot, u16 id, char *name, FlashMtx *mtx, FlashCxform *cx, Flash *flash) {
    if (slot->clip != NULL) {
        FlashClip_Reset(slot->clip, 1);
        slot->clip = NULL;
    }
    slot->clip = FlashClip_Start(flash->clips, id, name, mtx, cx);
}

/* Empties a depth: frees the shape, marks the sprite unused (its state is kept). */
void FlashSlot_Clear(FlashSlot *slot) {
    if (slot->shape != NULL) {
        FlashShape_Free(slot->shape);
        slot->shape = NULL;
    }
    if (slot->clip != NULL) {
        slot->clip->flags &= ~FLASH_CLIP_USED;
        slot->clip = NULL;
    }
}

/* Gives what is on a depth a new matrix and / or colour transform (NULL: keep). */
void FlashSlot_Move(FlashSlot *slot, FlashMtx *mtx, FlashCxform *cx) {
    if (slot->shape != NULL) {
        FlashShape_Set(slot->shape, mtx, cx);
    }
    if (slot->clip != NULL) {
        FlashClip_Set(slot->clip, mtx, cx);
    }
}

#ifdef PORT
/* Save the flying shapes before a real timeline tick. Drawing blends between
 * consecutive authored poses while the fractional clock continues each frame. */
static void FlashTraining_SaveFlight(FlashClip *clip) {
    s32 depth;
    sTrainingFlightPreviousCount = 0;
    if (clip->tl.state & FLASH_TL_GOTO) return;
    for (depth = 0; depth < clip->tl.depths && sTrainingFlightPreviousCount < 16; depth++) {
        FlashShape *shape = clip->tl.slots[depth].shape;
        if (shape != NULL && (shape->id == 1 || shape->id == 3)) {
            s32 index = sTrainingFlightPreviousCount++;
            sTrainingFlightPrevious[index].shape = shape;
            sTrainingFlightPrevious[index].id = shape->id;
            sTrainingFlightPrevious[index].matrix = shape->mtx;
        }
    }
}

static FlashMtx FlashTraining_FlightMatrix(FlashShape *shape) {
    FlashMtx result = shape->mtx;
    s32 index, term;
    if (!sTrainingGotenTree || Port_PageAspectMilli() <= 1333) return result;
    for (index = 0; index < sTrainingFlightPreviousCount; index++) {
        if (sTrainingFlightPrevious[index].shape == shape &&
            sTrainingFlightPrevious[index].id == shape->id) {
            FlashMtx *previous = &sTrainingFlightPrevious[index].matrix;
            f32 dx = result.m[2] - previous->m[2];
            f32 dy = result.m[5] - previous->m[5];
            /* A loop reset is a teleport, not a flight across the page. */
            if (dx > 128.0f || dx < -128.0f || dy > 128.0f || dy < -128.0f) break;
            for (term = 0; term < 9; term++)
                result.m[term] = previous->m[term] +
                    (result.m[term] - previous->m[term]) * sTrainingFlightPhase;
            break;
        }
    }
    return result;
}
#endif

/* Ticks the sprite on a depth. */
void FlashSlot_Advance(FlashSlot *slot, u32 count) {
    FlashClip *clip = slot->clip;

    if (clip != NULL) {
#ifdef PORT
        /* Only Goten's child movie follows the extended flight's pacing. The menu root, input,
         * guides and transitions advance normally; pending seeks are immediate. */
        if (strcmp(clip->name, "mc_ch_goten") == 0) {
            if (sTrainingSlowFlight) {
                if (!sTrainingFlightTick && !(clip->tl.state & FLASH_TL_GOTO)) return;
                FlashTraining_SaveFlight(clip);
            } else sTrainingFlightPreviousCount = 0;
        }
#endif
        if (clip->flags & FLASH_CLIP_FREE_RUN) {
            FlashTl_Advance(&clip->tl, count, 1, 1);
        } else {
            FlashTl_Advance(&clip->tl, count, 1, 0);
        }
    }
}

void FlashShape_Draw(FlashShape *shape, FlashProp *prop, Flash *flash);

static inline s32 Flash_Clamp255(s32 v) {
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

static inline s16 Flash_ClampAdd(s32 v) {
    if (v >= 0) {
        if (v > 255) {
            v = 255;
        }
        return v;
    }
    return 0;
}

/* Draws what is on a depth: the shape, then the sprite. The sprite's drawing state is combined with its
   parent's (matrix product, colour transforms multiplied and added, the parent's overrides applied). */
void FlashSlot_Draw(FlashSlot *slot, FlashProp *parent, Flash *flash) {
    FlashProp prop;
    FlashMtx m;
    FlashClip *clip = slot->clip;
    s32 i;
    s32 j;
#ifdef PORT
    s32 menuArt = 0;
    s32 historyGuest = 0;
    if(clip && sMenuArtScreen==68 && sOptionsVideoPicker!=-1 && !sOptionsPlateCopy &&
       (strcmp(clip->name,"mc_select_plate_1")==0 || strcmp(clip->name,"mc_select_plate_2")==0)) {
        int value=clip->name[16]-'1';
        sOptionsPlateCopy=1;sOptionsPickerCaption=1;sOptionsPickerBoundsValid=0;
        if(sOptionsVideoPicker==-3) {
            MenuTranslation_PortTextCopy(value ? "options/value/on":"options/value/off",
                value ? "On":"Off",sOptionsTranslatedLabel,sizeof(sOptionsTranslatedLabel));
        } else if(sOptionsVideoPicker==-2) {
            MenuTranslation_PortTextCopy(value ? "controls/clash-mode/strike":"controls/clash-mode/analogic",
                value ? "Tap Strike":"Rotate Stick",sOptionsTranslatedLabel,sizeof(sOptionsTranslatedLabel));
        } else {
            const PortVideoOption *option=Port_OptionsDescriptor(sOptionsVideoPicker);
            MenuTranslation_PortTextCopy(option->nameIds[value],option->names[value],sOptionsTranslatedLabel,sizeof(sOptionsTranslatedLabel));
        }
        sOptionsPlateLabel=sOptionsTranslatedLabel;
        FlashSlot_Draw(slot,parent,flash);
        sOptionsPlateCopy=0;sOptionsPickerCaption=0;sOptionsPlateLabel=NULL;
        return;
    }
    /* Preserve the native EXIT artwork in More Options. Only Video Default
       replaces the caption; both keep the original footer timeline and depth. */
    if(clip && sMenuArtScreen==68 && sOptionsVideoFooter==3 && !strcmp(clip->name,"mc_bottom_plate_1")) return;
    if(clip && sMenuArtScreen==68 && sOptionsVideoFooter && !sOptionsPlateCopy &&
       (strcmp(clip->name,"mc_bottom_plate_1")==0 ||
        (sOptionsVideoFooter==2 && strcmp(clip->name,"mc_bottom_plate_2")==0))) {
        s32 start=sSelectionMouseCount,k,mode=sOptionsVideoFooter;
        sOptionsVideoFooter=0; /* Guard recursion without replacing native EXIT. */
        sOptionsExitCaption=mode>=2;
        sOptionsPlateCopy=mode==1;
        if(mode==1) {
            MenuTranslation_PortTextCopy("options/footer/default","Default",sOptionsTranslatedLabel,sizeof(sOptionsTranslatedLabel));
            sOptionsPlateLabel=sOptionsTranslatedLabel;
        } else sOptionsPlateLabel=NULL;
        FlashSlot_Draw(slot,parent,flash);
        sOptionsExitCaption=0;
        sOptionsPlateCopy=0;sOptionsPlateLabel=NULL;sOptionsVideoFooter=mode;
        for(k=start;k<sSelectionMouseCount;k++)
            if(strcmp(sSelectionMouseClips[k].name,"mc_bottom_plate_1")==0)
                memcpy(sSelectionMouseClips[k].name,"mc_video_default",17);
        return;
    }
    if(clip && Flash_OptionsViewportRow(clip->name)) return;
    if (clip != NULL && parent == NULL && sMenuArtScreen == 9) {
        const char *name = clip->name;
        s32 panel = 0;
        if (strncmp(name, "mc_dp_", 6) == 0) panel = 1;
        if (strncmp(name, "mc_battle_set_text_", 19) == 0) panel = 2;
        if (strncmp(name, "mc_set_", 7) == 0) {
            panel = (name[8] == '_' || strncmp(name, "mc_set_4a_", 10) == 0 ||
                     strncmp(name, "mc_set_4b_", 10) == 0) ? 4 : 2;
        }
        if (strcmp(name, "set_4_1p2p") == 0) panel = 4;
        if (panel && !(sTitleMenuEntriesVisible & panel)) return;
    }
    if (clip != NULL && sMenuArtScreen == 36 &&
        strcmp(clip->name, "mc_menu_title_plate") == 0) return;
    if (clip != NULL && sMenuArtScreen == 31) {
        const char *name = clip->name;
        s32 phase = 0;
        if (strncmp(name, "mc_menu_plate_", 14) == 0) phase = 1;
        if (strncmp(name, "mc_taikai_plate_", 16) == 0) phase = 2;
        if (strncmp(name, "mc_taikaititle", 14) == 0 || strcmp(name, "mc_taikai_icon") == 0) phase = 4;
        if (strncmp(name, "mc_dragonball_", 14) == 0) phase = 8;
        if (strncmp(name, "mc_menu_text_01_", 15) == 0 ||
            strcmp(name, "mc_yajirusi_u_l") == 0) phase = 8;
        if (strncmp(name, "mc_plate_text_01_", 16) == 0) phase = 4;
        if (strcmp(name, "mc_suji_01") == 0) phase = 16;
        if (phase && !(sTitleMenuEntriesVisible & phase)) return;
    }
    if (clip != NULL && sMenuArtScreen == 20 &&
        strncmp(clip->name, "mc_disk_plate", 13) == 0 &&
        clip->prop.mtx.m[2] >= 512.0f) {
        /* These complete option trees wait beyond the right page boundary.
           Left-anchor narrowing must not expose them before their entry. */
        return;
    }
    if (clip != NULL && sMenuArtScreen == 5) {
        const char *name = clip->name;
        s32 phase = sTitleMenuEntriesVisible; /* bit 0 menu, bit 1 saga, bit 2 difficulty */
        if ((strncmp(name, "mc_menu_plate_", 14) == 0 && !(phase & 1)) ||
            ((strncmp(name, "mc_scenario_", 12) == 0 || strncmp(name, "mc_yajirusi_", 12) == 0 ||
              strcmp(name, "mc_icon_new") == 0) && !(phase & 2)) ||
            (strncmp(name, "mc_level_plate_", 15) == 0 && !(phase & 4))) {
            return;
        }
    }
    if (clip != NULL && sMenuArtScreen == 1 && !sTitleMenuEntriesVisible &&
        strncmp(clip->name, "mc_menu_plate_", 14) == 0) {
        /* These clips are staged before fl_menu_in. Scaling would bring their
           offscreen positions into view during the earlier title phases. */
        return;
    }
#endif

    if (slot->shape != NULL) {
        FlashShape_Draw(slot->shape, parent, flash);
    }
    if (clip != NULL && (clip->flags & FLASH_CLIP_VISIBLE)) {
#ifdef PORT
        menuArt = FlashClip_MenuArtMarker(clip->name);
        if (sMenuArtScreen == 68 &&
            (strcmp(clip->name, "mc_yajirusi_left") == 0 || strcmp(clip->name, "mc_yajirusi_right") == 0) &&
            Port_OptionsCameraArrow(clip->prop.mtx.m[5]))
            menuArt = 0x19; /* the camera timeline places both arrows at y=180 */
        if (sMenuArtScreen == 68 &&
            (strcmp(clip->name, "mc_yajirusi_left") == 0 || strcmp(clip->name, "mc_yajirusi_right") == 0)) {
            FlashClip *volume = FlashClipList_FindByName(flash->clips, "mc_vol_window");
            if (volume != NULL && volume->prop.mtx.m[2] < 521.9f)
                menuArt = 0x19; /* volume arrows share their panel's center anchor */
        }
        if (sMenuArtScreen == 67 && strcmp(clip->name, "mc_guide_chichi") == 0)
            menuArt = Port_CharacterReferenceGuideMarker(clip->prop.mtx.m[2]);
        if (sMenuArtScreen == 67 &&
            (strcmp(clip->name, "mc_yajirusi_up") == 0 || strcmp(clip->name, "mc_yajirusi_down") == 0))
            menuArt = clip->prop.mtx.m[2] < 256.0f ? 0x19 : 0x1B;

        if (sMenuArtScreen == 43 && strncmp(clip->name, "mc_guide_", 9) == 0 &&
            strlen(clip->name) == 11 && clip->tl.slots != NULL &&
            clip->tl.depths > 0 && clip->tl.slots[0].shape != NULL) {
            /* Outro guides animate body/eyes/mouth inside a stationary parent.
               Slot 0 is the body: its x distinguishes the left/right timelines.
               Anchor the whole tree to that edge so native parked poses leave
               the visible page, including the eyes and mouth. */
            menuArt = clip->tl.slots[0].shape->mtx.m[2] < 256.0f ? 0x1A : 0x1B;
        }
        if (sMenuArtScreen == 6 && strcmp(clip->name, "mc_battle_text_1") == 0 &&
            clip->prop.mtx.m[2] > -35.0f) {
            /* The movie hides the title, moves it from x=-65 (preview)
               to x=-5 (story panel), then fades it back in. Follow that
               placement so the story title shares its panel's centre.
               Use the midpoint: decoded -65 can round slightly above -65,
               which must not select the story anchor over the preview. */
            menuArt = 0x19;
        }
        if (menuArt && sMenuArtDrawDepth++ == 0) {
            sTrainingGotenTree = sMenuArtScreen == 60 && strcmp(clip->name, "mc_ch_goten") == 0;
            sTrainingCloudTree = sMenuArtScreen == 60 && strcmp(clip->name, "mc_bg_cloud") == 0;
            Flash_MenuMarker(menuArt);
        }
#endif
        prop = clip->prop;
#ifdef PORT
        if (sMenuArtScreen == 68) {
            prop.mtx.m[2] += Port_OptionsChoiceTravel(clip->name, prop.mtx.m[2], Port_PageAspectMilli());
            if (sMenuArtDrawDepth == 1 && menuArt == 0x19 &&
                (strncmp(clip->name, "mc_vol_", 7) == 0 ||
                 (!Port_OptionsCameraArrow(clip->prop.mtx.m[5]) &&
                  (strcmp(clip->name, "mc_yajirusi_left") == 0 || strcmp(clip->name, "mc_yajirusi_right") == 0)))) {
                FlashClip *volume = FlashClipList_FindByName(flash->clips, "mc_vol_window");
                if (volume != NULL)
                    prop.mtx.m[2] += Port_OptionsVolumeTravel(volume->prop.mtx.m[2], Port_PageAspectMilli());
            }
        }
        if (sMenuArtScreen == 65 && sMenuArtDrawDepth == 1 && menuArt == 0x19 &&
            flash->root->depths >= 3 && flash->root->slots[2].shape != NULL) {
            prop.mtx.m[2] += Port_DataCenterItemTravel(
                flash->root->slots[2].shape->mtx.m[2], Port_PageAspectMilli());
        }
        if (sMenuArtScreen == 60) {
            prop.mtx.m[2] = Port_TrainingGuideX(clip->name, prop.mtx.m[2], Port_PageAspectMilli());
            prop.mtx.m[2] += Port_TrainingOptionsShift(clip->name, prop.mtx.m[2], Port_PageAspectMilli());
        }
        if (sMenuArtScreen == 60 && Port_PageAspectMilli() > 1333 &&
            (strcmp(clip->name, "mc_ss_speace") == 0 || strcmp(clip->name, "mc_ss_dammy") == 0) &&
            flash->root->depths >= 14 && flash->root->slots[13].shape != NULL) {
            FlashShape *frame = flash->root->slots[13].shape;
            /* Root depth 14 is the left half of the ornate frame (image 7).
             * Its inner opening begins at (+128,+32). Align both placeholder
             * and loaded image to it throughout the native entry/exit. */
            if (frame->id == 7) {
                prop.mtx.m[2] = Port_TrainingPreviewX(frame->mtx.m[2], Port_PageAspectMilli());
                prop.mtx.m[5] = frame->mtx.m[5] + 32.0f;
            }
        }
        if (sMenuArtScreen == 5 && strncmp(clip->name, "mc_guide_0", 10) == 0 &&
            clip->name[10] >= '0' && clip->name[10] <= '7' && clip->name[11] == '\0') {
            historyGuest = 1;
            if (Port_PageAspectMilli() > 1333 && clip->tl.slots != NULL &&
                clip->tl.depths > 0 && clip->tl.slots[0].shape != NULL) {
                /* Saga guests move body/eyes/mouth inside stationary parents.
                   Body depth 1 travels 0 -> settled -> 0 in the original movie.
                   Extend that travel to the physical left edge, with no change
                   at rest, and move the complete portrait tree together. */
                static const f32 settled[8] = {250.0f, 190.0f, 200.0f, 230.0f,
                                              190.0f, 170.0f, 190.0f, 190.0f};
                s32 guest = clip->name[10] - '0';
                f32 travel = 1.0f - clip->tl.slots[0].shape->mtx.m[2] / settled[guest];
                if (travel < 0.0f) travel = 0.0f;
                if (travel > 1.0f) travel = 1.0f;
                prop.mtx.m[2] -= 256.0f *
                    ((f32)Port_PageAspectMilli() / 1333.333f - 1.0f) * travel;
            }
        }
        if (sMenuArtScreen == 55 && parent == NULL && Port_PageAspectMilli() > 1333) {
            /* The acquired-item row and Unlocked captions have their own
               timeline: the buy list is already parked while they are open. */
            if (strcmp(clip->name, "mc_list_plate_6") != 0 &&
                strncmp(clip->name, "mc_font_get", 11) != 0)
                prop.mtx.m[2] += FlashShop_BuyTravel(flash);
            if (strcmp(clip->name, "mc_list_plate_6") == 0) {
                f32 travel = (clip->prop.mtx.m[2] - 26.0f) / 540.0f;
                if (travel < 0.0f) travel = 0.0f;
                if (travel > 1.0f) travel = 1.0f;
                prop.mtx.m[2] += 256.0f *
                    ((f32)Port_PageAspectMilli() / 1333.333f - 1.0f) * travel;
            }
        }
        if ((sMenuArtScreen == 53 || sMenuArtScreen == 55 || sMenuArtScreen == 56) &&
            Port_PageAspectMilli() > 1333 && strncmp(clip->name, "mc_tab_btn_", 11) == 0 &&
            clip->name[11] >= '0' && clip->name[11] <= '3' && !clip->name[12]) {
            s32 tab = clip->name[11] - '0';
            f32 rest = 23.0f + 55.0f * tab;
            f32 parked = -77.0f + 5.0f * tab;
            f32 travel = (rest - prop.mtx.m[2]) / (rest - parked);
            if (travel < 0.0f) travel = 0.0f;
            if (travel > 1.0f) travel = 1.0f;
            /* Keep each tab and its NEW badge outside the wide page while
               parked; retain the original easing and open positions. */
            prop.mtx.m[2] -= 256.0f *
                ((f32)Port_PageAspectMilli() / 1333.333f - 1.0f) * travel;
        }
        if (sMenuArtScreen == 36 && strcmp(clip->name, "mc_taikaimoji_waku") == 0 &&
            Port_PageAspectMilli() > 1333) {
            /* This is the actual opening ring, including its white fill. Its
               local center is (128,128), and the movie zooms from 3.65152 to 1.
               Enlarge the initial radius for widescreen, preserving its center
               and the final pose, so it enters continuously from beyond view. */
            f32 nativeScale = __builtin_fabsf(prop.mtx.m[0]);
            f32 travel = (nativeScale - 1.0f) / (3.65152f - 1.0f);
            f32 extra, cx, cy;
            if (travel < 0.0f) travel = 0.0f;
            if (travel > 1.0f) travel = 1.0f;
            extra = 1.0f + ((f32)Port_PageAspectMilli() / 1333.333f - 1.0f) * travel;
            cx = prop.mtx.m[2] + 128.0f * (prop.mtx.m[0] + prop.mtx.m[3]);
            cy = prop.mtx.m[5] + 128.0f * (prop.mtx.m[1] + prop.mtx.m[4]);
            prop.mtx.m[2] = cx + (prop.mtx.m[2] - cx) * extra;
            prop.mtx.m[5] = cy + (prop.mtx.m[5] - cy) * extra;
            prop.mtx.m[0] *= extra;
            prop.mtx.m[1] *= extra;
            prop.mtx.m[3] *= extra;
            prop.mtx.m[4] *= extra;
        }
        if (sMenuArtScreen == 4 && Port_PageAspectMilli() > 1333 &&
            (strcmp(clip->name, "mc_menu_plate_1") == 0 || strcmp(clip->name, "mc_menu_plate_2") == 0)) {
            /* Dialog choices park at -130 / 517. Extend their existing travel
               to the wide page edges while preserving the open positions. */
            s32 left = strcmp(clip->name, "mc_menu_plate_1") == 0;
            f32 rest = left ? 88.5f : 308.0f;
            f32 travel = left ? (rest - prop.mtx.m[2]) / (rest + 130.0f) :
                                (prop.mtx.m[2] - rest) / (517.0f - rest);
            if (travel < 0.0f) travel = 0.0f;
            if (travel > 1.0f) travel = 1.0f;
            prop.mtx.m[2] += (left ? -256.0f : 256.0f) *
                ((f32)Port_PageAspectMilli() / 1333.333f - 1.0f) * travel;
        }
        if (sMenuArtScreen == 7 && strncmp(clip->name, "mc_result_text_", 15) == 0 &&
            Port_PageAspectMilli() > 1333 && prop.mtx.m[2] > 0.0f) {
            /* Extend the timeline's rightward travel across the wide page.
               Keep the resting pose at x=0 and preserve its animation easing;
               the original x=512 endpoint now reaches the actual right edge. */
            prop.mtx.m[2] *= (f32)Port_PageAspectMilli() / 1333.333f;
        }
        if (sMenuArtScreen == 1 && strncmp(clip->name, "mc_menu_plate_", 14) == 0 &&
            Port_PageAspectMilli() > 1333) {
            /* The title rows travel from x=-453 / 450 to x=0. Undo the
               centre-pivot inset at their parked pose, blending to zero at rest. */
            f32 parked = strcmp(clip->name, "mc_menu_plate_1") == 0 ? -453.0f : 450.0f;
            f32 travel = prop.mtx.m[2] / parked;
            if (travel < 0.0f) travel = 0.0f;
            if (travel > 1.0f) travel = 1.0f;
            prop.mtx.m[2] += (parked < 0.0f ? -256.0f : 256.0f) *
                ((f32)Port_PageAspectMilli() / 1333.333f - 1.0f) * travel;
        }
        if (sMenuArtScreen == 10 && strcmp(clip->name, "mc_map_name") == 0 &&
            Port_PageAspectMilli() > 1333 && prop.mtx.m[2] < 0.0f) {
            /* Blend the centred stage label toward the left page edge as
               its timeline slides away. At x=-512 the entire label must
               be outside, rather than parked visibly inside the wide page. */
            f32 progress = -prop.mtx.m[2] / 512.0f;
            if (progress > 1.0f) progress = 1.0f;
            prop.mtx.m[2] -= 256.0f * ((f32)Port_PageAspectMilli() / 1333.333f - 1.0f) * progress;
        }
        if (sMenuArtScreen == 7 && strncmp(clip->name, "mc_chara_", 9) == 0 &&
            Port_PageAspectMilli() > 1333) {
            /* Preserve the left-side entrance, blending into the right
               anchor only as the original illustration moves across. */
            f32 parked = strcmp(clip->name, "mc_chara_1") == 0 ? -327.0f :
                         strcmp(clip->name, "mc_chara_2") == 0 ? -240.0f :
                         strcmp(clip->name, "mc_chara_3") == 0 ? -257.0f : -295.0f;
            f32 progress = (prop.mtx.m[2] - parked) / 595.0f;
            if (progress < 0.0f) progress = 0.0f;
            if (progress > 1.0f) progress = 1.0f;
            prop.mtx.m[2] -= 512.0f * ((f32)Port_PageAspectMilli() / 1333.333f - 1.0f) * (1.0f - progress);
        }
        if (sMenuArtScreen == 6 && Port_PageAspectMilli() > 1333 &&
            strncmp(clip->name, "mc_guide_0", 10) == 0 && clip->name[11] == '\0') {
            FlashClip *preview = FlashClipList_FindByName(flash->clips, "mc_battle_mask");
            if (preview != NULL) {
                f32 scale = 1333.333f / (f32)Port_PageAspectMilli();
                /* The visible preview is 256 units wide inside its texture.
                   Find the gap in rendered page coordinates, then undo the
                   guide's right-anchor transform to position its centre. */
                f32 previewRight = (preview->prop.mtx.m[2] + 256.0f) * scale;
                f32 centre = (previewRight + 512.0f) * 0.5f;
                f32 target = 512.0f + (centre - 512.0f) / scale - 128.0f;
                f32 settled = strcmp(clip->name, "mc_guide_06") == 0 ? 312.0f : 314.0f;
                f32 parked = strcmp(clip->name, "mc_guide_06") == 0 ? 518.0f : 520.0f;
                f32 progress = (parked - prop.mtx.m[2]) / (parked - settled);
                if (progress < 0.0f) progress = 0.0f;
                if (progress > 1.0f) progress = 1.0f;
                prop.mtx.m[2] += (target - settled) * progress;
            }
        }
#endif
        if (parent != NULL) {
            prop.flags |= parent->flags & (FLASH_OV_MASK | FLASH_OV_MASK_SET);
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) {
                    m.m[i * 3 + j] = prop.mtx.m[j] * parent->mtx.m[i * 3] + prop.mtx.m[3 + j] * parent->mtx.m[i * 3 + 1] +
                                     prop.mtx.m[6 + j] * parent->mtx.m[i * 3 + 2];
                }
            }
            prop.mtx = m;
            if (parent->flags & FLASH_OV_POS) {
                prop.mtx.m[2] += parent->x;
                prop.mtx.m[5] += parent->y;
            }
            for (i = 0; i < 4; i++) {
                prop.cx.mul[i] *= parent->cx.mul[i];
                if (i < 3) {
                    if (parent->flags & FLASH_OV_COLOR) {
                        prop.cx.mul[i] *= parent->color;
                    }
                } else {
                    if (parent->flags & FLASH_OV_ALPHA) {
                        prop.cx.mul[i] *= parent->alpha;
                    }
                }
                if (prop.cx.mul[i] > 1.0f) {
                    prop.cx.mul[i] = 1.0f;
                } else if (prop.cx.mul[i] < 0.0f) {
                    prop.cx.mul[i] = 0.0f;
                }
                /* The statement expression makes the compiler take the destination's address before the clamp. */
                prop.cx.add[i] = ({ Flash_ClampAdd(prop.cx.add[i] + parent->cx.add[i]); });
            }
        }
        if (clip->preDraw != NULL) {
            clip->preDraw(clip->preArg);
        }
#ifdef PORT
        if (!(clip->flags & 0x40000000))
#endif
        {
#ifdef PORT
            if (historyGuest) sHistoryGuestDrawDepth++;
            if (sMainMouseDraw && strncmp(clip->name, "mc_menu_plate_", 14) == 0 &&
                clip->name[14] >= '1' && clip->name[14] <= '6' && clip->name[15] == 0)
                /* The dimmed neighbors are clickable scroll targets too. */
                Flash_MainMousePlate(clip->name[14] == '1' ? 4 : clip->name[14] == '6' ? 5 :
                                     clip->name[14] - '2', &prop, flash);
            if (sMainMouseDraw && (strcmp(clip->name, "mc_menu_yajirusi_up") == 0 ||
                                  strcmp(clip->name, "mc_menu_yajirusi_down") == 0))
                Flash_MouseRegion(sMainMouseArrows, strcmp(clip->name, "mc_menu_yajirusi_up") == 0 ? 0 : 1,
                                  32.0f, 32.0f, 0.0f, &prop, flash);
            if (sQuitMouseDraw && strncmp(clip->name, "mc_menu_plate_", 14) == 0 &&
                clip->name[14] >= '1' && clip->name[14] <= '2' && clip->name[15] == 0)
                Flash_MouseRegion(sQuitMousePlates, clip->name[14] - '1', 128.0f, 64.0f, 256.0f, &prop, flash);
            if (sTitleMouseDraw && strncmp(clip->name, "mc_menu_plate_", 14) == 0 &&
                clip->name[14] >= '1' && clip->name[14] <= '2' && clip->name[15] == 0)
                Flash_MousePlate(sTitleMousePlates, clip->name[14] - '1', 32.0f, 256.0f, &prop, flash);
#endif
            if (((sSelectionMouseScope >= 9 && sSelectionMouseScope <= 12) || sSelectionMouseScope == 4 || sSelectionMouseScope == 5 || sSelectionMouseScope == 6 ||
                 (sSelectionMouseScope >= 17 && sSelectionMouseScope <= 35) || sSelectionMouseScope==60 ||
                 (sSelectionMouseScope >= 45 && sSelectionMouseScope <= 69) || sSelectionMouseScope == 68 || sSelectionMouseScope == 70 || sSelectionMouseScope == 71) && sSelectionMouseCount < 512 &&
                strlen(clip->name) < 64 &&
                (strncmp(clip->name, "mc_chara_chip_", 14) == 0 ||
                 strncmp(clip->name, "mc_map_chip_", 12) == 0 ||
                 strncmp(clip->name, "mc_menu_plate", 13) == 0 ||
                 strncmp(clip->name, "mc_window_plate", 15) == 0 ||
                 strncmp(clip->name,"menu_plate_",11)==0 ||
                 strncmp(clip->name,"mc_tab_btn_",11)==0 ||
                 strncmp(clip->name,"mc_num_zp_",10)==0 ||
                 strncmp(clip->name,"mc_input_code_",14)==0 ||
                 strcmp(clip->name,"mc_custom_yajirusi")==0 ||
                 ((sSelectionMouseScope==67 || Flash_ItemMouseScope(sSelectionMouseScope)) && strcmp(clip->name,"mc_scroll_bar_point")==0) ||
                 strncmp(clip->name,"mc_reconfir_plate_",18)==0 ||
                 strncmp(clip->name, "mc_taikai_plate_",16)==0 ||
                 strncmp(clip->name, "mc_dragonball_off_",18)==0 ||
                 strncmp(clip->name, "mc_entry_text_",14)==0 ||
                 strcmp(clip->name,"mc_suji_01")==0 ||
                 strncmp(clip->name, "mc_ranking_plate", 16) == 0 ||
                 strncmp(clip->name, "mc_disk_plate", 13) == 0 ||
                 strncmp(clip->name, "mc_sim_plate", 12) == 0 ||
                 strncmp(clip->name, "mc_sim_botan", 12) == 0 ||
                 strncmp(clip->name, "mc_menu_yajirusi", 16) == 0 ||
                 strncmp(clip->name, "mc_yes_no_text", 14) == 0 ||
                 strncmp(clip->name, "mc_card_anime", 13) == 0 ||
                 strncmp(clip->name, "mc_popo", 7) == 0 ||
                 strncmp(clip->name, "mc_level_plate_", 15) == 0 ||
                 strncmp(clip->name, "mc_scenario_", 12) == 0 ||
                 strncmp(clip->name, "mc_mode_", 8) == 0 ||
                 strncmp(clip->name, "mc_dp_", 6) == 0 ||
                 strncmp(clip->name, "mc_battle_set_text_", 19) == 0 ||
                 strncmp(clip->name, "mc_custom_plate_", 16) == 0 ||
                 strncmp(clip->name, "mc_color_plate_", 15) == 0 ||
                 strncmp(clip->name, "mc_team_", 8) == 0 ||
                 strncmp(clip->name, "mc_set_", 7) == 0 ||
                 strncmp(clip->name, "mc_list_plate_", 14) == 0 ||
                 (sSelectionMouseScope == 68 && (strncmp(clip->name,"mc_bottom_plate_",16)==0 ||
                    strncmp(clip->name,"mc_select_plate_",16)==0 || strncmp(clip->name,"mc_vol_plate_",13)==0 ||
                    strncmp(clip->name,"mc_bgm_plate_",13)==0 || strncmp(clip->name,"mc_button_text_",15)==0 ||
                    strncmp(clip->name,"mc_type_image_",14)==0 || strcmp(clip->name,"mc_bgm_scroll_bar")==0)) ||
                 strcmp(clip->name, "mc_select_plate") == 0 ||
                 strncmp(clip->name, "mc_yajirusi", 11) == 0 ||
                 strcmp(clip->name, "mc_bgm_now") == 0 ||
                 strcmp(clip->name, "mc_yajirusi_up") == 0 || strcmp(clip->name, "mc_yajirusi_down") == 0)) {
                PortSelectionMouseClip *capture = &sSelectionMouseClips[sSelectionMouseCount++];
                if (sSelectionMouseParent && strncmp(clip->name, "mc_yajirusi",11) == 0) {
                    u32 parentLen = strlen(sSelectionMouseParent);
                    u32 childLen = strlen(clip->name);
                    if(childLen>40) childLen=40;
                    if (parentLen + childLen + 2 > sizeof(capture->name)) parentLen = sizeof(capture->name) - childLen - 2;
                    memcpy(capture->name, sSelectionMouseParent, parentLen);
                    capture->name[parentLen]='/';
                    memcpy(capture->name + parentLen + 1, clip->name, childLen);
                    capture->name[parentLen + childLen + 1]=0;
                } else memcpy(capture->name, clip->name, strlen(clip->name) + 1);
                memset(&capture->drawn,0,sizeof(capture->drawn));
                capture->scope = sSelectionMouseScope;
                capture->prop = prop;
                /* Only the bottom music line is interactive, not the preview above it. */
                if (sSelectionMouseScope == 10 && strcmp(clip->name, "mc_bgm_now") == 0) {
                    capture->prop.mtx.m[2] += 30.0f * prop.mtx.m[0] + 77.0f * prop.mtx.m[1];
                    capture->prop.mtx.m[5] += 30.0f * prop.mtx.m[3] + 77.0f * prop.mtx.m[4];
                }
                capture->flash = flash;
            }
            {
                const char *oldMouseParent = sSelectionMouseParent;
                s32 oldOptionMouseDrawClip = sOptionMouseDrawClip;
                s32 oldItemMousePlateBody = sItemMousePlateBody;
                s32 oldOptionsCaptionLayer = sOptionsCaptionLayer;
                /* Override the resolved draw property: timeline evaluation can
                   replace the earlier clip UV override during menu_change. */
                if(sOptionsExitCaption && strncmp(clip->name,"mc_bottom_text_",15)==0) {
                    prop.flags|=FLASH_OV_UV|FLASH_OV_TEX;
                    prop.uv.u0=0;prop.uv.v0=0;prop.uv.u1=256;prop.uv.v1=32;prop.uv.tex=0;
                }
                if(sOptionsPlateCopy && (strncmp(clip->name,"mc_menu_text_",13)==0 || strncmp(clip->name,"mc_bottom_text_",15)==0 || strncmp(clip->name,"mc_select_text_",15)==0))
                    sOptionsCaptionLayer=(strcmp(clip->name,"mc_menu_text_on")==0 || strcmp(clip->name,"mc_bottom_text_on")==0 || strcmp(clip->name,"mc_select_text_on")==0) ? 1:0;
                if(sOptionsPlateCopy && (strcmp(clip->name,"mc_menu_text_off")==0 || strcmp(clip->name,"mc_menu_text_on")==0)) {
                    /* The focus loop bobs the caption relative to its authored
                       resting origin. Preserve that motion for waiting bindings. */
                    sOptionsCaptionX[sOptionsCaptionLayer]=clip->prop.mtx.m[2]-45.0f;
                    sOptionsCaptionY[sOptionsCaptionLayer]=clip->prop.mtx.m[5]+2.5f;
                }
                if(sSelectionMouseScope==68 && sSelectionMouseCount>0 &&
                   strcmp(sSelectionMouseClips[sSelectionMouseCount-1].name,clip->name)==0 &&
                   Port_OptionsMouseControl(clip->name))
                    sOptionMouseDrawClip=sSelectionMouseCount-1;
                if(sSelectionMouseScope==4 && sSelectionMouseCount>0 &&
                   strcmp(sSelectionMouseClips[sSelectionMouseCount-1].name,clip->name)==0 &&
                   (strcmp(clip->name,"mc_menu_plate_1")==0 || strcmp(clip->name,"mc_menu_plate_2")==0))
                    sOptionMouseDrawClip=sSelectionMouseCount-1;
                if(sSelectionMouseScope==31 && sSelectionMouseCount>0 &&
                   strcmp(sSelectionMouseClips[sSelectionMouseCount-1].name,clip->name)==0 &&
                   strncmp(clip->name,"mc_dragonball_off_",18)==0)
                    sOptionMouseDrawClip=sSelectionMouseCount-1;
                if(sSelectionMouseScope==33 && sSelectionMouseCount>0 &&
                   strcmp(sSelectionMouseClips[sSelectionMouseCount-1].name,clip->name)==0 &&
                   (strcmp(clip->name,"mc_yajirusi_l")==0 || strcmp(clip->name,"mc_yajirusi_r")==0))
                    sOptionMouseDrawClip=sSelectionMouseCount-1;
                if(sSelectionMouseScope==62 && sSelectionMouseCount>0 &&
                   strcmp(sSelectionMouseClips[sSelectionMouseCount-1].name,clip->name)==0 &&
                   strncmp(clip->name,"mc_input_code_",14)==0)
                    sOptionMouseDrawClip=sSelectionMouseCount-1;
                if(sSelectionMouseCount>0 && (Flash_CharRefMouseControl(sSelectionMouseScope,clip->name) ||
                   Flash_ItemMouseThumb(sSelectionMouseScope,clip->name) || Flash_ItemMouseRow(sSelectionMouseScope,clip->name)) &&
                   strcmp(sSelectionMouseClips[sSelectionMouseCount-1].name,clip->name)==0)
                    sOptionMouseDrawClip=sSelectionMouseCount-1;
                /* Use the item plate itself; icons and captions can extend outside it. */
                if (Flash_ItemMouseScope(sSelectionMouseScope) &&
                    (strcmp(clip->name,"mc_list_plate_off")==0 || strcmp(clip->name,"mc_list_plate_on")==0))
                    sItemMousePlateBody = 1;
                if(sSelectionMouseCount>0 && Flash_EvoMouseArrow(sSelectionMouseScope,clip->name) &&
                   strcmp(Flash_MouseChildName(sSelectionMouseClips[sSelectionMouseCount-1].name),clip->name)==0)
                    sOptionMouseDrawClip=sSelectionMouseCount-1;
                if (strncmp(clip->name, "mc_custom_plate_", 16) == 0 ||
                    strcmp(clip->name,"mc_custom_yajirusi")==0 ||
                 (sSelectionMouseScope==67 && strcmp(clip->name,"mc_scroll_bar_point")==0) || strncmp(clip->name,"mc_num_zp_",10)==0)
                    sSelectionMouseParent = clip->name;
                FlashTl_Draw(&clip->tl, &prop);
                sSelectionMouseParent = oldMouseParent;
                sOptionMouseDrawClip = oldOptionMouseDrawClip;
                sItemMousePlateBody = oldItemMousePlateBody;
                sOptionsCaptionLayer = oldOptionsCaptionLayer;
            }
#ifdef PORT
            if (historyGuest) sHistoryGuestDrawDepth--;
#endif
        }
        if (clip->drawOver != NULL) {
            clip->drawOver(clip->overArg, &prop);
        }
        if (clip->postDraw != NULL) {
            clip->postDraw(clip->postArg);
        }
#ifdef PORT
        if ((sMenuArtScreen == 13 || sMenuArtScreen == 14 ||
             sMenuArtScreen == 15 || sMenuArtScreen == 16 || sMenuArtScreen == 17 || sMenuArtScreen == 18 ||
             sMenuArtScreen == 19 || sMenuArtScreen == 20 || sMenuArtScreen == 21 || sMenuArtScreen == 22 || sMenuArtScreen == 24 || sMenuArtScreen == 28 ||
             sMenuArtScreen == 31 || sMenuArtScreen == 35 || sMenuArtScreen == 39 || sMenuArtScreen == 40 || sMenuArtScreen == 43 || sMenuArtScreen == 44 || sMenuArtScreen == 45 || sMenuArtScreen == 46 || sMenuArtScreen == 47 || sMenuArtScreen == 49 || sMenuArtScreen == 50 || sMenuArtScreen == 51 || sMenuArtScreen == 53 || sMenuArtScreen == 54 || sMenuArtScreen == 55 || sMenuArtScreen == 56 || (sMenuArtScreen >= 60 && sMenuArtScreen <= 69)) && menuArt) {
            extern void Font_FlushAll(void);
            Font_FlushAll(); /* callback text must use its clip's proportions */
            if (((sMenuArtScreen >= 63 && sMenuArtScreen <= 65) || sMenuArtScreen == 67) && sMenuArtDrawDepth == 1)
                Gfx_AddDefaultEnv(); /* list font scissor must not crop later name/form backing shapes */
        }
        if (sMenuArtScreen == 10 &&
            (strcmp(clip->name, "mc_name_text_l") == 0 || strcmp(clip->name, "mc_form_text_l") == 0 ||
             strcmp(clip->name, "mc_name_text_r") == 0 || strcmp(clip->name, "mc_form_text_r") == 0)) {
            extern void Font_FlushAll(void);
            Font_FlushAll();
        }
        if (menuArt && --sMenuArtDrawDepth == 0) {
            sTrainingGotenTree = 0;
            sTrainingCloudTree = 0;
            Flash_MenuMarker(sMenuArtScreen == 40 || sMenuArtScreen == 43 || sMenuArtScreen == 69 ? 0x19 : 0x13); /* restore movie background layout */
        }
#endif
    }
}

/* Returns its argument: every screen Y goes through it (a stripped vertical adjustment). */
f32 FlashDraw_AdjustY(f32 y) {
    return y;
}

/* Queues the upload of texture `index` of an image's entry table to block `tbp` and its CLUT to the blocks behind
   it (+0x40), and returns the TEX0 value. Entry `index` holds the CLUT; when it has no pixels of its own the
   pixels are entry 0's (several palettes for one picture). */
u64 FlashDraw_UploadTex(FlashTex *tex, s32 index, s32 tbp) {
    TexBltPacket pkt = { { 0x10000002, 0, 0, 0x50000002 }, { 0x1000000000008001, 0xE }, 0, 0x50 };
    TexEntry *clut = (TexEntry *)((index << 6) + (u32)tex);
    TexEntry *pix;
    s64 base = tbp;
    u64 tex0;

    if (clut->pixSize == 0) {
        index = 0;
    }
    pix = (TexEntry *)((index << 6) + (u32)tex);
    pkt.bitbltbuf = (base << 32) | ((u64)pix->pixBlt << 48);
    Dma_AddData(&pkt, sizeof(pkt));
    Dma_AddRef(pix->pix, pix->pixSize);
    tbp += pix->tbpStep;
    tbp += 0x40;
    pkt.bitbltbuf = ((s64)tbp << 32) | ((u64)clut->clutBlt << 48);
    Dma_AddData(&pkt, sizeof(pkt));
    Dma_AddRef(clut->clut, clut->clutSize);
    tex0 = pix->tex0;
    tex0 |= (s64)tbp << 37;
    tex0 |= base;
    tex0 |= 1LL << 34;
    return tex0;
}

/* XYZ2 of a screen point: 12.4 fixed point, window origin at (1792, 1824), Z 0. */
#define FLASH_XY(x, y) ((s64)((s32)((x) * 16.0f) + ox) | ((s64)((s32)(FlashDraw_AdjustY(y) * 16.0f) + oy) << 16))
#define FLASH_RGBA(q) ((s64)(q)->rgba[0] | ((s64)(q)->rgba[1] << 8) | ((s64)(q)->rgba[2] << 16) | ((s64)(q)->rgba[3] << 24))

/* Writes a textured triangle strip of four vertices (REGLIST: RGBAQ, TEX0, PRIM, then UV / XYZ2 pairs) and
   returns the end of the packet. The texture rectangle is shrunk by one texel on each side unless flipped. */
u64 *FlashDraw_PutSprite(u64 *p, FlashQuad *q, u64 tex0) {
    s32 uv[4];
    s32 unused[4]; /* the original frame has 0x10 more bytes of locals */
    s32 oy;
    s32 ox;
    s32 uFraction = 0;
#ifdef PORT
    if (sTrainingCloudTree) uFraction = sTrainingCloudFraction;
#endif

    p[0] = 0xC400000000008001;
    p[1] = 0x5353535306E1;
    p += 2;
    p[0] = FLASH_RGBA(q);
    p[1] = 0;
    p += 2;
    p[0] = tex0;
    p[1] = 0x154;
    p += 2;
    uv[0] = q->uv.u0 + 1;
    uv[2] = q->uv.u1 - 1;
    uv[1] = q->uv.v0 + 1;
    uv[3] = q->uv.v1 - 1;
    if (q->flags & FLASH_QUAD_FLIPX) {
        uv[0] = q->uv.u1;
        uv[2] = q->uv.u0;
    }
    if (q->flags & FLASH_QUAD_FLIPY) {
        uv[1] = q->uv.v1;
        uv[3] = q->uv.v0;
    }
    oy = 0x7200;
    ox = 0x7000;
    p[0] = ((uv[0] << 4) + uFraction) | ((s64)(uv[1] << 4) << 16);
    p[1] = FLASH_XY(q->pt[0][0], q->pt[0][1]);
    p += 2;
    p[0] = ((uv[2] << 4) + uFraction) | ((s64)(uv[1] << 4) << 16);
    p[1] = FLASH_XY(q->pt[1][0], q->pt[1][1]);
    p += 2;
    p[0] = ((uv[0] << 4) + uFraction) | ((s64)(uv[3] << 4) << 16);
    p[1] = FLASH_XY(q->pt[2][0], q->pt[2][1]);
    p += 2;
    p[0] = ((uv[2] << 4) + uFraction) | ((s64)(uv[3] << 4) << 16);
    p[1] = FLASH_XY(q->pt[3][0], q->pt[3][1]);
    p += 2;
    return p;
}

/* Draws an untextured quad in its colour. */
void FlashDraw_FlatQuad(FlashQuad *q) {
    u64 *p = Dma_BeginDirect();
    s32 oy;
    s32 ox;

    p[0] = 0x1000000000008004;
    p[1] = 0xE;
    p += 2;
    p[0] = GFX_FRAME_REG();
    p[1] = 0x4C;
    p += 2;
    p[0] = 1;
    p[1] = 0x46;
    p += 2;
    if (q->flags & FLASH_QUAD_ADD) {
        p[0] = 0x48;
    } else if (q->flags & FLASH_QUAD_SUB) {
        p[0] = 0x42;
    } else {
        p[0] = 0x44;
    }
    p[1] = 0x42;
    p += 2;
    p[0] = 0x30000;
    p[1] = 0x47;
    p += 2;
    p[0] = 0x6400000000008001;
    p[1] = 0x555510;
    p += 2;
    p[0] = 0x44;
    oy = 0x7200;
    ox = 0x7000;
    p[1] = FLASH_RGBA(q);
    p += 2;
    p[0] = FLASH_XY(q->pt[0][0], q->pt[0][1]);
    p[1] = FLASH_XY(q->pt[1][0], q->pt[1][1]);
    p += 2;
    p[0] = FLASH_XY(q->pt[2][0], q->pt[2][1]);
    p[1] = FLASH_XY(q->pt[3][0], q->pt[3][1]);
    p += 2;
    Dma_EndDirect(p);
}

/* Draws a textured quad through the frame buffer's alpha plane. `set`: write only alpha (builds the mask).
   Otherwise: write only colour, where the texture's alpha is above 0x3C, blended by the alpha already there. */
void FlashDraw_MaskQuad(FlashQuad *q, u64 tex0, s32 set) {
    u64 *p = Dma_BeginDirect();
    u64 frame;
    u64 reg;

    p[0] = 0x1000000000008007;
    p[1] = 0xE;
    p += 2;
    frame = GFX_FRAME_REG();
    if (set) {
        reg = frame | 0x00FFFFFF00000000;
    } else {
        reg = frame | 0xFF00000000000000;
    }
    p[0] = reg;
    p[1] = 0x4C;
    p += 2;
    p[0] = 1;
    p[1] = 0x46;
    p += 2;
    if (set) {
        p[0] = 0x50000;
    } else {
        p[0] = 0x503CD;
    }
    p[1] = 0x47;
    p += 2;
    if (q->flags & FLASH_QUAD_ADD) {
        p[0] = !set ? 0x58 : 0x48;
    } else if (q->flags & FLASH_QUAD_SUB) {
        p[0] = !set ? 0x52 : 0x42;
    } else {
        p[0] = !set ? 0x54 : 0x44;
    }
    p[1] = 0x42;
    p += 2;
    p[0] = 0;
    p[1] = 0x3F;
    p += 2;
    p[0] = 0x60;
    p[1] = 0x14;
    p += 2;
    if (q->flags & FLASH_QUAD_REPEAT) {
        p[0] = 0;
    } else {
        p[0] = 5;
    }
    p[1] = 8;
    p += 2;
    Dma_EndDirect(FlashDraw_PutSprite(p, q, tex0));
}

/* Draws one quad: nothing when its alpha is 0, flat without a texture, else uploads the texture to block 0x3000
   and draws it normally or through the alpha-plane mask. */
void FlashDraw_Quad(FlashTex *tex, FlashQuad *q) {
    u64 tex0;
    u64 *p;

    if (q->rgba[3] == 0) {
        return;
    }
    if (tex != NULL) {
        tex0 = FlashDraw_UploadTex(tex, q->uv.tex, 0x3000);
        Dma_AddTexFlush();
    } else {
        FlashDraw_FlatQuad(q);
        return;
    }
    if (q->flags & FLASH_QUAD_MASK_SET) {
        Gfx_ClearScreen(0xFFFFFF, 0);
        FlashDraw_MaskQuad(q, tex0, 1);
        return;
    }
    if (q->flags & FLASH_QUAD_MASK) {
        FlashDraw_MaskQuad(q, tex0, 0);
        return;
    }
    p = Dma_BeginDirect();
    p[0] = 0x1000000000008006;
    p[1] = 0xE;
    p += 2;
    p[0] = GFX_FRAME_REG();
    p[1] = 0x4C;
    p += 2;
    p[0] = 1;
    p[1] = 0x46;
    p += 2;
    if (q->flags & FLASH_QUAD_ADD) {
        p[0] = 0x48;
    } else if (q->flags & FLASH_QUAD_SUB) {
        p[0] = 0x42;
    } else {
        p[0] = 0x44;
    }
    p[1] = 0x42;
    p += 2;
    p[0] = 0x60;
    p[1] = 0x14;
    p += 2;
    p[0] = 0x30000;
    p[1] = 0x47;
    p += 2;
    if (q->flags & FLASH_QUAD_REPEAT) {
        p[0] = 0;
    } else {
        p[0] = 5;
    }
    p[1] = 8;
    p += 2;
    Dma_EndDirect(FlashDraw_PutSprite(p, q, tex0));
}

/* Brings a shape's quad to the screen: the shape's matrix and colour transform, then the owning sprite's drawing
   state (matrix, position / scale / colour / alpha overrides, texture rectangle, draw flags), then the movie's
   screen offset. Only three corners are transformed; the fourth closes the parallelogram. */
void FlashDraw_TransformQuad(FlashQuad *q, FlashMtx *mtx, FlashCxform *cx, FlashProp *prop, s32 *ofs) {
    s32 i;
    f32 x;
    f32 y;
    f32 sx;
    f32 sy;

    {
        for (i = 0; i < 3; i++) {
            f32 *xs = &q->pt[0][0];
            f32 *ys = &q->pt[0][1];
            f32 *px = &xs[i * 2];
            f32 *py = &ys[i * 2];

            x = *px;
            y = *py;
            *px = x * mtx->m[0] + y * mtx->m[3] + mtx->m[2];
            *py = x * mtx->m[1] + y * mtx->m[4] + mtx->m[5];
            if (prop == NULL) {
                *px += (f32)ofs[0];
                *py += (f32)ofs[1];
            }
        }
    }
    q->pt[3][0] = q->pt[1][0] + q->pt[2][0] - q->pt[0][0];
    q->pt[3][1] = q->pt[1][1] + q->pt[2][1] - q->pt[0][1];
    q->rgba[0] = Flash_Clamp255((s32)((f32)q->rgba[0] * cx->mul[0]) + cx->add[0]);
    q->rgba[1] = Flash_Clamp255((s32)((f32)q->rgba[1] * cx->mul[1]) + cx->add[1]);
    q->rgba[2] = Flash_Clamp255((s32)((f32)q->rgba[2] * cx->mul[2]) + cx->add[2]);
    q->rgba[3] = Flash_Clamp255((s32)((f32)q->rgba[3] * cx->mul[3]) + cx->add[3]);
    if (prop != NULL) {
        sx = prop->mtx.m[0];
        sy = prop->mtx.m[4];
        if (prop->flags & FLASH_OV_SCALE) {
            sx *= prop->scaleX;
            sy *= prop->scaleY;
        }
        {
            for (i = 0; i < 3; i++) {
                f32 *xs = &q->pt[0][0];
                f32 *ys = &q->pt[0][1];
                f32 *px = &xs[i * 2];
                f32 *py = &ys[i * 2];

                x = *px;
                y = *py;
                *px = x * sx + y * prop->mtx.m[3] + prop->mtx.m[2];
                *py = x * prop->mtx.m[1] + y * sy + prop->mtx.m[5];
                if (prop->flags & FLASH_OV_POS) {
                    *px += prop->x;
                    *py += prop->y;
                }
                if (!(prop->flags & FLASH_OV_NO_OFS)) {
                    *px += (f32)ofs[0];
                    *py += (f32)ofs[1];
                }
            }
        }
        q->pt[3][0] = q->pt[1][0] + q->pt[2][0] - q->pt[0][0];
        q->pt[3][1] = q->pt[1][1] + q->pt[2][1] - q->pt[0][1];
        q->rgba[0] = Flash_Clamp255((s32)((f32)q->rgba[0] * prop->cx.mul[0]) + prop->cx.add[0]);
        q->rgba[1] = Flash_Clamp255((s32)((f32)q->rgba[1] * prop->cx.mul[1]) + prop->cx.add[1]);
        q->rgba[2] = Flash_Clamp255((s32)((f32)q->rgba[2] * prop->cx.mul[2]) + prop->cx.add[2]);
        q->rgba[3] = Flash_Clamp255((s32)((f32)q->rgba[3] * prop->cx.mul[3]) + prop->cx.add[3]);
        if (prop->flags & FLASH_OV_COLOR) {
            q->rgba[0] = Flash_Clamp255((s32)((f32)q->rgba[0] * prop->color));
            q->rgba[1] = Flash_Clamp255((s32)((f32)q->rgba[1] * prop->color));
            q->rgba[2] = Flash_Clamp255((s32)((f32)q->rgba[2] * prop->color));
        }
        if (prop->flags & FLASH_OV_ALPHA) {
            q->rgba[3] = Flash_Clamp255((s32)((f32)q->rgba[3] * prop->alpha));
        }
        if (prop->flags & FLASH_OV_UV) {
            q->uv = prop->uv;
            q->uv.tex = 0;
        }
        if (prop->flags & FLASH_OV_TEX) {
            q->uv.tex = prop->uv.tex;
        }
        if (prop->flags & FLASH_OV_FLIPX) {
            q->flags |= FLASH_QUAD_FLIPX;
        }
        if (prop->flags & FLASH_OV_FLIPY) {
            q->flags |= FLASH_QUAD_FLIPY;
        }
        if (prop->flags & FLASH_OV_REPEAT) {
            q->flags |= FLASH_QUAD_REPEAT;
        }
        if (prop->flags & FLASH_OV_ADD) {
            q->flags |= FLASH_QUAD_ADD;
        }
        if (prop->flags & FLASH_OV_SUB) {
            q->flags |= FLASH_QUAD_SUB;
        }
        if (prop->flags & FLASH_OV_MASK) {
            q->flags |= FLASH_QUAD_MASK;
        }
        if (prop->flags & FLASH_OV_MASK_SET) {
            q->flags |= FLASH_QUAD_MASK_SET;
        }
    }
}


void FlashClipList_Init(FlashClipList *list, Flash *flash);
void FlashClipList_Term(FlashClipList *list);
void FlashClipList_Reset(FlashClipList *list);
void FlashClipList_FindRef(FlashClipList *list, char *parent, char *name, FlashRef *out);
void FlashShapePool_Init(FlashShapePool *pool, u32 count);
void FlashShapePool_Term(FlashShapePool *pool);
void FlashShapePool_Clear(FlashShapePool *pool);

/* Sets a movie up on a FOD file and its textures: allocates the root timeline, the sprite instances and the
   shape pool (one shape per depth of every timeline). The object must be zeroed or freshly destroyed. */
void Flash_Create(Flash *flash, u8 *file, FlashTex **tex) {
    Flash_CheckHeader(file);
    memset(flash, 0, sizeof(Flash));
    flash->tex = tex;
    flash->data = file + 8;
    flash->speed = 0;
    flash->ofs[0] = 0;
    flash->ofs[1] = 0;
    if (flash->root == NULL) {
        flash->root = Heap_Alloc(sizeof(FlashTl), 0x20, 0, 2);
        memset(flash->root, 0, sizeof(FlashTl));
    }
    FlashTl_Init(flash->root, Flash_FindTag(flash->data, 7) + 8, flash);
    flash->root->state |= FLASH_TL_PLAY;
    if (flash->clips == NULL) {
        flash->clips = Heap_Alloc(sizeof(FlashClipList), 0x20, 0, 2);
        memset(flash->clips, 0, sizeof(FlashClipList));
    }
    FlashClipList_Init(flash->clips, flash);
    if (flash->shapes == NULL) {
        flash->shapes = Heap_Alloc(sizeof(FlashShapePool), 0x20, 0, 2);
        memset(flash->shapes, 0, sizeof(FlashShapePool));
    }
    FlashShapePool_Init(flash->shapes, flash->root->depths + flash->clips->depths);
}

/* Frees everything Flash_Create allocated. */
void Flash_Destroy(Flash *flash) {
    if (flash->shapes != NULL) {
        FlashShapePool_Term(flash->shapes);
        if (flash->shapes != NULL) {
            Heap_Free(flash->shapes);
            flash->shapes = NULL;
        }
    }
    if (flash->clips != NULL) {
        FlashClipList_Term(flash->clips);
        if (flash->clips != NULL) {
            Heap_Free(flash->clips);
            flash->clips = NULL;
        }
    }
    if (flash->root != NULL) {
        FlashTl_Term(flash->root);
        if (flash->root != NULL) {
            Heap_Free(flash->root);
            flash->root = NULL;
        }
    }
}

/* One tick: clears the event bits of the last tick and, while playing, runs `speed` frames of the root. */
/* The play test reads the flags back through a pointer to the field that is set AFTER the masking store (the
   same habit as `speed`): the compiler knows the value but keeps it as a second register, which is the
   original's `and v1,v1,v0 / move v0,v1 / andi v0,v0,1`. Testing a local or the field itself gives
   `andi v0,v1,1` (one instruction short), and a pointer initialised at its declaration makes the function
   two instructions longer. Found from a decomp-permuter candidate
   (`if ((*(p = &flash->flags) & ~FLASH_END) & FLASH_PLAY)`), of which only the late pointer is needed. */
void Flash_Advance(Flash *flash) {
    s32 *speed = &flash->speed;
    u32 *flags;

    flash->trig = 0;
    flash->se = 0;
    flash->flags &= ~FLASH_END;
    flags = &flash->flags;
    if (*flags & FLASH_PLAY) {
        FlashTl_Advance(flash->root, *speed, 0, 0);
    }
}

/* Draws the movie (after the default drawing environment) unless it is hidden. */
void Flash_Draw(Flash *flash) {
    Gfx_AddDefaultEnv();
    if (!(flash->flags & FLASH_HIDE)) {
        FlashTl_Draw(flash->root, NULL);
    }
}

#ifdef PORT
typedef struct PortFlashHiddenSlot {
    FlashSlot *slot;
    FlashSlot contents;
} PortFlashHiddenSlot;

static PortFlashHiddenSlot *sPortFlashHidden;
static s32 sPortFlashHiddenCap;

static s32 Port_FlashIsInputClip(char *name) {
    return strncmp(name, "mc_input_code_", 14) == 0 || strcmp(name, "mc_text_keyboard") == 0;
}

/* Mask unrelated movie depths only for this draw. The password screen has
   unnamed shapes, so hiding its named decoration clips leaves those shapes
   (including the old title) visible. Restoring slots lets timelines advance. */
static s32 Port_FlashMaskInput(FlashTl *tl, s32 *count, s32 root) {
    s32 i, kept = 0;

    for (i = 0; i < tl->depths; i++) {
        FlashSlot *slot = &tl->slots[i];
        FlashSlot before = *slot;

        if (slot->clip != NULL) {
            if (Port_FlashIsInputClip(slot->clip->name)) {
                kept = 1;
            } else if (Port_FlashMaskInput(&slot->clip->tl, count, 0)) {
                kept = 1;
            } else {
                slot->clip = NULL;
            }
        }
        /* Password Input root shapes 7 and 9 are the keyboard backing and
           delete mark. Keep their original placement with the key clips. */
        if (root && slot->shape != NULL && (slot->shape->id == 7 || slot->shape->id == 9))
            kept = 1;
        else
            slot->shape = NULL;
        if (before.clip != slot->clip || before.shape != slot->shape) {
            if (*count < sPortFlashHiddenCap) {
                sPortFlashHidden[*count].slot = slot;
                sPortFlashHidden[*count].contents = before;
                (*count)++;
            } else {
                *slot = before;
            }
        }
    }
    return kept;
}

s32 Port_FlashInputOnlyInit(Flash *flash) {
    s32 i;
    sPortFlashHiddenCap = flash->root->depths;
    for (i = 0; i < flash->clips->count; i++)
        sPortFlashHiddenCap += flash->clips->items[i].tl.depths;
    if (sPortFlashHiddenCap == 0) return 0;
    sPortFlashHidden = Heap_Alloc(sizeof(*sPortFlashHidden) * sPortFlashHiddenCap, 0x20, 0, 2);
    return sPortFlashHidden != NULL;
}

void Port_FlashInputOnlyTerm(void) {
    if (sPortFlashHidden != NULL) Heap_Free(sPortFlashHidden);
    sPortFlashHidden = NULL;
    sPortFlashHiddenCap = 0;
}

void Port_FlashInputOnlyDraw(Flash *flash) {
    s32 oldScope=sSelectionMouseScope;
    s32 count = 0;
    if (sPortFlashHidden == NULL) return;
    Port_FlashMaskInput(flash->root, &count, 1);
    sSelectionMouseScope=62;
    Flash_MenuMarker(0x13); /* shared keyboard uses the full page */
    Flash_Draw(flash);
    sSelectionMouseScope=oldScope;
    while (count > 0) {
        count--;
        *sPortFlashHidden[count].slot = sPortFlashHidden[count].contents;
    }
}

s32 Port_FlashRootPlaying(void *movie) {
    Flash *flash = (Flash *)movie;
    return flash->root != NULL && (flash->root->state & FLASH_TL_PLAY) != 0;
}

/* Called before FlashAnim_Scroll increments the phase. Keep its integer UV
 * rectangle and supply the fractional texel only while drawing this cloud tree. */
f32 Port_FlashTrainingCloudScroll(f32 offset, f32 speed) {
    s32 aspect = Port_PageAspectMilli();
    sTrainingCloudFraction = 0;
    if (Port_TitleMainChars43() && !Port_Menus43() && aspect > 1333) {
        sTrainingCloudFraction = Port_TrainingCloudFraction(offset);
        speed *= 1333.333f / (f32)aspect;
    }
    return speed;
}

/* Training's decorative child animation has its own pacing. */
void Port_FlashAdvanceTraining(void *movie) {
    s32 old = sTrainingSlowFlight;
    sTrainingSlowFlight = Port_TitleMainChars43() && !Port_Menus43() && Port_PageAspectMilli() > 1333;
    sTrainingFlightPhase += Port_TrainingFlightRate(Port_PageAspectMilli());
    sTrainingFlightTick = sTrainingFlightPhase >= 1.0f;
    if (sTrainingFlightTick) sTrainingFlightPhase -= 1.0f;
    Flash_Advance((Flash *)movie);
    sTrainingSlowFlight = old;
}

/* Scope foreground markers to selected movies/windows; other menus call Flash_Draw as before. */
void Port_FlashDrawMenuArt(void *movie, s32 screen, s32 titleMenuVisible) {
    s32 old = sMenuArtScreen;
    s32 oldTitleVisible = sTitleMenuEntriesVisible;
    s32 oldSelectionScope = sSelectionMouseScope;
    sSelectionMouseScope = screen == 13 ? 70 + titleMenuVisible : screen;
    s32 oldMouseDraw = sMainMouseDraw;
    s32 oldTitleMouseDraw = sTitleMouseDraw;
    sTitleMouseDraw = screen == 1 && titleMenuVisible;
    if (screen == 1) memset(sTitleMousePlates, 0, sizeof(sTitleMousePlates));
    if (screen == 2) {
        memset(sMainMousePlates, 0, sizeof(sMainMousePlates));
        memset(sMainMouseArrows, 0, sizeof(sMainMouseArrows));
        sMainMouseDraw = 1;
    }
    if (!Port_TitleMainChars43() || Port_Menus43()) {
        Flash_Draw((Flash *)movie);
        sMainMouseDraw = oldMouseDraw;
        sTitleMouseDraw = oldTitleMouseDraw;
        sSelectionMouseScope = oldSelectionScope;
        return;
    }
    s32 oldOffsetX = ((Flash *)movie)->ofs[0];
    sMenuArtScreen = screen;
    sTitleMenuEntriesVisible = titleMenuVisible;
    if (screen == 11 || screen == 12 || screen == 23 || screen == 32 || screen == 37 || screen == 38 || screen == 42 || screen == 48 || screen == 57) {
        /* Disc Fusion recognition (42) shares the left-anchored panel's coordinates. */
        Flash_MenuMarker(screen == 12 || screen == 32 || screen == 57 ? 0x1B : 0x1A);
    }
    if (screen == 4 || screen == 14 || screen == 30 || screen == 33 || screen == 34 || screen == 36 || screen == 40 || screen == 43 || (screen >= 25 && screen <= 27) || screen == 52 || screen == 58 || screen == 59 || screen == 69) {
        /* Dialogs, item details and Sim Dragon minigames share one centre transform. */
        Flash_MenuMarker(0x19);
    }
    if (screen == 58 && Port_PageAspectMilli() > 1333) {
        Flash *flash = (Flash *)movie;
        if (flash->root->depths && flash->root->slots[0].shape) {
            f32 travel = (123.45f - flash->root->slots[0].shape->mtx.m[2]) / 380.0f;
            if (travel < 0.0f) travel = 0.0f;
            if (travel > 1.0f) travel = 1.0f;
            flash->ofs[0] -= (s32)ceilf(256.0f * ((f32)Port_PageAspectMilli() / 1333.333f - 1.0f) * travel);
        }
    }
    Flash_Draw((Flash *)movie);
    if (screen == 11 || screen == 12 || screen == 14 || screen == 23 || screen == 30 || screen == 32 || screen == 37 || screen == 33 || screen == 34 || screen == 38 || screen == 36 || screen == 40 || screen == 43 || screen == 48 || (screen >= 25 && screen <= 27) || screen == 52 || screen == 58 || screen == 59 || screen == 57 || screen == 69) {
        extern void Font_FlushAll(void);
        Font_FlushAll(); /* Callback glyphs must flush before the enclosing group resets. */
    }
    if (screen == 4 || screen == 11 || screen == 12 || screen == 14 || screen == 23 || screen == 30 || screen == 32 || screen == 37 || screen == 33 || screen == 34 || screen == 38 || screen == 36 || screen == 40 || screen == 42 || screen == 43 || screen == 48 || (screen >= 25 && screen <= 27) || screen == 52 || screen == 58 || screen == 59 || screen == 57 || screen == 69) {
        Flash_MenuMarker(0x13);
    }
    ((Flash *)movie)->ofs[0] = oldOffsetX;
    sMenuArtScreen = old;
    sTitleMenuEntriesVisible = oldTitleVisible;
    sMainMouseDraw = oldMouseDraw;
    sTitleMouseDraw = oldTitleMouseDraw;
    sSelectionMouseScope = oldSelectionScope;
}
#endif

/* Back to the first frame, playing, every other flag cleared. Unless `keepClips`, every sprite instance and
   shape is cleared as well. */
void Flash_Reset(Flash *flash, s32 keepClips) {
    flash->flags = FLASH_PLAY;
    FlashTl_Rewind(flash->root, keepClips);
    flash->root->state |= FLASH_TL_PLAY;
    if (!keepClips) {
        FlashClipList_Reset(flash->clips);
        FlashShapePool_Clear(flash->shapes);
    }
}

/* Starts playing at `speed` frames per tick. */
void Flash_Play(Flash *flash, s32 speed) {
    flash->speed = speed;
    flash->flags |= FLASH_PLAY;
}

/* Stops Flash_Advance from running the timelines. */
void Flash_Stop(Flash *flash) {
    flash->flags &= ~FLASH_PLAY;
}

/* Sets the screen offset added to everything drawn. */
void Flash_SetOffset(Flash *flash, s32 x, s32 y) {
    flash->ofs[0] = x;
    flash->ofs[1] = y;
}

/* Sets or clears bits of Flash.flags. */
void Flash_SetFlag(Flash *flash, u32 mask, u8 on) {
    if (on) {
        flash->flags |= mask;
    } else {
        flash->flags &= ~mask;
    }
}

/* Jumps the root timeline to a frame label ("fl_..."). */
void Flash_GotoLabel(Flash *flash, char *label, u8 restart) {
    FlashTl_GotoLabel(flash->root, label, restart);
}

/* Moves the root timeline by `step` frames. */
void Flash_StepFrames(Flash *flash, s32 step) {
    FlashTl_StepFrames(flash->root, step);
}

/* Looks a sprite instance up by name ("mc_..."), optionally among the children of the instance called `parent`.
   out->index is -1 when there is none. (Existing name; it finds a clip, not a label.) */
void Flash_FindLabel(Flash *flash, char *parent, char *name, FlashRef *out) {
    memset(out, 0, sizeof(FlashRef));
    FlashClipList_FindRef(flash->clips, parent, name, out);
}

/* The clip-list functions of the next file (src/sys/gfxm_d.c), declared here as this file calls them. */
extern void FlashClipList_GotoLabel(FlashClipList *list, FlashRef *ref, char *label);
extern void FlashClipList_SetPreDraw(FlashClipList *list, FlashRef *ref, void *fn, void *arg);
extern void FlashClipList_SetPostDraw(FlashClipList *list, FlashRef *ref, void *fn, void *arg);
extern void FlashClipList_SetDrawOver(FlashClipList *list, FlashRef *ref, void *fn, void *arg);
extern void FlashClipList_SetFlags(FlashClipList *list, FlashRef *ref, s32 mask, u8 on);
extern void FlashClipList_SetOverride(FlashClipList *list, FlashRef *ref, s32 mask, u8 on);
extern void FlashClipList_SetOffset(FlashClipList *list, FlashRef *ref, s32 x, s32 y);
extern void FlashClipList_SetScale(FlashClipList *list, FlashRef *ref, f32 x, f32 y);
extern void FlashClipList_SetAlpha(FlashClipList *list, FlashRef *ref, f32 alpha);
extern void FlashClipList_SetColor(FlashClipList *list, FlashRef *ref, f32 value);
extern void FlashClipList_SetTex(FlashClipList *list, FlashRef *ref, s32 user);
extern void FlashClipList_SetUv(FlashClipList *list, FlashRef *ref, void *rect);
extern void FlashClipList_GetPos(FlashClipList *list, FlashRef *ref, s32 *x, s32 *y);
extern f32 FlashClipList_GetAlpha(FlashClipList *list, FlashRef *ref);
void FlashClipList_Play(FlashClipList *list, FlashRef *ref);
void FlashClipList_Stop(FlashClipList *list, FlashRef *ref);

/* Jumps a clip (and its namesakes) to a label of its own timeline. */
void Flash_ClipGotoLabel(Flash *flash, FlashRef *ref, char *label) {
    if (ref->index >= 0) {
        FlashClipList_GotoLabel(flash->clips, ref, label);
    }
}

/* Sets a clip's callback run before its children are drawn (FlashClip.preDraw / preArg). */
void Flash_ClipSetCallbackA(Flash *flash, FlashRef *ref, void *fn, void *arg) {
    if (ref->index >= 0) {
        FlashClipList_SetPreDraw(flash->clips, ref, fn, arg);
    }
}

/* Sets a clip's callback run after everything of it is drawn (FlashClip.postDraw / postArg). */
void Flash_ClipSetCallbackB(Flash *flash, FlashRef *ref, void *fn, void *arg) {
    if (ref->index >= 0) {
        FlashClipList_SetPostDraw(flash->clips, ref, fn, arg);
    }
}

/* Sets a clip's callback run after its children, which gets the clip's drawing state (FlashClip.drawOver). */
void Flash_ClipSetCallbackC(Flash *flash, FlashRef *ref, void *fn, void *arg) {
    if (ref->index >= 0) {
        FlashClipList_SetDrawOver(flash->clips, ref, fn, arg);
    }
}

/* Sets or clears properties of a clip, one bit of `props` each: 1 playing, 2 visible, 4 flip x, 8 flip y,
   0x10 no screen offset, 0x20 additive, 0x40 subtractive, 0x80 masked, 0x100 mask writer, 0x200 repeat,
   0x400 keeps running while its parent is stopped. */
s32 Flash_ClipSetFlags(Flash *flash, FlashRef *ref, s32 props, u8 on) {
    if (ref->index >= 0) {
        if (props & 1) {
            if (on) {
                FlashClipList_Play(flash->clips, ref);
            } else {
                FlashClipList_Stop(flash->clips, ref);
            }
        }
        if (props & 2) {
            FlashClipList_SetFlags(flash->clips, ref, FLASH_CLIP_VISIBLE, on);
        }
        if (props & 0x400) {
            FlashClipList_SetFlags(flash->clips, ref, FLASH_CLIP_FREE_RUN, on);
        }
        if (props & 4) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_FLIPX, on);
        }
        if (props & 8) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_FLIPY, on);
        }
        if (props & 0x10) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_NO_OFS, on);
        }
        if (props & 0x20) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_ADD, on);
        }
        if (props & 0x40) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_SUB, on);
        }
        if (props & 0x80) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_MASK, on);
        }
        if (props & 0x100) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_MASK_SET, on);
        }
        if (props & 0x200) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_REPEAT, on);
        }
    }
}

/* Gives a clip a position offset. */
void Flash_ClipSetOffset(Flash *flash, FlashRef *ref, s32 x, s32 y) {
    if (ref->index >= 0) {
        FlashClipList_SetOffset(flash->clips, ref, x, y);
    }
}

/* Gives a clip a scale; negative values are refused. */
void Flash_ClipSetScale(Flash *flash, FlashRef *ref, f32 x, f32 y) {
    if (ref->index >= 0 && !(x < 0.0f) && !(y < 0.0f)) {
        FlashClipList_SetScale(flash->clips, ref, x, y);
    }
}

/* Gives a clip an alpha multiplier; a negative value is refused. */
void Flash_ClipSetAlpha(Flash *flash, FlashRef *ref, f32 alpha) {
    if (ref->index >= 0 && !(alpha < 0.0f)) {
        FlashClipList_SetAlpha(flash->clips, ref, alpha);
    }
}

/* Gives a clip a colour multiplier (r, g, b); a negative value is refused. */
void Flash_ClipSetColor(Flash *flash, FlashRef *ref, f32 color) {
    if (ref->index >= 0 && !(color < 0.0f)) {
        FlashClipList_SetColor(flash->clips, ref, color);
    }
}

/* Makes a clip draw with texture `tex` of its image's texture file (a palette or a frame of a strip). */
void Flash_ClipSetTex(Flash *flash, FlashRef *ref, s32 tex) {
    if (ref->index >= 0) {
        FlashClipList_SetTex(flash->clips, ref, tex);
    }
}

/* Replaces a clip's texture rectangle (four texel coordinates). */
void Flash_ClipSetUv(Flash *flash, FlashRef *ref, void *rect) {
    if (ref->index >= 0) {
        FlashClipList_SetUv(flash->clips, ref, rect);
    }
}

/* Returns a clip's screen position. */
void Flash_ClipGetPos(Flash *flash, FlashRef *ref, s32 *x, s32 *y) {
    if (ref->index >= 0) {
        FlashClipList_GetPos(flash->clips, ref, x, y);
    }
}

/* Returns a clip's alpha, 0 when the reference is empty. */
f32 Flash_ClipGetAlpha(Flash *flash, FlashRef *ref) {
    f32 alpha = 0.0f;

    if (ref->index < 0) {
        return alpha;
    }
    return FlashClipList_GetAlpha(flash->clips, ref);
}

#ifdef PORT
/* Fractional placement and scale of a named clip in its own timeline. */
void Flash_ClipGetTransform(Flash *flash, FlashRef *ref, f32 *x, f32 *y, f32 *sx, f32 *sy) {
    FlashProp *prop;

    *x = *y = 0.0f;
    *sx = *sy = 1.0f;
    if (ref->index < 0) return;
    prop = &flash->clips->items[ref->index].prop;
    *x = prop->mtx.m[2];
    *y = prop->mtx.m[5];
    if (prop->flags & FLASH_OV_POS) {
        *x += prop->x;
        *y += prop->y;
    }
    *sx = prop->mtx.m[0];
    *sy = prop->mtx.m[4];
    if (prop->flags & FLASH_OV_SCALE) {
        *sx *= prop->scaleX;
        *sy *= prop->scaleY;
    }
}

/* A placed shape in a clip's own timeline (the main header title is shape 2). */
s32 Flash_ClipGetShapeTransform(Flash *flash, FlashRef *ref, s32 depth,
                                f32 *x, f32 *y, f32 *sx, f32 *sy) {
    FlashClip *clip;
    FlashShape *shape;

    if (ref->index < 0) return 0;
    clip = &flash->clips->items[ref->index];
    if (depth < 1 || depth > clip->tl.depths) return 0;
    shape = clip->tl.slots[depth - 1].shape;
    if (shape == NULL) return 0;
    *x = shape->mtx.m[2];
    *y = shape->mtx.m[5];
    *sx = shape->mtx.m[0];
    *sy = shape->mtx.m[4];
    return 1;
}
#endif

/* Allocates the pool of placed shapes. */
void FlashShapePool_Init(FlashShapePool *pool, u32 count) {
    pool->count = count;
    if (pool->items == NULL) {
        pool->items = Heap_Alloc(count * sizeof(FlashShape), 0x20, 0, 2);
        memset(pool->items, 0, pool->count * sizeof(FlashShape));
    }
}

/* Frees the pool's storage. */
void FlashShapePool_Term(FlashShapePool *pool) {
    if (pool->items != NULL) {
        Heap_Free(pool->items);
        pool->items = NULL;
    }
}

/* Frees every shape of the pool. */
void FlashShapePool_Clear(FlashShapePool *pool) {
    if (pool->items != NULL) {
        memset(pool->items, 0, pool->count * sizeof(FlashShape));
    }
}

/* Returns a cleared free shape, or NULL when the pool is full. */
FlashShape *FlashShapePool_Alloc(FlashShapePool *pool) {
    u32 i;
    FlashShape *shape = pool->items;

    for (i = 0; i < pool->count; i++, shape++) {
        if (shape->used == 0) {
            memset(shape, 0, sizeof(FlashShape));
            return shape;
        }
    }
    return NULL;
}

/* Places shape record `id` with a matrix and a colour transform (NULL: identity). */
FlashShape *FlashShape_Create(FlashShapePool *pool, u16 id, FlashMtx *mtx, FlashCxform *cx) {
    FlashShape *shape = FlashShapePool_Alloc(pool);

    if (shape == NULL) {
        return NULL;
    }
    shape->id = id;
    shape->used = 1;
    if (mtx != NULL) {
        shape->mtx = *mtx;
    } else {
        Flash_MtxIdentity(&shape->mtx);
    }
    if (cx != NULL) {
        shape->cx = *cx;
    } else {
        Flash_CxformIdentity(&shape->cx);
    }
    return shape;
}

#ifdef PORT
/* Custom captions are drawn inside their native text timelines, inheriting
   their focus crossfade, pulse, scale, alpha and offsets. Top-level Video also
   inherits the live Controller row's entrance/exit transform. */
static s32 sOptionsPlateDim;
s32 Port_FlashOptionsCaptionStyle(s32 layer,u32 *tint) {
    if(layer<0 || layer>1) return 0;
    *tint=sOptionsCaptionTint[layer];return sOptionsCaptionAlpha[layer];
}
/* Dragon Net Battle uses GS RGB 0x58 (neutral is 0x80), with
   unchanged alpha. Tint the complete native row, including its caption. */
void Port_FlashOptionsCaptionMotion(s32 layer,f32 *x,f32 *y) {
    *x=layer>=0 && layer<2 ? sOptionsCaptionX[layer]:0;
    *y=layer>=0 && layer<2 ? sOptionsCaptionY[layer]:0;
}
static void Flash_OptionsDisabledProp(FlashProp *prop) {
    s32 i;
    for(i=0;i<3;i++) {
        prop->cx.mul[i]*=88.0f/128.0f;
        prop->cx.add[i]=(s16)(prop->cx.add[i]*88/128);
    }
}
void Port_FlashOptionsPlateDim(s32 disabled) { sOptionsPlateDim=disabled; }
static void Flash_OptionsPlateAt(void *movie,void *reference,const char *liveName,f32 x,f32 y,const char *label,s32 relative) {
    Flash *flash=(Flash *)movie;
    FlashClip *clip=FlashClipList_FindByName(flash->clips,"mc_menu_plate_4");
    FlashClip *live=reference ? FlashClipList_FindByName(((Flash *)reference)->clips,(char *)liveName):NULL;
    FlashSlot slot;
    FlashProp saved;
    u32 flags;
    s32 oldScreen=sMenuArtScreen,oldScope=sSelectionMouseScope;
    s32 oldDepth=sMenuArtDrawDepth,oldMarker=sFlashMouseMarker;
    if(!clip || (reference && (!live || !(live->flags&FLASH_CLIP_VISIBLE)))) return;
    sOptionsCaptionAlpha[0]=sOptionsCaptionAlpha[1]=0;
    saved=clip->prop;flags=clip->flags;
    if(live) {
        clip->prop=live->prop;
        clip->prop.mtx.m[2]+=x-(relative ? 0:saved.mtx.m[2]);
        clip->prop.mtx.m[5]+=y-(relative ? 0:saved.mtx.m[5]);
    } else {
        clip->prop.mtx.m[2]=x;clip->prop.mtx.m[5]=y;
        clip->prop.flags &= ~FLASH_OV_POS;
    }
    if(sOptionsPlateDim) Flash_OptionsDisabledProp(&clip->prop);
    clip->flags|=FLASH_CLIP_VISIBLE;
    memset(&slot,0,sizeof(slot));slot.clip=clip;
    sMenuArtScreen=68;sSelectionMouseScope=0;sMenuArtDrawDepth=0;sOptionsPlateCopy=1;sOptionsPlateLabel=label;
    FlashSlot_Draw(&slot,NULL,flash);
    sOptionsPlateCopy=0;sOptionsPlateLabel=NULL;sMenuArtScreen=oldScreen;sSelectionMouseScope=oldScope;
    clip->prop=saved;clip->flags=flags;
    sMenuArtDrawDepth=oldDepth;Flash_MenuMarker(oldMarker);
}
void Port_FlashOptionsPlate(void *movie,void *reference,f32 x,f32 y,const char *label) {
    Flash_OptionsPlateAt(movie,reference,"mc_menu_plate_4",x,y,label,0);
}
f32 Port_FlashOptionsRowSlide(void *movie,s32 row) {
    static char *names[]={"mc_menu_plate_1","mc_menu_plate_2","mc_menu_plate_3","mc_menu_plate_4"};
    FlashClip *clip;
    if(row<0 || row>=4) return 0;
    clip=FlashClipList_FindByName(((Flash *)movie)->clips,names[row]);
    /* Read even hidden Controller rows: their root timelines still animate.
       Subtract the authored resting position, preserving the mapping layout. */
    return clip ? clip->prop.mtx.m[2]-(95-20*row):0;
}
#endif

#ifdef PORT
/* Main Menu arrow motion sampled from its authored FOD timeline:
   19-frame lead, 15-frame stretch/fade entrance, and 40-frame six-pixel bob.
   Keep the Options atlas and resting anchors, including their mouse targets. */
static s32 sOptionsChevronFrame,sOptionsChevronPage=-1;
static struct { s32 frame,age;f32 y; } sOptionsChevronMotion[4];
void Port_FlashOptionsChevronFrame(s32 page) {
    s32 i;
    sOptionsChevronFrame++;
    if(page!=sOptionsChevronPage) {
        for(i=0;i<4;i++) sOptionsChevronMotion[i].frame=-2;
        sOptionsChevronPage=page;
    }
}
void Port_FlashOptionsChevron(void *movie,s32 down,f32 x,f32 y) {
    static const u8 bobTwips[40]={0,19,30,39,47,55,62,68,75,81,87,92,97,102,106,111,114,117,119,120,119,118,115,113,109,106,102,98,93,88,83,78,72,66,59,52,44,34,23,0};
    Flash *flash=(Flash *)movie;
    FlashQuad q;
    s32 age,alpha;
    f32 progress,stretch,bob,halfX=16,halfY=16;
    if(down<0 || down>3) return;
    age=sOptionsChevronMotion[down].frame==sOptionsChevronFrame-1 && sOptionsChevronMotion[down].y==y ?
        sOptionsChevronMotion[down].age+1:0;
    sOptionsChevronMotion[down].frame=sOptionsChevronFrame;
    sOptionsChevronMotion[down].age=age;sOptionsChevronMotion[down].y=y;
    progress=down>=2 ? 1:age<19 ? 0:age>=34 ? 1:(f32)(age-19)/15;
    stretch=1+(1-progress)*0.46774f;
    alpha=(s32)(128*progress);
    bob=bobTwips[age%40]/20.0f;
    if(down<2) {
        y=158+(y-158)*stretch+(down ? bob:-bob)*stretch;
        halfY*=stretch;
    } else {
        x=420+(x-420)*stretch+(down==3 ? bob:-bob)*stretch;
        halfX*=stretch;
    }
    memset(&q,0,sizeof(q));
    q.pt[0][0]=q.pt[2][0]=x-halfX;q.pt[1][0]=q.pt[3][0]=x+halfX;
    q.pt[0][1]=q.pt[1][1]=y-halfY;q.pt[2][1]=q.pt[3][1]=y+halfY;
    q.uv.u0=(down&1) ? 32:0;q.uv.u1=q.uv.u0+32;
    q.uv.v0=down>=2 ? 0:32;q.uv.v1=q.uv.v0+32;
    q.rgba[0]=q.rgba[1]=q.rgba[2]=128;q.rgba[3]=alpha;
    Flash_MenuMarker(0x1B);FlashDraw_Quad(flash->tex[34],&q);Flash_MenuMarker(0x13);
}

#endif

#ifdef PORT
/* Use a native 512x64 heading sheet with the authored sprite's UV mapping.
   There is no independent fitting of letters or conversion from font metrics. */
static void Flash_OptionsHeadingQuad(FlashQuad *title,f32 x0,f32 y0,f32 x1,f32 y1,
                                     f32 u0,f32 v0,f32 u1,f32 v1) {
    f32 sx=u1>u0 ? (x1-x0)/(u1-u0):1;
    f32 sy=v1>v0 ? (y1-y0)/(v1-v0):1;
    f32 left=x0-u0*sx,top=y0-v0*sy;
    memset(title,0,sizeof(*title));
    title->pt[0][0]=title->pt[2][0]=left;title->pt[1][0]=title->pt[3][0]=left+512*sx;
    title->pt[0][1]=title->pt[1][1]=top;title->pt[2][1]=title->pt[3][1]=top+64*sy;
}

/* Centre the replacement's ink in the authored caption shape's local space. */
static void Flash_OptionsCaptionQuad(FlashQuad *caption,f32 x0,f32 y0,f32 x1,f32 y1,f32 inkX,f32 inkY) {
    f32 x=(x0+x1)*0.5f-inkX,y=(y0+y1)*0.5f-inkY;
    memset(caption,0,sizeof(*caption));
    caption->pt[0][0]=caption->pt[2][0]=x;caption->pt[1][0]=caption->pt[3][0]=x+512;
    caption->pt[0][1]=caption->pt[1][1]=y;caption->pt[2][1]=caption->pt[3][1]=y+32;
}
#endif

/* Gives a shape back to the pool. */
void FlashShape_Free(FlashShape *shape) {
    memset(shape, 0, sizeof(FlashShape));
}

/* Replaces a shape's matrix and / or colour transform (NULL: keep). */
void FlashShape_Set(FlashShape *shape, FlashMtx *mtx, FlashCxform *cx) {
    if (mtx != NULL) {
        shape->mtx = *mtx;
    }
    if (cx != NULL) {
        shape->cx = *cx;
    }
}

/* Draws a placed shape: each quad of its record (tag 4), textured with its image's texture file or flat when
   the image index is negative. A quad whose image has no texture file is skipped. */
void FlashShape_Draw(FlashShape *shape, FlashProp *prop, Flash *flash) {
    FlashShapeQuad rec;
    FlashQuad quad;
    u16 count;
    u16 w;
    u16 h;
    u8 *data;
    u8 *img;
    FlashTex *tex;
    s32 i;
    FlashQuad *pq;
#ifdef PORT
    s32 markImage;
    s32 fullWidthDialogDim;
    FlashMtx trainingMatrix = FlashTraining_FlightMatrix(shape);
#endif

    data = Flash_GetRecord(flash->data, 4, shape->id);
    memcpy(&count, data + 8, 2);
    for (i = 0; i < count; i++) {
        FlashQuad *pm = &quad;

        pq = pm;
        tex = NULL;
        memset(pm, 0, sizeof(quad));
        memset(&rec, 0, sizeof(rec));
        memcpy(&rec, data + 10 + i * sizeof(rec), sizeof(rec));
#ifdef PORT
        if (sMenuArtScreen == 9) {
            s32 panel = (rec.image == 42 || rec.image == 43) ? 1 :
                        (rec.image >= 44 && rec.image <= 46) ? 2 :
                        (rec.image == 47 || rec.image == 48) ? 4 : 0;
            if (panel && !(sTitleMenuEntriesVisible & panel)) continue;
        }
        if (sMenuArtScreen == 1 && !sTitleMenuEntriesVisible && (rec.image == 6 || rec.image == 9)) {
            continue; /* title option labels are not drawn before fl_menu_in */
        }
        if (sMenuArtScreen == 5 &&
            ((rec.image == 37 && !(sTitleMenuEntriesVisible & 2)) ||
             (rec.image == 76 && !(sTitleMenuEntriesVisible & 6)) ||
             (rec.image == 82 && !(sTitleMenuEntriesVisible & 4)))) {
            /* Image 76 is reused by BOTH saga and difficulty timelines. */
            continue;
        }
#endif
        if (rec.image >= 0) {
            img = Flash_GetRecord(flash->data, 3, rec.image);
            tex = flash->tex[rec.image];
            if (tex == NULL) {
                continue;
            }
            quad.uv.v0 = 0;
            quad.uv.u0 = 0;
            quad.uv.tex = 0;
            memcpy(&w, img + 8, 2);
            memcpy(&h, img + 10, 2);
            quad.uv.u1 = w;
            quad.uv.v1 = h;
        }
        quad.pt[2][0] = rec.x0;
        quad.pt[3][0] = rec.x1;
        quad.pt[1][1] = rec.y0;
        quad.pt[3][1] = rec.y1;
        quad.pt[0][0] = rec.x0;
        quad.pt[1][0] = rec.x1;
        quad.pt[0][1] = rec.y0;
        quad.pt[2][1] = rec.y1;
        quad.rgba[0] = rec.rgba >> 24;
        quad.rgba[1] = (rec.rgba >> 16) & 0xFF;
        quad.rgba[2] = (rec.rgba & 0xFF00) >> 8;
        quad.rgba[3] = rec.rgba & 0xFF;
#ifdef PORT
        FlashDraw_TransformQuad(pq, &trainingMatrix, &shape->cx, prop, flash->ofs);
#else
        FlashDraw_TransformQuad(pq, &shape->mtx, &shape->cx, prop, flash->ofs);
#endif
#ifdef PORT
        if(sMenuArtScreen==3 && rec.image==4 && sOptionsHeading) {
            extern s32 PortTbo_TextId(const char *text,s32 kind);
            extern void Ps_SpriteScreen(s32 id,f32 x0,f32 y0,f32 x1,f32 y1,f32 u0,f32 v0,f32 u1,f32 v1,u32 tint,s32 alpha);
            s32 id=PortTbo_TextId(sOptionsHeading,5);
            if(id>=0 && pq->rgba[3]>0) {
                FlashQuad title;
                FlashCxform color;
                Flash_OptionsHeadingQuad(&title,rec.x0,rec.y0,rec.x1,rec.y1,
                    (f32)pq->uv.u0,(f32)pq->uv.v0,(f32)pq->uv.u1,(f32)pq->uv.v1);
                Flash_CxformIdentity(&color);
                FlashDraw_TransformQuad(&title,&trainingMatrix,&color,prop,flash->ofs);
                Ps_SpriteScreen(id,title.pt[0][0],title.pt[0][1],title.pt[3][0],title.pt[3][1],0,0,1,1,0x808080,pq->rgba[3]);
            }
            continue;
        }
        if(sOptionsPickerCaption && sOptionsCaptionLayer<0 &&
           pq->uv.u0==0 && pq->uv.v0==0 && pq->uv.u1==256 && pq->uv.v1==64) {
            /* Same visible body used by Flash_MouseOptionsSelectorPiece:
               exclude transparent atlas padding and the larger focus glow. */
            f32 x0=pq->pt[0][0],y0=pq->pt[0][1];
            f32 x1=x0+(pq->pt[1][0]-x0)*(168.0f/256.0f);
            f32 y1=y0+(pq->pt[2][1]-y0)*(40.0f/64.0f);
            sOptionsPickerBounds[0]=x0<x1 ? x0:x1;
            sOptionsPickerBounds[1]=y0<y1 ? y0:y1;
            sOptionsPickerBounds[2]=x0>x1 ? x0:x1;
            sOptionsPickerBounds[3]=y0>y1 ? y0:y1;
            sOptionsPickerBoundsValid=1;
        }
        if(sOptionsPlateCopy && sOptionsCaptionLayer>=0) {
            extern s32 PortTbo_TextId(const char *text,s32 kind);
            extern void Ps_SpriteScreen(s32 id,f32 x0,f32 y0,f32 x1,f32 y1,f32 u0,f32 v0,f32 u1,f32 v1,u32 tint,s32 alpha);
            s32 id=PortTbo_TextId(sOptionsPlateLabel,sOptionsCaptionLayer ? 4:3);
            if(id>=0 && pq->rgba[3]>0) {
                u32 tint=pq->rgba[0] | (pq->rgba[1]<<8) | (pq->rgba[2]<<16);
                sOptionsCaptionTint[sOptionsCaptionLayer]=tint;
                sOptionsCaptionAlpha[sOptionsCaptionLayer]=pq->rgba[3];
                if(sOptionsPickerCaption) {
                    extern s32 PortTbo_TextInfo(s32 id,s32 what);
                    FlashCxform color;
                    FlashQuad caption;
                    f32 scale=19.0f,inkWidth=PortTbo_TextInfo(id,7);
                    f32 inkX=scale*(PortTbo_TextInfo(id,5)+0.5f*inkWidth)/1000.0f;
                    f32 inkY=scale*(PortTbo_TextInfo(id,6)+0.5f*PortTbo_TextInfo(id,8))/1000.0f;
                    Flash_OptionsCaptionQuad(&caption,rec.x0,rec.y0,rec.x1,rec.y1,inkX,inkY);
                    caption.pt[1][0]=caption.pt[3][0]=caption.pt[0][0]+512*scale/19.0f;
                    caption.pt[2][1]=caption.pt[3][1]=caption.pt[0][1]+32*scale/19.0f;
                    Flash_CxformIdentity(&color);
                    /* The original shape supplies the local centre/pivot. Apply
                       its full live matrix and caption clip, just like native text. */
                    FlashDraw_TransformQuad(&caption,&trainingMatrix,&color,prop,flash->ofs);
                    if(sOptionsPickerBoundsValid) {
                        f32 width=caption.pt[3][0]-caption.pt[0][0],height=caption.pt[3][1]-caption.pt[0][1];
                        f32 textureW=PortTbo_TextInfo(id,1),textureH=PortTbo_TextInfo(id,2);
                        if(textureW>0 && textureH>0) {
                            f32 inkW=width*PortTbo_TextInfo(id,7)/textureW;
                            f32 inkH=height*PortTbo_TextInfo(id,8)/textureH;
                            f32 roomW=(sOptionsPickerBounds[2]-sOptionsPickerBounds[0])*0.88f;
                            f32 roomH=(sOptionsPickerBounds[3]-sOptionsPickerBounds[1])*0.78f;
                            f32 fit=1,cx=(sOptionsPickerBounds[0]+sOptionsPickerBounds[2])*0.5f;
                            f32 cy=(sOptionsPickerBounds[1]+sOptionsPickerBounds[3])*0.5f;
                            if(inkW>roomW && roomW>0) fit=roomW/inkW;
                            if(inkH*fit>roomH && roomH>0) fit=roomH/inkH;
                            {
                                f32 animatedX=caption.pt[0][0]+width*(PortTbo_TextInfo(id,5)+0.5f*PortTbo_TextInfo(id,7))/textureW;
                                f32 animatedY=caption.pt[0][1]+height*(PortTbo_TextInfo(id,6)+0.5f*PortTbo_TextInfo(id,8))/textureH;
                                f32 slackX=(roomW-inkW*fit)*0.5f,slackY=(roomH-inkH*fit)*0.5f;
                                if(slackX<0) slackX=0;if(slackY<0) slackY=0;
                                cx=animatedX<cx-slackX ? cx-slackX:animatedX>cx+slackX ? cx+slackX:animatedX;
                                cy=animatedY<cy-slackY ? cy-slackY:animatedY>cy+slackY ? cy+slackY:animatedY;
                            }
                            width*=fit;height*=fit;
                            caption.pt[0][0]=cx-width*(PortTbo_TextInfo(id,5)+0.5f*PortTbo_TextInfo(id,7))/textureW;
                            caption.pt[0][1]=cy-height*(PortTbo_TextInfo(id,6)+0.5f*PortTbo_TextInfo(id,8))/textureH;
                            caption.pt[3][0]=caption.pt[0][0]+width;
                            caption.pt[3][1]=caption.pt[0][1]+height;
                        }
                    }
                    Ps_SpriteScreen(id,caption.pt[0][0],caption.pt[0][1],caption.pt[3][0],caption.pt[3][1],0,0,1,1,tint,pq->rgba[3]);
                    continue;
                }
                f32 span=(f32)(pq->uv.u1-pq->uv.u0);
                f32 right=pq->pt[0][0]+(pq->pt[3][0]-pq->pt[0][0])*(span>0 ? 512.0f/span:1.0f);
                Ps_SpriteScreen(id,pq->pt[0][0],pq->pt[0][1],right,pq->pt[3][1],0,0,1,1,tint,pq->rgba[3]);
            }
            continue;
        }
#endif
#ifdef PORT
        if (sMenuArtScreen == 36 && (rec.image == 9 || rec.image == 36 || rec.image == 37))
            continue; /* Top tournament/round strip belongs to the post-intro page. */
        markImage = !sMenuArtDrawDepth ? FlashShape_MenuArtMarker(rec.image) : 0;
        if (sMenuArtScreen == 68 && rec.image == 34)
            markImage = 0x19; /* shared arrow atlas: preserve native 4:3 size and spacing */
        if (sMenuArtScreen == 68 && rec.image == 35)
            markImage = 0x13; /* dimmer can replace a right-anchored menu clip */
        if (sMenuArtScreen == 67 && !sMenuArtDrawDepth && rec.image == 25 &&
            Port_PageAspectMilli() > 1333) {
            s32 corner;
            /* The native dimmer starts at x=256 and covers only the right
             * half. A wide marker alone retains that midpoint: extend the
             * geometry to cover the whole page before the wide transform. */
            for (corner = 0; corner < 4; corner++)
                pq->pt[corner][0] = (pq->pt[corner][0] - 256.0f) * 2.0f;
        }
        if (sMenuArtScreen == 63 && !sMenuArtDrawDepth && (rec.image == 25 || rec.image == 26))
            markImage = Port_DataCenterNamePlateMarker(shape->mtx.m[5]);
        if (sMenuArtScreen == 65 && markImage == 0x19 &&
            flash->root->depths >= 3 && flash->root->slots[2].shape != NULL) {
            f32 shift = Port_DataCenterItemTravel(
                flash->root->slots[2].shape->mtx.m[2], Port_PageAspectMilli());
            s32 corner;
            for (corner = 0; corner < 4; corner++) pq->pt[corner][0] += shift;
        }
        if (sMenuArtScreen == 60) {
            f32 shift = 0.0f;
            s32 corner;
            if (sTrainingGotenTree)
                shift = Port_TrainingFlightShift(rec.image, trainingMatrix.m[2], Port_PageAspectMilli());
            /* Image 2 in the Goten tree is a tree/cliff scenery cutout, not a
             * character. It overlays the same tree baked into the background. */
            if (sTrainingGotenTree && rec.image == 2) markImage = 0x13;
            /* Through 16:9 retain the backing and connect the option composition.
             * Wider pages omit the backing and retain the right-side layout. */
            if (!sMenuArtDrawDepth && rec.image == 5) {
                if (Port_PageAspectMilli() > 1778) continue;
                if (shape->mtx.m[0] >= 0.0f) markImage = 0x1A;
                else shift = Port_TrainingRightBackingShift(shape->mtx.m[2], Port_PageAspectMilli());
            }
            if (!sMenuArtDrawDepth && rec.image == 6 && Port_PageAspectMilli() <= 1778)
                markImage = 0x1A;
            if (!sMenuArtDrawDepth && rec.image == 7) {
                markImage = 0x1B;
                shift = Port_TrainingPreviewFrameShift(shape->mtx.m[2], shape->mtx.m[0] < 0.0f,
                                                       Port_PageAspectMilli());
            }
            for (corner = 0; corner < 4; corner++) pq->pt[corner][0] += shift;
        }
        if (sMenuArtScreen == 55 && markImage == 0x19 && Port_PageAspectMilli() > 1333) {
            f32 shift = FlashShop_BuyTravel(flash);
            s32 corner;
            for (corner = 0; corner < 4; corner++) pq->pt[corner][0] += shift;
        }
        if (sMenuArtScreen == 59 && rec.image == 0 && Port_PageAspectMilli() > 1333) {
            f32 shift = (shape->mtx.m[2] + 1.0f) *
                ((f32)Port_PageAspectMilli() / 1333.333f - 1.0f);
            s32 corner;
            for (corner = 0; corner < 4; corner++) pq->pt[corner][0] += shift;
        }
        if (sMenuArtScreen == 51 && rec.image == 40 && markImage == 0x19 &&
            Port_PageAspectMilli() > 1333) {
            FlashClip *limit = FlashClipList_FindByName(flash->clips, "mc_ability_limit_up");
            if (limit != NULL && limit->tl.depths >= 12) {
                static const f32 settled[3] = {4.4f, 17.2f, -21.2f};
                s32 layer;
                for (layer = 0; layer < 3; layer++) {
                    if (limit->tl.slots[9 + layer].shape == shape) {
                        f32 travel = (shape->mtx.m[2] - settled[layer]) / (550.0f - settled[layer]);
                        f32 shift;
                        s32 corner;
                        if (travel < 0.0f) travel = 0.0f;
                        if (travel > 1.0f) travel = 1.0f;
                        /* All three lettering layers park at x=550. Extend
                           their native travel, preserving each settled pose,
                           so centring cannot expose the waiting artwork. */
                        shift = 256.0f * ((f32)Port_PageAspectMilli() / 1333.333f - 1.0f) * travel;
                        for (corner = 0; corner < 4; corner++) pq->pt[corner][0] += shift;
                        break;
                    }
                }
            }
        }
        if (sMenuArtScreen == 39 && !sMenuArtDrawDepth) {
            /* The tournament movie has different image IDs from Versus.
               Keep its unparented name/form backings with their page side. */
            if (rec.image >= 7 && rec.image <= 10)
                markImage = (pq->pt[0][0] + pq->pt[3][0]) * 0.5f < 256.0f ? 0x1A : 0x1B;
        }
        if (sMenuArtScreen == 10 && rec.image == 38 && markImage && Port_PageAspectMilli() > 1333) {
            FlashClip *name = FlashClipList_FindByName(flash->clips, "mc_map_name");
            if (name != NULL && name->prop.mtx.m[2] < 0.0f) {
                /* This backing is a root shape, outside the name clip's
                   tree. Follow the same exit correction as its text. */
                f32 progress = -name->prop.mtx.m[2] / 512.0f;
                f32 shift;
                s32 corner;
                if (progress > 1.0f) progress = 1.0f;
                shift = 256.0f * ((f32)Port_PageAspectMilli() / 1333.333f - 1.0f) * progress;
                for (corner = 0; corner < 4; corner++) pq->pt[corner][0] -= shift;
            }
        }
        if (Port_PageAspectMilli() > 1333 &&
            ((sMenuArtScreen == 22 && shape->id == 8 && rec.image == 8) ||
             (sMenuArtScreen == 35 && shape->id == 4 && rec.image == 4))) {
            /* Each one-player selector's divider follows the selected portrait,
               just like the player-one divider in the Versus selector. */
            FlashClip *portrait = FlashClipList_FindByName(flash->clips, "mc_single_chara_l");
            if (portrait != NULL && (portrait->flags & FLASH_CLIP_VISIBLE)) {
                f32 edge = portrait->prop.mtx.m[2] + 256.0f + (f32)flash->ofs[0];
                f32 centre = (pq->pt[0][0] + pq->pt[3][0]) * 0.5f;
                s32 corner;
                for (corner = 0; corner < 4; corner++) pq->pt[corner][0] += edge - centre;
                markImage = 0x1A;
            } else {
                continue;
            }
        }
        if (((sMenuArtScreen == 44 && shape->id == 13 && rec.image == 15) ||
             (sMenuArtScreen == 49 && shape->id == 10 && rec.image == 12)) &&
            flash->root != NULL && flash->root->depths > 14) {
            /* The two animated dividers follow their portrait's inner edge,
               as in Versus Duel. Both instances share one shape record. */
            s32 left = flash->root->slots[13].shape == shape;
            FlashClip *portrait = FlashClipList_FindByName(flash->clips,
                left ? "mc_single_chara_l" : "mc_single_chara_r");
            if (portrait != NULL && (portrait->flags & FLASH_CLIP_VISIBLE)) {
                f32 edge = portrait->prop.mtx.m[2] + (left ? 256.0f : 0.0f) + (f32)flash->ofs[0];
                /* Image 15 is 32 texels wide but only texels 0..7 are
                   opaque. Track that stripe's center, including mirroring. */
                f32 stripe = (pq->flags & FLASH_QUAD_FLIPX) ? 0.875f : 0.125f;
                f32 centre = pq->pt[0][0] + (pq->pt[1][0] - pq->pt[0][0]) * stripe;
                s32 corner;
                for (corner = 0; corner < 4; corner++) pq->pt[corner][0] += edge - centre;
                markImage = left ? 0x1A : 0x1B;
            } else continue;
        }
        if (sMenuArtScreen == 39 && Port_PageAspectMilli() > 1333 &&
            shape->id == 3 && rec.image == 3) {
            FlashShape *leftDivider = NULL;
            FlashClip *portrait;
            s32 depth, left, corner;
            /* Like Versus, two native copies close the inner portrait edges.
               The later root copy belongs to the left player's picture. */
            for (depth = 0; depth < flash->root->depths; depth++) {
                FlashShape *candidate = flash->root->slots[depth].shape;
                if (candidate != NULL && candidate->id == 3) leftDivider = candidate;
            }
            left = shape == leftDivider;
            portrait = FlashClipList_FindByName(flash->clips,
                left ? "mc_single_chara_l" : "mc_single_chara_r");
            if (portrait != NULL && (portrait->flags & FLASH_CLIP_VISIBLE)) {
                f32 edge = portrait->prop.mtx.m[2] + (left ? 256.0f : 0.0f) + (f32)flash->ofs[0];
                f32 centre = (pq->pt[0][0] + pq->pt[3][0]) * 0.5f;
                for (corner = 0; corner < 4; corner++) pq->pt[corner][0] += edge - centre;
                markImage = left ? 0x1A : 0x1B;
            } else continue;
        }
        if (sMenuArtScreen == 51 && Port_PageAspectMilli() > 1333 &&
            shape->id == 41 && rec.image == 41 && flash->root->depths >= 3 &&
            flash->root->slots[2].shape == shape) {
            /* Resource 13's root depth 3 is the centre divider. Close the
               left portrait's inner edge, following its entry/exit position. */
            FlashClip *portrait = FlashClipList_FindByName(flash->clips, "mc_single_chara_l");
            if (portrait != NULL && (portrait->flags & FLASH_CLIP_VISIBLE)) {
                f32 edge = portrait->prop.mtx.m[2] + 256.0f + (f32)flash->ofs[0];
                f32 centre = (pq->pt[0][0] + pq->pt[3][0]) * 0.5f;
                s32 corner;
                for (corner = 0; corner < 4; corner++) pq->pt[corner][0] += edge - centre;
                markImage = 0x1A;
            } else continue;
        }
        if (sMenuArtScreen == 10 && Port_PageAspectMilli() > 1333 &&
            shape->id == 21 && rec.image == 21 && flash->root->depths >= 14 &&
            (flash->root->slots[12].shape == shape || flash->root->slots[13].shape == shape)) {
            /* The original centre divider already has two copies. Attach
               one to each portrait's inner edge instead of overlapping them. */
            s32 left = flash->root->slots[13].shape == shape;
            FlashClip *portrait = FlashClipList_FindByName(flash->clips,
                left ? "mc_single_chara_l" : "mc_single_chara_r");
            if (portrait != NULL && (portrait->flags & FLASH_CLIP_VISIBLE)) {
                f32 edge = portrait->prop.mtx.m[2] + (left ? 256.0f : 0.0f) + (f32)flash->ofs[0];
                f32 centre = (pq->pt[0][0] + pq->pt[3][0]) * 0.5f;
                s32 corner;
                for (corner = 0; corner < 4; corner++) pq->pt[corner][0] += edge - centre;
                markImage = left ? 0x1A : 0x1B;
            } else {
                continue;
            }
        }

        if ((sMenuArtScreen == 25 ||
             ((sMenuArtScreen == 5 || sMenuArtScreen == 20) &&
              !sHistoryGuestDrawDepth && (sMenuArtDrawDepth || markImage))) &&
            Flash_MenuQuadOffPage(pq)) {
            /* Card entrance/exit timelines park their art outside the native page.
               The entire card movie is centred, including unmarked child shapes. */
            continue;
        }
        /* Dialogs/item details use image 0 as their screen dimmer. Sim Dragon
           minigames share dimmer 0 but have movie-specific playfield images. Stretch those
           backgrounds while foreground elements retain centred proportions. */
        fullWidthDialogDim = (sMenuArtScreen == 51 && rec.image == 0) ||
            (sMenuArtScreen == 47 && rec.image == 24) ||
            (sMenuArtScreen == 43 && rec.image == 0) ||
            ((sMenuArtScreen == 36 || sMenuArtScreen == 40) &&
             ((rec.image >= 19 && rec.image <= 22) || (rec.image >= 27 && rec.image <= 30))) ||
            (sMenuArtScreen == 34 &&
            (rec.image == 0 || rec.image == 1 || rec.image == 2 || rec.image == 5)) ||
            (sMenuArtScreen == 30 && rec.image == 3) ||
            ((sMenuArtScreen == 4 || sMenuArtScreen == 14 || sMenuArtScreen == 36 || sMenuArtScreen == 40 || sMenuArtScreen == 52) && rec.image == 0) ||
            ((sMenuArtScreen >= 25 && sMenuArtScreen <= 27) &&
             (rec.image == 0 || rec.image == 1 || rec.image == 3)) ||
            (sMenuArtScreen == 25 && (rec.image == 13 || rec.image == 15)) ||
            (sMenuArtScreen == 26 && (rec.image == 6 || rec.image == 13)) ||
            (sMenuArtScreen == 27 && (rec.image == 6 || rec.image == 14));
        if (fullWidthDialogDim) {
            Flash_MenuMarker(0x13);
        }
        if (markImage) {
            Flash_MenuMarker(markImage);
        }
#endif
#ifdef PORT
        /* Use the actual visible atlas quad, including shape offsets. Arrows
           are centered and proportional; the cyan thumb uses full-page scale. */
        if(sOptionMouseDrawClip>=0 && pq->rgba[3] &&
           (Flash_CharRefMouseControl(sSelectionMouseScope,sSelectionMouseClips[sOptionMouseDrawClip].name) ||
            Flash_ItemMouseThumb(sSelectionMouseScope,sSelectionMouseClips[sOptionMouseDrawClip].name) ||
            (sItemMousePlateBody && Flash_ItemMouseRow(sSelectionMouseScope,sSelectionMouseClips[sOptionMouseDrawClip].name)) ||
            Flash_EvoMouseArrow(sSelectionMouseScope,sSelectionMouseClips[sOptionMouseDrawClip].name) ||
            (sSelectionMouseScope==68 && (rec.image==33 || rec.image==34 ||
             Port_OptionsMouseSelector(sSelectionMouseClips[sOptionMouseDrawClip].name))) || sSelectionMouseScope==4 ||
            (sSelectionMouseScope==31 && strncmp(sSelectionMouseClips[sOptionMouseDrawClip].name,"mc_dragonball_off_",18)==0) ||
            (sSelectionMouseScope==62 && strncmp(sSelectionMouseClips[sOptionMouseDrawClip].name,"mc_input_code_",14)==0) ||
            (sSelectionMouseScope==33 && (strcmp(sSelectionMouseClips[sOptionMouseDrawClip].name,"mc_yajirusi_l")==0 ||
             strcmp(sSelectionMouseClips[sOptionMouseDrawClip].name,"mc_yajirusi_r")==0)))) {
            PortSelectionMouseClip *capture=&sSelectionMouseClips[sOptionMouseDrawClip];
            PortMousePlate piece;
            if(sSelectionMouseScope==68 && Port_OptionsMouseSelector(capture->name))
                Flash_MouseOptionsSelectorPiece(&piece,pq);
            else Flash_MouseDrawnPiece(&piece,pq);
            Flash_MouseUnion(&capture->drawn,&piece);
        }
#endif
        FlashDraw_Quad(tex, pq);
#ifdef PORT
        if (markImage) {
            /* The scenery cutout temporarily leaves Goten's right-anchored
             * group. Restore that group before drawing another flying copy. */
            Flash_MenuMarker(sMenuArtScreen == 68 && rec.image == 34 ? 0x19 :
                          sTrainingGotenTree || (sMenuArtScreen == 68 && sMenuArtDrawDepth) ? 0x1B :
                          sMenuArtScreen == 40 ? 0x19 : 0x13);
        }
        if (fullWidthDialogDim) {
            Flash_MenuMarker(sMenuArtScreen == 47 || sMenuArtScreen == 51 ? 0x13 : 0x19); /* restore this movie's base layout */
        }
#endif
    }
}

/* Makes one sprite instance per record of the sprite list (tag 2), each on its own frame record of tag 6. */
void FlashClipList_Init(FlashClipList *list, Flash *flash) {
    FlashTag hdr;
    u8 *tag;
    u8 *rec;
    FlashClip *clip;
    s32 i;

    memset(&hdr, 0, sizeof(hdr));
    tag = Flash_FindTag(flash->data, 2);
    if (tag == NULL) {
        return;
    }
    memcpy(&hdr, tag, sizeof(hdr));
    if (list->items == NULL) {
        list->items = Heap_Alloc(hdr.count * sizeof(FlashClip), 0x20, 0, 2);
        memset(list->items, 0, hdr.count * sizeof(FlashClip));
    }
    list->count = hdr.count;
    for (i = 0; i < hdr.count; i++) {
        clip = &list->items[i];
        clip->index = i;
        rec = Flash_SkipRecords(tag, clip->index);
        memcpy(&clip->id, rec + 8, 2);
        clip->flags = 0;
        memset(&clip->prop, 0, sizeof(FlashProp));
        Flash_MtxIdentity(&clip->prop.mtx);
        Flash_CxformIdentity(&clip->prop.cx);
        clip->prop.alpha = 1.0f;
        FlashTl_Init(&clip->tl, Flash_GetRecord(flash->data, 6, clip->id), flash);
        list->depths += clip->tl.depths;
    }
}

/* Frees every instance's depth slots and the list. */
void FlashClipList_Term(FlashClipList *list) {
    u32 i;

    for (i = 0; i < list->count; i++) {
        FlashTl_Term(&list->items[i].tl);
    }
    if (list->items != NULL) {
        Heap_Free(list->items);
        list->items = NULL;
    }
}

/* Ticks every instance's timeline `count` times. Not called by anything in this file. */
void FlashClipList_Advance(FlashClipList *list, u32 count) {
    u32 i;

    for (i = 0; i < list->count; i++) {
        if (list->items[i].flags & FLASH_CLIP_FREE_RUN) {
            FlashTl_Advance(&list->items[i].tl, count, 1, 1);
        } else {
            FlashTl_Advance(&list->items[i].tl, count, 1, 0);
        }
    }
}

/* Resets every instance. */
void FlashClipList_Reset(FlashClipList *list) {
    u32 i;

    for (i = 0; i < list->count; i++) {
        FlashClip_Reset(&list->items[i], 0);
    }
}

/* Returns the first unused instance of sprite `id`, or NULL. */
FlashClip *FlashClipList_FindFree(FlashClipList *list, u16 id) {
    u32 i;
    FlashClip *clip = list->items;

    for (i = 0; i < list->count; i++, clip++) {
        if (!(clip->flags & FLASH_CLIP_USED) && clip->id == id) {
            return clip;
        }
    }
    return NULL;
}

/* Places sprite `id` under an instance name: takes a free instance, makes it used and visible, resets its
   overrides and callbacks and lets its timeline play (from the frame a jump left pending, if any). */
FlashClip *FlashClip_Start(FlashClipList *list, u16 id, char *name, FlashMtx *mtx, FlashCxform *cx) {
    FlashClip *clip = FlashClipList_FindFree(list, id);

    if (clip == NULL) {
        return NULL;
    }
    memset(clip->name, 0, sizeof(clip->name));
    memcpy(clip->name, name, strlen(name));
    clip->flags |= FLASH_CLIP_USED | FLASH_CLIP_VISIBLE;
    memset(&clip->prop, 0, sizeof(FlashProp));
    clip->prop.alpha = 1.0f;
    if (mtx != NULL) {
        clip->prop.mtx = *mtx;
    } else {
        Flash_MtxIdentity(&clip->prop.mtx);
    }
    if (cx != NULL) {
        clip->prop.cx = *cx;
    } else {
        Flash_CxformIdentity(&clip->prop.cx);
    }
    clip->preDraw = NULL;
    clip->postDraw = NULL;
    clip->preArg = NULL;
    clip->postArg = NULL;
    clip->tl.state |= FLASH_TL_PLAY;
    if (clip->tl.prev != 0) {
        clip->tl.frame = clip->tl.prev;
        clip->tl.state |= FLASH_TL_GOTO;
        clip->tl.prev = 0;
    }
    return clip;
}

/* Replaces an instance's matrix and / or colour transform (NULL: keep). */
void FlashClip_Set(FlashClip *clip, FlashMtx *mtx, FlashCxform *cx) {
    if (mtx != NULL) {
        clip->prop.mtx = *mtx;
    }
    if (cx != NULL) {
        clip->prop.cx = *cx;
    }
}

/* Clears an instance: name, flags, overrides, callbacks, and its timeline back to frame 0. */
void FlashClip_Reset(FlashClip *clip, s32 arg) {
    memset(clip->name, 0, sizeof(clip->name));
    clip->flags = 0;
    memset(&clip->prop, 0, sizeof(FlashProp));
    clip->prop.alpha = 1.0f;
    Flash_MtxIdentity(&clip->prop.mtx);
    Flash_CxformIdentity(&clip->prop.cx);
    clip->preDraw = NULL;
    clip->postDraw = NULL;
    clip->preArg = NULL;
    clip->postArg = NULL;
    FlashTl_Rewind(&clip->tl, arg);
}

/* Returns the first instance with the given name, or NULL. */
FlashClip *FlashClipList_FindByName(FlashClipList *list, char *name) {
    u32 i;
    FlashClip *clip = list->items;

    for (i = 0; i < list->count; i++, clip++) {
        if (strcmp(clip->name, name) == 0) {
            return clip;
        }
    }
    return NULL;
}

/* Fills a reference to the instances called `name`. With `parent`: the one placed on a depth of the first
   instance called `parent`. Without: the first in the list, counting the further ones in out->more. */
void FlashClipList_FindRef(FlashClipList *list, char *parent, char *name, FlashRef *out) {
    s32 found = 0;
    u32 i;
    FlashClip *clip;

    memset(out, 0, sizeof(FlashRef));
    if (parent != NULL) {
        clip = list->items;
        for (i = 0; i < list->count; i++, clip++) {
            if (strcmp(clip->name, parent) == 0) {
                FlashSlot *slot = clip->tl.slots;
                s32 j;

                for (j = 0; j < clip->tl.depths; j++) {
                    FlashClip *child = slot->clip;

                    slot++;
                    if (child != NULL && strcmp(child->name, name) == 0) {
                        found = 1;
                        out->index = child->index;
                        break;
                    }
                }
                break;
            }
        }
    } else {
        clip = list->items;
        for (i = 0; i < list->count; i++, clip++) {
            if (strcmp(clip->name, name) == 0) {
                if (found) {
                    out->more++;
                } else {
                    out->index = i;
                    found = 1;
                }
            }
        }
    }
    if (!found) {
        out->more = 0;
        out->index = -1;
    }
}

/* Lets the referenced instance and its namesakes play. */
void FlashClipList_Play(FlashClipList *list, FlashRef *ref) {
    FlashClip *clip = &list->items[ref->index];
    s32 n;
    u32 i;

    clip->tl.state |= FLASH_TL_PLAY;
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            if (strcmp(clip[i].name, clip->name) == 0) {
                clip[i].tl.state |= FLASH_TL_PLAY;
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

/* Stops the referenced instance and its namesakes. */
void FlashClipList_Stop(FlashClipList *list, FlashRef *ref) {
    FlashClip *clip = &list->items[ref->index];
    s32 n;
    u32 i;

    clip->tl.state &= ~FLASH_TL_PLAY;
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            if (strcmp(clip[i].name, clip->name) == 0) {
                clip[i].tl.state &= ~FLASH_TL_PLAY;
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

#ifdef PORT
#include "menu/port_screen.h"
#include "menu/tbo_art.h"
extern s32 gPortNetStagePicker;
static void Flash_PortDrawStageBadge(void *arg, FlashProp *prop) {
    FlashClip *clip = arg;
    Flash *flash = clip->tl.owner;
    s32 depth;
    for (depth = 0; depth < clip->tl.depths; depth++) {
        FlashShape *shape = clip->tl.slots[depth].shape;
        if (shape != NULL) {
            FlashShapeQuad rec;
            FlashQuad q;
            u8 *data = Flash_GetRecord(flash->data, 4, shape->id);
            memset(&q, 0, sizeof(q));
            memcpy(&rec, data + 10, sizeof(rec));
            q.pt[0][0] = rec.x0; q.pt[0][1] = rec.y0;
            q.pt[1][0] = rec.x1; q.pt[1][1] = rec.y0;
            q.pt[2][0] = rec.x0; q.pt[2][1] = rec.y1;
            q.pt[3][0] = rec.x1; q.pt[3][1] = rec.y1;
            q.rgba[0] = q.rgba[1] = q.rgba[2] = 255; q.rgba[3] = 128;
            FlashDraw_TransformQuad(&q, &shape->mtx, &shape->cx, prop, flash->ofs);
            {
                f32 cx = (q.pt[0][0] + q.pt[3][0]) * 0.5f;
                f32 cy = (q.pt[0][1] + q.pt[3][1]) * 0.5f;
                q.pt[0][0] = cx + (q.pt[0][0] - cx) * 0.75f;
                q.pt[0][1] = cy + (q.pt[0][1] - cy) * 0.75f;
                q.pt[3][0] = cx + (q.pt[3][0] - cx) * 0.75f;
                q.pt[3][1] = cy + (q.pt[3][1] - cy) * 0.75f;
            }
            Ps_SpriteScreen(TBO_WII_VS_PLAYERS, q.pt[0][0], q.pt[0][1], q.pt[3][0], q.pt[3][1],
                            0, gPortNetStagePicker * 0.5f, 1, (gPortNetStagePicker + 1) * 0.5f,
                            PS_PLAIN, q.rgba[3]);
            return;
        }
    }
}
void Flash_PortStageBadge(Flash *flash, FlashRef *ref) {
    if (ref->index >= 0) {
        FlashClip *clip = &flash->clips->items[ref->index];
        clip->flags |= 0x40000000; /* draw only the replacement, retaining the native timeline */
        clip->drawOver = Flash_PortDrawStageBadge;
        clip->overArg = clip;
    }
}
#endif
