#include <pebble.h>

static Window *s_window;
static TextLayer *s_text_layer;
static GRect s_text_bounds;
static int s_x, s_y;
static int s_dx = 2, s_dy = 2;
static AppTimer *s_animation_timer;
static time_t s_activation_time;
static int s_transition_frame;
static bool s_in_transition;

#define ANIMATION_INTERVAL_MS 50
#define IDLE_START_SEC 5
#define TRANSITION_DURATION_SEC 2
#define TRANSITION_FRAMES ((TRANSITION_DURATION_SEC * 1000) / ANIMATION_INTERVAL_MS)

static void update_position() {
  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(window_layer);

  // Update position
  s_x += s_dx;
  s_y += s_dy;

  // Bounce off edges
  if (s_x <= 0 || s_x + s_text_bounds.size.w >= bounds.size.w) {
    s_dx = -s_dx;
    s_x = (s_x <= 0) ? 0 : bounds.size.w - s_text_bounds.size.w;
  }

  if (s_y <= 0 || s_y + s_text_bounds.size.h >= bounds.size.h) {
    s_dy = -s_dy;
    s_y = (s_y <= 0) ? 0 : bounds.size.h - s_text_bounds.size.h;
  }

  // Update text layer position
  layer_set_frame(text_layer_get_layer(s_text_layer),
                  GRect(s_x, s_y, s_text_bounds.size.w, s_text_bounds.size.h));
}

static void animation_timer_callback(void *data) {
  time_t now = time(NULL);
  int elapsed_sec = (int)difftime(now, s_activation_time);

  if (!s_in_transition && elapsed_sec >= IDLE_START_SEC) {
    // Just entered transition
    s_in_transition = true;
    s_transition_frame = 0;
  }

  if (!s_in_transition) {
    // Active mode: full speed
    update_position();
    s_animation_timer = app_timer_register(ANIMATION_INTERVAL_MS, animation_timer_callback, NULL);
  } else if (s_transition_frame < TRANSITION_FRAMES) {
    // Transition mode: slow down movement, keep frame rate constant
    float progress = (float)s_transition_frame / TRANSITION_FRAMES;
    // Reduce movement speed using inverse quadratic (fast slowdown at start, gradual at end)
    float speed_multiplier = (1.0f - progress) * (1.0f - progress);

    // Only update position if speed is significant
    if (speed_multiplier > 0.01f) {
      // Scale velocity and remember original
      int original_dx = s_dx;
      int original_dy = s_dy;
      int scaled_dx = (int)(s_dx * speed_multiplier);
      int scaled_dy = (int)(s_dy * speed_multiplier);

      s_dx = scaled_dx;
      s_dy = scaled_dy;

      // Update with scaled velocity (may reverse direction on bounce)
      if (s_dx != 0 || s_dy != 0) {
        update_position();
      }

      // Restore velocity, preserving any bounce reversals
      bool dx_reversed = (scaled_dx * s_dx < 0);
      bool dy_reversed = (scaled_dy * s_dy < 0);

      s_dx = dx_reversed ? -original_dx : original_dx;
      s_dy = dy_reversed ? -original_dy : original_dy;
    }

    s_transition_frame++;
    s_animation_timer = app_timer_register(ANIMATION_INTERVAL_MS, animation_timer_callback, NULL);
  } else {
    // Animation stopped - set timer to NULL so it can be restarted
    s_animation_timer = NULL;
    s_in_transition = false;
  }
}

static void compute_text_bounds() {
  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(window_layer);
  GFont font = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
  const char *text = text_layer_get_text(s_text_layer);
  GSize text_size = graphics_text_layout_get_content_size(text, font, bounds,
                                                          GTextOverflowModeTrailingEllipsis,
                                                          GTextAlignmentLeft);
  s_text_bounds = GRect(0, 0, text_size.w, text_size.h);
}

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  static char buffer[8];
  strftime(buffer, sizeof(buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_text_layer, buffer);
  compute_text_bounds();
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void activate_animation() {
  // Reset activation time and transition state
  s_activation_time = time(NULL);
  s_in_transition = false;
  s_transition_frame = 0;

  // Cancel existing timer if running
  if (s_animation_timer) {
    app_timer_cancel(s_animation_timer);
  }

  // Start animation at full speed
  s_animation_timer = app_timer_register(ANIMATION_INTERVAL_MS, animation_timer_callback, NULL);
}

static void focus_handler(bool in_focus) {
  if (in_focus) {
    activate_animation();
  }
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  activate_animation();
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Set window background to black
  window_set_background_color(window, GColorBlack);

  // Create text layer
  s_text_layer = text_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  text_layer_set_background_color(s_text_layer, GColorClear);
  text_layer_set_text_color(s_text_layer, GColorWhite);
  text_layer_set_font(s_text_layer, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));

  // Display initial time and compute text size
  update_time();

  // Initialize position at center
  s_x = (bounds.size.w - s_text_bounds.size.w) / 2;
  s_y = (bounds.size.h - s_text_bounds.size.h) / 2;

  layer_add_child(window_layer, text_layer_get_layer(s_text_layer));

  // Initialize activation time and start animation
  s_activation_time = time(NULL);
  s_in_transition = false;
  s_transition_frame = 0;
  s_animation_timer = app_timer_register(ANIMATION_INTERVAL_MS, animation_timer_callback, NULL);
}

static void window_unload(Window *window) {
  if (s_animation_timer) {
    app_timer_cancel(s_animation_timer);
  }
  text_layer_destroy(s_text_layer);
}

static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  // Subscribe to time updates
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Subscribe to app focus events
  app_focus_service_subscribe(focus_handler);

  // Subscribe to accelerometer tap events (wrist gestures)
  accel_tap_service_subscribe(tap_handler);

  window_stack_push(s_window, true);
}

static void deinit(void) {
  accel_tap_service_unsubscribe();
  app_focus_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
