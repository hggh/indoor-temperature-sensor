#include <Arduino.h>
#include <WiFi.h>
#include "driver/adc.h"

#define uS_TO_S_FACTOR 1000000
#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>

#include <PubSubClient.h>

#include "DHTesp.h"
DHTesp dht;
int dhtPin = 22;

#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

#include "config.h"

GxEPD2_BW<GxEPD2_154, GxEPD2_154::HEIGHT> display(GxEPD2_154(SS, 17, 16, 4));
RTC_DATA_ATTR unsigned int bootCount = 0;
WiFiClient espClient;
PubSubClient client(espClient);

void mqtt_check_connection() {
  if (!client.connected()) {
    client.connect(HOSTNAME, MQTT_USERNAME, MQTT_PASSWORD);
  }
}

void wifi_got_ip_event(WiFiEvent_t event, WiFiEventInfo_t info) {
  mqtt_check_connection();
}

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("Booting...");
#endif

  adc_power_off();
  btStop();

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.onEvent(wifi_got_ip_event, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  WiFi.setHostname(HOSTNAME);

  client.setServer(MQTT_SERVER, 1883);

  dht.setup(dhtPin, DHTesp::DHT22);

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

  if (bootCount == 0) {
    display.display();
  }
  else {
    display.display(true);
  }
  ++bootCount;
  display.hibernate();
}

void loop() {
  TempAndHumidity newValues = dht.getTempAndHumidity();
  
  float h = newValues.humidity;
  float t = newValues.temperature;
  float p = random(900, 1111);
  if (dht.getStatus() != 0) {
    Serial.println("error dht");
    delay(200);
    return;
  }

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

  // TODO Timeout for wifi and mqtt
  while (WiFi.status() != WL_CONNECTED) {
    delay(10);
    Serial.print(".");
  }
  mqtt_check_connection();

  char value_buffer[10] = "";
  char buffer[60] = "";
  dtostrf(t, 3, 2, value_buffer);
  sprintf(buffer, "/temperature/%s/state", HOSTNAME);
  client.publish(buffer, value_buffer);

  dtostrf(h, 3, 2, value_buffer);
  sprintf(buffer, "/humidity/%s/state", HOSTNAME);
  client.publish(buffer, value_buffer);

  dtostrf(p, 3, 2, value_buffer);
  sprintf(buffer, "/pressure/%s/state", HOSTNAME);
  client.publish(buffer, value_buffer);

  sprintf(value_buffer, "%i", bootCount);
  sprintf(buffer, "/bootcount/%s/state", HOSTNAME);
  client.publish(buffer, value_buffer);

  espClient.flush();
  delay(1);
  client.disconnect();
  delay(1);
  espClient.flush();
  delay(1);
  WiFi.disconnect(true);

  esp_sleep_enable_timer_wakeup(30 * uS_TO_S_FACTOR);
  esp_deep_sleep_start();

}
