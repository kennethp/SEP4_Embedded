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
#include "tsl2591.h"
//added
#include <ihal.h>
#include <lora_driver.h>

#define LED_TASK_PRIORITY   (tskIDLE_PRIORITY + 5)
#define LORA_appEUI ""
#define LORA_appKEY ""
//
#define TEMP_TASK_PRIORITY (tskIDLE_PRIORITY + 5)
#define CO2_TASK_PRIORITY (tskIDLE_PRIORITY + 5)
#define LIGHT_TASK_PRIORITY (tskIDLE_PRIORITY + 5)
#define WATER_TASK_PRIORITY (tskIDLE_PRIORITY + 5)


TaskHandle_t tempSensorHandle = NULL;
TaskHandle_t co2SensorHandle = NULL;
TaskHandle_t lightSensorHandle = NULL;
TaskHandle_t WaterHandle = NULL;
//add:
TaskHandle_t ledHandle = NULL;//

int temperature;
int humidity;
uint16_t co2;
float light;
int water;
//added:
void hal_create(uint8_t led_task_priority);
void lora_driver_create(e_com_port_t com_port);
void lora_driver_reset_rn2483(uint8_t state);
void lora_driver_flush_buffers(void);



//

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
		printf("Hum: %d  Temp: %d\n", humidity, temperature);
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
	co2 = ppm;
	printf("CO2 level: %u\n", ppm);
}

void lightSensorTask(void* pvParameters) {
	(void)pvParameters;

	while(1) {
		_delay_ms(1000);
		int r = tsl2591FetchData();
		if(r != TSL2591_OK) {
			printf("Failed to fetch light data: %d\n", r);
		}
	}

	vTaskDelete(NULL);
}

void lightCallback(tsl2591ReturnCode_t rc) {
	float measure;
	if(rc != TSL2591_DATA_READY) {
		printf("Light sensor not ready\n");
		return;
	}
	if(TSL2591_OK == tsl2591GetLux(&measure)) {
		light = measure;
		printf("Light: %d\n", (uint16_t) measure);
	}
	else {
		printf("Lux overflow\n");
	}
}

void waterTask(void* pvParamters) {
	(void)pvParamters;

	while(1) {
		while(water > 0) {
			//set servo output high
			_delay_ms(100);
			water--;
		}

		//set servo output low
	}

	vTaskDelete(NULL);
}


//added:

void lora_driver_get_rn2483_hweui(char dev_eui);
void settingLoraTask(void* pvParamters){
UnknownType LoRA_OK;
	
	static char dev_eui[17]; // It is static to avoid it to occupy stack space in the task
	if (lora_driver_get_rn2483_hweui(dev_eui); != LoRA_OK)
	{
		printf("something went wrong\n");
	}
	else{
		printf("temp-wake error: %d\n", dev_eui);

	}
	while(1){
		lora_driver_reset_rn2483(1);
		vTaskDelay(2);
		lora_driver_reset_rn2483(0);
		vTaskDelay(150);
		lora_driver_flush_buffers();
	}
	
	
	vTaskDelete(NULL);
}
//

int main() {
	
	hal_create(LED_TASK_PRIORITY);
	lora_driver_create(ser_USART1);
	stdioCreate(0);
	sei();
	DDRC = 0xFF;
	_delay_ms(50);
	PORTC = 0x01;
	while(1) {
		_delay_ms(500);
		printf("Running PORTC\n");
	}
	xTaskCreate(tempSensorTask, "Temperature measurement", configMINIMAL_STACK_SIZE, NULL, TEMP_TASK_PRIORITY, &tempSensorHandle);
	xTaskCreate(co2SensorTask, "CO2 measurement", configMINIMAL_STACK_SIZE, NULL, CO2_TASK_PRIORITY, &co2SensorHandle);
	xTaskCreate(lightSensorTask, "Light measurement", configMINIMAL_STACK_SIZE, NULL, LIGHT_TASK_PRIORITY, &lightSensorHandle);
	xTaskCreate(waterTask, "Water servo", configMINIMAL_STACK_SIZE, NULL, WATER_TASK_PRIORITY, &WaterHandle);
	//added:
	xTaskCreate(settingLoraTask, "Led", configMINIMAL_STACK_SIZE, NULL,LED_TASK_PRIORITY, &ledHandle);
	//
	stdioCreate(0);
	sei();
	//setup temperature/humidity sensor
	if(HIH8120_OK != hih8120Create()) {
		printf("Failed to initialize temperature sensor\n");
		return 1;
	}

	//setup co2 sensor
	mh_z19_create(ser_USART3, co2Callback);

	//setup light sensor
	int r = tsl2591Create(lightCallback);
	if(r != TSL2591_OK) {
		printf("Failed to initialize light sensor: %d\n", r);
	}

	r = tsl2591Enable();
	if(r != TSL2591_OK) {
		printf("Failed to enable light sensor %d\n", r);
	}
	
	vTaskStartScheduler();
	
	while(1) {
		;
	}
}

