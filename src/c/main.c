/**
 * Pixel Camp — Camping watchface for Pebble Round 2
 * Target: gabbro (260x260, round, 64-color)
 *
 * Mountain lake scene with tent, campfire, pine trees.
 * Sun/moon tracking, weather, stars at night, fire animation.
 */

#include <pebble.h>
#include <stdlib.h>

// ============================================================================
// CONSTANTS
// ============================================================================
#define ANIM_INTERVAL  80
#define ANIM_DURATION  5000

// Layout — percentages of screen height/width, computed at draw time
// Use pct_h(pct) and pct_w(pct) macros with current bounds
#define BODY_RADIUS  10

// Persist keys
#define P_SUNRISE_H  0
#define P_SUNRISE_M  1
#define P_SUNSET_H   2
#define P_SUNSET_M   3
#define P_TEMP       4
#define P_WX         5
#define P_TEMP_HI    6
#define P_TEMP_LO    7
#define P_VALID      8
#define P_SHOW_SUN   9
#define P_SHOW_HILO  10
#define P_DEV_MODE   11

// Weather
#define WX_CLEAR   0
#define WX_CLOUDY  1
#define WX_OVERCAST 2
#define WX_FOG     3
#define WX_RAIN    4
#define WX_STORM   5
#define WX_SNOW    6
#define WX_WIND    7

#define NUM_PRESETS 6

// ============================================================================
// COLORS
// ============================================================================
#ifdef PBL_COLOR
  #define C_SKY       GColorPictonBlue
  #define C_SKY_NIGHT GColorOxfordBlue
  #define C_SUN       GColorYellow
  #define C_GLOW      GColorRajah
  #define C_MOON      GColorPastelYellow
  #define C_MOON_DK   GColorOxfordBlue
  #define C_MTN_FAR   GColorFromHEX(0x445577)
  #define C_MTN_MID   GColorFromHEX(0x556688)
  #define C_MTN_NEAR  GColorFromHEX(0x667799)
  #define C_SNOW      GColorWhite
  #define C_LAKE      GColorCobaltBlue
  #define C_LAKE_LT   GColorVividCerulean
  #define C_GROUND    GColorArmyGreen
  #define C_GROUND_DK GColorFromHEX(0x335522)
  #define C_DIRT      GColorFromHEX(0x665533)
  #define C_TENT      GColorFromHEX(0xCC4400)
  #define C_TENT_DK   GColorFromHEX(0x993300)
  #define C_TREE      GColorDarkGreen
  #define C_TREE_LT   GColorFromHEX(0x228833)
  #define C_TRUNK     GColorFromHEX(0x664422)
  #define C_LOG       GColorFromHEX(0x553311)
  #define C_TEXT      GColorWhite
  #define C_SHAD      GColorBlack
  #define C_INFO      GColorCeleste
#else
  #define C_SKY       GColorWhite
  #define C_SKY_NIGHT GColorBlack
  #define C_SUN       GColorWhite
  #define C_GLOW      GColorWhite
  #define C_MOON      GColorWhite
  #define C_MOON_DK   GColorBlack
  #define C_MTN_FAR   GColorDarkGray
  #define C_MTN_MID   GColorDarkGray
  #define C_MTN_NEAR  GColorLightGray
  #define C_SNOW      GColorWhite
  #define C_LAKE      GColorBlack
  #define C_LAKE_LT   GColorDarkGray
  #define C_GROUND    GColorLightGray
  #define C_GROUND_DK GColorDarkGray
  #define C_DIRT      GColorDarkGray
  #define C_TENT      GColorLightGray
  #define C_TENT_DK   GColorDarkGray
  #define C_TREE      GColorBlack
  #define C_TREE_LT   GColorDarkGray
  #define C_TRUNK     GColorBlack
  #define C_LOG       GColorBlack
  #define C_TEXT      GColorBlack
  #define C_SHAD      GColorWhite
  #define C_INFO      GColorBlack
#endif

// ============================================================================
// DATA
// ============================================================================
typedef struct {
  int sr_h,sr_m, ss_h,ss_m;
  int temp,wx, hi,lo;
  int pk_wx[3], pk_t[3], pk_h[3];  // 3 peek slots: wx code, temp, hour
  char town[24];
  bool valid;
} Data;

// ============================================================================
// GLOBALS
// ============================================================================
static Window *s_win;
static Layer *s_canvas;
static AppTimer *s_timer;
static bool s_anim=false;
static int s_anim_ms=0;

static int s_bat=100;
static bool s_bt=true;
static Data s_d={.sr_h=6,.sr_m=0,.ss_h=20,.ss_m=0,.temp=55,.wx=WX_CLEAR,
  .hi=65,.lo=40,.town="Locust Lake, PA",.valid=false};

static char s_tbuf[8],s_dbuf[16],s_sr[8],s_ss[8],s_tmp[8];
static int s_hr=12, s_mn=0;
static bool s_dev=false;
static bool s_show_sun=true, s_show_hilo=true;
static int s_fire_frame=0;  // Animation frame counter
static int s_peek=-1;       // Peek state: -1=normal, 0/1/2=peek slot
static AppTimer *s_peek_timer=NULL;

// Layout values (set once in win_load based on screen size)
static int L_W, L_H;        // Screen width/height
static int L_MTN1_X, L_MTN1_Y, L_MTN2_X, L_MTN2_Y, L_MTN3_X, L_MTN3_Y;
static int L_LAKE_TOP, L_LAKE_BOT, L_GROUND;
static int L_ARC_TOP, L_ARC_BOT;

static void init_layout(int w, int h) {
  L_W=w; L_H=h;
  // Mountains pushed well below weather/time area
  L_MTN1_X=w*27/100;  L_MTN1_Y=h*38/100;
  L_MTN2_X=w*62/100;  L_MTN2_Y=h*34/100;
  L_MTN3_X=w*81/100;  L_MTN3_Y=h*40/100;
  L_LAKE_TOP=h*62/100;
  L_LAKE_BOT=h*72/100;
  L_GROUND=h*72/100;
  L_ARC_TOP=h*10/100;
  L_ARC_BOT=h*50/100;
}

