/*
 * RectWatch — Pebble Time Steel (basalt) only
 * Pure drawn analog watchface — rectangular clock face top-left (4,4), 72x88, CR=10
 *
 * Markings:
 *   12       — inverted triangle, dark body + red tip
 *   3, 6, 9  — filled circle dots
 *   1,5,7,11 — short line from corner arc inward
 *   2,4,8,10 — tiny dots
 *   Red seconds, dark hour, grey minute
 *
 * Settings (appKeys alphabetical -> index):
 *   Key 0: A_STEPS  0=hide  1=show step scale
 *   Key 1: B_THEME  0=light 1=dark
 */

#include <pebble.h>

#define SETTINGS_KEY  1

/* ── Face geometry ── */
#define FX   4
#define FY   4
#define FW  72
#define FH  88
#define CX  (FX + FW/2)
#define CY  (FY + FH/2)
#define CR  10

/* ── Step scale geometry ── */
#define SC_X1   2
#define SC_X2  142
#define SC_SW  (SC_X2 - SC_X1)
#define SC_Y   158
#define STEPS_GOAL  10000

/* ── AppMessage keys ── */
#define KEY_STEPS  0   /* A_STEPS */
#define KEY_THEME  1   /* B_THEME */

/* ── Settings ── */
typedef struct { uint8_t steps; uint8_t theme; } Settings;
static Settings s = { .steps = 0, .theme = 0 };
static void settings_load(void) { if (persist_exists(SETTINGS_KEY)) persist_read_data(SETTINGS_KEY, &s, sizeof(s)); }
static void settings_save(void) { persist_write_data(SETTINGS_KEY, &s, sizeof(s)); }

/* ── Health ── */
static int s_steps = 0;

/* ── Colours ── */
#define LIGHT_FACE    GColorWhite
#define LIGHT_BORDER  GColorLightGray
#define LIGHT_HANDS   GColorBlack
#define LIGHT_HANDM   GColorDarkGray
#define LIGHT_MARK    GColorDarkGray

#define DARK_FACE     GColorBlack
#define DARK_BORDER   GColorDarkGray
#define DARK_HANDS    GColorWhite
#define DARK_HANDM    GColorLightGray
#define DARK_MARK     GColorDarkGray

#define COL_SEC       GColorRed
#define COL_TRI_RED   GColorRed

static GColor col_face(void)   { return s.theme ? DARK_FACE   : LIGHT_FACE;   }
static GColor col_border(void) { return s.theme ? DARK_BORDER : LIGHT_BORDER; }
static GColor col_hands(void)  { return s.theme ? DARK_HANDS  : LIGHT_HANDS;  }
static GColor col_handm(void)  { return s.theme ? DARK_HANDM  : LIGHT_HANDM;  }
static GColor col_mark(void)   { return s.theme ? DARK_MARK   : LIGHT_MARK;   }

/* ── Time ── */
static Window *s_win;
static Layer  *s_canvas;
static int     s_hours, s_minutes, s_seconds;

/* ── Geometry ── */
static GPoint border_point(int angle_deg) {
  float a  = ((float)(angle_deg - 90)) * 3.14159f / 180.0f;
  float dx = cosf(a), dy = sinf(a);
  float hw = FW / 2.0f, hh = FH / 2.0f;
  float t;
  if      (fabsf(dx) < 0.001f) t = hh / fabsf(dy);
  else if (fabsf(dy) < 0.001f) t = hw / fabsf(dx);
  else                          t = fminf(hw / fabsf(dx), hh / fabsf(dy));
  return GPoint((int)(CX + dx*t + 0.5f), (int)(CY + dy*t + 0.5f));
}

static GPoint corner_point(GPoint cc, int arc_angle_deg) {
  float a = ((float)(arc_angle_deg - 90)) * 3.14159f / 180.0f;
  return GPoint((int)(cc.x + CR * cosf(a) + 0.5f),
                (int)(cc.y + CR * sinf(a) + 0.5f));
}

