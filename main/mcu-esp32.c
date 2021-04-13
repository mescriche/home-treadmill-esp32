#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "ambsensor.h"
#include "speedctrl.h"
#include "speedmeter.h"
#include "slopectrl.h"
#include "buzzer.h"
#include "screen.h"
#include "keypad.h"
#include "mcu-esp32.h"



#define M_PI 3.14159265358979323846
#define WHEEL_DIAMETER 6.5 // cm
#define STACK_SIZE 2048
#define LED_PRIORITY 1 // 0 is lowest priority used by the idle task
#define PRO_CPU 0
#define APP_CPU 1

//#define TEST_PIN KB_SPDP
//#define GREEN_LED TM_SPDP

const char *TAG = "HT";

QueueHandle_t rspeed_queue;

Blackboard bbrd = {._switch = OFF, .status = OFF_STOPPED,
  .temperature = 0.0, .humidity = 0.0,
  .rspeed = 0.0, .ispeed = 0.0, .aspeed = 0.0,
  .slope = 0,    .distance = 0, .duration = 0};

Platform pltfrm = {
  .buzzer =     {.pin = BUZZER },
  .screen =     {.sda=LCD_SDA, .scl=LCD_SCL, .dc=LCD_DC, .rst=LCD_RST, .cs=LCD_CS, .bl=LCD_BL },
  .keypad =     {.sda=KP_SDA, .scl=KP_SCL, .intr=KP_INT },
  .speedmeter = {.pin=TM_ISPD,  .unit=PCNT_UNIT_0, .channel=PCNT_CHANNEL_0, .wheel_diameter=6.5 },
  .speedctrl =  {.pspd_pin=TM_SPDP, .mspd_pin=TM_SPDM, .sw=TM_SW, .max_speed=MAX_SPEED},
  .slopectrl =  {.pslp_pin=TM_INCP, .mslp_pin=TM_INCM, .nlevels= MAX_SLOPE, .height=11.5, .lenght=115},
  .ambsensor =  {.i2c_sda=I2C_SDA, .i2c_scl=I2C_SCL}
};


static void ambsensor_updater(void *pvParameter)
{
  static float temperature, humidity;
  //UBaseType_t uxHighWaterMark;
  TickType_t xLastWakeTime =  xTaskGetTickCount();
  vTaskDelay(pdMS_TO_TICKS(5000));
  while(1){
    if(ambsensor_read(&temperature, &humidity) == pdTRUE){
	bbrd.temperature = temperature;
	bbrd.humidity = humidity;
    };

    //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    //ESP_LOGI(TAG, "AMBSENSOR :: watermark=%d", uxHighWaterMark);
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(5000));
  }
}

static void screen_updater(void *pvParameter)
{
  TickType_t xLastWakeTime =  xTaskGetTickCount();
  //UBaseType_t uxHighWaterMark;
  while(1){
    //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    //ESP_LOGI(TAG, "SCREEN :: watermark=%d", uxHighWaterMark);
    show_status(bbrd.status);
    show_speed(bbrd.rspeed, bbrd.ispeed);
    show_slope(bbrd.slope);
    show_duration(bbrd.duration);
    show_distance(bbrd.distance);
    show_aspeed(bbrd.aspeed);
    show_ambsensor(bbrd.temperature, bbrd.humidity);
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
  }
}

static void speedctrl_actor(void* pvParameter)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  //BaseType_t xResult;
  //UBaseType_t uxHighWaterMark;
  while(1){
    //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    //ESP_LOGI(TAG, "SPEEDCTRL :: watermark=%d", uxHighWaterMark);
    if (bbrd._switch != OFF)
      speedctrl_do(bbrd.rspeed, bbrd.ispeed);
    else speedctrl_slowdown(bbrd.ispeed);
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
  }
}

