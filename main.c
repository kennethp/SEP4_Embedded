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
#include <task.h>
#include <serial/serial.h>
#include "hih8120.h"
#include "mh_z19.h"
#include "serial/serial.h"
#include "tsl2591.h"
//added
#include <ihal.h>
#include <lora_driver.h>
#include "Plantdata.h"
//servo
#include "rcServo.h"

//highest priority
#define LED_TASK_PRIORITY   (configMAX_PRIORITIES - 1)
#define LORA_appEUI "c53e8f9f10801fc4"
#define LORA_appKEY "018cc25f724a8517cbfd763dc1126614"
//
#define TEMP_TASK_PRIORITY (configMAX_PRIORITIES - 3)
#define CO2_TASK_PRIORITY (configMAX_PRIORITIES - 3)
#define LIGHT_TASK_PRIORITY (configMAX_PRIORITIES - 3)
#define WATER_TASK_PRIORITY (configMAX_PRIORITIES - 3)
#define SERVO_TASK_PRIORITY (configMAX_PRIORITIES - 3)

///////////////////
///Tick Speed 62///
///////////////////


TaskHandle_t tempSensorHandle = NULL;
TaskHandle_t co2SensorHandle = NULL;
TaskHandle_t lightSensorHandle = NULL;
TaskHandle_t WaterHandle = NULL;
TaskHandle_t ServoMotorHandle = NULL;
//add:
TaskHandle_t loRaWanHandle = NULL;
SemaphoreHandle_t semaphore = NULL;
//


Plantdata plantdata;
TickType_t lastWateringTime;
char _out_buff[100];



//this is the connection keys.
 

void tempSensorTask(void* pvParameters) {
	(void)pvParameters;
	printf("Temp sensor start!!!\n");
	printf("tick %d", configTICK_RATE_HZ);
	 
	//Do temperature measurement
	while(1) {
		vTaskDelay(1000);
		
		int r = hih8120Wakeup();
		if(r != HIH8120_OK && r != HIH8120_TWI_BUSY) {
			printf("temp-wake error: %d\n", r);
		}
		
		vTaskDelay(100);
		r = hih8120Meassure();
		if(r != HIH8120_OK && r != HIH8120_TWI_BUSY) {
			printf("Temp-read error: %d\n", r);
		}
		vTaskDelay(100);
		///////////////////semaphore:
		xSemaphoreTake(semaphore, portMAX_DELAY);
			plantdata.humidity = hih8120GetHumidity();
			plantdata.temperature = hih8120GetTemperature();
			printf("Hum: %d  Temp: %d\n", plantdata.humidity, plantdata.temperature);
			
		xSemaphoreGive(semaphore);

		}

	
	vTaskDelete(NULL);
}

void co2SensorTask(void *pvParamters) {
	(void)pvParamters;
	printf("Temp sensor start!!!\n");

	while(1) {
		vTaskDelay(1000);
		int r = mh_z19_take_meassuring();
		if(r != MHZ19_OK) {
			printf("CO2 sensor: %d", r);
		
		}
	}


	vTaskDelete(NULL);
}

void co2Callback(uint16_t ppm) {
	///////////////////semaphore:
		xSemaphoreTake(semaphore, portMAX_DELAY);
			plantdata.co2 = ppm;
			printf("CO2 level: %u\n", ppm);
		
		xSemaphoreGive(semaphore);
}

void lightSensorTask(void* pvParameters) {
	(void)pvParameters;
	printf("Light sensor start!!!\n");

	while(1) {
		vTaskDelay(1000);
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
		
		///////////////////semaphore:
		xSemaphoreTake(semaphore, portMAX_DELAY);
		plantdata.light = (uint16_t) measure;
		printf("Light: %d\n", (uint16_t) measure);	
		xSemaphoreGive(semaphore);
	}
	else {
		printf("Lux overflow\n");
	}
	
}

void servoMotorTask(void* pvParamters){
	(void)pvParamters;
	TickType_t waterInterval = (lastWateringTime + 10000);
	
	while(1){
		if(xTaskGetTickCount() >= waterInterval){
			rcServoSet(0,100);
			lastWateringTime = xTaskGetTickCount();
			printf("Plant has been watered!!!!");
			vTaskDelay(100);
		}
		vTaskDelay(1000);
	}
}


//added:



