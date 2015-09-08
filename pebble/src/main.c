#include <pebble.h>
#include "windows/pin_window.h"

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

static Window *s_main_window;
static MenuLayer *s_menu_layer;
// static TextLayer *s_output_layer;

static SmartstrapAttribute *selected_input_attribute;
static SmartstrapAttribute *selected_output_attribute;
static int selected_input_attribute_index;
static int selected_output_attribute_index;

#define NUM_WINDOWS 1
#define CELL_HEIGHT 30

/******************************** Smartstraps *********************************/

static char* smartstrap_result_to_string(SmartstrapResult result) {
  switch(result) {
    case SmartstrapResultOk:                   return "SmartstrapResultOk";
    case SmartstrapResultInvalidArgs:          return "SmartstrapResultInvalidArgs";
    case SmartstrapResultNotPresent:           return "SmartstrapResultNotPresent";
    case SmartstrapResultBusy:                 return "SmartstrapResultBusy";
    case SmartstrapResultServiceUnavailable:   return "SmartstrapResultServiceUnavailable";
    case SmartstrapResultAttributeUnsupported: return "SmartstrapResultAttributeUnsupported";
    case SmartstrapResultTimeOut:              return "SmartstrapResultTimeOut";
    default: return "Not a SmartstrapResult value!";
  }
}

static void strap_availability_handler(SmartstrapServiceId service_id, bool is_available) {
  // A service's availability has changed
  APP_LOG(APP_LOG_LEVEL_INFO, "Service %d is %s available", (int)service_id, is_available ? "now" : "NOT");
}

/********************************** Output ************************************/

static void prv_set_attribute_value(SmartstrapAttribute *attribute, uint8_t value) {
  //updateUIValue(value);

  SmartstrapResult result;
  uint8_t *buffer;
  size_t length;
  result = smartstrap_attribute_begin_write(attribute, &buffer, &length);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Begin write failed with error %s", smartstrap_result_to_string(result));
    return;
  }

  buffer[0] = value;

  result = smartstrap_attribute_end_write(attribute, 1, false);
  if (result != SmartstrapResultOk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "End write failed with error %s", smartstrap_result_to_string(result));
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
    APP_LOG(APP_LOG_LEVEL_ERROR, "Read failed with result %s", smartstrap_result_to_string(result));
    return;
  }
  if (length != ATTRIBUTE_LENGTH) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Got response of unexpected length (%s)", smartstrap_result_to_string(result));
    return;
  }

  prv_set_attribute_value(selected_output_attribute, *data);
}

/************************************* UI *************************************/

static void updateUIValue(uint8_t value) {
  // static char s_buffer[128];
  // snprintf(s_buffer, sizeof(s_buffer), "%d", value);
  // text_layer_set_text(s_output_layer, s_buffer);
}

static SmartstrapAttribute* get_input_attribute(int index) {
  switch(index) {
    case 0:
      return top_input_attribute;
      break;
    case 1:
      return center_input_attribute;
      break;
    case 2:
      return bottom_input_attribute;
      break;
    default:
      return top_input_attribute;
      break;
  }
}

static SmartstrapAttribute* get_output_attribute(int index) {
  switch(index) {
    case 0:
      return top_output_attribute;
      break;
    case 1:
      return center_output_attribute;
      break;
    case 2:
      return bottom_output_attribute;
      break;
    default:
      return top_output_attribute;
      break;
  }
}

static void play_recipe() {
  // reset all outputs
  prv_set_attribute_value(top_output_attribute, 0);
  prv_set_attribute_value(center_output_attribute, 0);
  prv_set_attribute_value(bottom_output_attribute, 0);

  selected_input_attribute = get_input_attribute(selected_input_attribute_index);
  selected_output_attribute = get_output_attribute(selected_output_attribute_index);
  smartstrap_attribute_read(selected_input_attribute);
}

static void pin_complete_callback(PIN pin, void *context) {
  selected_input_attribute_index = pin.digits[0];
  selected_output_attribute_index = pin.digits[2];

  APP_LOG(APP_LOG_LEVEL_INFO, "Pin was %d %d %d", pin.digits[0], pin.digits[1], pin.digits[2]);
  pin_window_pop((PinWindow*)context, true);

  play_recipe();
}

static uint16_t get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return NUM_WINDOWS;
}

static void draw_row_callback(GContext *ctx, Layer *cell_layer, MenuIndex *cell_index, void *context) {
  switch(cell_index->row) {
    case 0:
      menu_cell_basic_draw(ctx, cell_layer, "Edit Recipe", NULL, NULL);
      break;
    default:
      break;
  }
}

static int16_t get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  return CELL_HEIGHT;
}

static void select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  switch(cell_index->row) {
    case 0: {
        PinWindow *pin_window = pin_window_create((PinWindowCallbacks) {
          .pin_complete = pin_complete_callback
        });
        pin_window_push(pin_window, true);
      }
      break;
    default:
      break;
  }
}

static void draw_header_callback(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *context) {
  menu_cell_basic_header_draw(ctx, cell_layer, "Choose a Component");
}

static int16_t get_header_height_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static uint16_t get_num_sections_callback(struct MenuLayer *menu_layer, void *context) {
  return 1;
}

static void main_window_load(Window *window) {
  // text layer for connection status
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
      .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback)get_num_rows_callback,
      .draw_row = (MenuLayerDrawRowCallback)draw_row_callback,
      .get_cell_height = (MenuLayerGetCellHeightCallback)get_cell_height_callback,
      .select_click = (MenuLayerSelectCallback)select_callback,
      .draw_header = (MenuLayerDrawHeaderCallback)draw_header_callback,
      .get_header_height = (MenuLayerGetHeaderHeightCallback)get_header_height_callback,
      .get_num_sections = (MenuLayerGetNumberOfSectionsCallback)get_num_sections_callback,
  });
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));

  // int window_height = bounds.size.h;
  // int text_height = 40;
  // int y = (window_height / 2) - (text_height / 2);
  //
  // window_set_background_color(s_main_window, GColorRed);

  // s_output_layer = text_layer_create(GRect(0, y, bounds.size.w, text_height));
  // text_layer_set_font(s_output_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  // text_layer_set_text_alignment(s_output_layer, GTextAlignmentCenter);
  // text_layer_set_text_color(s_output_layer, GColorBlack);
  // text_layer_set_background_color(s_output_layer, GColorWhite);
  // layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_output_layer));
}

static void main_window_unload(Window *window) {
  window_destroy(s_main_window);
}

/******************************* Accel Handling *******************************/
// accel_data_service_subscribe(1, data_handler);
// accel_data_service_unsubscribe();
// static void data_handler(AccelData *data, uint32_t num_samples) {
//   if (mode == 1 && !data[0].did_vibrate) {
//     uint8_t s_accel_y = (255 * (abs(data[0].y) / 1000.0));
//     prv_set_attribute_value(selected_input_attribute, s_accel_y);
//   }
// }

/****************************** Button Handling *******************************/
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {}

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