/* static void speedctrl_req_actor(void * pvParameter) */
/* { */
/*   TickType_t xLastWakeTime = xTaskGetTickCount(); */
/*   TickType_t start, end, now, begin = xLastWakeTime; */
/*   SpdCtrlRec_t req_backup, request = {0, 5}; */
/*   BaseType_t xResult; */
/*   UBaseType_t uxHighWaterMark; */
/*   uint32_t    queue_event = pdFALSE; //cleared queue */
/*   while(1){ */
/*     // uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL ); */
/*     //ESP_LOGI(TAG, "SPEEDCTRL :: watermark=%d", uxHighWaterMark); */
/*     now = xTaskGetTickCount(); */
/*     req_backup = request; */
/*     xResult = speedctrl_get(&request); // no wait */
/*     ESP_LOGI(TAG, "SPEEDCTRL :: ENGINE:request={speed=%.1f, duration=%d}, time=%d ", request.rspeed, request.duration, (now-begin)/100 ); */
/*     if (xResult == pdFALSE) request = req_backup; */
/*     start = xTaskGetTickCount(); */
/*     end = start + request.duration * 100; */
/*     do { */
/*       speedctrl_do(request.rspeed, bbrd.ispeed); */
/*       now = xTaskGetTickCount(); */
/*       //ESP_LOGI(TAG, "SPEEDCTRL :: DO rspeed=%.1f, ispeed=%.1f, time=%d} ", request.rspeed, bbrd.ispeed, (now - start)/100); */
/*       queue_event = ulTaskNotifyTake(pdTRUE, (TickType_t)0); //clear on exit, no wait */
/*       //ESP_LOGI(TAG, "SPEEDCTRL :: QUEUE EVENT=%d", queue_event); */
/*       vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100)); */
/*     } while (xLastWakeTime < end && queue_event != pdTRUE); */
/*   } */
/* } */

static void slopectrl_actor()
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  //UBaseType_t uxHighWaterMark;
  while(1){
    //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    //ESP_LOGI(TAG, "SLOPE :: watermark=%d", uxHighWaterMark);
    if (bbrd._switch == ON) slopectrl_do(bbrd.slope);
    else slopectrl_getdown();
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(5000)); // 14 seconds is minimum time
  }
}

static void speedmeter_updater(void* pvParameter)
{
  const float step = (M_PI * WHEEL_DIAMETER + 2)/100; // meters
  BaseType_t xResult;
  //UBaseType_t uxHighWaterMark;
  SpeedRecord_t spd_rec;
  float distance, time_delta, speed;
  TickType_t timestamp = xTaskGetTickCount();
  while(1){
    xResult = speedmeter_read(&spd_rec);
    //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    //ESP_LOGI(TAG, "SPEEDMETER :: watermark=%d", uxHighWaterMark);
    if ( xResult == pdPASS) {
      distance = step * spd_rec.nstep; // meters
      time_delta = (spd_rec.timestamp - timestamp)/100.0; // seconds
      speed = distance / time_delta;
      bbrd.ispeed = 3.6 * speed;
      bbrd.nsteps += spd_rec.nstep;
      bbrd.distance = step * (bbrd.nsteps);
      //ESP_LOGI(TAG, "SPEEDMETER:: speed=%f, steps=%d, time_delta=%f", 3.6*speed, spd_rec.nstep, time_delta); 
      timestamp = spd_rec.timestamp;
    } else bbrd.ispeed = 0;
  }
}

extern bool keypad_read(uint8_t* value);

static void start()
{
  bbrd.nsteps = 0;
  bbrd.start = xTaskGetTickCount();
  bbrd.duration = 0;
  bbrd.distance = 0;
  speedctrl_start();
  bbrd.rspeed = 4.0;
  //speedctrl_speedup(bbrd.ispeed, bbrd.rspeed);
  buzzer_beep_START();
}

static void stop()
{
  bbrd.rspeed = 0;
  //speedctrl_slowdown(bbrd.ispeed, 0);
  //xTaskNotify(pltfrm.speedctrl.worker, pdTRUE, eSetValueWithOverwrite);
  while(bbrd.ispeed > 2.0) vTaskDelay(pdMS_TO_TICKS(1000));
  speedctrl_stop();
  //
  bbrd.slope = 0;
  slopectrl_stop();
  bbrd.end = xTaskGetTickCount();
  bbrd.duration = (bbrd.end - bbrd.start)/100;
  bbrd.aspeed = 3.6 * bbrd.distance / bbrd.duration; // Km/h
  vTaskDelay(pdMS_TO_TICKS(2000));
  buzzer_beep_STOP();
}

