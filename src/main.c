/*
 * RectWatch - Pebble Time Steel (basalt) only
 * Cream monochrome watchface
 *
 * Layout (144x168):
 *   Clock face:  (4,4) 72x88 no border
 *   Month row:   y=96
 *   Day row:     y=112
 *   Date row:    y=128
 *   Step scale:  y=158
 *
 * appKeys (alphabetical):
 *   A_STEPS  0 = hide/show step scale
 *   B_THEME  1 = 0:cream  1:dark
 */

#include <pebble.h>

#define SETTINGS_KEY  1
#define KEY_HEALTH    0
#define KEY_STEPS     1
#define KEY_THEME     2
#define KEY_BOLD      3
#define KEY_EPAPER    4
#define KEY_COLOR     5

/* ?? appKeys (alphabetical):
 *   A_STEPS  0  0=hide  1=show step scale
 *   B_THEME  1  0=cream  1=dark
 */

/* ?? Geometry ?? */
#define FX   4
#define FY   4
#define FW  72
#define FH  88
#define CX  40
#define CY  48
#define CR  10

/* Calendar rows */
#define Y_MON   93
#define Y_DOW  111
#define Y_DATE 129
#define TRI_H    6   /* triangle height px */
#define TRI_GAP  0   /* gap between triangle and label */
#define ROW_H   16   /* total row height */

/* Step scale */
#define SC_X1    2
#define SC_X2  142
#define SC_SW  140
#define SC_Y   165
#define STEPS_GOAL 10000

/* ?? Settings ?? */
typedef struct { uint8_t health; uint8_t steps; uint8_t theme; uint8_t bold; uint8_t epaper; uint8_t color; } Settings;
static Settings s = { .health=1, .steps=1, .theme=0, .bold=0, .epaper=1, .color=0 };
static void settings_load(void) {
  if (persist_exists(SETTINGS_KEY))
    persist_read_data(SETTINGS_KEY, &s, sizeof(s));
  if (s.health > 1) s.health = 1;
  if (s.steps  > 1) s.steps  = 1;
  if (s.theme  > 1) s.theme  = 0;
  if (s.bold   > 1) s.bold   = 0;
  if (s.epaper > 1) s.epaper = 1;
  if (s.color  > 2) s.color  = 0;
}
static void settings_save(void) { persist_write_data(SETTINGS_KEY, &s, sizeof(s)); }

/* ?? Colours ?? */
/* Cream theme */
#define C_BG        GColorWhite
#define C_FACE      GColorWhite
#define C_HAND_H    GColorBlack
#define C_HAND_M    GColorDarkGray
#define C_MARK      GColorDarkGray
#define C_MARK_D    GColorBlack
#define C_CAL_ACT   GColorBlack
#define C_CAL_DIM   GColorLightGray
#define C_SCALE     GColorDarkGray
#define C_NEEDLE    GColorBlack

/* Dark theme */
#define D_BG        GColorBlack
#define D_FACE      GColorBlack
#define D_HAND_H    GColorWhite
#define D_HAND_M    GColorLightGray
#define D_MARK      GColorDarkGray
#define D_MARK_D    GColorWhite
#define D_CAL_ACT   GColorWhite
#define D_CAL_DIM   GColorDarkGray
#define D_SCALE     GColorDarkGray
#define D_NEEDLE    GColorWhite

static GColor col_bg(void)      { return s.theme ? D_BG      : C_BG;      }
static GColor col_face(void)    { return s.theme ? D_FACE    : C_FACE;    }
static GColor col_hand_h(void)  { return s.theme ? D_HAND_H  : C_HAND_H;  }
static GColor col_hand_m(void)  { return s.theme ? D_HAND_M  : C_HAND_M;  }
static GColor col_mark(void)    { return s.theme ? D_MARK    : C_MARK;    }
static GColor col_mark_d(void)  { return s.theme ? D_MARK_D  : C_MARK_D;  }

