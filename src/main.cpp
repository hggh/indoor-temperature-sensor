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
#include "SparkFunBME280.h"

#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSerif12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

#include "config.h"
#include "battery_symbols.h"
#include <TP4056.h>
#include <VoltageDivider.h>

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
 *  TP4056 CHRG  = GPIO36 / SENSOR_VP
 *  TP4056 STDBY = GPIO39 / SENSOR_VN
 *  TP4056 USB   = GPIO34
 *  
 *  Voltage Input  = GPIO 35 / ADC7
 *  ENABLE Voltage = GPIO 25
 *
 *  LED            = GPIO 26
 */
GxEPD2_BW<GxEPD2_154, GxEPD2_154::HEIGHT> display(GxEPD2_154(5, 17, 16, 4));
RTC_DATA_ATTR unsigned int bootCount = 0;
RTC_DATA_ATTR unsigned int sleep_time_remaining = 0;
RTC_DATA_ATTR time_t sleep_time_start;
WiFiClient espClient;
PubSubClient client(espClient);
String hostname = String(HOSTNAME);
TP4056 tp4056;
VoltageDivider voltage_divider;
const short pin_touch_pad = T9;
const short pin_enable_voltage_divider = 25;
const short pin_battery_adc = 35;
const short pin_led = 26;
const float volt_r1 = 10000.0;
const float volt_r2 = 10000.0;
const float voltage = 3.3;
const short pin_tp4056_chrg = 36;
const short pin_tp4056_stdby = 39;
const short pin_tp4056_usb = 34;
const float battery_low = 2.7;
// 2^36 + 2^34 + 2^39 FIXME
#define WAKEUP_EXT1_GPIO_MASK 0x9400000000
BME280 bme;
short status_wakeup_via_timer = 0;

int get_wakeup_reason() {
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

  return wakeup_reason;
}

void mqtt_check_connection() {
  if (!client.connected()) {
    client.connect(HOSTNAME, MQTT_USERNAME, MQTT_PASSWORD);
  }
}

void touch_interrupt() {
}

void setup() {
  setCpuFrequencyMhz(80);
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("Booting count: " + String(bootCount));
#endif
  time_t startup_time = time(0);
  esp_bluedroid_disable();
  esp_bt_controller_disable();

  pinMode(pin_led, OUTPUT);
  digitalWrite(pin_led, LOW);

  adc_power_on();
  esp_wifi_start();

  voltage_divider.init(pin_enable_voltage_divider, pin_battery_adc);
  voltage_divider.set_r1(volt_r1);
  voltage_divider.set_r2(volt_r1);

  tp4056.init(pin_tp4056_usb, pin_tp4056_chrg, pin_tp4056_stdby);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  WiFi.setHostname(HOSTNAME);

  client.setServer(MQTT_SERVER, 1883);

  Wire.begin();
  bme.setI2CAddress(0x76);
  bool status = bme.beginI2C();
#ifdef DEBUG
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
  }
#endif
  bme.setMode(MODE_FORCED);

  // if wakeup was by timer or after reset set time to sleep and send data via mqtt
  if (get_wakeup_reason() == ESP_SLEEP_WAKEUP_TIMER || get_wakeup_reason() == 0) {
    status_wakeup_via_timer = 1;
    sleep_time_remaining = sleep_time_min * 60 * uS_TO_S_FACTOR;
#ifdef DEBUG
    Serial.println("Wakeup via timer or reset");
    Serial.print("sleep_time_remaining: ");
    Serial.println(sleep_time_remaining);
#endif
  }
  else {
    status_wakeup_via_timer = 0;
    sleep_time_remaining = sleep_time_remaining - ((startup_time - sleep_time_start) * uS_TO_S_FACTOR);
#ifdef DEBUG
    Serial.println("Wakeup via ext1 or touchpad calc time");
    Serial.print("startup_time: ");
    Serial.println(startup_time);
    Serial.print("sleep_time_start: ");
    Serial.println(sleep_time_start);
    Serial.print("sleep_time_remaining: ");
    Serial.println(sleep_time_remaining);
#endif
  }

  display.init(0, false);
  //SPI.end();
  //SPI.begin(18, 19, 23, 5);
  display.setRotation(2);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeSans18pt7b);

  display.fillScreen(GxEPD_WHITE);

  display.fillCircle(130, 30, 5, GxEPD_BLACK);
  display.fillCircle(130, 30, 2, GxEPD_WHITE);
  display.setCursor(135, 55);
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

  // check if usb power is on or if battery is low
  if (tp4056.has_usb_power()) {
    if (tp4056.state() == tp4056.CHARGING) {
      display.drawInvertedBitmap(170, 5, icon_battery_charing, 25, 43, GxEPD_BLACK);
    }
    if (tp4056.state() == tp4056.CHARGED) {
      display.drawInvertedBitmap(170, 5, icon_battery_full, 25, 43, GxEPD_BLACK);
    }
  }
  else {
    // we have no USB power, so check batter voltage
    if (battery_low > voltage_divider.get_voltage()) {
      display.drawInvertedBitmap(170, 5, icon_battery_low, 25, 43, GxEPD_BLACK);
    }
    else {
      // remove drawing
      display.fillRect(170, 5, 25, 43, GxEPD_WHITE);
    }
  }

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
  float h = bme.readFloatHumidity();
  float t = bme.readTempC();
  float p = bme.readFloatPressure() / 100.0F;
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

  float battery_voltage = voltage_divider.get_voltage();

  if (status_wakeup_via_timer == 1) {
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
  display.setCursor(5, 55);
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
  bme.setMode(MODE_SLEEP);
  esp_sleep_enable_timer_wakeup(sleep_time_remaining);
  sleep_time_start = time(0);
  esp_sleep_enable_touchpad_wakeup();
  //esp_sleep_enable_ext1_wakeup(WAKEUP_EXT1_GPIO_MASK, ESP_EXT1_WAKEUP_ANY_HIGH);

  esp_deep_sleep_start();
}
