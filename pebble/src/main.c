#include <pebble.h>

static const SmartstrapServiceId SERVICE_ID = 0x1001;
static const SmartstrapAttributeId CENTER_INPUT_ATTRIBUTE_ID = 0x0001;
static const SmartstrapAttributeId CENTER_OUTPUT_ATTRIBUTE_ID = 0x0002;

static const size_t CENTER_INPUT_ATTRIBUTE_LENGTH = 1;
static const size_t CENTER_OUTPUT_ATTRIBUTE_LENGTH = 1;

static SmartstrapAttribute *center_input_attribute;
static SmartstrapAttribute *center_output_attribute;

static long s_last_time = 0;
static int16_t s_accel_x;
static int16_t s_accel_y;
static int16_t s_accel_z;
static int s_button_presses;
static bool s_service_available;

static Window *s_main_window;
static TextLayer *s_output_layer;

/******************************** Smartstraps *********************************/

static void strap_availability_handler(SmartstrapServiceId service_id, bool is_available) {
  // A service's availability has changed
  APP_LOG(APP_LOG_LEVEL_INFO, "Service %d is %s available", (int)service_id, is_available ? "now" : "NOT");

  // Remember if the raw service is available
  s_service_available = (is_available && service_id == SERVICE_ID);
}

/********************************** Input *************************************/
static void strap_notify_handler(SmartstrapAttribute *attribute) {
  if (attribute != center_input_attribute) {
    return;
  }
  smartstrap_attribute_read(center_input_attribute);
  vibes_short_pulse();
}

static void strap_did_read(SmartstrapAttribute *attr, SmartstrapResult result,
                         const uint8_t *data, size_t length) {
  if (attr != center_input_attribute) {
    return;
  }
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Read failed with result %d", result);
    return;
  }
  if (length != CENTER_INPUT_ATTRIBUTE_LENGTH) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Got response of unexpected length (%d)", length);
    return;
  }

  s_button_presses++;
}

/********************************** Output ************************************/

static void prv_set_center_attribute(uint8_t value) {
  SmartstrapResult result;
  uint8_t *buffer;
  size_t length;
  result = smartstrap_attribute_begin_write(center_output_attribute, &buffer, &length);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Begin write failed with error %d", result);
    return;
  }

  buffer[0] = value;

  result = smartstrap_attribute_end_write(center_output_attribute, 1, false);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "End write failed with error %d", result);
    return;
  }
}

/******************************* Accel Handling *******************************/
static void data_handler(AccelData *data, uint32_t num_samples) {
  if (!data[0].did_vibrate) {
    s_accel_y = (255 * (abs(data[0].y) / 1000.0));
    prv_set_center_attribute(s_accel_y);

    // s_accel_x = data[0].x;
    // s_accel_y = data[0].y;
    // s_accel_z = data[0].z;

    // Long lived buffer
    static char s_buffer[128];
    snprintf(s_buffer, sizeof(s_buffer), "Y %d", s_accel_y);
    text_layer_set_text(s_output_layer, s_buffer);
  }
}

/****************************** Button Handling *******************************/

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  //prv_set_center_attribute(true);
  accel_data_service_subscribe(1, data_handler);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  //prv_set_center_attribute(false);
  accel_data_service_unsubscribe();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

/************************************* UI *************************************/

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  // text layer for connection status
  GRect bounds = layer_get_bounds(window_layer);
  s_output_layer = text_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  text_layer_set_font(s_output_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text_alignment(s_output_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_output_layer));
}

static void main_window_unload(Window *window) {
  window_destroy(s_main_window);
}

/************************************ App *************************************/

static void init() {
  s_main_window = window_create();
  window_set_click_config_provider(s_main_window, click_config_provider);
  window_set_background_color(s_main_window, GColorGreen);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);

  SmartstrapHandlers handlers = (SmartstrapHandlers) {
    .availability_did_change = strap_availability_handler,
    .did_read = strap_did_read,
    .notified = strap_notify_handler
  };
  smartstrap_subscribe(handlers);
  center_input_attribute = smartstrap_attribute_create(SERVICE_ID, CENTER_INPUT_ATTRIBUTE_ID, CENTER_INPUT_ATTRIBUTE_LENGTH);
  center_output_attribute = smartstrap_attribute_create(SERVICE_ID, CENTER_OUTPUT_ATTRIBUTE_ID, CENTER_OUTPUT_ATTRIBUTE_LENGTH);
}

static void deinit() {
  smartstrap_attribute_destroy(center_input_attribute);
  smartstrap_attribute_destroy(center_output_attribute);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