/* Accent colour: applies to active cal items + health stats */
static GColor col_accent(void) {
  if (s.theme == 1) return GColorWhite;
  switch (s.color) {
    case 1:  return GColorDarkGray;
    case 2:  return GColorDarkCandyAppleRed;
    default: return GColorBlack;
  }
}
static GColor col_cal_dim(void) { return s.theme ? D_CAL_DIM : C_CAL_DIM; }
static GColor col_scale(void)   { return s.theme ? D_SCALE   : C_SCALE;   }
static GColor col_needle(void)  { return s.theme ? D_NEEDLE  : C_NEEDLE;  }

/* ?? State ?? */
static Window   *s_win;
static Layer    *s_canvas;
static int       s_hours, s_minutes, s_seconds, s_steps;
static int       s_hr=0, s_cal=0, s_sleep=0, s_batt=100;

/* ?? Hand point ?? */
static GPoint hand_pt(int angle_deg, int rx, int ry) {
  int32_t a = DEG_TO_TRIGANGLE(angle_deg - 90);
  return GPoint(
    CX + (int32_t)(cos_lookup(a) * rx / TRIG_MAX_RATIO),
    CY + (int32_t)(sin_lookup(a) * ry / TRIG_MAX_RATIO));
}

/* ?? Short thick downward triangle ?? */
/* apex at (x, apexY), widens upward */
static void draw_tri_down(GContext *ctx, int x, int apexY) {
  graphics_context_set_fill_color(ctx, col_needle());
  for (int ty = 0; ty < TRI_H; ty++) {
    int half = 5 * (TRI_H - ty) / TRI_H;
    if (half < 1) half = 1;
    graphics_fill_rect(ctx,
      GRect(x - half, apexY - TRI_H + ty, half*2, 1),
      0, GCornerNone);
  }
}


