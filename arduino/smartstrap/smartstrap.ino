#include <Arduino.h>
#include <ArduinoPebbleSerial.h>

static const uint16_t SERVICE_ID = 0x1001;

static const uint16_t CENTER_INPUT_ATTRIBUTE_ID = 0x0001;
static const uint16_t CENTER_OUTPUT_ATTRIBUTE_ID = 0x0002;

static const size_t CENTER_INPUT_ATTRIBUTE_LENGTH = 1;
static const size_t CENTER_OUTPUT_ATTRIBUTE_LENGTH = 1;

static const uint16_t SERVICES[] = {SERVICE_ID};
static const uint8_t NUM_SERVICES = 1;

static const uint8_t CONNECTED_OUTPUT_PIN = 11;
static const uint8_t CENTER_INPUT_PIN = A0;
static const uint8_t CENTER_OUTPUT_PIN = 5;
static const uint8_t PEBBLE_DATA_PIN = 10;
static uint8_t buffer[GET_PAYLOAD_BUFFER_SIZE(4)];

void setup() {
  pinMode(CONNECTED_OUTPUT_PIN, OUTPUT);
  pinMode(CENTER_INPUT_PIN, INPUT);
  pinMode(CENTER_OUTPUT_PIN, OUTPUT);

  // Setup the Pebble smartstrap connection using one wire software serial
  ArduinoPebbleSerial::begin_software(PEBBLE_DATA_PIN, buffer, sizeof(buffer), Baud57600, SERVICES, NUM_SERVICES);
}

void handle_center_input_request(RequestType type, size_t length) {
  if (type != RequestTypeRead) {
    // unexpected request type
    return;
  }
  
  int inputValue = analogRead(CENTER_INPUT_PIN);
  const uint8_t mapInputValue = map(inputValue, 0, 1023, 0, 255);
  ArduinoPebbleSerial::write(true, (uint8_t *)&mapInputValue, sizeof(mapInputValue));
}

void handle_center_output_request(RequestType type, size_t length) {
  if (type != RequestTypeWrite) {
    // unexpected request type
    return;
  } else if (length != CENTER_OUTPUT_ATTRIBUTE_LENGTH) {
    // unexpected request length
    return;
  }
  
  // set the LED
  digitalWrite(CENTER_OUTPUT_PIN, (bool) buffer[0]);
  // ACK that the write request was received
  ArduinoPebbleSerial::write(true, NULL, 0);
}

void loop() {
  // If the button is pressed...
  if (digitalRead(CENTER_INPUT_PIN) == HIGH) {
    if (ArduinoPebbleSerial::is_connected()) {
      static uint32_t last_notify_time = 0;
      const uint32_t current_time = millis() / 1000;
      if (current_time > last_notify_time) {
        ArduinoPebbleSerial::notify(SERVICE_ID, CENTER_INPUT_ATTRIBUTE_ID);
        last_notify_time = current_time;
      }
    }
  }

  uint16_t service_id;
  uint16_t attribute_id;
  size_t length;
  RequestType type;
  if(ArduinoPebbleSerial::feed(&service_id, &attribute_id, &length, &type)) {
    // process the request
    if (service_id == SERVICE_ID) {
      switch (attribute_id) {
        case CENTER_INPUT_ATTRIBUTE_ID:
          handle_center_input_request(type, length);
          break;
        case CENTER_OUTPUT_ATTRIBUTE_ID:
          handle_center_output_request(type, length);
          break;
        default:
          break;
      }
    }
  }

  digitalWrite(CONNECTED_OUTPUT_PIN, ArduinoPebbleSerial::is_connected() ? HIGH : LOW);
}

