#include <Arduino.h>
#include <WiFi.h>
#include "driver/adc.h"
#include <esp_wifi.h>
#include <esp_bt.h>
#include <esp_bt_main.h>

#define uS_TO_S_FACTOR 1000000
#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>

#include <PubSubClient.h>

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSerif12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

#include "config.h"

/*
 *  DIN  = GPIO 23
 *  CLK  = GPIO 18
 *  CS   = GPIO  5
 *  DC   = GPIO 17
 *  RST  = GPIO 16
 *  BUSY = GPIO  4
 *
 *  BME280
 *  SDA = GPIO 21
 *  SCL = GPIO 22
 *
 *  Touch Sensor
 *  T9  = GPIO 32 aka 33?!?(wrong label on breakout?)
 *  
 *  Voltage Input  = GPIO 35 / ADC7
 *  ENABLE Voltage = GPIO 19 
 */
GxEPD2_BW<GxEPD2_154, GxEPD2_154::HEIGHT> display(GxEPD2_154(5, 17, 16, 4));
RTC_DATA_ATTR unsigned int bootCount = 0;
static RTC_DATA_ATTR struct timeval sleep_enter_time;
WiFiClient espClient;
PubSubClient client(espClient);
String hostname = String(HOSTNAME);
const unsigned int sleep_time_min = 10;
const short pin_touch_pad = T9;
const short pin_enable_voltage_divider = 19;
const short pin_battery_adc = 35;
const float volt_r1 = 10000.0;
const float volt_r2 = 10000.0;
const float voltage = 3.3;
Adafruit_BME280 bme;
uint64_t sleep_time;
#define WAKEUP_STATUS_TIMER 0
#define WAKEUP_STATUS_TOUCHPAD 1
uint8_t wakeup_status = WAKEUP_STATUS_TIMER;

bool wakeup_by_touch() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

#ifdef DEBUG
  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
#endif

  if (wakeup_reason == ESP_SLEEP_WAKEUP_TOUCHPAD) {
    return true;
  }
  return false;
}

void mqtt_check_connection() {
  if (!client.connected()) {
    client.connect(HOSTNAME, MQTT_USERNAME, MQTT_PASSWORD);
  }
}

float get_battery_voltage() {
  digitalWrite(pin_enable_voltage_divider, HIGH);
  float val = 0.0;

  val = analogRead(pin_battery_adc);
  float vin = ((val * voltage) / 4095.0) / (volt_r2 / ( volt_r1 + volt_r2));

  digitalWrite(pin_enable_voltage_divider, LOW);

  return vin;
}

void touch_interrupt() {
}

void setup() {
  struct timeval now;
  gettimeofday(&now, NULL);
  unsigned int actual_sleep_time_s = now.tv_sec - sleep_enter_time.tv_sec;
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("Booting count: " + String(bootCount));
#endif
  esp_bluedroid_disable();
  esp_bt_controller_disable();

  adc_power_on();
  esp_wifi_start();

  pinMode(pin_battery_adc, INPUT);
  pinMode(pin_enable_voltage_divider, OUTPUT);
  digitalWrite(pin_enable_voltage_divider, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  WiFi.setHostname(HOSTNAME);

  client.setServer(MQTT_SERVER, 1883);

  unsigned status = bme.begin(0x76, &Wire);
#ifdef DEBUG
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
    Serial.print("SensorID was: 0x");
    Serial.println(bme.sensorID(),16);
  }
#endif

  // if wakeup was via touch, we check the sleep time and calc the new sleep time
  // so our home automation receives the data in the same interval
  if (wakeup_by_touch() == true) {
    wakeup_status = WAKEUP_STATUS_TOUCHPAD;
    sleep_time = ((sleep_time_min * 60) - actual_sleep_time_s) * uS_TO_S_FACTOR;
  }
  else {
    wakeup_status = WAKEUP_STATUS_TIMER;
    sleep_time = sleep_time_min * 60 * uS_TO_S_FACTOR;
  }

  display.init(0, false);
  display.setRotation(2);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeSans18pt7b);

  display.fillScreen(GxEPD_WHITE);

  display.fillCircle(130, 25, 5, GxEPD_BLACK);
  display.fillCircle(130, 25, 2, GxEPD_WHITE);
  display.setCursor(135, 50);
  display.print("C");

  display.setCursor(130, 120);
  display.print("%");

  display.setCursor(130, 185);
  display.print("hPa");

#ifdef DEBUG
  display.setFont(&FreeSerif12pt7b);
  display.setCursor(90, 80);
  display.print("bootCount");
#endif

  if (bootCount == 0) {
    display.display();
  }
  else {
    display.display(true);
  }
  ++bootCount;
  display.hibernate();

  touchAttachInterrupt(pin_touch_pad, touch_interrupt, 30);
}

void loop() {
  float h = bme.readHumidity();
  float t = bme.readTemperature();
  float p = bme.readPressure() / 100.0F;
#ifdef DEBUG
  Serial.print("bme.readHumidity: ");
  Serial.println(h);
  Serial.print("bme.readTemperature: ");
  Serial.println(t);
  Serial.print("bme.readPressure: ");
  Serial.println(p);
#endif
  // TODO Timeout for wifi and mqtt
  while (WiFi.status() != WL_CONNECTED) {
    delay(10);
    Serial.print(".");
  }
  Serial.println("");
  mqtt_check_connection();

  //float battery_voltage = get_battery_voltage();
  float battery_voltage = 3.3;

  if (wakeup_status == WAKEUP_STATUS_TIMER) {
    client.publish(String("/temperature/" + hostname + "/state").c_str(), String(t, 2).c_str());
    client.publish(String("/humidity/" + hostname + "/state").c_str(), String(h, 2).c_str());
    client.publish(String("/pressure/" + hostname + "/state").c_str(), String(p, 2).c_str());
    client.publish(String("/batteryvoltage/" + hostname + "/state").c_str(), String(battery_voltage).c_str());
    client.publish(String("/bootcount/" + hostname + "/state").c_str(), String(bootCount).c_str());

    espClient.flush();
  }

#ifdef DEBUG
  display.setCursor(5, 80);
  display.setFont(&FreeSerif12pt7b);
  display.print(bootCount, 1);
#endif

  display.setFont(&FreeSansBold24pt7b);
  display.setCursor(5, 50);
  display.print(t, 1);

  display.setCursor(5, 120);
  display.print(h, 1);

  display.setCursor(5, 185);
  if (p < 1000) {
    display.print(" ");
  }
  display.print(int(p));

  display.display(true);
  display.hibernate();

  client.disconnect();
  delay(1);
  WiFi.disconnect();

  esp_wifi_stop();
  adc_power_off();
  esp_sleep_enable_timer_wakeup(sleep_time);
  esp_sleep_enable_touchpad_wakeup();
  if (wakeup_status == WAKEUP_STATUS_TIMER) {
    // only set current time if wakeup was called by periodic timer
    gettimeofday(&sleep_enter_time, NULL);
  }
  esp_deep_sleep_start();
}