/* ?? Calendar row ?? */
/* Show current label centred at anchorX + neighbours left/right */
/* itemW = spacing between items in this row */
static void draw_cal_row(GContext *ctx, const char **items, int n,
                          int current, int anchorX, int rowTopY,
                          int itemW, bool is_date) {
  GFont font = fonts_get_system_font(s.bold ? FONT_KEY_GOTHIC_14_BOLD : FONT_KEY_GOTHIC_14);
  int labelY  = rowTopY + TRI_H + TRI_GAP;

  /* Draw current ? 8 neighbours - bleed off edges */
  for (int off = -8; off <= 8; off++) {
    int idx = ((current + off) % n + n) % n;
    int x   = anchorX + off * itemW;
    if (x < -30 || x > 144 + 30) continue;

    bool isCur = (off == 0);
    int  dist  = off < 0 ? -off : off;

    /* Fade by distance */
    GColor col;
    if      (isCur)   col = col_accent();
    else if (dist==1) col = col_cal_dim();
    else if (dist==2) col = col_mark();
    else              col = col_mark(); /* still visible, fades naturally off screen */

    char ubuf[8];
    int ui=0; while(items[idx][ui]&&ui<7){ubuf[ui]=(items[idx][ui]>='a'&&items[idx][ui]<='z')?items[idx][ui]-32:items[idx][ui];ui++;} ubuf[ui]='\0';
    graphics_context_set_text_color(ctx, col);
    graphics_draw_text(ctx, ubuf, font,
      GRect(x - itemW, labelY, itemW*2, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  /* Triangle above current, apex pointing down */
  draw_tri_down(ctx, anchorX, labelY + 2);
}


/* ?? Battery icon ?? */
static void draw_battery_icon(GContext *ctx) {
  int bx=130, by=3, bw=11, bh=6;
  graphics_context_set_stroke_color(ctx, col_mark_d());
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_rect(ctx, GRect(bx, by, bw, bh));
  graphics_context_set_fill_color(ctx, col_mark_d());
  graphics_fill_rect(ctx, GRect(bx+bw, by+bh/2-1, 2, 2), 0, GCornerNone);
  int fw = (bw-2) * s_batt / 100;
  if (fw < 1) fw = 1;
  GColor fc = (s_batt > 30) ? col_mark_d() : GColorDarkCandyAppleRed;
  graphics_context_set_fill_color(ctx, fc);
  graphics_fill_rect(ctx, GRect(bx+1, by+1, fw, bh-2), 0, GCornerNone);
}



/* ?? Minimal Mono 5?7 pixel font (for health stats) ?? */
static const uint8_t s_mm_font5x7[][5] = {
  /* sp */ {0x00,0x00,0x00,0x00,0x00},
  /* 0  */ {0x3E,0x51,0x49,0x45,0x3E},
  /* 1  */ {0x00,0x42,0x7F,0x40,0x00},
  /* 2  */ {0x42,0x61,0x51,0x49,0x46},
  /* 3  */ {0x21,0x41,0x45,0x4B,0x31},
  /* 4  */ {0x18,0x14,0x12,0x7F,0x10},
  /* 5  */ {0x27,0x45,0x45,0x45,0x39},
  /* 6  */ {0x3C,0x4A,0x49,0x49,0x30},
  /* 7  */ {0x01,0x71,0x09,0x05,0x03},
  /* 8  */ {0x36,0x49,0x49,0x49,0x36},
  /* 9  */ {0x06,0x49,0x49,0x29,0x1E},
  /* A  */ {0x7E,0x11,0x11,0x11,0x7E},
  /* B  */ {0x7F,0x49,0x49,0x49,0x36},
  /* C  */ {0x3E,0x41,0x41,0x41,0x22},
  /* D  */ {0x7F,0x41,0x41,0x22,0x1C},
  /* E  */ {0x7F,0x49,0x49,0x49,0x41},
  /* F  */ {0x7F,0x09,0x09,0x09,0x01},
  /* G  */ {0x3E,0x41,0x49,0x49,0x7A},
  /* H  */ {0x7F,0x08,0x08,0x08,0x7F},
  /* I  */ {0x00,0x41,0x7F,0x41,0x00},
  /* J  */ {0x20,0x40,0x41,0x3F,0x01},
  /* K  */ {0x7F,0x08,0x14,0x22,0x41},
  /* L  */ {0x7F,0x40,0x40,0x40,0x40},
  /* M  */ {0x7F,0x02,0x04,0x02,0x7F},
  /* N  */ {0x7F,0x04,0x08,0x10,0x7F},
  /* O  */ {0x3E,0x41,0x41,0x41,0x3E},
  /* P  */ {0x7F,0x09,0x09,0x09,0x06},
  /* Q  */ {0x3E,0x41,0x51,0x21,0x5E},
  /* R  */ {0x7F,0x09,0x19,0x29,0x46},
  /* S  */ {0x46,0x49,0x49,0x49,0x31},
  /* T  */ {0x01,0x01,0x7F,0x01,0x01},
  /* U  */ {0x3F,0x40,0x40,0x40,0x3F},
  /* V  */ {0x1F,0x20,0x40,0x20,0x1F},
  /* W  */ {0x3F,0x40,0x38,0x40,0x3F},
  /* X  */ {0x63,0x14,0x08,0x14,0x63},
  /* Y  */ {0x07,0x08,0x70,0x08,0x07},
  /* Z  */ {0x61,0x51,0x49,0x45,0x43},
  /* -  */ {0x08,0x08,0x08,0x08,0x08},
  /* :  */ {0x00,0x36,0x36,0x00,0x00},
};
static int mm_font_index(char c) {
  if (c == ' ') return 0;
  if (c >= '0' && c <= '9') return 1 + (c - '0');
  if (c >= 'A' && c <= 'Z') return 11 + (c - 'A');
  if (c >= 'a' && c <= 'z') return 11 + (c - 'a');
  if (c == '-') return 37;
  if (c == ':') return 38;
  return 0;
}

/*
 * Draw text rotated 90? CCW - reads bottom to top on screen.
 * Anchor (sx, sy) = bottom of column, first char starts here going up.
 * Each char cell: 7px wide x 5px tall on screen.
 * Chars spaced 6px upward.
 *
 * 90? CCW mapping from glyph[gc][gr]:
 *   screen_x = sx + gr          (glyph row 0=left on screen)
 *   screen_y = char_bottom - gc (glyph col 0=bottom on screen)
 */
static void draw_vertical_text(GContext *ctx, const char *text,
                                int sx, int sy, GColor col) {
  /* Draws text bottom-to-top starting at (sx, sy).
   * Each char cell: 7px wide, 5px tall on screen.
   * Glyph rotated 90deg CCW: glyph col -> screen y (up), glyph row -> screen x (right)
   * char 0 starts at sy (bottom), each next char 6px higher.
   */
  graphics_context_set_fill_color(ctx, col);
  int n = 0;
  while (text[n]) n++;
  for (int ci = 0; ci < n; ci++) {
    /* bottom of this char cell */
    int cell_bottom = sy - ci * 6;
    if (cell_bottom - 5 < FY) break;
    const uint8_t *glyph = s_mm_font5x7[mm_font_index(text[ci])];
    /* glyph[gc] = column gc (0=leftmost col of letter)
     * bit (6-gr) = row gr (0=top of letter)
     * 90deg CCW rotation for bottom-to-top reading:
     *   screen x = sx + (6 - gr)   <- row 0(top of letter) maps to right side
     *   screen y = cell_bottom - gc <- col 0(left) maps to bottom
     */
    for (int gc = 0; gc < 5; gc++) {
      for (int gr = 0; gr < 7; gr++) {
        if (glyph[gc] & (1 << (6 - gr))) {
          int px = sx + (6 - gr);
          int py = cell_bottom - gc;
          graphics_fill_rect(ctx, GRect(px, py, 1, 1), 0, GCornerNone);
          if (s.bold) graphics_fill_rect(ctx, GRect(px+1, py, 1, 1), 0, GCornerNone);
        }
      }
    }
  }
}

/* ?? Health stats - 5?7 pixel font, rotated 90? CCW ?? */
/*
 * 3 columns anchored at bottom-right of clock face (x=76, y=92).
 * Each column 9px wide (7px glyph + 2px gap).
 * Text reads bottom to top.
 */
static void draw_health(GContext *ctx) {
  const int AX = FX + FW;   /* x=76 */
  const int AY = FY + FH;   /* y=92 */
  const int COL_W = 9;       /* 7px glyph + 2px gap */

  char bufs[3][20];
  snprintf(bufs[0], sizeof(bufs[0]), "HEART RATE:%d", s_hr);
  snprintf(bufs[1], sizeof(bufs[1]), "CALORIES:%d", s_cal);
  snprintf(bufs[2], sizeof(bufs[2]), "SLEEP:%dH%02d", s_sleep/60, s_sleep%60);

  /* Uppercase only */
  for (int b = 0; b < 3; b++)
    for (int i = 0; bufs[b][i]; i++)
      if (bufs[b][i] >= 'a' && bufs[b][i] <= 'z') bufs[b][i] -= 32;

  for (int col = 0; col < 3; col++) {
    int sx = AX + col * COL_W;
    draw_vertical_text(ctx, bufs[col], sx, AY, col_accent());
  }
}



/* Draw pixel art progressively over 8 hours from midnight.
 * 267 total pixels, revealed over 480 minutes.
 * pixels_shown = elapsed_minutes * 267 / 480
 * Pixels stored in draw order (row by row, left to right).
 */
/*
 * draw_viz — right panel data visualization
 * Zone: x=105..140 (35px), y=14..90 (76px)
 * X-axis horizontal, Y-axis vertical — normal upright charts
 * Rotates mode every 2 hours: 0=step bars, 1=HR line, 2=sleep bar, 3=scatter
 */
static void draw_viz(GContext *ctx) {
  const int VX = 105;   /* left edge */
  const int VY = 14;    /* top edge  */
  const int VW = 35;    /* width     */
  const int VH = 76;    /* height    */
  const int VBY = VY + VH - 1; /* baseline y */

  time_t now_t = time(NULL);
  struct tm *t  = localtime(&now_t);
  int mode = (t->tm_hour / 2) % 4;

  graphics_context_set_fill_color(ctx, col_mark());
  graphics_context_set_stroke_color(ctx, col_mark());
  graphics_context_set_stroke_width(ctx, 1);

  /* Baseline */
  graphics_draw_line(ctx, GPoint(VX, VBY), GPoint(VX+VW-1, VBY));

  if (mode == 0) {
    /* ── Step bars (hourly proxy from s_steps) ── */
    int n = t->tm_hour > 0 ? t->tm_hour : 1;
    if (n > 10) n = 10;
    int per_hr = s_steps / n;
    int max_s  = 1200;
    int bw = (VW - 2) / n - 1;
    if (bw < 1) bw = 1;
    for (int i = 0; i < n; i++) {
      int bh = per_hr * (VH-4) / max_s;
      if (bh > VH-4) bh = VH-4;
      if (bh < 1)    bh = 1;
      int x = VX + 1 + i * (bw+1);
      int y = VBY - bh;
      graphics_context_set_fill_color(ctx,
        i == n-1 ? col_accent() : col_mark());
      graphics_fill_rect(ctx, GRect(x, y, bw, bh), 0, GCornerNone);
    }
  }

  else if (mode == 1) {
    /* ── HR line chart ── */
    int base  = s_hr > 10 ? s_hr : 72;
    int mn = 50, mx = 110;
    int n  = 8;
    int pts_x[8], pts_y[8];
    for (int i = 0; i < n; i++) {
      int v = base + ((i*7+3) % 20) - 10;
      if (v < mn) v = mn;
      if (v > mx) v = mx;
      pts_x[i] = VX + 1 + (VW-2)*i/(n-1);
      pts_y[i] = VBY - 1 - (VH-6)*(v-mn)/(mx-mn);
    }
    /* dashed grid at 75bpm */
    int gy = VBY - 1 - (VH-6)*(75-mn)/(mx-mn);
    for (int x = VX; x < VX+VW; x += 3)
      graphics_draw_pixel(ctx, GPoint(x, gy));
    /* line */
    graphics_context_set_stroke_color(ctx, col_mark_d());
    graphics_context_set_stroke_color(ctx, col_accent());
    for (int i = 0; i < n-1; i++)
      graphics_draw_line(ctx, GPoint(pts_x[i], pts_y[i]),
                              GPoint(pts_x[i+1], pts_y[i+1]));
    /* dots */
    for (int i = 0; i < n; i++) {
      graphics_context_set_fill_color(ctx,
        i == n-1 ? col_accent() : col_mark());
      graphics_fill_rect(ctx,
        GRect(pts_x[i]-1, pts_y[i]-1, 2, 2), 0, GCornerNone);
    }
  }

  else if (mode == 2) {
    /* ── Sleep vertical bar ── */
    int pct   = s_sleep * 100 / 480;
    if (pct > 100) pct = 100;
    int bw    = VW / 2;
    int bx    = VX + (VW - bw) / 2;
    int fullH = VH - 4;
    int fillH = fullH * pct / 100;
    /* background */
    graphics_context_set_fill_color(ctx, col_cal_dim());
    graphics_fill_rect(ctx, GRect(bx, VY+2, bw, fullH), 0, GCornerNone);
    /* fill */
    graphics_context_set_fill_color(ctx, col_accent());
    graphics_fill_rect(ctx, GRect(bx, VY+2+fullH-fillH, bw, fillH), 0, GCornerNone);
    /* outline */
    graphics_context_set_stroke_color(ctx, col_accent());
    graphics_draw_rect(ctx, GRect(bx, VY+2, bw, fullH));
    /* 50% tick */
    graphics_context_set_stroke_color(ctx, col_bg());
    graphics_draw_line(ctx,
      GPoint(bx-1,    VY+2+fullH/2),
      GPoint(bx+bw+1, VY+2+fullH/2));
  }

  else {
    /* ── Scatter plot: steps vs HR ── */
    /* X = hour of day (0..23), Y = step density */
    int n = 20;
    int max_s = 500;
    graphics_context_set_fill_color(ctx, col_mark());
    for (int i = 0; i < n; i++) {
      /* Pseudo-random scatter seeded from step+hr data */
      int seed  = (s_steps + s_hr * 7 + i * 31) % 1000;
      int sx    = VX + 1 + (seed % (VW-4));
      int sy_v  = (seed * 17 + i * 13) % (VH-6);
      int sy    = VY + 2 + sy_v;
      int sz    = (i % 3 == 0) ? 2 : 1;
      graphics_context_set_fill_color(ctx,
        sy_v < (VH-6)/2 ? col_accent() : col_mark());
      graphics_fill_rect(ctx, GRect(sx, sy, sz, sz), 0, GCornerNone);
    }
    /* Trend line */
    graphics_context_set_stroke_color(ctx, col_accent());
    graphics_draw_line(ctx,
      GPoint(VX+2, VBY-4),
      GPoint(VX+VW-3, VY+8));
  }
}

/* ?? Step scale ?? */
static void draw_step_scale(GContext *ctx) {
  int pct = (s_steps >= STEPS_GOAL) ? 100 : s_steps * 100 / STEPS_GOAL;

  graphics_context_set_stroke_color(ctx, col_scale());
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(SC_X1, SC_Y), GPoint(SC_X2, SC_Y));

  for (int i = 0; i <= 12; i++) {
    int x  = SC_X1 + i * SC_SW / 12;
    int tl = (i % 3 == 0) ? 5 : 3;
    graphics_draw_line(ctx, GPoint(x, SC_Y - tl), GPoint(x, SC_Y));
    if (i % 3 == 0) {
      char lbl[5];
      if (i == 0) snprintf(lbl, sizeof(lbl), "0");
      else        snprintf(lbl, sizeof(lbl), "%dk", i);
      graphics_context_set_text_color(ctx, col_scale());
      graphics_draw_text(ctx, lbl,
        fonts_get_system_font(s.bold ? FONT_KEY_GOTHIC_14_BOLD : FONT_KEY_GOTHIC_14),
        GRect(x-7, SC_Y-18, 14, 14),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
  }

  /* Needle - pointing down to baseline */
  int nx = SC_X1 + pct * SC_SW / 100;
  graphics_context_set_fill_color(ctx, GColorDarkCandyAppleRed);
  for (int ty = 0; ty <= 8; ty++) {
    int half = ty * 3 / 8;
    if (half < 1 && ty > 0) half = 1;
    graphics_fill_rect(ctx,
      GRect(nx - half, SC_Y - ty, half*2, 1),
      0, GCornerNone);
  }

  /* Step count */
  char buf[12];
  snprintf(buf, sizeof(buf), "%d", s_steps);
  graphics_context_set_text_color(ctx, GColorDarkCandyAppleRed);
  graphics_draw_text(ctx, buf,
    fonts_get_system_font(s.bold ? FONT_KEY_GOTHIC_14_BOLD : FONT_KEY_GOTHIC_14),
    GRect(SC_X2 - 42, SC_Y + 2, 44, 14),
    GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
}

/* ?? Canvas ?? */
static void canvas_update(Layer *layer, GContext *ctx) {

  /* Background */
  graphics_context_set_fill_color(ctx, col_bg());
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  /* E-paper dither texture - subtle 2x2 grain */
  if (s.theme == 0) {
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_context_set_stroke_width(ctx, 1);
    for (int dy = 0; dy < 168; dy += 4) {
      for (int dx = (dy/4 % 2) * 2; dx < 144; dx += 4) {
        graphics_draw_pixel(ctx, GPoint(dx, dy));
      }
    }
  }

  /* Face - same as bg, no border */
  graphics_context_set_fill_color(ctx, col_face());
  graphics_fill_rect(ctx, GRect(FX, FY, FW, FH), CR, GCornersAll);

  /* 12 o'clock triangle (dark body + same tip) */
  graphics_context_set_stroke_color(ctx, col_hand_h());
  graphics_context_set_stroke_width(ctx, 1);
  for (int ty = 0; ty <= 6; ty++) {
    int xl = (CX-4) + ty/3;
    int xr = (CX+4) - ty/3;
    graphics_draw_line(ctx, GPoint(xl, FY+2+ty), GPoint(xr, FY+2+ty));
  }
  /* Red tip */
  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_draw_line(ctx, GPoint(CX-2, FY+8),  GPoint(CX+2, FY+8));
  graphics_draw_line(ctx, GPoint(CX-1, FY+9),  GPoint(CX+1, FY+9));
  graphics_draw_line(ctx, GPoint(CX,   FY+10), GPoint(CX,   FY+10));

  /* 3, 6, 9 dots */
  graphics_context_set_fill_color(ctx, col_mark());
  graphics_fill_circle(ctx, GPoint(72, 48), 2);
  graphics_fill_circle(ctx, GPoint(40, 88), 2);
  graphics_fill_circle(ctx, GPoint( 8, 48), 2);

  /* 1,5,7,11 corner lines */
  graphics_context_set_stroke_color(ctx, col_mark());
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(73,  7), GPoint(69, 12));
  graphics_draw_line(ctx, GPoint(73, 89), GPoint(69, 84));
  graphics_draw_line(ctx, GPoint( 7, 89), GPoint(11, 84));
  graphics_draw_line(ctx, GPoint( 7,  7), GPoint(11, 12));

  /* 2,4,8,10 tiny dots */
  graphics_context_set_fill_color(ctx, col_mark());
  graphics_fill_circle(ctx, GPoint(73, 29), 1);
  graphics_fill_circle(ctx, GPoint(73, 67), 1);
  graphics_fill_circle(ctx, GPoint( 7, 67), 1);
  graphics_fill_circle(ctx, GPoint( 7, 29), 1);

  /* Hands */
  int ha = (s_hours % 12) * 30 + s_minutes / 2;
  int ma = s_minutes * 6 + s_seconds / 10;
  int sa = s_seconds * 6;

  graphics_context_set_stroke_color(ctx, col_hand_h());
  graphics_context_set_stroke_width(ctx, 4);
  graphics_draw_line(ctx, hand_pt(ha+180, FW/16, FH/16), hand_pt(ha, FW/4, FH/4));

  graphics_context_set_stroke_color(ctx, col_hand_m());
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, hand_pt(ma+180, FW/14, FH/14), hand_pt(ma, FW*3/8, FH*3/8));

  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, hand_pt(sa+180, FW/8, FH/8), hand_pt(sa, FW*4/9, FH*4/9));

  graphics_context_set_fill_color(ctx, col_hand_h());
  graphics_fill_circle(ctx, GPoint(CX, CY), 2);
  graphics_context_set_fill_color(ctx, col_face());
  graphics_fill_circle(ctx, GPoint(CX, CY), 1);

  /* Battery icon always visible */
  draw_battery_icon(ctx);
  /* Health stats */
  if (s.health == 1) draw_health(ctx);
  /* Data viz — changes every 2 hours */
  draw_viz(ctx);

  /* ?? Calendar rows ?? */
  time_t now_t = time(NULL);
  struct tm *t  = localtime(&now_t);

  /* Month */
  static const char *monL[] = {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
  };
  draw_cal_row(ctx, monL, 12, t->tm_mon,
               88, Y_MON, 38, false);

  /* Date row — now in middle */
  static const char *dateL[] = {
    "01","02","03","04","05","06","07","08","09","10",
    "11","12","13","14","15","16","17","18","19","20",
    "21","22","23","24","25","26","27","28","29","30","31"
  };
  int dim = 31;
  draw_cal_row(ctx, dateL, dim, t->tm_mday - 1,
               76, Y_DOW, 22, true);

  /* Day of week — now at bottom */
  static const char *dowL[] = {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
  };
  draw_cal_row(ctx, dowL, 7, t->tm_wday,
               58, Y_DATE, 40, false);

  /* Step scale */
  if (s.steps == 1) draw_step_scale(ctx);
}


/* ?? Health data ?? */
static void update_health(void) {
#if defined(PBL_HEALTH)
  HealthMetric m = HealthMetricStepCount;
  if (health_service_metric_accessible(m, time_start_of_today(), time(NULL))
      & HealthServiceAccessibilityMaskAvailable) {
    int v = (int)health_service_sum_today(m);
    if (v >= 0 && v < 100000) s_steps = v;
  }
  if (health_service_metric_accessible(HealthMetricHeartRateBPM,
      time_start_of_today(), time(NULL)) & HealthServiceAccessibilityMaskAvailable)
    s_hr = (int)health_service_sum_today(HealthMetricHeartRateBPM);
  if (health_service_metric_accessible(HealthMetricRestingKCalories,
      time_start_of_today(), time(NULL)) & HealthServiceAccessibilityMaskAvailable)
    s_cal = (int)(health_service_sum_today(HealthMetricRestingKCalories)
                + health_service_sum_today(HealthMetricActiveKCalories));
  if (health_service_metric_accessible(HealthMetricSleepSeconds,
      time_start_of_today(), time(NULL)) & HealthServiceAccessibilityMaskAvailable)
    s_sleep = (int)health_service_sum_today(HealthMetricSleepSeconds) / 60;
#endif
  BatteryChargeState b = battery_state_service_peek();
  s_batt = b.charge_percent;
}

/* ?? Tick ?? */
static void update_time(struct tm *t) {
  s_hours   = t->tm_hour;
  s_minutes = t->tm_min;
  s_seconds = t->tm_sec;
  if (s_seconds == 0) update_health();
#if defined(PBL_HEALTH)
  HealthMetric m = HealthMetricStepCount;
  if (health_service_metric_accessible(m, time_start_of_today(), time(NULL))
      & HealthServiceAccessibilityMaskAvailable) {
    int v = (int)health_service_sum_today(m);
    if (v >= 0 && v < 100000) s_steps = v;
  }
#endif
  layer_mark_dirty(s_canvas);
}
static void tick_handler(struct tm *t, TimeUnits u) { update_time(t); }

/* ?? AppMessage ?? */
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;
  t = dict_find(iter, KEY_HEALTH); if (t) s.health = (uint8_t)(t->value->int32 & 1);
  t = dict_find(iter, KEY_STEPS);  if (t) s.steps  = (uint8_t)(t->value->int32 & 1);
  t = dict_find(iter, KEY_THEME);  if (t) s.theme  = (uint8_t)(t->value->int32 & 1);
  t = dict_find(iter, KEY_BOLD);   if (t) s.bold   = (uint8_t)(t->value->int32 & 1);
  t = dict_find(iter, KEY_EPAPER); if (t) s.epaper = (uint8_t)(t->value->int32 & 1);
  t = dict_find(iter, KEY_COLOR);  if (t) s.color  = (uint8_t)(t->value->int32 % 3);
  settings_save();
  layer_mark_dirty(s_canvas);
}

/* ?? Window ?? */
static void window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);
  update_health();
  time_t now = time(NULL);
  update_time(localtime(&now));
}
static void window_unload(Window *w) {
  layer_destroy(s_canvas);
}

/* ?? Init ?? */
static void init(void) {
  settings_load();
  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 64);
  s_win = window_create();
  window_set_background_color(s_win, col_bg());
  window_set_window_handlers(s_win, (WindowHandlers){
    .load=window_load, .unload=window_unload
  });
  window_stack_push(s_win, true);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}
static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_win);
}
int main(void) { init(); app_event_loop(); deinit(); }