void _loRa_setup(void){
	
	e_LoRa_return_code_t rc;
	
	
	//For factory reset.
	printf("FactoryRest >%s<\n", 
	lora_driver_map_return_code_to_text(lora_driver_rn2483_factory_reset()));
	
	//Configure to EU868 LoRaWAN standards.
	printf("Configure to EU868 >%s<\n", 
	lora_driver_map_return_code_to_text(lora_driver_configure_to_eu868()));
	
	//Get the transceivers HW EUI
	rc = lora_driver_get_rn2483_hweui(_out_buff);
	printf("Get HWEUI: %s >%s< \n", lora_driver_map_return_code_to_text(rc), _out_buff );
	
	
	// Set the HWEUI as DevEUI in the LoRaWAN software stack in the transceiver
	printf("Set DevEUI: %s >%s<\n", _out_buff, lora_driver_map_return_code_to_text(lora_driver_set_device_identifier(_out_buff)));

	// Set Over The Air Activation parameters to be ready to join the LoRaWAN
	printf("Set OTAA Identity appEUI:%s appKEY:%s devEUI:%s >%s<\n", LORA_appEUI, LORA_appKEY, _out_buff, lora_driver_map_return_code_to_text(lora_driver_set_otaa_identity(LORA_appEUI,LORA_appKEY,_out_buff)));

	// Save all the MAC settings in the transceiver
	printf("Save mac >%s<\n",lora_driver_map_return_code_to_text(lora_driver_save_mac()));

	// Enable Adaptive Data Rate
	printf("Set Adaptive Data Rate: ON >%s<\n", lora_driver_map_return_code_to_text(lora_driver_set_adaptive_data_rate(LoRa_ON)));

	// Join the LoRaWAN
	uint8_t maxJoinTriesLeft = 5;
	do {
		rc = lora_driver_join(LoRa_OTAA);
		printf("Join Network TriesLeft:%d >%s<\n", maxJoinTriesLeft, lora_driver_map_return_code_to_text(rc));
		if ( rc == LoRa_ACCEPTED){
			break;
		}

	} while (--maxJoinTriesLeft);

}

void loRaWanTask(void* pvParamters){
	(void)pvParamters;

	//for resetting the LoRaWAN hardware.
	printf("HALLO THERE\n");
	lora_driver_reset_rn2483(1);
	vTaskDelay(150);
	lora_driver_reset_rn2483(0);
	vTaskDelay(150);
	lora_driver_flush_buffers();
	
	_loRa_setup();
	vTaskDelay(200);
	
	lora_payload_t _uplink_payload;
	
	_uplink_payload.len = 7;
	_uplink_payload.port_no = 2;
	
	
	while(1){
		
		///////////////////semaphore:
		xSemaphoreTake(semaphore, portMAX_DELAY);
		_uplink_payload.bytes[0] = plantdata.humidity;
		_uplink_payload.bytes[1] = plantdata.temperature;
		_uplink_payload.bytes[2] = plantdata.co2 >> 8;
		_uplink_payload.bytes[3] = plantdata.co2 & 0xFF;
		_uplink_payload.bytes[4] = plantdata.light >> 8;
		_uplink_payload.bytes[5] = plantdata.light& 0xFF;
		_uplink_payload.bytes[6] = plantdata.water;
		
		printf("Upload Message >%s<\n", lora_driver_map_return_code_to_text(lora_driver_sent_upload_message(false, &_uplink_payload)));
		vTaskDelay(1000);
	};
	
	vTaskDelete(NULL);
}
	
//

int main() {
	
	hal_create(LED_TASK_PRIORITY);
	lora_driver_create(ser_USART1);
	stdioCreate(0);

	
	xTaskCreate(tempSensorTask, "Temperature measurement", configMINIMAL_STACK_SIZE, NULL, TEMP_TASK_PRIORITY, &tempSensorHandle);
	//xTaskCreate(co2SensorTask, "CO2 measurement", configMINIMAL_STACK_SIZE, NULL, CO2_TASK_PRIORITY, &co2SensorHandle);
	//xTaskCreate(lightSensorTask, "Light measurement", configMINIMAL_STACK_SIZE, NULL, LIGHT_TASK_PRIORITY, &lightSensorHandle);
	semaphore = xSemaphoreCreateMutex();
	//added:
	xTaskCreate(loRaWanTask, "Led", configMINIMAL_STACK_SIZE, NULL,LED_TASK_PRIORITY, &loRaWanHandle);
	//xTaskCreate(servoMotorTask, "Servo Motor", configMINIMAL_STACK_SIZE, NULL, SERVO_TASK_PRIORITY,&ServoMotorHandle);
	//
	
	//setup temperature/humidity sensor
	if(HIH8120_OK != hih8120Create()) {
		printf("Failed to initialize temperature sensor\n");
		return 1;
	}
	//setup servoMotor
	rcServoCreate();

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

