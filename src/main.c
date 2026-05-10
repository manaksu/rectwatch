/*
 * RectWatch — Pebble Time Steel (basalt) only
 * No runtime math — all geometry pre-calculated at compile time.
 *
 * Key 0: A_STEPS  0=hide  1=show step scale
 * Key 1: B_THEME  0=light 1=dark
 */

#include <pebble.h>

#define SETTINGS_KEY  1
#define KEY_STEPS     0
#define KEY_THEME     1

/* ── Face ── */
#define FX   4
#define FY   4
#define FW  72
#define FH  88
#define CX  40
#define CY  48

/* ── Step scale ── */
#define SC_X1     2
#define SC_X2   142
#define SC_SW   140
#define SC_Y    158
#define STEPS_GOAL 10000

/* ── Settings ── */
typedef struct { uint8_t steps; uint8_t theme; } Settings;
static Settings s = { .steps=0, .theme=0 };
static void settings_load(void) {
  if (persist_exists(SETTINGS_KEY))
    persist_read_data(SETTINGS_KEY, &s, sizeof(s));
  if (s.theme > 1) s.theme = 0;
  if (s.steps > 1) s.steps = 0;
}
static void settings_save(void) { persist_write_data(SETTINGS_KEY, &s, sizeof(s)); }

/* ── Colours ── */
static GColor col_face(void)   { return s.theme ? GColorBlack    : GColorWhite;     }
static GColor col_border(void) { return s.theme ? GColorDarkGray : GColorLightGray; }
static GColor col_hands(void)  { return s.theme ? GColorWhite    : GColorBlack;     }
static GColor col_handm(void)  { return s.theme ? GColorLightGray: GColorDarkGray;  }
static GColor col_mark(void)   { return GColorDarkGray; }

/* ── State ── */
static Window *s_win;
static Layer  *s_canvas;
static int     s_hours, s_minutes, s_seconds, s_steps;

/* ── Hand point via Pebble integer trig ── */
static GPoint hand_pt(int angle_deg, int rx, int ry) {
  int32_t a = DEG_TO_TRIGANGLE(angle_deg - 90);
  int x = CX + (int32_t)(cos_lookup(a) * rx / TRIG_MAX_RATIO);
  int y = CY + (int32_t)(sin_lookup(a) * ry / TRIG_MAX_RATIO);
  return GPoint(x, y);
}