static GPoint toward_centre(GPoint p, int len) {
  float dx = CX - p.x, dy = CY - p.y;
  float d  = sqrtf(dx*dx + dy*dy);
  if (d < 0.001f) return p;
  return GPoint((int)(p.x + dx/d*len + 0.5f),
                (int)(p.y + dy/d*len + 0.5f));
}

static GPoint hand_tip(int angle_deg, int rx, int ry) {
  float a = ((float)(angle_deg - 90)) * 3.14159f / 180.0f;
  return GPoint((int)(CX + rx * cosf(a) + 0.5f),
                (int)(CY + ry * sinf(a) + 0.5f));
}

/* ── Step scale ── */
static void draw_step_scale(GContext *ctx) {
  int pct = (s_steps * 100) / STEPS_GOAL;
  if (pct > 100) pct = 100;

  /* Scale baseline */
  graphics_context_set_stroke_color(ctx, col_mark());
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(SC_X1, SC_Y), GPoint(SC_X2, SC_Y));

  /* 12 tick marks */
  for (int i = 0; i <= 12; i++) {
    int x = SC_X1 + (i * SC_SW) / 12;
    bool major = (i % 3 == 0);
    int tl = major ? 5 : 3;
    graphics_draw_line(ctx, GPoint(x, SC_Y - tl), GPoint(x, SC_Y));

    if (major) {
      char lbl[4];
      if (i == 0) snprintf(lbl, sizeof(lbl), "0");
      else        snprintf(lbl, sizeof(lbl), "%dk", i);
      graphics_context_set_text_color(ctx, col_mark());
      graphics_draw_text(ctx, lbl,
        fonts_get_system_font(FONT_KEY_GOTHIC_09),
        GRect(x - 7, SC_Y - 14, 14, 10),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
  }

  /* Needle — filled triangle */
  int nx = SC_X1 + (pct * SC_SW) / 100;
  GPoint needle[3] = {
    GPoint(nx,     SC_Y - 10),
    GPoint(nx - 3, SC_Y),
    GPoint(nx + 3, SC_Y),
  };
  graphics_context_set_fill_color(ctx, GColorRed);
  graphics_fill_polygon(ctx, needle, 3);

  /* Step count — right aligned */
  char steps_str[12];
  snprintf(steps_str, sizeof(steps_str), "%d", s_steps);
  graphics_context_set_text_color(ctx, GColorRed);
  graphics_draw_text(ctx, steps_str,
    fonts_get_system_font(FONT_KEY_GOTHIC_09),
    GRect(SC_X2 - 36, SC_Y + 2, 38, 10),
    GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
}

/* ── Canvas ── */
static void canvas_update(Layer *layer, GContext *ctx) {

  /* Screen bg */
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  /* ── Face ── */
  graphics_context_set_fill_color(ctx, col_face());
  graphics_fill_rect(ctx, GRect(FX, FY, FW, FH), CR, GCornersAll);
  graphics_context_set_stroke_color(ctx, col_border());
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, GRect(FX, FY, FW, FH), CR);

  /* Corner centres */
  GPoint tl = GPoint(FX+CR,    FY+CR);
  GPoint tr = GPoint(FX+FW-CR, FY+CR);
  GPoint br = GPoint(FX+FW-CR, FY+FH-CR);
  GPoint bl = GPoint(FX+CR,    FY+FH-CR);

  /* ── 12 o'clock — inverted triangle ── */
  GPoint tri_tl  = GPoint(CX-4, FY+2);
  GPoint tri_tr  = GPoint(CX+4, FY+2);
  GPoint tri_tip = GPoint(CX,   FY+11);
  GPoint split_l = GPoint(CX-2, FY+8);
  GPoint split_r = GPoint(CX+2, FY+8);

  graphics_context_set_fill_color(ctx, col_hands());
  GPoint dark_pts[] = { tri_tl, tri_tr, split_r, split_l };
  graphics_fill_polygon(ctx, dark_pts, 4);

  graphics_context_set_fill_color(ctx, COL_TRI_RED);
  GPoint red_pts[] = { split_l, split_r, tri_tip };
  graphics_fill_polygon(ctx, red_pts, 3);

  /* ── 3, 6, 9 — filled dots ── */
  GPoint dots369[3] = {
    toward_centre(border_point(90),  4),
    toward_centre(border_point(180), 4),
    toward_centre(border_point(270), 4),
  };
  graphics_context_set_fill_color(ctx, col_mark());
  for (int i = 0; i < 3; i++) graphics_fill_circle(ctx, dots369[i], 2);

  /* ── 1,5,7,11 — corner arc lines ── */
  struct { GPoint cc; int arc_ang; } corners4[4] = {
    { tr, 45  },
    { br, 135 },
    { bl, 225 },
    { tl, 315 },
  };
  graphics_context_set_stroke_color(ctx, col_mark());
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 0; i < 4; i++) {
    GPoint outer = corner_point(corners4[i].cc, corners4[i].arc_ang);
    GPoint inner = toward_centre(outer, 7);
    graphics_draw_line(ctx, outer, inner);
  }

  /* ── 2,4,8,10 — tiny dots ── */
  int tiny_angles[] = { 60, 120, 240, 300 };
  graphics_context_set_fill_color(ctx, col_border());
  for (int i = 0; i < 4; i++) {
    GPoint dot = toward_centre(border_point(tiny_angles[i]), 4);
    graphics_fill_circle(ctx, dot, 1);
  }

  /* ── Hands ── */
  int hour_angle = (s_hours % 12) * 30 + s_minutes / 2;
  int min_angle  = s_minutes * 6 + s_seconds / 10;
  int sec_angle  = s_seconds * 6;

  GPoint h_tip  = hand_tip(hour_angle,     FW/4,   FH/4);
  GPoint h_tail = hand_tip(hour_angle+180, FW/16,  FH/16);
  graphics_context_set_stroke_color(ctx, col_hands());
  graphics_context_set_stroke_width(ctx, 4);
  graphics_draw_line(ctx, h_tail, h_tip);

  GPoint m_tip  = hand_tip(min_angle,     FW*3/8, FH*3/8);
  GPoint m_tail = hand_tip(min_angle+180, FW/14,  FH/14);
  graphics_context_set_stroke_color(ctx, col_handm());
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, m_tail, m_tip);

  GPoint s_tip  = hand_tip(sec_angle,     FW*4/9, FH*4/9);
  GPoint s_tail = hand_tip(sec_angle+180, FW/8,   FH/8);
  graphics_context_set_stroke_color(ctx, COL_SEC);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, s_tail, s_tip);

  graphics_context_set_fill_color(ctx, col_hands());
  graphics_fill_circle(ctx, GPoint(CX, CY), 2);
  graphics_context_set_fill_color(ctx, col_face());
  graphics_fill_circle(ctx, GPoint(CX, CY), 1);

  /* ── Step scale (optional) ── */
  if (s.steps == 1) draw_step_scale(ctx);
}

/* ── Tick ── */
static void update_time(struct tm *t) {
  s_hours   = t->tm_hour;
  s_minutes = t->tm_min;
  s_seconds = t->tm_sec;
#if defined(PBL_HEALTH)
  HealthMetric metric = HealthMetricStepCount;
  HealthServiceAccessibilityMask mask =
    health_service_metric_accessible(metric, time_start_of_today(), time(NULL));
  if (mask & HealthServiceAccessibilityMaskAvailable)
    s_steps = (int)health_service_sum_today(metric);
#endif
  layer_mark_dirty(s_canvas);
}

static void tick_handler(struct tm *t, TimeUnits u) { update_time(t); }

/* ── AppMessage ── */
static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *t;
  t = dict_find(iter, KEY_STEPS); if (t) s.steps = (uint8_t)t->value->int32;
  t = dict_find(iter, KEY_THEME); if (t) s.theme = (uint8_t)t->value->int32;
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
