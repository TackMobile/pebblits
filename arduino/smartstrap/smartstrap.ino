#include <Arduino.h>
#include <ArduinoPebbleSerial.h>

static const uint16_t SERVICE_ID = 0x1001;

static const uint16_t TOP_INPUT_ATTRIBUTE_ID = 0x0001;
static const uint16_t TOP_OUTPUT_ATTRIBUTE_ID = 0x0002;
static const uint16_t CENTER_INPUT_ATTRIBUTE_ID = 0x0003;
static const uint16_t CENTER_OUTPUT_ATTRIBUTE_ID = 0x0004;
static const uint16_t BOTTOM_INPUT_ATTRIBUTE_ID = 0x0005;
static const uint16_t BOTTOM_OUTPUT_ATTRIBUTE_ID = 0x0006;

// Analog input is 0-1024.
// Inputs are mapped to 0-255.
static const size_t INPUT_ATTRIBUTE_LENGTH = 1;
static const size_t OUTPUT_ATTRIBUTE_LENGTH = 1;

static const uint16_t SERVICES[] = {SERVICE_ID};
static const uint8_t NUM_SERVICES = 1;

// LED is soldered to this pin. Drive HIGH for on and LOW for off.
// LED is used for "connected to pebble" indicator.
static const uint8_t CONNECTED_OUTPUT_PIN = 11;

// top input/output is digital only.
static const uint8_t TOP_INPUT_PIN = 0;
static const uint8_t TOP_OUTPUT_PIN = 1;
static const uint8_t CENTER_INPUT_PIN = A0;
static const uint8_t CENTER_OUTPUT_PIN = 5;
static const uint8_t BOTTOM_INPUT_PIN = A1;
static const uint8_t BOTTOM_OUTPUT_PIN = 9;

static uint8_t last_top_value_notified;
static uint8_t last_center_value_notified;
static uint8_t last_bottom_value_notified;

// Pebble tether is connected to this pin for software serial mode.
static const uint8_t PEBBLE_DATA_PIN = 10;
static uint8_t buffer[GET_PAYLOAD_BUFFER_SIZE(4)];

void setup() {
  Serial.begin(9600);
  Serial.println("Hello from Arduino at heart");
  
  //setup light for "connected" indicator.
  pinMode(CONNECTED_OUTPUT_PIN, OUTPUT);
  // initially off for "not connected".
  digitalWrite(CONNECTED_OUTPUT_PIN, LOW);

  // setup output for the LittleBits outputs
  pinMode(TOP_OUTPUT_PIN, OUTPUT);
  pinMode(CENTER_OUTPUT_PIN, OUTPUT);
  pinMode(BOTTOM_OUTPUT_PIN, OUTPUT);

  last_top_value_notified = 0;
  last_center_value_notified = 0;
  last_bottom_value_notified = 0;

  //write LittleBits to LOW state.
  digitalWrite(TOP_OUTPUT_PIN, LOW);
  analogWrite(CENTER_OUTPUT_PIN, 0);
  analogWrite(BOTTOM_OUTPUT_PIN, 0);
  
  // Setup the Pebble smartstrap connection using one wire software serial
  ArduinoPebbleSerial::begin_software(PEBBLE_DATA_PIN, buffer, sizeof(buffer), Baud57600, SERVICES, NUM_SERVICES);
}

void handle_input_request(RequestType type, size_t length, uint16_t attribute_id) {
  Serial.println("Arduino -> SmartStrap (START)");
  if (type != RequestTypeRead) {
    // unexpected request type
    return;
  }

  int inputValue = 0;
  switch (attribute_id) {
    case TOP_INPUT_ATTRIBUTE_ID:
      inputValue = last_top_value_notified;
      break;
    case CENTER_INPUT_ATTRIBUTE_ID:
      inputValue = last_center_value_notified;
      break;
    case BOTTOM_INPUT_ATTRIBUTE_ID:
      inputValue = last_bottom_value_notified;
      break;
    default:
      break; 
  }
  const uint8_t mapInputValue = inputValue;
  ArduinoPebbleSerial::write(true, (uint8_t *)&mapInputValue, sizeof(mapInputValue));
  Serial.println("Arduino -> SmartStrap (SUCCESS)");
}