// DEV presets
static int s_pre=-1;
static const char *s_pnames[]={"Dawn","Morn","Noon","Dusk","NightE","NightL"};
typedef struct {
  int h,m,srh,srm,ssh,ssm,tmp,wx,hi,lo;
} Pre;
static const Pre s_pres[NUM_PRESETS]={
  { 6, 0, 5,45,20,30, 42,WX_FOG,   58,38},
  { 9,30, 5,45,20,30, 55,WX_CLOUDY, 65,40},
  {12, 0, 5,45,20,30, 68,WX_CLEAR,  72,45},
  {20,15, 5,45,20,30, 52,WX_WIND,   65,40},
  {22, 0, 5,45,20,30, 45,WX_CLEAR,  58,38},
  { 2,30, 5,45,20,30, 38,WX_SNOW,   48,32},
};

static void apply_pre(int i) {
  if(i<0||i>=NUM_PRESETS) return;
  const Pre *p=&s_pres[i];
  s_hr=p->h; s_mn=p->m;
  s_d.sr_h=p->srh; s_d.sr_m=p->srm; s_d.ss_h=p->ssh; s_d.ss_m=p->ssm;
  s_d.temp=p->tmp; s_d.wx=p->wx; s_d.hi=p->hi; s_d.lo=p->lo;
  snprintf(s_d.town,sizeof(s_d.town),"Locust Lake, PA");
  s_d.valid=true;
  if(clock_is_24h_style()) snprintf(s_tbuf,sizeof(s_tbuf),"%d:%02d",p->h,p->m);
  else { int h=p->h%12; if(!h)h=12; snprintf(s_tbuf,sizeof(s_tbuf),"%d:%02d",h,p->m); }
  snprintf(s_dbuf,sizeof(s_dbuf),"DEV:%s",s_pnames[i]);
}

// ============================================================================
// UTILITY
// ============================================================================
static int sun_prog(void) {
  int sr=s_d.sr_h*60+s_d.sr_m, ss=s_d.ss_h*60+s_d.ss_m, now=s_hr*60+s_mn;
  if(now<sr||now>ss) return -1;
  int len=ss-sr; return len>0?((now-sr)*100)/len:50;
}
static bool is_night(void) { return sun_prog()<0; }
static int twi_pct(void) {
  int p=sun_prog(); if(p<0) return 0;
  if(p<15) return (15-p)*100/15;
  if(p>85) return (p-85)*100/15;
  return 0;
}
static int night_prog(void) {
  int ss=s_d.ss_h*60+s_d.ss_m, sr=s_d.sr_h*60+s_d.sr_m, now=s_hr*60+s_mn;
  int nl,el;
  if(sr<ss){nl=(24*60-ss)+sr; el=(now>=ss)?now-ss:(24*60-ss)+now;}
  else{nl=sr-ss; el=now-ss;}
  return nl>0?(el*100)/nl:50;
}
static int moon_phase(void) {
  if(s_dev&&s_pre>=0){int ph[]={1,7,15,22,25,29}; return ph[s_pre];}
  time_t t=time(NULL); struct tm *tm=localtime(&t); if(!tm) return 15;
  int y=tm->tm_year+1900,m=tm->tm_mon+1,d=tm->tm_mday;
  long days=(y-2000)*365+(y-2000)/4-(y-2000)/100+(y-2000)/400;
  int md[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
  for(int i=1;i<m;i++) days+=md[i];
  if(m>2&&((y%4==0&&y%100!=0)||y%400==0)) days++;
  days+=d-6;
  int ph=(int)((days*100)%2953); if(ph<0) ph+=2953;
  return (ph*30)/2953;
}
static void arc_xy(GRect b,int prog,int *ox,int *oy) {
  int top=L_ARC_TOP, bot=L_ARC_BOT, ah=bot-top;
  *ox=b.size.w*10/100+(b.size.w*80/100*prog)/100;
  int c=prog-50; *oy=bot-(ah*(2500-c*c))/2500;
}
static int fmt_h(int h){if(clock_is_24h_style())return h; int r=h%12; return r?r:12;}

// ============================================================================
// DRAW: SKY
// ============================================================================
static void draw_sky(GContext *ctx, GRect b) {
  if(is_night()) {
    graphics_context_set_fill_color(ctx,C_SKY_NIGHT);
    graphics_fill_rect(ctx,GRect(0,0,b.size.w,L_LAKE_TOP),0,GCornerNone);
  } else {
    int twi=twi_pct();
    #ifdef PBL_COLOR
    if(twi>20) {
      GColor sc[]={GColorOxfordBlue,GColorImperialPurple,GColorPurple,
                   GColorMagenta,GColorSunsetOrange,GColorOrange,GColorRajah};
      int n=7;
      for(int y=0;y<L_LAKE_TOP;y+=2){
        int pos=(y*(n-1)*1000)/L_LAKE_TOP;
        int idx=pos/1000; int frac=pos%1000;
        if(idx>=n-1){idx=n-2;frac=999;}
        int thr=((y/2)%2==0)?550:450;
        GColor c=(frac<thr)?sc[idx]:sc[idx+1];
        graphics_context_set_fill_color(ctx,c);
        graphics_fill_rect(ctx,GRect(0,y,b.size.w,2),0,GCornerNone);
      }
    } else {
      graphics_context_set_fill_color(ctx,C_SKY);
      graphics_fill_rect(ctx,GRect(0,0,b.size.w,L_LAKE_TOP),0,GCornerNone);
    }
    #else
    graphics_context_set_fill_color(ctx,C_SKY);
    graphics_fill_rect(ctx,GRect(0,0,b.size.w,L_LAKE_TOP),0,GCornerNone);
    #endif
  }
}

// ============================================================================
// DRAW: STARS
// ============================================================================
static void draw_stars(GContext *ctx, GRect b) {
  if(!is_night()) return;
  graphics_context_set_fill_color(ctx,GColorWhite);
  // Star positions as % of screen
  int w=b.size.w, h=b.size.h;
  int st[][2]={{10,6},{21,3},{35,8},{50,5},{65,3},{81,7},{92,5},
               {15,14},{31,12},{58,10},{75,12},{87,11},{23,18},{44,15},
               {71,16},{13,21},{56,19},{85,17}};
  for(int i=0;i<18;i++){
    int sx=st[i][0]*w/100, sy=st[i][1]*h/100;
    if(sy<L_MTN2_Y-5)
      graphics_draw_pixel(ctx,GPoint(sx,sy));
  }
  // A few bright stars (2x2)
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx,GColorPastelYellow);
  #endif
  graphics_fill_rect(ctx,GRect(w*37/100,h*4/100,2,2),0,GCornerNone);
  graphics_fill_rect(ctx,GRect(w*69/100,h*2/100,2,2),0,GCornerNone);
  graphics_fill_rect(ctx,GRect(w*17/100,h*9/100,2,2),0,GCornerNone);
}

