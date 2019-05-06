/*
 * app.c
 *
 * Created: 05/04/2019 07.51.28
 * Author : kup
 */ 

#include <ATMEGA_FreeRTOS.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdio_driver.h>
#include <semphr.h>
#include <serial/serial.h>
#include "hih8120.h"
#include "mh_z19.h"
#include "serial/serial.h"

#define TEMP_TASK_PRIORITY (tskIDLE_PRIORITY + 5)
#define CO2_TASK_PRIORITY (tskIDLE_PRIORITY + 5)

TaskHandle_t tempSensorHandle = NULL;
TaskHandle_t co2SensorHandle = NULL;

int temperature;
int humidity;
int co2;
int light;


void tempSensorTask(void* pvParameters) {
	(void)pvParameters;
	
	//Do temperature measurement
	while(1) {
		_delay_ms(1000);
		
		int r = hih8120Wakeup();
		if(r != HIH8120_OK && r != HIH8120_TWI_BUSY) {
			printf("temp-wake error: %d\n", r);
		}
		
		_delay_ms(100);

		r = hih8120Meassure();
		if(r != HIH8120_OK && r != HIH8120_TWI_BUSY) {
			printf("Temp-read error: %d\n", r);
		}
		_delay_ms(100);

		humidity = hih8120GetHumidity();
		temperature = hih8120GetTemperature();
		printf("Hum: %d  Temp: %d", humidity, temperature);
	}

	
	vTaskDelete(NULL);
}

void co2SensorTask(void *pvParamters) {
	(void)pvParamters;

	while(1) {
		_delay_ms(1000);

		int r = mh_z19_take_meassuring();
		if(r != MHZ19_OK) {
			printf("CO2 sensor: %d", r);
		}
	}


	vTaskDelete(NULL);
}

void co2Callback(uint16_t ppm) {
	printf("CO2 level: %u\n", ppm);
}


int main() {
	xTaskCreate(tempSensorTask, "Temperature measurement", configMINIMAL_STACK_SIZE, NULL, TEMP_TASK_PRIORITY, &tempSensorHandle);
	xTaskCreate(co2SensorTask, "CO2 measurement", configMINIMAL_STACK_SIZE, NULL, CO2_TASK_PRIORITY, &co2SensorHandle);
	stdioCreate(0);
	sei();
	//setup temperature/humidity sensor
	if(HIH8120_OK != hih8120Create()) {
		printf("Failed to initialize temperature sensor\n");
		return 1;
	}

	//setup co2 sensor
	mh_z19_create(ser_USART3, co2Callback);
	
	vTaskStartScheduler();
	
	while(1) {
		;
	}
}