static void keypad_reader(void * pvParameter)
{
  //TaskHandle_t stopTask = NULL;
  uint8_t  keypad_value;
  //SpdCtrlRec_t request = {0, 5};
  //  BaseType_t xResult;
  UBaseType_t uxHighWaterMark;
  while (1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); //clear on exit, wait forever

    uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    ESP_LOGI(TAG, "KEYPAD :: watermark=%d", uxHighWaterMark);
    
    if (keypad_read(&keypad_value) != pdTRUE ) continue;
    ESP_LOGI(TAG, "KEYPAD :: KEYCODE %X",  keypad_value & 0xFF);

    // ON / OFF
    if (keypad_value & 0x80){
      if (bbrd._switch == PAUSE){
	bbrd.start += xTaskGetTickCount() - bbrd.pause_start;
	bbrd.rspeed = (bbrd.rspeed_backup > 6) ? 6 : bbrd.rspeed_backup;
	//speedctrl_speedup(bbrd.ispeed, bbrd.rspeed);
	bbrd.slope = bbrd.slope_backup;
	bbrd._switch = ON;
	speedctrl_start();
	continue;
      }
      
      //
      bbrd._switch = (bbrd._switch == OFF) ? ON : OFF;
      buzzer_beep_keyOK();
      if (bbrd._switch == ON) start();
      else if (bbrd._switch == OFF) stop();
      continue;
    }
    if (bbrd._switch == OFF){
      buzzer_beep_keyFAIL();
      continue;
    }
    // PAUSE
    if (keypad_value & 0x40){
      bbrd._switch = (bbrd._switch == ON) ? PAUSE : ON;
      buzzer_beep_keyOK();
      if (bbrd._switch == ON){
	bbrd.start += xTaskGetTickCount() - bbrd.pause_start;
	bbrd.rspeed = bbrd.rspeed_backup;
	//speedctrl_speedup(bbrd.ispeed, bbrd.rspeed);
	bbrd.slope = bbrd.slope_backup;
	
      } else if (bbrd._switch == PAUSE){
	bbrd.pause_start = xTaskGetTickCount();
	bbrd.rspeed_backup = bbrd.rspeed;
	bbrd.slope_backup = bbrd.slope;
	bbrd.slope = 0;
	bbrd.rspeed = 0;
	/* if (speedctrl_slowdown(bbrd.ispeed, 0) == pdTRUE) */
	/*   xTaskNotify(pltfrm.speedctrl.worker, pdTRUE, eSetValueWithOverwrite); */
	/* else { */
	/*   request.rspeed = 0; request.duration = 5; */
	/*   speedctrl_put(&request); */
	/* } */

	//ESP_LOGI(TAG, "KEYPAD :: PAUSE, send NOTIFY ");
      }      
      continue;
    }
    
    if (bbrd._switch != ON){
      buzzer_beep_keyFAIL();
      continue;
    }
    
    float rspeed = bbrd.rspeed;
    uint32_t slope = bbrd.slope;
  
    
    if ((keypad_value & 0x04) && (bbrd.rspeed >= 1)) --bbrd.rspeed;  //SPD--
    else if ((keypad_value & 0x08) && (bbrd.rspeed >= 0.1)) bbrd.rspeed -= 0.1; //SPD-
    else if ((keypad_value & 0x01) && (bbrd.slope > 0)) --bbrd.slope; //INC-
    else if ((keypad_value & 0x02) && (bbrd.slope <  MAX_SLOPE)) ++bbrd.slope; //INC+
    else if ((keypad_value & 0x10) && (bbrd.rspeed <= (MAX_SPEED - 0.1)))  bbrd.rspeed += 0.1;//SPD+
    else if ((keypad_value & 0x20) && (bbrd.rspeed <= (MAX_SPEED - 1))) ++bbrd.rspeed;//SPD++

    void (*beeper)();    
    if ((rspeed != bbrd.rspeed) || (slope != bbrd.slope)) {
      beeper = &buzzer_beep_keyOK;
      show_speed(bbrd.rspeed, bbrd.ispeed);
      show_slope(bbrd.slope);
    } else beeper = &buzzer_beep_keyFAIL;
    (*beeper)();
    
  }
}