// ============================================================================
// DRAW: MOUNTAINS
// ============================================================================
static void draw_mountains(GContext *ctx, GRect b) {
  // Back mountain (largest, darkest)
  graphics_context_set_fill_color(ctx,C_MTN_FAR);
  for(int y=L_MTN2_Y;y<L_LAKE_TOP;y++){
    int spread=(y-L_MTN2_Y)*2;
    int lx=L_MTN2_X-spread, rx=L_MTN2_X+spread;
    if(lx<0) lx=0;
    if(rx>b.size.w) rx=b.size.w;
    graphics_fill_rect(ctx,GRect(lx,y,rx-lx,1),0,GCornerNone);
  }
  // Snow cap (small)
  graphics_context_set_fill_color(ctx,C_SNOW);
  int sc2=L_H*3/100;
  for(int y=L_MTN2_Y;y<L_MTN2_Y+sc2;y++){
    int spread=(y-L_MTN2_Y)*2;
    int lx=L_MTN2_X-spread+2, rx=L_MTN2_X+spread-2;
    if(lx<rx) graphics_fill_rect(ctx,GRect(lx,y,rx-lx,1),0,GCornerNone);
  }

  // Left mountain (medium)
  graphics_context_set_fill_color(ctx,C_MTN_MID);
  for(int y=L_MTN1_Y;y<L_LAKE_TOP;y++){
    int spread=(y-L_MTN1_Y)*18/10;
    int lx=L_MTN1_X-spread, rx=L_MTN1_X+spread;
    if(lx<0) lx=0;
    if(rx>b.size.w) rx=b.size.w;
    graphics_fill_rect(ctx,GRect(lx,y,rx-lx,1),0,GCornerNone);
  }
  graphics_context_set_fill_color(ctx,C_SNOW);
  int sc1=L_H*3/100;
  for(int y=L_MTN1_Y;y<L_MTN1_Y+sc1;y++){
    int spread=(y-L_MTN1_Y)*18/10;
    int lx=L_MTN1_X-spread+2, rx=L_MTN1_X+spread-2;
    if(lx<rx) graphics_fill_rect(ctx,GRect(lx,y,rx-lx,1),0,GCornerNone);
  }

  // Right foothill (smallest, lightest)
  graphics_context_set_fill_color(ctx,C_MTN_NEAR);
  for(int y=L_MTN3_Y;y<L_LAKE_TOP;y++){
    int spread=(y-L_MTN3_Y)*15/10;
    int lx=L_MTN3_X-spread, rx=L_MTN3_X+spread;
    if(lx<0) lx=0;
    if(rx>b.size.w) rx=b.size.w;
    graphics_fill_rect(ctx,GRect(lx,y,rx-lx,1),0,GCornerNone);
  }
}

// ============================================================================
// DRAW: SUN
// ============================================================================
static void draw_sun(GContext *ctx, GRect b) {
  int p=sun_prog(); if(p<0) return;
  int sx,sy; arc_xy(b,p,&sx,&sy);
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx,C_GLOW);
  graphics_fill_circle(ctx,GPoint(sx,sy),BODY_RADIUS+3);
  #endif
  graphics_context_set_fill_color(ctx,C_SUN);
  graphics_fill_circle(ctx,GPoint(sx,sy),BODY_RADIUS);
  #ifdef PBL_COLOR
  // Rays
  graphics_context_set_fill_color(ctx,C_GLOW);
  int r=BODY_RADIUS+5;
  graphics_fill_rect(ctx,GRect(sx-1,sy-r-3,3,3),0,GCornerNone);
  graphics_fill_rect(ctx,GRect(sx-1,sy+r,3,3),0,GCornerNone);
  graphics_fill_rect(ctx,GRect(sx-r-3,sy-1,3,3),0,GCornerNone);
  graphics_fill_rect(ctx,GRect(sx+r,sy-1,3,3),0,GCornerNone);
  #endif
}

// ============================================================================
// DRAW: MOON
// ============================================================================
static void draw_moon(GContext *ctx, GRect b) {
  if(!is_night()) return;
  int p=night_prog(); int mx,my; arc_xy(b,p,&mx,&my);
  int ph=moon_phase(), r=BODY_RADIUS;
  graphics_context_set_fill_color(ctx,C_MOON);
  graphics_fill_circle(ctx,GPoint(mx,my),r);
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx,C_MOON_DK);
  #else
  graphics_context_set_fill_color(ctx,C_SKY_NIGHT);
  #endif
  if(ph<15){int off=(15-ph)*r*2/15;
    if(ph<2) graphics_fill_circle(ctx,GPoint(mx,my),r-1);
    else graphics_fill_circle(ctx,GPoint(mx-off+r,my),r);
  } else {int off=(ph-15)*r*2/15;
    if(ph>27) graphics_fill_circle(ctx,GPoint(mx,my),r-1);
    else graphics_fill_circle(ctx,GPoint(mx+off-r,my),r);
  }
}

