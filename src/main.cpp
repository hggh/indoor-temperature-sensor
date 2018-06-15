#include <Arduino.h>
#include <Wire.h>
#include <avr/power.h>
#include <JeeLib.h>
#include <SparkFunBME280.h>
#include <RFM69.h>

#include "config.h"

RFM69 radio;
byte ADCSRA_status;
BME280 bme;
const float InternalReferenceVoltage = 1.1;
short battery_status = 0;

ISR(WDT_vect) {
	Sleepy::watchdogEvent();
}

float read_battery_volatage() {
	power_adc_enable();
	ADCSRA = ADCSRA_status;
	ADCSRA |= bit (ADPS0) |  bit (ADPS1) | bit (ADPS2);  // Prescaler of 128
	ADMUX = bit (REFS0) | bit (MUX3) | bit (MUX2) | bit (MUX1);

	delay(10);
	bitSet (ADCSRA, ADSC);
	while (bit_is_set(ADCSRA, ADSC)) {
	}
	float battery_voltage = InternalReferenceVoltage / float (ADC + 0.5) * 1024.0;

	ADCSRA &= ~(1 << 7);
	power_adc_disable();
	return battery_voltage;
}

void send_information() {
	double humidity = bme.readFloatHumidity();
	double pressure = bme.readFloatPressure();
	double temp = bme.readTempC();
	double battery;
	char buffer[32] = "";

	char humidityStr[10];
	char pressureStr[10];
	char tempStr[10];
	char batteryStr[10];

	dtostrf(humidity, 3, 2, humidityStr);
	dtostrf(pressure, 3, 2, pressureStr);
	dtostrf(temp, 3, 2, tempStr);

	if (battery_status % 4) {
		battery = read_battery_volatage();
		dtostrf(battery, 3, 2, batteryStr);
		sprintf(buffer, "%d;%s;%s;%s;%s", humidityStr, pressureStr, tempStr, batteryStr);
	}
	else {
		sprintf(buffer, "%d;%s;%s;%s", humidityStr, pressureStr, tempStr);
	}

	radio.sendWithRetry(GATEWAYID, buffer, strlen(buffer), 5);
	radio.sleep();

	battery_status++;
}

void setup() {
	ADCSRA_status = ADCSRA;
	ADCSRA &= ~(1 << 7);
	power_adc_disable();

	power_usart0_disable();
	power_timer1_disable();
	power_timer2_disable();

	Wire.begin();
	bme.setI2CAddress(0x76);
	bme.beginI2C();
	bme.setMode(MODE_SLEEP);

	radio.initialize(FREQUENCY, NODEID, NETWORKID);
	radio.encrypt(ENCRYPTKEY);
	radio.sleep();
}

void loop() {
	bme.setMode(MODE_FORCED);
	Sleepy::loseSomeTime(15);
	send_information();

	Sleepy::loseSomeTime(1000 * 60 * 30);
}
