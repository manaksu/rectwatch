/*
 * RectWatch — Pebble Time Steel (basalt) only
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

/* ── appKeys (alphabetical):
 *   A_STEPS  0  0=hide  1=show step scale
 *   B_THEME  1  0=cream  1=dark
 */

/* ── Geometry ── */
#define FX   4
#define FY   4
#define FW  72
#define FH  88
#define CX  40
#define CY  48
#define CR  10

/* Calendar rows */
#define Y_MON   96
#define Y_DOW  112
#define Y_DATE 128
#define TRI_H    3   /* triangle height px */
#define TRI_GAP  2   /* gap between triangle and label */
#define ROW_H   16   /* total row height */

/* Step scale */
#define SC_X1    2
#define SC_X2  142
#define SC_SW  140
#define SC_Y   158
#define STEPS_GOAL 10000

/* ── Settings ── */
typedef struct { uint8_t health; uint8_t steps; uint8_t theme; } Settings;
static Settings s = { .health=1, .steps=1, .theme=0 };
static void settings_load(void) {
  if (persist_exists(SETTINGS_KEY))
    persist_read_data(SETTINGS_KEY, &s, sizeof(s));
  if (s.health > 1) s.health = 1;
  if (s.steps  > 1) s.steps  = 1;
  if (s.theme  > 1) s.theme  = 0;
}
static void settings_save(void) { persist_write_data(SETTINGS_KEY, &s, sizeof(s)); }

/* ── Colours ── */
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
static GColor col_cal_act(void) { return s.theme ? D_CAL_ACT : C_CAL_ACT; }
static GColor col_cal_dim(void) { return s.theme ? D_CAL_DIM : C_CAL_DIM; }
static GColor col_scale(void)   { return s.theme ? D_SCALE   : C_SCALE;   }
static GColor col_needle(void)  { return s.theme ? D_NEEDLE  : C_NEEDLE;  }

/* ── State ── */
static Window   *s_win;
static Layer    *s_canvas;
static int       s_hours, s_minutes, s_seconds, s_steps;

/* ── Hand point ── */
static GPoint hand_pt(int angle_deg, int rx, int ry) {
  int32_t a = DEG_TO_TRIGANGLE(angle_deg - 90);
  return GPoint(
    CX + (int32_t)(cos_lookup(a) * rx / TRIG_MAX_RATIO),
    CY + (int32_t)(sin_lookup(a) * ry / TRIG_MAX_RATIO));
}

/* ── Short thick downward triangle ── */
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


/* ── Calendar row ── */
/* Show current label centred at anchorX + neighbours left/right */
/* itemW = spacing between items in this row */
static void draw_cal_row(GContext *ctx, const char **items, int n,
                          int current, int anchorX, int rowTopY,
                          int itemW, bool is_date) {
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_09);
  int labelY  = rowTopY + TRI_H + TRI_GAP;

  /* Draw current ± 8 neighbours — bleed off edges */
  for (int off = -8; off <= 8; off++) {
    int idx = ((current + off) % n + n) % n;
    int x   = anchorX + off * itemW;
    if (x < -30 || x > 144 + 30) continue;

    bool isCur = (off == 0);
    int  dist  = off < 0 ? -off : off;

    /* Fade by distance */
    GColor col;
    if      (isCur)   col = is_date ? col_mark_d() : col_cal_act();
    else if (dist==1) col = col_cal_dim();
    else if (dist==2) col = col_mark();
    else              col = col_mark(); /* still visible, fades naturally off screen */

    char ubuf[8];
    int ui=0; while(items[idx][ui]&&ui<7){ubuf[ui]=(items[idx][ui]>='a'&&items[idx][ui]<='z')?items[idx][ui]-32:items[idx][ui];ui++;} ubuf[ui]='\0';
    graphics_context_set_text_color(ctx, col);
    graphics_draw_text(ctx, ubuf, font,
      GRect(x - itemW/2, labelY, itemW, 10),
      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }

  /* Triangle above current, apex pointing down */
  draw_tri_down(ctx, anchorX, rowTopY + TRI_H);
}