// ============================================================================
// DRAW: LAKE
// ============================================================================
static void draw_lake(GContext *ctx, GRect b) {
  #ifdef PBL_COLOR
  // Gradient lake
  int lh=L_LAKE_BOT-L_LAKE_TOP;
  for(int y=0;y<lh;y++){
    GColor c=(y<lh/2)?C_LAKE:C_LAKE_LT;
    graphics_context_set_fill_color(ctx,c);
    graphics_fill_rect(ctx,GRect(0,L_LAKE_TOP+y,b.size.w,1),0,GCornerNone);
  }
  // Subtle wave shimmer
  graphics_context_set_fill_color(ctx,GColorPictonBlue);
  int phase=s_fire_frame*50; // Reuse animation counter
  for(int x=10;x<b.size.w-10;x+=12){
    int wy=L_LAKE_TOP+8+(sin_lookup((phase+x*200)%TRIG_MAX_ANGLE)*2)/TRIG_MAX_RATIO;
    graphics_fill_rect(ctx,GRect(x,wy,6,1),0,GCornerNone);
  }
  // Shore reflection line
  graphics_context_set_fill_color(ctx,GColorTiffanyBlue);
  graphics_fill_rect(ctx,GRect(20,L_LAKE_BOT-3,b.size.w-40,1),0,GCornerNone);
  #else
  graphics_context_set_fill_color(ctx,C_LAKE);
  graphics_fill_rect(ctx,GRect(0,L_LAKE_TOP,b.size.w,L_LAKE_BOT-L_LAKE_TOP),0,GCornerNone);
  #endif
}

// ============================================================================
// DRAW: GROUND
// ============================================================================
static void draw_ground(GContext *ctx, GRect b) {
  // Main ground
  graphics_context_set_fill_color(ctx,C_GROUND);
  graphics_fill_rect(ctx,GRect(0,L_GROUND,b.size.w,b.size.h-L_GROUND),0,GCornerNone);

  #ifdef PBL_COLOR
  // Dirt path
  graphics_context_set_fill_color(ctx,C_DIRT);
  graphics_fill_rect(ctx,GRect(100,L_GROUND+15,60,4),0,GCornerNone);
  graphics_fill_rect(ctx,GRect(95,L_GROUND+19,50,3),0,GCornerNone);

  // Darker grass patches
  graphics_context_set_fill_color(ctx,C_GROUND_DK);
  int gp[][2]={{30,8},{70,14},{140,10},{200,12},{50,22},{170,20},{230,16}};
  for(int i=0;i<7;i++)
    graphics_fill_rect(ctx,GRect(gp[i][0],L_GROUND+gp[i][1],8,3),0,GCornerNone);

  // Small rocks
  graphics_context_set_fill_color(ctx,GColorLightGray);
  graphics_fill_rect(ctx,GRect(115,L_GROUND+10,4,3),0,GCornerNone);
  graphics_fill_rect(ctx,GRect(180,L_GROUND+18,3,2),0,GCornerNone);
  graphics_fill_rect(ctx,GRect(45,L_GROUND+25,5,3),0,GCornerNone);
  #endif
}

// ============================================================================
// DRAW: PINE TREES
// ============================================================================
static void draw_tree(GContext *ctx, int cx, int bot_y, int size) {
  // Trunk
  graphics_context_set_fill_color(ctx,C_TRUNK);
  graphics_fill_rect(ctx,GRect(cx-1,bot_y-size/3,3,size/3),0,GCornerNone);
  // Canopy (3 stacked triangles)
  int layers=3;
  for(int l=0;l<layers;l++){
    int ly=bot_y-size/3-l*(size/4);
    int lw=size-l*6;
    #ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx,(l==0)?C_TREE:C_TREE_LT);
    #else
    graphics_context_set_fill_color(ctx,C_TREE);
    #endif
    for(int y=0;y<size/4;y++){
      int w=lw-y*lw/(size/4);
      if(w>0) graphics_fill_rect(ctx,GRect(cx-w/2,ly-y,w,1),0,GCornerNone);
    }
  }
}
static void draw_trees(GContext *ctx, GRect b) {
  int w=b.size.w;
  draw_tree(ctx,w*12/100,L_GROUND+8,L_H*15/100);
  draw_tree(ctx,w*34/100,L_GROUND+4,L_H*12/100);
  draw_tree(ctx,w*81/100,L_GROUND+6,L_H*14/100);
  draw_tree(ctx,w*92/100,L_GROUND+10,L_H*11/100);
}

// ============================================================================
// DRAW: TENT
// ============================================================================
static void draw_tent(GContext *ctx, GRect b) {
  int cx=b.size.w*21/100, by=L_GROUND+L_H*8/100;
  int tw=L_W*14/100, th=L_H*9/100;

  // Tent body (triangle)
  graphics_context_set_fill_color(ctx,C_TENT);
  for(int y=0;y<th;y++){
    int w=tw*y/th;
    graphics_fill_rect(ctx,GRect(cx-w/2,by-th+y,w,1),0,GCornerNone);
  }
  // Dark side
  graphics_context_set_fill_color(ctx,C_TENT_DK);
  for(int y=0;y<th;y++){
    int w=tw*y/th;
    graphics_fill_rect(ctx,GRect(cx,by-th+y,w/2,1),0,GCornerNone);
  }
  // Entrance
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx,GColorFromHEX(0x331100));
  #else
  graphics_context_set_fill_color(ctx,GColorBlack);
  #endif
  for(int y=0;y<th/2;y++){
    int w=6*y/(th/2);
    graphics_fill_rect(ctx,GRect(cx-w/2,by-th/2+y,w,1),0,GCornerNone);
  }
}