void handle_output_request(RequestType type, size_t length, uint16_t attribute_id) {
  Serial.println("SmartStrap -> Arduino (START)");
  
  if (type != RequestTypeWrite) {
    // unexpected request type
    return;
  } else if (length != OUTPUT_ATTRIBUTE_LENGTH) {
    // unexpected request length
    return;
  }
  bool do_ack = HIGH;
  switch (attribute_id) {
    case TOP_OUTPUT_ATTRIBUTE_ID:
      if (buffer[0] > 0) {
        digitalWrite(TOP_OUTPUT_PIN, HIGH);
      } else {
        digitalWrite(TOP_OUTPUT_PIN, LOW);
      }
      break;
    case CENTER_OUTPUT_ATTRIBUTE_ID:
      analogWrite(CENTER_OUTPUT_PIN, buffer[0]);
      break;
    case BOTTOM_OUTPUT_ATTRIBUTE_ID:
      analogWrite(BOTTOM_OUTPUT_PIN, buffer[0]);
      break;
   default:
      do_ack = LOW;
  }
  
  // ACK that the write request was received
  if (do_ack == HIGH) {
    ArduinoPebbleSerial::write(true, NULL, 0);
  } else {
    ArduinoPebbleSerial::write(false, NULL, 0);
  }

  Serial.println("SmartStrap -> Arduino (SUCCESS)");
}

void loop() {
    
  uint16_t service_id;
  uint16_t attribute_id;
  size_t length;
  RequestType type;
  bool fed = ArduinoPebbleSerial::feed(&service_id, &attribute_id, &length, &type);

  bool pebble_connected = ArduinoPebbleSerial::is_connected();
  digitalWrite(CONNECTED_OUTPUT_PIN, pebble_connected ? HIGH : LOW);

  uint8_t top_new_value = (digitalRead(TOP_INPUT_PIN) == HIGH) ? 0 : 255;
  uint8_t center_new_value = map(analogRead(CENTER_INPUT_PIN), 0, 1023, 0, 255);
  uint8_t bottom_new_value = map(analogRead(BOTTOM_INPUT_PIN), 0, 1023, 0, 255);

  // decide whether to notify each input
  bool should_notify_top = LOW;
  bool should_notify_center = LOW;
  bool should_notify_bottom = LOW;
  
  if (top_new_value != last_top_value_notified) {
    should_notify_top = HIGH;
  }

  if (abs(last_center_value_notified - center_new_value) > 2) {
    should_notify_center = HIGH;
  }

  if (abs(last_bottom_value_notified - bottom_new_value) > 2) {
    should_notify_bottom = HIGH;
  }

  const uint32_t current_time = millis();
  static uint32_t top_notified_time = current_time;
  static uint32_t center_notified_time = current_time;
  static uint32_t bottom_notified_time = current_time;

  bool top_clamped = LOW;
  bool center_clamped = LOW;
  bool bottom_clamped = LOW;

  static const uint32_t CLAMP_MILLISECONDS = 100;
  if (current_time - top_notified_time < CLAMP_MILLISECONDS) {
    top_clamped = HIGH;
  }
  if (current_time - center_notified_time < CLAMP_MILLISECONDS) {
    center_clamped = HIGH;
  }
  if (current_time - bottom_notified_time < CLAMP_MILLISECONDS) {
    bottom_clamped = HIGH;
  }

  // only attempt to communicate with the pebble if we are connected;
  if (pebble_connected) {

    if (should_notify_top && (top_clamped == LOW)) {
      last_top_value_notified = top_new_value;
      ArduinoPebbleSerial::notify(SERVICE_ID, TOP_INPUT_ATTRIBUTE_ID);
      top_notified_time = current_time;
    }

    if (should_notify_center && (center_clamped == LOW)) {
      last_center_value_notified = center_new_value;
      ArduinoPebbleSerial::notify(SERVICE_ID, CENTER_INPUT_ATTRIBUTE_ID);
      center_notified_time = current_time;
    }

    if (should_notify_bottom && (bottom_clamped == LOW)) {
      last_bottom_value_notified = bottom_new_value;
      ArduinoPebbleSerial::notify(SERVICE_ID, BOTTOM_INPUT_ATTRIBUTE_ID);
      bottom_notified_time = current_time;
    }
    
    if (fed) {
      // process the request
      if (service_id == SERVICE_ID) {
        switch (type) {
          case RequestTypeRead:
            handle_input_request(type, length, attribute_id);
            break;
          case RequestTypeWrite:
            handle_output_request(type, length, attribute_id);
            break;
          default:
            break;
        }
      }
    }  
  }
}