extern void stats_task(void *arg);
extern void heart_beat(void *arg);

void app_main()
{

  BaseType_t xResult;
  UBaseType_t uxHighWaterMark;
  //
  buzzer_init(&pltfrm.buzzer);
  uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
  ESP_LOGI(TAG, "HELLO WORLD!! :: watermark=%d", uxHighWaterMark);
  //
  keypad_init(&pltfrm.keypad);
  xResult = xTaskCreatePinnedToCore(keypad_reader, "keypad", STACK_SIZE + 1024, NULL, 10, &pltfrm.keypad.reader, PRO_CPU);
  configASSERT(xResult == pdPASS);
  //
  buzzer_beep_HELLO();
  screen_init(&pltfrm.screen);
  show_message(WELCOME);
  vTaskDelay(pdMS_TO_TICKS(2000)); // await a few seconds to display welcome
  //
  xResult = xTaskCreatePinnedToCore(screen_updater, "screen", STACK_SIZE, NULL, 4, &pltfrm.screen.updater, APP_CPU);
  configASSERT(xResult == pdPASS);
  //
  speedmeter_init(&pltfrm.speedmeter);
  vTaskDelay(pdMS_TO_TICKS(1000));
  xResult = xTaskCreatePinnedToCore(speedmeter_updater, "speedmeter", STACK_SIZE, NULL, 8, &pltfrm.speedmeter.updater, PRO_CPU);
  configASSERT(xResult == pdPASS);
  //
  //xResult = xTaskCreate(heart_beat, "heart_beat", STACK_SIZE, (void*)GPIO_NUM_16, 8, NULL);
  //configASSERT(xResult == pdPASS);
  //
  vTaskDelay(pdMS_TO_TICKS(1000));
  speedctrl_init(&pltfrm.speedctrl);
  xResult = xTaskCreatePinnedToCore(speedctrl_actor, "speedctrl", STACK_SIZE + 1024, NULL, 7, &pltfrm.speedctrl.worker, PRO_CPU);
  configASSERT(xResult == pdPASS);
  //
  slopectrl_init(&pltfrm.slopectrl);
  xResult = xTaskCreatePinnedToCore(slopectrl_actor, "slopectrl", STACK_SIZE, NULL, 6, &pltfrm.slopectrl.worker, PRO_CPU);
  configASSERT(xResult == pdPASS);
  //
  vTaskDelay(pdMS_TO_TICKS(1000));
  ambsensor_init(&pltfrm.ambsensor);
  xResult = xTaskCreatePinnedToCore(ambsensor_updater, "ambsensor", STACK_SIZE, NULL, 5, &pltfrm.ambsensor.updater, APP_CPU);
  configASSERT(xResult == pdPASS);
  //
  //xResult = xTaskCreatePinnedToCore(stats_task, "stats", STACK_SIZE, NULL, 12, NULL, PRO_CPU);
  //configASSERT(xResult == pdPASS);
  //
  bbrd.start = xTaskGetTickCount();
  TickType_t xLastWakeTime =  xTaskGetTickCount();
  TickType_t now;
  //UBaseType_t uxHighWaterMark;
  while (1){
    //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    //ESP_LOGI(TAG, "MAIN :: watermark=%d", uxHighWaterMark);
    now = xTaskGetTickCount();
    if (bbrd._switch == ON){
      bbrd.duration = (now - bbrd.start)/100;
      bbrd.status = (bbrd.ispeed > 0) ? ON_RUNNING: ON_READY;
    }
    else if (bbrd._switch == OFF)
      bbrd.status = (bbrd.ispeed > 0) ? OFF_STOPPING : OFF_STOPPED;
    else if (bbrd._switch == PAUSE){
      bbrd.status = ON_PAUSE;
      if ((now -  bbrd.pause_start)/100 > 5*3600) {
	bbrd._switch = OFF;
	bbrd.start += now - bbrd.pause_start;
	stop();
      }
    }
    if (bbrd.duration > 0)
      bbrd.aspeed =  3.6 * bbrd.distance / bbrd.duration; // Km/h
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
  }
}
  