// ============================================================================
// DRAW: CAMPFIRE (animated at night, logs during day)
// ============================================================================
static void draw_campfire(GContext *ctx, GRect b) {
  int cx=b.size.w*46/100, by=L_GROUND+L_H*8/100;

  // Log base (always)
  graphics_context_set_fill_color(ctx,C_LOG);
  graphics_fill_rect(ctx,GRect(cx-10,by-2,20,4),0,GCornerNone);
  graphics_fill_rect(ctx,GRect(cx-8,by-4,6,3),0,GCornerNone);
  graphics_fill_rect(ctx,GRect(cx+3,by-4,6,3),0,GCornerNone);

  // Fire ring stones
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx,GColorLightGray);
  int stones[][2]={{-12,-1},{-11,2},{-8,4},{-3,5},{3,5},{8,4},{11,2},{12,-1}};
  for(int i=0;i<8;i++)
    graphics_fill_rect(ctx,GRect(cx+stones[i][0],by+stones[i][1],3,3),0,GCornerNone);
  #endif

  if(!is_night()) {
    // Daytime: thin smoke wisps
    #ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx,GColorLightGray);
    int smokey=by-8-(s_fire_frame%8);
    graphics_fill_rect(ctx,GRect(cx-1,smokey,2,3),0,GCornerNone);
    graphics_fill_rect(ctx,GRect(cx+1,smokey-5,2,2),0,GCornerNone);
    #endif
    return;
  }

  // Night: animated fire!
  #ifdef PBL_COLOR
  // Flame tongues (randomized per frame)
  int seed=s_fire_frame*7+13;
  for(int i=0;i<5;i++){
    int fx=cx-6+i*3;
    int fh=8+(seed+i*37)%10;  // Random height 8-17px
    seed=(seed*13+7)%97;
    // Flame gradient: red base, orange mid, yellow tip
    graphics_context_set_fill_color(ctx,GColorRed);
    graphics_fill_rect(ctx,GRect(fx,by-fh,3,fh/3),0,GCornerNone);
    graphics_context_set_fill_color(ctx,GColorOrange);
    graphics_fill_rect(ctx,GRect(fx,by-fh+fh/3,3,fh/3),0,GCornerNone);
    graphics_context_set_fill_color(ctx,GColorYellow);
    graphics_fill_rect(ctx,GRect(fx,by-fh+2*fh/3,3,fh/3+1),0,GCornerNone);
  }

  // Embers (small particles floating up)
  graphics_context_set_fill_color(ctx,GColorOrange);
  int e1y=by-20-(s_fire_frame%12)*2;
  int e2y=by-15-((s_fire_frame+5)%10)*2;
  if(e1y>by-40) graphics_fill_rect(ctx,GRect(cx-3,e1y,2,2),0,GCornerNone);
  if(e2y>by-35) graphics_fill_rect(ctx,GRect(cx+4,e2y,1,1),0,GCornerNone);
  #else
  // B&W: simple fire shape
  graphics_context_set_fill_color(ctx,GColorWhite);
  for(int i=0;i<3;i++){
    int fx=cx-3+i*3, fh=6+i*2;
    graphics_fill_rect(ctx,GRect(fx,by-fh,3,fh),0,GCornerNone);
  }
  #endif
}

// ============================================================================
// DRAW: FIREWOOD PILE (battery indicator)
// ============================================================================
static void draw_firewood(GContext *ctx, GRect b) {
  int cx=b.size.w*78/100, by=L_GROUND+L_H*4/100;
  int r=5;  // Log radius
  int filled=(s_bat+19)/20;  // 0-5 logs filled

  #ifdef PBL_COLOR
  GColor wood=GColorFromHEX(0xBB8844);
  GColor ring=GColorFromHEX(0x885522);
  GColor empty=GColorFromHEX(0x444444);
  GColor empty_ring=GColorFromHEX(0x333333);
  #else
  GColor wood=GColorWhite;
  GColor ring=GColorLightGray;
  GColor empty=GColorDarkGray;
  GColor empty_ring=GColorBlack;
  #endif

  // Stacked circles viewed from the end: 3 bottom, 2 top
  // Bottom row — spaced so circles just touch (2*r apart center to center)
  int sp=r*2+1;  // spacing: diameter + 1px gap
  int bx[]={cx-sp, cx, cx+sp};
  for(int i=0;i<3;i++){
    bool on=i<filled;
    graphics_context_set_fill_color(ctx,on?ring:empty_ring);
    graphics_fill_circle(ctx,GPoint(bx[i],by),r);
    graphics_context_set_fill_color(ctx,on?wood:empty);
    graphics_fill_circle(ctx,GPoint(bx[i],by),r-2);
    graphics_context_set_fill_color(ctx,on?ring:empty_ring);
    graphics_fill_circle(ctx,GPoint(bx[i],by),1);
  }
  // Top row: 2 circles nestled between bottom row
  int tx[]={cx-sp/2, cx+sp/2};
  int top_y=by-r*2+1;
  for(int i=0;i<2;i++){
    bool on=(i+3)<filled;
    graphics_context_set_fill_color(ctx,on?ring:empty_ring);
    graphics_fill_circle(ctx,GPoint(tx[i],top_y),r);
    graphics_context_set_fill_color(ctx,on?wood:empty);
    graphics_fill_circle(ctx,GPoint(tx[i],top_y),r-2);
    graphics_context_set_fill_color(ctx,on?ring:empty_ring);
    graphics_fill_circle(ctx,GPoint(tx[i],top_y),1);
  }
}

