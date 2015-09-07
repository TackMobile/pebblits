#include <pebble.h>

static const SmartstrapServiceId SERVICE_ID = 0x1001;
static const SmartstrapAttributeId TOP_INPUT_ATTRIBUTE_ID = 0x0001;
static const SmartstrapAttributeId TOP_OUTPUT_ATTRIBUTE_ID = 0x0002;
static const SmartstrapAttributeId CENTER_INPUT_ATTRIBUTE_ID = 0x0003;
static const SmartstrapAttributeId CENTER_OUTPUT_ATTRIBUTE_ID = 0x0004;
static const SmartstrapAttributeId BOTTOM_INPUT_ATTRIBUTE_ID = 0x0005;
static const SmartstrapAttributeId BOTTOM_OUTPUT_ATTRIBUTE_ID = 0x0006;

static const size_t ATTRIBUTE_LENGTH = 1;

static SmartstrapAttribute *top_input_attribute;
static SmartstrapAttribute *top_output_attribute;
static SmartstrapAttribute *center_input_attribute;
static SmartstrapAttribute *center_output_attribute;
static SmartstrapAttribute *bottom_input_attribute;
static SmartstrapAttribute *bottom_output_attribute;

SmartstrapAttribute *selected_input_attribute;
SmartstrapAttribute *selected_output_attribute;

static Window *s_main_window;
static TextLayer *s_output_layer;
static int mode = 0; // 0 is input, 1 is accel

/******************************** Smartstraps *********************************/

static void strap_availability_handler(SmartstrapServiceId service_id, bool is_available) {
  // A service's availability has changed
  APP_LOG(APP_LOG_LEVEL_INFO, "Service %d is %s available", (int)service_id, is_available ? "now" : "NOT");
}

/************************************* UI *************************************/

static void updateUIValue(uint8_t value) {
  static char s_buffer[128];
  snprintf(s_buffer, sizeof(s_buffer), "%d", value);
  text_layer_set_text(s_output_layer, s_buffer);
}

static void main_window_load(Window *window) {
  // text layer for connection status
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  int window_height = bounds.size.h;
  int text_height = 40;
  int y = (window_height / 2) - (text_height / 2);

  window_set_background_color(s_main_window, GColorRed);

  s_output_layer = text_layer_create(GRect(0, y, bounds.size.w, text_height));
  text_layer_set_font(s_output_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text_alignment(s_output_layer, GTextAlignmentCenter);
  text_layer_set_text_color(s_output_layer, GColorBlack);
  text_layer_set_background_color(s_output_layer, GColorWhite);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_output_layer));
}

static void main_window_unload(Window *window) {
  window_destroy(s_main_window);
}

/********************************** Output ************************************/

static void prv_set_attribute(uint8_t value) {
  updateUIValue(value);

  SmartstrapResult result;
  uint8_t *buffer;
  size_t length;
  result = smartstrap_attribute_begin_write(selected_output_attribute, &buffer, &length);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Begin write failed with error %d", result);
    return;
  }

  buffer[0] = value;

  result = smartstrap_attribute_end_write(selected_output_attribute, 1, false);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "End write failed with error %d", result);
    return;
  }
}

/********************************** Input *************************************/

static void strap_notify_handler(SmartstrapAttribute *attribute) {
  if (attribute != selected_input_attribute) {
    return;
  }

  smartstrap_attribute_read(attribute);
}

static void strap_did_read(SmartstrapAttribute *attribute, SmartstrapResult result,
                         const uint8_t *data, size_t length) {
  if (attribute != selected_input_attribute) {
    return;
  }
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Read failed with result %d", result);
    return;
  }
  if (length != ATTRIBUTE_LENGTH) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Got response of unexpected length (%d)", length);
    return;
  }

  if(mode == 0) {
    prv_set_attribute(*data);
  }
}

/******************************* Accel Handling *******************************/
static void data_handler(AccelData *data, uint32_t num_samples) {
  if (mode == 1 && !data[0].did_vibrate) {
    uint8_t s_accel_y = (255 * (abs(data[0].y) / 1000.0));
    prv_set_attribute(s_accel_y);
  }
}

/****************************** Button Handling *******************************/
static void select_input_mode(int new_mode) {
  mode = new_mode;
  if(mode == 0) {
  //   accel_data_service_subscribe(1, data_handler);
  // }else{
  //   accel_data_service_unsubscribe();
    smartstrap_attribute_read(selected_input_attribute);
  }
}

static void toggle_input_mode() {
  select_input_mode(mode == 0 ? 1 : 0);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  toggle_input_mode();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

/************************************ App *************************************/

static void init() {
  s_main_window = window_create();
  window_set_click_config_provider(s_main_window, click_config_provider);
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
  top_input_attribute = smartstrap_attribute_create(SERVICE_ID, TOP_INPUT_ATTRIBUTE_ID, ATTRIBUTE_LENGTH);
  top_output_attribute = smartstrap_attribute_create(SERVICE_ID, TOP_OUTPUT_ATTRIBUTE_ID, ATTRIBUTE_LENGTH);
  center_input_attribute = smartstrap_attribute_create(SERVICE_ID, CENTER_INPUT_ATTRIBUTE_ID, ATTRIBUTE_LENGTH);
  center_output_attribute = smartstrap_attribute_create(SERVICE_ID, CENTER_OUTPUT_ATTRIBUTE_ID, ATTRIBUTE_LENGTH);
  bottom_input_attribute = smartstrap_attribute_create(SERVICE_ID, BOTTOM_INPUT_ATTRIBUTE_ID, ATTRIBUTE_LENGTH);
  bottom_output_attribute = smartstrap_attribute_create(SERVICE_ID, BOTTOM_OUTPUT_ATTRIBUTE_ID, ATTRIBUTE_LENGTH);
  selected_input_attribute = center_input_attribute;
  selected_output_attribute = center_output_attribute;

  select_input_mode(0);
  accel_data_service_subscribe(1, data_handler);
}

static void deinit() {
  smartstrap_attribute_destroy(top_input_attribute);
  smartstrap_attribute_destroy(top_output_attribute);
  smartstrap_attribute_destroy(center_input_attribute);
  smartstrap_attribute_destroy(center_output_attribute);
  smartstrap_attribute_destroy(bottom_input_attribute);
  smartstrap_attribute_destroy(bottom_output_attribute);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
