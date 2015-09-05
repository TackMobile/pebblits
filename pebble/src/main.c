#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_output_layer;

static const SmartstrapServiceId SERVICE_ID = 0x1001;
static const SmartstrapAttributeId CENTER_INPUT_ATTRIBUTE_ID = 0x0001;
static const SmartstrapAttributeId CENTER_OUTPUT_ATTRIBUTE_ID = 0x0002;

static const size_t CENTER_INPUT_ATTRIBUTE_LENGTH = 1;
static const size_t CENTER_OUTPUT_ATTRIBUTE_LENGTH = 1;

static SmartstrapAttribute *center_input_attribute;
static SmartstrapAttribute *center_output_attribute;

static int s_button_presses;
static bool s_service_available;

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

static void prv_set_center_attribute(bool on) {
  SmartstrapResult result;
  uint8_t *buffer;
  size_t length;
  result = smartstrap_attribute_begin_write(center_output_attribute, &buffer, &length);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Begin write failed with error %d", result);
    return;
  }

  buffer[0] = on;

  result = smartstrap_attribute_end_write(center_output_attribute, 1, false);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "End write failed with error %d", result);
    return;
  }
}

/****************************** Button Handling *******************************/

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_set_center_attribute(true);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_set_center_attribute(false);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

/************************************* UI *************************************/

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
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
  window_destroy(s_main_window);
  smartstrap_attribute_destroy(center_input_attribute);
  smartstrap_attribute_destroy(center_output_attribute);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