// ============================================================================
// DRAW: WEATHER ICON (reused from beach)
// ============================================================================
static void draw_wx(GContext *ctx, int x, int y, int code) {
  graphics_context_set_fill_color(ctx,C_TEXT);
  switch(code){
    case WX_CLOUDY: case WX_OVERCAST:
      graphics_fill_circle(ctx,GPoint(x-4,y),5);
      graphics_fill_circle(ctx,GPoint(x+4,y-2),6);
      graphics_fill_circle(ctx,GPoint(x+10,y),4);
      graphics_fill_rect(ctx,GRect(x-8,y,22,6),0,GCornerNone);
      break;
    case WX_FOG:
      graphics_context_set_stroke_color(ctx,C_TEXT);
      graphics_context_set_stroke_width(ctx,2);
      graphics_draw_line(ctx,GPoint(x-10,y-4),GPoint(x+10,y-4));
      graphics_draw_line(ctx,GPoint(x-8,y),GPoint(x+12,y));
      graphics_draw_line(ctx,GPoint(x-10,y+4),GPoint(x+10,y+4));
      break;
    case WX_RAIN:
      graphics_fill_circle(ctx,GPoint(x-3,y-4),4);
      graphics_fill_circle(ctx,GPoint(x+5,y-5),5);
      graphics_fill_rect(ctx,GRect(x-7,y-4,16,4),0,GCornerNone);
      #ifdef PBL_COLOR
      graphics_context_set_fill_color(ctx,GColorPictonBlue);
      #endif
      graphics_fill_rect(ctx,GRect(x-5,y+2,2,4),0,GCornerNone);
      graphics_fill_rect(ctx,GRect(x+1,y+4,2,4),0,GCornerNone);
      graphics_fill_rect(ctx,GRect(x+7,y+2,2,4),0,GCornerNone);
      break;
    case WX_STORM:
      graphics_fill_circle(ctx,GPoint(x-3,y-4),4);
      graphics_fill_circle(ctx,GPoint(x+5,y-5),5);
      graphics_fill_rect(ctx,GRect(x-7,y-4,16,4),0,GCornerNone);
      #ifdef PBL_COLOR
      graphics_context_set_fill_color(ctx,GColorYellow);
      #endif
      graphics_fill_rect(ctx,GRect(x+1,y+1,3,3),0,GCornerNone);
      graphics_fill_rect(ctx,GRect(x-1,y+4,3,3),0,GCornerNone);
      break;
    case WX_SNOW:
      graphics_fill_circle(ctx,GPoint(x-3,y-4),4);
      graphics_fill_circle(ctx,GPoint(x+5,y-5),5);
      graphics_fill_rect(ctx,GRect(x-7,y-4,16,4),0,GCornerNone);
      graphics_fill_rect(ctx,GRect(x-4,y+2,1,3),0,GCornerNone);
      graphics_fill_rect(ctx,GRect(x+5,y+4,1,3),0,GCornerNone);
      break;
    case WX_WIND:
      graphics_context_set_stroke_color(ctx,C_TEXT);
      graphics_context_set_stroke_width(ctx,2);
      graphics_draw_line(ctx,GPoint(x-10,y-3),GPoint(x+10,y-5));
      graphics_draw_line(ctx,GPoint(x-8,y+2),GPoint(x+12,y));
      break;
    default: break;
  }
}

// ============================================================================
// TEXT HELPER
// ============================================================================
static void txt(GContext *ctx, const char *s, GFont f, GRect r, GTextAlignment a) {
  graphics_context_set_text_color(ctx,C_SHAD);
  graphics_draw_text(ctx,s,f,GRect(r.origin.x+1,r.origin.y+1,r.size.w,r.size.h),
    GTextOverflowModeTrailingEllipsis,a,NULL);
  graphics_context_set_text_color(ctx,C_TEXT);
  graphics_draw_text(ctx,s,f,r,GTextOverflowModeTrailingEllipsis,a,NULL);
}

// ============================================================================
// DRAW: HUD
// ============================================================================
static void draw_hud(GContext *ctx, GRect b) {
  GFont f42=fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
  GFont f18=fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont f14=fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f24=fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  int w=b.size.w;

  // Temp + weather (top) — use peek data when peeking
  int show_temp = (s_peek>=0) ? s_d.pk_t[s_peek] : s_d.temp;
  int show_wx   = (s_peek>=0) ? s_d.pk_wx[s_peek] : s_d.wx;
  snprintf(s_tmp,sizeof(s_tmp),"%d°",show_temp);
  int temp_y=26;
  if(show_wx!=WX_CLEAR){
    txt(ctx,s_tmp,f24,GRect(w/2-48,temp_y,48,28),GTextAlignmentRight);
    draw_wx(ctx,w/2+16,temp_y+18,show_wx);
  } else {
    txt(ctx,s_tmp,f24,GRect(0,temp_y,w,28),GTextAlignmentCenter);
  }

  // Time
  int ty=58;
  txt(ctx,s_tbuf,f42,GRect(0,ty,w,50),GTextAlignmentCenter);

  // Date (smaller font)
  txt(ctx,s_dbuf,f14,GRect(0,ty+44,w,18),GTextAlignmentCenter);

  // Sunrise/sunset times (same line as date)
  if(s_show_sun) {
    snprintf(s_sr,sizeof(s_sr),"%d:%02d",fmt_h(s_d.sr_h),s_d.sr_m);
    snprintf(s_ss,sizeof(s_ss),"%d:%02d",fmt_h(s_d.ss_h),s_d.ss_m);
    int sun_y=ty+44;
    txt(ctx,s_sr,f14,GRect(14,sun_y,50,18),GTextAlignmentLeft);
    txt(ctx,s_ss,f14,GRect(w-64,sun_y,50,18),GTextAlignmentRight);
  }

  // Hi/Lo temps
  if(s_show_hilo && s_d.valid) {
    char hilo[16];
    snprintf(hilo,sizeof(hilo),"H:%d L:%d",s_d.hi,s_d.lo);
    graphics_context_set_text_color(ctx,C_INFO);
    graphics_draw_text(ctx,hilo,f14,GRect(6,b.size.h-26,w,16),
      GTextOverflowModeTrailingEllipsis,GTextAlignmentCenter,NULL);
  }

}