/* ── Step scale ── */
static void draw_step_scale(GContext *ctx) {
  int pct = (s_steps >= STEPS_GOAL) ? 100 : (s_steps * 100 / STEPS_GOAL);

  graphics_context_set_stroke_color(ctx, col_mark());
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(SC_X1, SC_Y), GPoint(SC_X2, SC_Y));

  for (int i = 0; i <= 12; i++) {
    int x  = SC_X1 + i * SC_SW / 12;
    int tl = (i % 3 == 0) ? 5 : 3;
    graphics_draw_line(ctx, GPoint(x, SC_Y-tl), GPoint(x, SC_Y));
    if (i % 3 == 0) {
      char lbl[5];
      if (i == 0) snprintf(lbl, sizeof(lbl), "0");
      else        snprintf(lbl, sizeof(lbl), "%dk", i);
      graphics_context_set_text_color(ctx, col_mark());
      graphics_draw_text(ctx, lbl,
        fonts_get_system_font(FONT_KEY_GOTHIC_09),
        GRect(x-7, SC_Y-14, 14, 10),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
  }

  /* Needle */
  int nx = SC_X1 + pct * SC_SW / 100;
  graphics_context_set_stroke_color(ctx, GColorRed);
  for (int ty = 0; ty <= 10; ty++) {
    int half = (10-ty) * 3 / 10;
    graphics_draw_line(ctx,
      GPoint(nx-half, SC_Y-10+ty), GPoint(nx+half, SC_Y-10+ty));
  }

  char buf[12];
  snprintf(buf, sizeof(buf), "%d", s_steps);
  graphics_context_set_text_color(ctx, GColorRed);
  graphics_draw_text(ctx, buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_09),
    GRect(SC_X2-36, SC_Y+2, 38, 10),
    GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
}

/* ── Canvas ── */
static void canvas_update(Layer *layer, GContext *ctx) {

  /* Screen bg */
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  /* Face */
  graphics_context_set_fill_color(ctx, col_face());
  graphics_fill_rect(ctx, GRect(FX, FY, FW, FH), 10, GCornersAll);
  graphics_context_set_stroke_color(ctx, col_border());
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, GRect(FX, FY, FW, FH), 10);

  /* ── 12 o'clock — inverted triangle ── */
  /* Dark body: y=6..12, width tapers from 8 to 4 */
  graphics_context_set_stroke_color(ctx, col_hands());
  graphics_context_set_stroke_width(ctx, 1);
  for (int ty = 0; ty <= 6; ty++) {
    int xl = (CX-4) + ty/3;
    int xr = (CX+4) - ty/3;
    graphics_draw_line(ctx, GPoint(xl, FY+2+ty), GPoint(xr, FY+2+ty));
  }
  /* Red tip: y=8..11 */
  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_draw_line(ctx, GPoint(CX-2, FY+8),  GPoint(CX+2, FY+8));
  graphics_draw_line(ctx, GPoint(CX-1, FY+9),  GPoint(CX+1, FY+9));
  graphics_draw_line(ctx, GPoint(CX,   FY+10), GPoint(CX,   FY+10));

  /* ── 3, 6, 9 dots (pre-calculated) ── */
  graphics_context_set_fill_color(ctx, col_mark());
  graphics_fill_circle(ctx, GPoint(72, 48), 2);  /* 3 */
  graphics_fill_circle(ctx, GPoint(40, 88), 2);  /* 6 */
  graphics_fill_circle(ctx, GPoint( 8, 48), 2);  /* 9 */

  /* ── 1,5,7,11 corner lines (pre-calculated) ── */
  graphics_context_set_stroke_color(ctx, col_mark());
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(73, 7),  GPoint(69, 12)); /* 1  */
  graphics_draw_line(ctx, GPoint(73, 89), GPoint(69, 84)); /* 5  */
  graphics_draw_line(ctx, GPoint( 7, 89), GPoint(11, 84)); /* 7  */
  graphics_draw_line(ctx, GPoint( 7, 7),  GPoint(11, 12)); /* 11 */

  /* ── 2,4,8,10 tiny dots (pre-calculated) ── */
  graphics_context_set_fill_color(ctx, col_border());
  graphics_fill_circle(ctx, GPoint(73, 29), 1); /* 2  */
  graphics_fill_circle(ctx, GPoint(73, 67), 1); /* 4  */
  graphics_fill_circle(ctx, GPoint( 7, 67), 1); /* 8  */
  graphics_fill_circle(ctx, GPoint( 7, 29), 1); /* 10 */

  /* ── Hands ── */
  int ha = (s_hours % 12) * 30 + s_minutes / 2;
  int ma = s_minutes * 6 + s_seconds / 10;
  int sa = s_seconds * 6;

  graphics_context_set_stroke_color(ctx, col_hands());
  graphics_context_set_stroke_width(ctx, 4);
  graphics_draw_line(ctx, hand_pt(ha+180, FW/16, FH/16), hand_pt(ha, FW/4, FH/4));

  graphics_context_set_stroke_color(ctx, col_handm());
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, hand_pt(ma+180, FW/14, FH/14), hand_pt(ma, FW*3/8, FH*3/8));

  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, hand_pt(sa+180, FW/8, FH/8), hand_pt(sa, FW*4/9, FH*4/9));

  /* Centre */
  graphics_context_set_fill_color(ctx, col_hands());
  graphics_fill_circle(ctx, GPoint(CX, CY), 2);
  graphics_context_set_fill_color(ctx, col_face());
  graphics_fill_circle(ctx, GPoint(CX, CY), 1);

  /* Step scale */
  if (s.steps == 1) draw_step_scale(ctx);
}

/* ── Tick ── */
static void update_time(struct tm *t) {
  s_hours   = t->tm_hour;
  s_minutes = t->tm_min;
  s_seconds = t->tm_sec;
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
  t = dict_find(iter, KEY_STEPS); if (t) s.steps = (uint8_t)(t->value->int32 & 1);
  t = dict_find(iter, KEY_THEME); if (t) s.theme = (uint8_t)(t->value->int32 & 1);
  settings_save();
  layer_mark_dirty(s_canvas);
}

/* ── Window ── */
static void window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);
  time_t now = time(NULL);
  update_time(localtime(&now));
}
static void window_unload(Window *w) { layer_destroy(s_canvas); }

/* ── Init ── */
static void init(void) {
  settings_load();
  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 64);
  s_win = window_create();
  window_set_background_color(s_win, GColorBlack);
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