/* ── Battery icon ── */
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

/* ── Health stats ── */
static void draw_health(GContext *ctx) {
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_09);
  int px=80, py=10, lh=24;
  char buf[12];
  /* Heart Rate */
  graphics_context_set_text_color(ctx, col_mark());
  graphics_draw_text(ctx, "HEART RATE", font, GRect(px,py,62,10),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  snprintf(buf, sizeof(buf), "%d", s_hr);
  graphics_context_set_text_color(ctx, col_mark_d());
  graphics_draw_text(ctx, buf, font, GRect(px,py+9,62,10),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  /* Calories */
  py += lh;
  graphics_context_set_text_color(ctx, col_mark());
  graphics_draw_text(ctx, "CALORIES", font, GRect(px,py,62,10),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  snprintf(buf, sizeof(buf), "%d", s_cal);
  graphics_context_set_text_color(ctx, col_mark_d());
  graphics_draw_text(ctx, buf, font, GRect(px,py+9,62,10),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  /* Sleep */
  py += lh;
  graphics_context_set_text_color(ctx, col_mark());
  graphics_draw_text(ctx, "SLEEP", font, GRect(px,py,62,10),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  snprintf(buf, sizeof(buf), "%dH%02dM", s_sleep/60, s_sleep%60);
  graphics_context_set_text_color(ctx, col_mark_d());
  graphics_draw_text(ctx, buf, font, GRect(px,py+9,62,10),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

/* ── Step scale ── */
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
        fonts_get_system_font(FONT_KEY_GOTHIC_09),
        GRect(x-7, SC_Y-14, 14, 10),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
  }

  /* Needle — pointing down to baseline */
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
    fonts_get_system_font(FONT_KEY_GOTHIC_09),
    GRect(SC_X2 - 36, SC_Y + 2, 38, 10),
    GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
}

/* ── Canvas ── */
static void canvas_update(Layer *layer, GContext *ctx) {

  /* Background */
  graphics_context_set_fill_color(ctx, col_bg());
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  /* E-paper dither texture — subtle 2x2 grain */
  if (s.theme == 0) {
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_context_set_stroke_width(ctx, 1);
    for (int dy = 0; dy < 168; dy += 4) {
      for (int dx = (dy/4 % 2) * 2; dx < 144; dx += 4) {
        graphics_draw_pixel(ctx, GPoint(dx, dy));
      }
    }
  }

  /* Face — same as bg, no border */
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

  /* ── Calendar rows ── */
  time_t now_t = time(NULL);
  struct tm *t  = localtime(&now_t);

  /* Month */
  static const char *monL[] = {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
  };
  draw_cal_row(ctx, monL, 12, t->tm_mon,
               88, Y_MON, 22, false);

  /* Day of week */
  static const char *dowL[] = {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
  };
  draw_cal_row(ctx, dowL, 7, t->tm_wday,
               58, Y_DOW, 26, false);

  /* Date */
  static const char *dateL[] = {
    "01","02","03","04","05","06","07","08","09","10",
    "11","12","13","14","15","16","17","18","19","20",
    "21","22","23","24","25","26","27","28","29","30","31"
  };
  int dim = 31; /* use all 31, wrap naturally */
  draw_cal_row(ctx, dateL, dim, t->tm_mday - 1,
               76, Y_DATE, 19, true);

  /* Step scale */
  if (s.steps == 1) draw_step_scale(ctx);
}


/* ── Health data ── */
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

/* ── Tick ── */
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

/* ── AppMessage ── */
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;
  t = dict_find(iter, KEY_HEALTH); if (t) s.health = (uint8_t)(t->value->int32 & 1);
  t = dict_find(iter, KEY_STEPS);  if (t) s.steps  = (uint8_t)(t->value->int32 & 1);
  t = dict_find(iter, KEY_THEME);  if (t) s.theme  = (uint8_t)(t->value->int32 & 1);
  settings_save();
  layer_mark_dirty(s_canvas);
}

/* ── Window ── */
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

/* ── Init ── */
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