// ============================================================================
// CANVAS
// ============================================================================
static void canvas_proc(Layer *l, GContext *ctx) {
  GRect b=layer_get_bounds(l);
  draw_sky(ctx,b); draw_stars(ctx,b);
  draw_sun(ctx,b); draw_moon(ctx,b);
  draw_mountains(ctx,b);  // Mountains drawn OVER sun/moon near horizon
  draw_lake(ctx,b);
  draw_ground(ctx,b);
  draw_trees(ctx,b); draw_tent(ctx,b); draw_campfire(ctx,b); draw_firewood(ctx,b);
  draw_hud(ctx,b);
}

// ============================================================================
// UPDATE TIME
// ============================================================================
static void upd_time(void){
  time_t t=time(NULL); struct tm *tm=localtime(&t); if(!tm) return;
  s_hr=tm->tm_hour; s_mn=tm->tm_min;
  strftime(s_tbuf,sizeof(s_tbuf),clock_is_24h_style()?"%H:%M":"%I:%M",tm);
  if(!clock_is_24h_style()&&s_tbuf[0]=='0')
    memmove(s_tbuf,&s_tbuf[1],sizeof(s_tbuf)-1);
  strftime(s_dbuf,sizeof(s_dbuf),"%a, %b %d",tm);
}

// ============================================================================
// ANIMATION
// ============================================================================
static void anim_cb(void *data){
  s_timer=NULL;
  s_fire_frame++;
  s_anim_ms+=ANIM_INTERVAL;
  if(s_canvas) layer_mark_dirty(s_canvas);

  bool indev=(s_dev||!s_d.valid)&&s_pre>=0;
  bool stop=indev?false:(s_anim_ms>=ANIM_DURATION);
  // At night, keep animating for campfire
  if(is_night()) stop=false;
  if(stop){s_anim=false; return;}
  s_timer=app_timer_register(ANIM_INTERVAL,anim_cb,NULL);
  if(!s_timer) s_anim=false;
}
static void start_anim(void){
  if(s_anim && s_timer) return;
  if(s_timer){app_timer_cancel(s_timer);s_timer=NULL;}
  s_anim=true; s_anim_ms=0;
  s_timer=app_timer_register(ANIM_INTERVAL,anim_cb,NULL);
}

// ============================================================================
// EVENTS
// ============================================================================
static void tick_cb(struct tm *t, TimeUnits u){
  if((s_dev||!s_d.valid)&&s_pre>=0){
    if(!s_anim || !s_timer) start_anim();
    else if(s_canvas) layer_mark_dirty(s_canvas);
    return;
  }
  upd_time();
  // Keep fire animated at night
  if(is_night() && (!s_anim || !s_timer)) start_anim();
  if(t->tm_min%5==0) start_anim();
  else if(s_canvas) layer_mark_dirty(s_canvas);
  if(t->tm_min%30==0){
    DictionaryIterator *it;
    if(app_message_outbox_begin(&it)==APP_MSG_OK){
      dict_write_uint8(it,MESSAGE_KEY_REQUEST_DATA,1);
      app_message_outbox_send();
    }
  }
}
static void bat_cb(BatteryChargeState s){ s_bat=s.charge_percent; }
static void bt_cb(bool c){
  s_bt=c; if(!c) vibes_short_pulse();
  if(s_canvas) layer_mark_dirty(s_canvas);
}
static void peek_revert_cb(void *data){
  s_peek_timer=NULL; s_peek=-1;
  upd_time();  // Restore real time
  if(s_canvas) layer_mark_dirty(s_canvas);
}
static void tap_cb(AccelAxisType a, int32_t d){
  if(s_dev||!s_d.valid){
    s_pre++; if(s_pre>=NUM_PRESETS) s_pre=0;
    apply_pre(s_pre);
    APP_LOG(APP_LOG_LEVEL_INFO,"DEV %d",s_pre);
    start_anim();
  } else {
    // Cycle peek: -1 → 0 → 1 → 2 → -1
    s_peek++;
    if(s_peek>2) s_peek=-1;
    if(s_peek>=0) {
      // Override time/weather with peek slot
      s_hr=s_d.pk_h[s_peek]; s_mn=0;
      // Format peeked time
      if(clock_is_24h_style()) snprintf(s_tbuf,sizeof(s_tbuf),"%d:00",s_hr);
      else { int h=s_hr%12; if(!h)h=12; snprintf(s_tbuf,sizeof(s_tbuf),"%d:00",h); }
      // Show peek label as date line
      bool is_tmrw=(s_d.pk_h[s_peek]!=s_hr); // Approximate
      int ph=s_d.pk_h[s_peek];
      const char *period;
      if(ph>=5 && ph<12) period="Morning";
      else if(ph>=12 && ph<17) period="Afternoon";
      else if(ph>=17 && ph<21) period="Tonight";
      else period="Tomorrow";
      const char *arrows = (s_peek==0) ? ">" : (s_peek==1) ? ">>" : ">>>";
      snprintf(s_dbuf,sizeof(s_dbuf),"%s %s",arrows,period);
    } else {
      upd_time();  // Restore real time
    }
    // Reset/extend the revert timer (5 seconds of inactivity)
    if(s_peek_timer) app_timer_cancel(s_peek_timer);
    if(s_peek>=0)
      s_peek_timer=app_timer_register(5000,peek_revert_cb,NULL);
    start_anim();
    if(s_canvas) layer_mark_dirty(s_canvas);
  }
}

// ============================================================================
// APPMESSAGE
// ============================================================================
static void inbox_cb(DictionaryIterator *it, void *c){
  Tuple *t;
  // Dev mode toggle
  t=dict_find(it,MESSAGE_KEY_DEV_MODE);
  if(t){ s_dev=(bool)t->value->int32; persist_write_bool(P_DEV_MODE,s_dev);
    if(!s_dev){s_pre=-1; upd_time();} }
  // Exit auto-dev when real data arrives
  if(s_pre>=0 && !s_dev){s_pre=-1; upd_time();}

  t=dict_find(it,MESSAGE_KEY_SUNRISE_HOUR);  if(t) s_d.sr_h=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_SUNRISE_MIN);   if(t) s_d.sr_m=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_SUNSET_HOUR);   if(t) s_d.ss_h=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_SUNSET_MIN);    if(t) s_d.ss_m=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_TEMPERATURE);   if(t) s_d.temp=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_WEATHER_CODE);  if(t) s_d.wx=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_TEMP_HIGH);     if(t) s_d.hi=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_TEMP_LOW);      if(t) s_d.lo=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_DISPLAY_MODE);
  if(t) {
    int mode=(int)t->value->int32;
    // Mode encodes two bits: bit0=show_sun, bit1=show_hilo
    s_show_sun=mode&1; s_show_hilo=(mode>>1)&1;
    persist_write_bool(P_SHOW_SUN,s_show_sun);
    persist_write_bool(P_SHOW_HILO,s_show_hilo);
    APP_LOG(APP_LOG_LEVEL_INFO,"Config: sun=%d hilo=%d (mode=%d)",s_show_sun,s_show_hilo,mode);
  }
  t=dict_find(it,MESSAGE_KEY_TOWN_NAME);
  if(t) snprintf(s_d.town,sizeof(s_d.town),"%s",t->value->cstring);

  // Peek forecast slots
  t=dict_find(it,MESSAGE_KEY_PEEK_WX1); if(t) s_d.pk_wx[0]=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_PEEK_T1);  if(t) s_d.pk_t[0]=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_PEEK_H1);  if(t) s_d.pk_h[0]=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_PEEK_WX2); if(t) s_d.pk_wx[1]=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_PEEK_T2);  if(t) s_d.pk_t[1]=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_PEEK_H2);  if(t) s_d.pk_h[1]=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_PEEK_WX3); if(t) s_d.pk_wx[2]=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_PEEK_T3);  if(t) s_d.pk_t[2]=(int)t->value->int32;
  t=dict_find(it,MESSAGE_KEY_PEEK_H3);  if(t) s_d.pk_h[2]=(int)t->value->int32;

  s_d.valid=true;
  if(s_canvas) layer_mark_dirty(s_canvas);

  // Persist
  persist_write_int(P_SUNRISE_H,s_d.sr_h);
  persist_write_int(P_SUNRISE_M,s_d.sr_m);
  persist_write_int(P_SUNSET_H,s_d.ss_h);
  persist_write_int(P_SUNSET_M,s_d.ss_m);
  persist_write_int(P_TEMP,s_d.temp);
  persist_write_int(P_WX,s_d.wx);
  persist_write_int(P_TEMP_HI,s_d.hi);
  persist_write_int(P_TEMP_LO,s_d.lo);
  persist_write_bool(P_VALID,true);
}
static void drop_cb(AppMessageResult r,void *c){}
static void fail_cb(DictionaryIterator *i,AppMessageResult r,void *c){}
static void sent_cb(DictionaryIterator *i,void *c){}

// ============================================================================
// PERSIST
// ============================================================================
static void load_data(void){
  if(persist_exists(P_VALID)&&persist_read_bool(P_VALID)){
    s_d.sr_h=persist_read_int(P_SUNRISE_H);
    s_d.sr_m=persist_read_int(P_SUNRISE_M);
    s_d.ss_h=persist_read_int(P_SUNSET_H);
    s_d.ss_m=persist_read_int(P_SUNSET_M);
    s_d.temp=persist_read_int(P_TEMP);
    s_d.wx=persist_read_int(P_WX);
    s_d.hi=persist_read_int(P_TEMP_HI);
    s_d.lo=persist_read_int(P_TEMP_LO);
    s_d.valid=true;
  }
  if(persist_exists(P_SHOW_SUN)) s_show_sun=persist_read_bool(P_SHOW_SUN);
  if(persist_exists(P_SHOW_HILO)) s_show_hilo=persist_read_bool(P_SHOW_HILO);
  if(persist_exists(P_DEV_MODE)) s_dev=persist_read_bool(P_DEV_MODE);
}

// ============================================================================
// WINDOW
// ============================================================================
static void win_load(Window *w){
  Layer *wl=window_get_root_layer(w);
  GRect b=layer_get_bounds(wl);
  init_layout(b.size.w, b.size.h);
  s_canvas=layer_create(b);
  layer_set_update_proc(s_canvas,canvas_proc);
  layer_add_child(wl,s_canvas);
  s_bat=battery_state_service_peek().charge_percent;
  s_bt=connection_service_peek_pebble_app_connection();
  upd_time();
  if(!s_d.valid){s_pre=0;apply_pre(0);}
  start_anim();
}
static void win_unload(Window *w){
  if(s_timer){app_timer_cancel(s_timer);s_timer=NULL;}
  if(s_peek_timer){app_timer_cancel(s_peek_timer);s_peek_timer=NULL;}
  if(s_canvas){layer_destroy(s_canvas);s_canvas=NULL;}
}

// ============================================================================
// LIFECYCLE
// ============================================================================
static void init(void){
  srand(time(NULL));
  load_data();
  s_win=window_create();
  window_set_background_color(s_win,GColorBlack);
  window_set_window_handlers(s_win,(WindowHandlers){.load=win_load,.unload=win_unload});
  window_stack_push(s_win,true);
  tick_timer_service_subscribe(MINUTE_UNIT,tick_cb);
  battery_state_service_subscribe(bat_cb);
  connection_service_subscribe((ConnectionHandlers){.pebble_app_connection_handler=bt_cb});
  accel_tap_service_subscribe(tap_cb);
  app_message_register_inbox_received(inbox_cb);
  app_message_register_inbox_dropped(drop_cb);
  app_message_register_outbox_failed(fail_cb);
  app_message_register_outbox_sent(sent_cb);
  app_message_open(1024,64);
}
static void deinit(void){
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
  accel_tap_service_unsubscribe();
  window_destroy(s_win);
}
int main(void){init();app_event_loop();deinit();return 0;}
