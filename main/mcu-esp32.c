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
#include "session.h"
#include "mcu-esp32.h"



#define M_PI 3.14159265358979323846
#define WHEEL_DIAMETER 6.5 // cm
#define STACK_SIZE 2048
#define LED_PRIORITY 1 // 0 is lowest priority used by the idle task
#define PRO_CPU 0
#define APP_CPU 1

//#define TEST_PIN KB_SPDP
//#define GREEN_LED TM_SPDP


static const char *TAG = "HT";

QueueHandle_t ispeed_track;
//QueueHandle_t ispeed_mailbox;

Blackboard bbrd = {.lead = MANUAL, .mode = WELCOME, .status = OFF_STOPPED,
  .temperature = 0.0, .humidity = 0.0,
  .rspeed = 0.0, .ispeed = 0.0, .aspeed = 0.0,
  .rspeed_time_left = 0,
  .slope = 0,    .distance = 0, .duration = 0,
  .pause_duration = 0, 
  .programId = 0
};

Platform pltfrm = {
  .buzzer =     {.pin = BUZZER },
  .screen =     {.sda=LCD_SDA, .scl=LCD_SCL, .dc=LCD_DC, .rst=LCD_RST, .cs=LCD_CS, .bl=LCD_BL },
  .keypad =     {.sda=KP_SDA, .scl=KP_SCL, .intr=KP_INT },
  .speedmeter = {.pin=TM_ISPD,  .unit=PCNT_UNIT_0, .channel=PCNT_CHANNEL_0, .wheel_diameter=6.5 },
  .speedctrl =  {.pspd_pin=TM_SPDP, .mspd_pin=TM_SPDM, .sw=TM_SW, .max_speed=MAX_SPEED},
  .slopectrl =  {.pslp_pin=TM_INCP, .mslp_pin=TM_INCM, .nlevels= MAX_SLOPE, .height=11.5, .lenght=115},
  .ambsensor =  {.i2c_sda=I2C_SDA, .i2c_scl=I2C_SCL}
};

static void speed_leader(void* pvParameter);

Time ticks_to_time(TickType_t ticks)
{
  Time data;
  uint lapse = ticks / 100;
  data.min = lapse / 60;
  data.sec = lapse - (data.min * 60);
  return data;
}

static void start()
{
  BaseType_t xResult;
  bbrd.mode = RUN;
  bbrd.status = ON_READY;
  bbrd.nsteps = 0;
  bbrd.duration = 0;
  bbrd.distance = 0;
  bbrd.pause_duration = 0;
  speedctrl_start();
  if (bbrd.lead == MANUAL) bbrd.rspeed = 4.0;
  else {
    session_load_program(bbrd.programId);
    xResult = xTaskCreatePinnedToCore(speed_leader, "speedleader", STACK_SIZE, NULL, 7, &pltfrm.speedctrl.leader, PRO_CPU);
    configASSERT(xResult == pdPASS);
  }
  buzzer_beep_START();  
  bbrd.session_begin = xTaskGetTickCount();
  bbrd.start = bbrd.session_begin;

  //xTaskNotify(pltfrm.speedctrl.admin, pdTRUE, eSetValueWithOverwrite);
}

static void stop(void* pvParam)
{
  //xTaskNotify(pltfrm.speedctrl.admin, pdFALSE, eSetValueWithOverwrite);
  if (bbrd.lead == PROGRAMMED){
    session_end_program();
    vTaskDelete(pltfrm.speedctrl.leader);
  }
  bbrd.rspeed = 0;
  bbrd.slope = 0;
  bbrd.mode = REPORT;
  bbrd.status = OFF_STOPPING;
  bbrd.lead = MANUAL;
  vTaskDelay(pdMS_TO_TICKS(1000));
  bbrd.session_end = xTaskGetTickCount();
  bbrd.duration = (bbrd.session_end - bbrd.session_begin - bbrd.pause_duration)/100;
  bbrd.aspeed = 3.6 * bbrd.distance / (float) bbrd.duration; // Km/h
  while(bbrd.ispeed > 2.0) vTaskDelay(pdMS_TO_TICKS(1000));
  speedctrl_stop();
  slopectrl_stop();
  buzzer_beep_STOP();
  vTaskDelete(NULL);
}

static void ambsensor_updater(void *pvParameter)
{
  float temperature, humidity;
  //UBaseType_t uxHighWaterMark;
  TickType_t xLastWakeTime =  xTaskGetTickCount();
  vTaskDelay(pdMS_TO_TICKS(5000));
  while(1){
    //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    //ESP_LOGI(TAG, "AMBSENSOR :: watermark=%d", uxHighWaterMark);
    if(ambsensor_read(&temperature, &humidity) == pdTRUE){
	bbrd.temperature = temperature;
	bbrd.humidity = humidity;
    };
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(5000));
  }
}

static void screen_updater(void *pvParameter)
{
  TickType_t xLastWakeTime =  xTaskGetTickCount();
  char* username;
  Program *prgm = session_get_program(bbrd.programId);
  float *speed_prgm;
  static uint8_t mode = 0, status = 0;
  //UBaseType_t uxHighWaterMark;
  while(1){
    //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    //ESP_LOGI(TAG, "SCREEN :: watermark=%d", uxHighWaterMark);
    if ((bbrd.mode != mode)||(bbrd.status != status)) clear_screen();
    mode = bbrd.mode; status = bbrd.status;
    switch(bbrd.mode){
    case CONF:
      if (ulTaskNotifyTake(pdTRUE, (TickType_t)0) == pdTRUE){
	if (bbrd.lead == MANUAL)
	  show_configuration_manual_screen();
	else  {
	  prgm = session_get_program(bbrd.programId);
	  speed_prgm = malloc(prgm->duration * sizeof(float));
	  session_get_speed_program(bbrd.programId, speed_prgm);
	  username = session_get_username();
	  show_configuration_program_screen(username, bbrd.programId,
					    prgm->aspeed, prgm->max_speed, prgm->duration,
					    prgm->max_slope, speed_prgm);
	  free(speed_prgm);
	}
      }
      break;
    case RUN:
      show_run_screen(bbrd.lead, bbrd.mode, bbrd.status, bbrd.rspeed, bbrd.is_speed, bbrd.aspeed,
		      bbrd.slope, bbrd.distance, bbrd.duration,
		      bbrd.temperature, bbrd.humidity);
      if (bbrd.lead == PROGRAMMED){
	//ESP_LOGI(TAG, "SCREEN :: spent=%d, total=%d, ratio=%3.1f", bbrd.duration, prgm_duration, progress);
	show_lead_run_screen(bbrd.programId, bbrd.rspeed, bbrd.rspeed_next, bbrd.slope, bbrd.slope_next,
			     bbrd.rspeed_time_left, bbrd.duration, prgm->duration*60);
      }
      break;
    case PAUSE:
      show_pause_screen(PAUSE_MAX - (bbrd.pause_duration + (xLastWakeTime -  bbrd.pause_begin))/100);
      break;
    case REPORT:
      if (bbrd.status == OFF_STOPPING){
	show_run_screen(bbrd.lead, bbrd.mode, bbrd.status, bbrd.rspeed, bbrd.ispeed, bbrd.aspeed,
			bbrd.slope, bbrd.distance, bbrd.duration,
			bbrd.temperature, bbrd.humidity);
	xTaskNotify(pltfrm.screen.updater, pdTRUE, eSetValueWithOverwrite);
      } else if ((bbrd.status == OFF_STOPPED) && (ulTaskNotifyTake(pdTRUE, (TickType_t)0) == pdTRUE)) {
	int32_t race_size = ((bbrd.session_end - bbrd.session_begin)/100); // seconds
	if (race_size <= 60) 
	  show_report_screen(bbrd.status, bbrd.aspeed, bbrd.distance, bbrd.duration, race_size, bbrd.srace);
	else {
	  race_size /= 60; // minutes
	  show_report_screen(bbrd.status, bbrd.aspeed, bbrd.distance, bbrd.duration, race_size, bbrd.race);
	}
      }
      break;
    default: show_welcome_screen();
    };
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
    switch (bbrd.mode){
    case RUN: speedctrl_do(bbrd.rspeed, bbrd.ispeed); break;
    case PAUSE:
      if (bbrd.ispeed > 3) speedctrl_do(3, bbrd.ispeed);
      else speedctrl_slowdown(bbrd.ispeed);
      break;
    default: speedctrl_slowdown(bbrd.ispeed); 
    };
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
  }
}

static void speed_leader(void* pvParameter)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  TickType_t deadline = xLastWakeTime;
  Order  order, next_order;
  //UBaseType_t uxHighWaterMark;
  
  while (1) {
    //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    //ESP_LOGI(TAG, "SPEED LEADER :: watermark=%d", uxHighWaterMark);
    switch (bbrd.mode){
    case RUN:
      //ESP_LOGI(TAG, "SPEEDLEADER :: deadline=%d, wakeuptime=%d", deadline, xLastWakeTime); // seconds
      bbrd.rspeed_time_left = (deadline >= xLastWakeTime) ? (deadline - xLastWakeTime)/100 : 0;
      //ESP_LOGI(TAG, "SPEEDLEADER :: timeleft=%d", bbrd.rspeed_time_left); // seconds
      if ( xLastWakeTime >= deadline){
	session_get_order(&order, &next_order);  // wait forever
	if (order.speed == 0){
	  xTaskCreatePinnedToCore(stop, "stop", STACK_SIZE, NULL, 12, NULL, PRO_CPU);
	  //	  vTaskSuspend(NULL);
	} else {
	  buzzer_beep_RSPEED();
	  bbrd.rspeed = order.speed;
	  bbrd.rspeed_next = next_order.speed;
	  bbrd.slope = order.slope;
	  bbrd.slope_next = next_order.slope;
	  deadline = xTaskGetTickCount() + order.duration * 60 * 100; // ticks
	}
      }
      break;
    case PAUSE:
      deadline += pdMS_TO_TICKS(1000); // added 1 second for each loop step
      break;
    default: deadline = xLastWakeTime;
    };
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
  }
}

static void slopectrl_actor()
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  //UBaseType_t uxHighWaterMark;
  while(1){
    //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    //ESP_LOGI(TAG, "SLOPE :: watermark=%d", uxHighWaterMark);
    switch (bbrd.mode){
    case RUN: slopectrl_do(bbrd.slope); break;
    default: slopectrl_getdown();
    };
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(5000)); // 14 seconds is minimum time
  }
}

static void speedmeter_updater(void* pvParameter)
{
  const float step = (M_PI * WHEEL_DIAMETER + 2)/100; // meters
  static ISpeedRecord ispeed;
  BaseType_t xResult;
  //UBaseType_t uxHighWaterMark;
  SpeedRecord_t spd_rec;
  float distance, speed;
  TickType_t time_delta, timetick = xTaskGetTickCount();

  while(1){
    xResult = speedmeter_read(&spd_rec); // waits 3 seconds to report 0 speed
    //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    //ESP_LOGI(TAG, "SPEEDMETER :: watermark=%d", uxHighWaterMark);
    if ( xResult == pdPASS) {
      distance = step * spd_rec.nstep; // meters
      time_delta = spd_rec.timetick - timetick; // ticks
      if (time_delta > 0){
	speed = 100 * distance / (float) time_delta;
	ispeed.speed = 3.6 * speed;
	ispeed.timetick = spd_rec.timetick;
	//	ESP_LOGI(TAG, "SPEEDMETER:: speed=%3.1f, timetick=%d, delta=%d", 3.6*speed, spd_rec.timetick, time_delta); 
      }
      //      else ESP_LOGI(TAG, "SPEEDMETER:: error :: time_delta=%d", time_delta);
      bbrd.nsteps += spd_rec.nstep;
      bbrd.distance = step * bbrd.nsteps;
      timetick = spd_rec.timetick;
    } else {
      timetick =  xTaskGetTickCount();
      ispeed.speed = 0;
      ispeed.timetick = timetick;
    }
    bbrd.ispeed = ispeed.speed;
    //ESP_LOGI(TAG, "SPEEDMETER:: ispeed record(%3.1f, %d)", ispeed.speed, ispeed.timetick);
    xQueueSend(ispeed_track, &ispeed, pdMS_TO_TICKS(50)); // Tmin = 62 ms for fmax = 16Hz
  }
}

static void ispeed_recorder(TickType_t start, TickType_t now)
{
  //static float ispeed_book[60]; // 60 seconds
  static ISpeedRecord ispeed, ispeed_prev = {0,0};
  Time ptime, etime;
  uint16_t nsec;
  float av_speed = 0, speed_delta = 0;
  uint32_t delta, delta_sum = 0;
  int32_t nmin;
  
  // ESP_LOGI(TAG, "SPEED RECORDED :: start=%d, now=%d", start, now);
  
  ptime = ticks_to_time(now - start);
  nsec = (ptime.sec == 0) ? 59 : ptime.sec - 1;

  //ESP_LOGI(TAG, "SPEED RECORDED :: ptime[mm:ss]=%02u:%02u, nsec=%02u", ptime.min, ptime.sec, nsec);
  bool loop;
  do {
    loop = false;
    if (xQueuePeek(ispeed_track, &ispeed, pdMS_TO_TICKS(50))){
      etime = ticks_to_time(ispeed.timetick - start);
      //	ESP_LOGI(TAG, "etime[mm:ss]=%02u:%02u", etime.min, etime.sec);
      if ( nsec >= etime.sec ){
	xQueueReceive(ispeed_track, &ispeed, 0);
	if (nsec == etime.sec){
	  delta = ispeed.timetick - ispeed_prev.timetick;
	  /* ESP_LOGI(TAG, "SPEED RECORDED :: ispeed=%.1f, tick=%d, delta=%d, %02u:%02u", */
	  /* 	     ispeed.speed, ispeed.timetick, delta, */
	  /* 	     etime.min, etime.sec); */
	  speed_delta += ispeed.speed * delta;
	  delta_sum += delta;
	}
	//	else ESP_LOGI(TAG, "SPEED RECORDED :: discarded: (ispeed=%.1f, tick=%d)", ispeed.speed, ispeed.timetick);
	ispeed_prev = ispeed;
	loop = true;
      }
    }
  } while (loop);
  if (delta_sum > 0) av_speed = speed_delta / (float) delta_sum;
  else av_speed = 0;
  //ispeed_book[nsec] = av_speed;
  bbrd.srace[nsec] = av_speed;
  bbrd.is_speed = av_speed;
  //xQueueOverwrite(ispeed_mailbox, &av_speed);
  //ESP_LOGI(TAG, "SPEED RECORDER :: srace[%d]=%.1f",nsec, bbrd.srace[nsec]);
  
  if (nsec == 59){
    av_speed = 0;
    for(uint16_t i=0; i<60; i++) av_speed += bbrd.srace[i];
    nmin = ptime.min - 1;
    if ((nmin >= 0) && (nmin < MAX_TIME)){
      bbrd.race[nmin] = av_speed/60.0;
      ESP_LOGI(TAG, "SPEED RECORDED :: ptime[mm:ss]= %02u:%02u, race[%d]=%.1f", ptime.min, ptime.sec, nmin, bbrd.race[nmin]);
    }
  }
}


extern bool keypad_read(uint8_t* value);

static void keypad_reader(void * pvParameter)
{
  uint8_t  keypad_value;
  //  BaseType_t xResult;
  //UBaseType_t uxHighWaterMark;
  while (1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); //clear on exit, wait forever
    //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    //ESP_LOGI(TAG, "KEYPAD :: watermark=%d", uxHighWaterMark);
    //ESP_LOGI(TAG, "Lead=%d, Mode=%d, Status=%d", bbrd.lead, bbrd.mode, bbrd.status);
    if (keypad_read(&keypad_value) != pdTRUE ) continue;
    //ESP_LOGI(TAG, "KEYPAD :: KEYCODE %X",  keypad_value & 0xFF);

    bool beep = true;
    switch (bbrd.mode){
    case WELCOME:
      bbrd.mode = CONF; bbrd.lead = MANUAL;
      break;
    case CONF:
      if ((keypad_value & 0x80) && (bbrd.status == OFF_STOPPED)){
	bbrd.mode = RUN;
	start();
      }
      else if (keypad_value & 0x40){
	bbrd.lead = (bbrd.lead == MANUAL) ? PROGRAMMED : MANUAL;
	xTaskNotify(pltfrm.screen.updater, pdTRUE, eSetValueWithOverwrite);
      }
      else if (keypad_value & 0x20) {
	session_set_user(3); bbrd.programId=0;
	xTaskNotify(pltfrm.screen.updater, pdTRUE, eSetValueWithOverwrite);
      }
      else if (keypad_value & 0x10) {
	session_set_user(2); bbrd.programId=0;
	xTaskNotify(pltfrm.screen.updater, pdTRUE, eSetValueWithOverwrite);
      }
      else if (keypad_value & 0x08) {
	session_set_user(1); bbrd.programId=0;
	xTaskNotify(pltfrm.screen.updater, pdTRUE, eSetValueWithOverwrite);
      }
      else if (keypad_value & 0x04) {
	session_set_user(0); bbrd.programId=0;
	xTaskNotify(pltfrm.screen.updater, pdTRUE, eSetValueWithOverwrite);
      }
      else if ((keypad_value & 0x02) && (bbrd.programId < session_get_nPrograms()-1)) {
	bbrd.programId++;
	xTaskNotify(pltfrm.screen.updater, pdTRUE, eSetValueWithOverwrite);
      }
      else if ((keypad_value & 0x01) && (bbrd.programId > 0)) {
	bbrd.programId--;
	xTaskNotify(pltfrm.screen.updater, pdTRUE, eSetValueWithOverwrite);
      }
      else beep = false;
      break;
    case RUN: 
      if (keypad_value & 0x80){
	xTaskCreatePinnedToCore(stop, "stop", STACK_SIZE, NULL, 12, NULL, PRO_CPU);
	xTaskNotify(pltfrm.screen.updater, pdTRUE, eSetValueWithOverwrite);
      }
      else if (keypad_value & 0x40) { // start PAUSE
	bbrd.mode = PAUSE;
	bbrd.pause_begin = xTaskGetTickCount();
	bbrd.rspeed_backup = bbrd.rspeed;
	bbrd.slope_backup = bbrd.slope;
	bbrd.slope = 0;
	bbrd.rspeed = 0;
      }
      else if ((keypad_value & 0x20) && (bbrd.rspeed <= (MAX_SPEED - 1))) ++bbrd.rspeed;//SPD++      
      else if ((keypad_value & 0x10) && (bbrd.rspeed <= (MAX_SPEED - 0.1)))  bbrd.rspeed += 0.1;//SPD+
      else if ((keypad_value & 0x08) && (bbrd.rspeed >= 0.1)) bbrd.rspeed -= 0.1; //SPD-
      else if ((keypad_value & 0x04) && (bbrd.rspeed >= 1)) --bbrd.rspeed;  //SPD--      
      else if ((keypad_value & 0x02) && (bbrd.slope <  MAX_SLOPE)) ++bbrd.slope; //INC+      
      else if ((keypad_value & 0x01) && (bbrd.slope > 0)) --bbrd.slope; //INC-
      else beep = false;
      break;
    case PAUSE:
      if (keypad_value & 0x80)
	xTaskCreatePinnedToCore(stop, "stop", STACK_SIZE, NULL, 12, NULL, PRO_CPU);
      else if ((keypad_value & 0x40) && (bbrd.status == OFF_STOPPED))  { // end PAUSE
	bbrd.mode = RUN;
	bbrd.pause_end = xTaskGetTickCount();
	bbrd.pause_duration += bbrd.pause_end - bbrd.pause_begin;
	bbrd.rspeed = (bbrd.rspeed_backup > 6) ? 6 : bbrd.rspeed_backup;
	bbrd.slope = bbrd.slope_backup;
      }
      else beep = false;
      break;
    case REPORT:
      if ((keypad_value & 0x80) && (bbrd.status == OFF_STOPPED)){
	bbrd.mode = CONF;
	xTaskNotify(pltfrm.screen.updater, pdTRUE, eSetValueWithOverwrite);
      }
      else if ((keypad_value & 0x40) && (bbrd.status == OFF_STOPPED)) {
	bbrd.mode = CONF;
	xTaskNotify(pltfrm.screen.updater, pdTRUE, eSetValueWithOverwrite);
      }
      else beep = false;
      break;
    };
    //
    if (beep) buzzer_beep_keyOK();
    else buzzer_beep_keyFAIL();
  }
}

extern void stats_task(void *arg);
extern void heart_beat(void *arg);

void app_main()
{
  BaseType_t xResult;
  TickType_t   xLastWakeTime, now;
  bbrd.start =  xTaskGetTickCount();
  session_init();
  bbrd.programId = 0;
  //UBaseType_t uxHighWaterMark;
  //ispeed_mailbox = xQueueCreate(1, sizeof(float));
  ispeed_track = xQueueCreate(20, sizeof(ISpeedRecord));
  //xQueueSend(ispeed_mailbox, &bbrd.ispeed, (TickType_t)0);
  //
  buzzer_init(&pltfrm.buzzer);
  //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
  //ESP_LOGI(TAG, "HELLO WORLD!! :: watermark=%d", uxHighWaterMark);
  //
  keypad_init(&pltfrm.keypad);
  xResult = xTaskCreatePinnedToCore(keypad_reader, "keypad", STACK_SIZE + 1024, NULL, 10, &pltfrm.keypad.reader, PRO_CPU);
  configASSERT(xResult == pdPASS);
  //
  buzzer_beep_HELLO();
  screen_init(&pltfrm.screen);
  //
  xResult = xTaskCreatePinnedToCore(screen_updater, "screen", STACK_SIZE + 1024, NULL, 4, &pltfrm.screen.updater, APP_CPU);
  configASSERT(xResult == pdPASS);
  //
  speedmeter_init(&pltfrm.speedmeter);
  vTaskDelay(pdMS_TO_TICKS(1000));
  xResult = xTaskCreatePinnedToCore(speedmeter_updater, "speedmeter", STACK_SIZE, NULL, 8, &pltfrm.speedmeter.updater, PRO_CPU);
  configASSERT(xResult == pdPASS);
  //
  vTaskDelay(pdMS_TO_TICKS(1000));
  speedctrl_init(&pltfrm.speedctrl);
  xResult = xTaskCreatePinnedToCore(speedctrl_actor, "speedctrl", STACK_SIZE, NULL, 7, &pltfrm.speedctrl.controller, PRO_CPU);
  configASSERT(xResult == pdPASS);
  
  /* xResult = xTaskCreatePinnedToCore(speed_leader, "speedleader", STACK_SIZE, NULL, 7, &pltfrm.speedctrl.leader, PRO_CPU); */
  /* configASSERT(xResult == pdPASS); */
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
  //  bbrd.session_begin = xTaskGetTickCount();
  xLastWakeTime =  xTaskGetTickCount();
  
  //Time work_time;
  //UBaseType_t uxHighWaterMark;

  while (1){
    //xQueuePeek(ispeed_mailbox, &ispeed, (TickType_t)0);
    //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    //ESP_LOGI(TAG, "MAIN :: watermark=%d", uxHighWaterMark);
    now = xLastWakeTime;
    switch (bbrd.mode){
    case WELCOME:
      if (now > bbrd.start + pdMS_TO_TICKS(5000)){
	bbrd.mode = CONF;
	xTaskNotify(pltfrm.screen.updater, pdTRUE, eSetValueWithOverwrite);
      }
      break;
    case CONF:
      bbrd.status = (bbrd.ispeed > 0) ? OFF_STOPPING : OFF_STOPPED;
      if ((now - bbrd.start)/100 > 60) // 60 seconds = 1 minutes
	bbrd.start = now;
      break;
    case RUN:
      bbrd.duration = (now - bbrd.session_begin - bbrd.pause_duration )/100;
      bbrd.status = (bbrd.ispeed > 0) ? ON_RUNNING : ON_READY;
      if ((now - bbrd.session_begin)/100 >= MAX_TIME * 60)
	xTaskCreatePinnedToCore(stop, "stop", STACK_SIZE, NULL, 12, NULL, PRO_CPU);
      break;
    case PAUSE:
      bbrd.status = (bbrd.ispeed > 0) ? OFF_STOPPING : OFF_STOPPED;
      bbrd.pause_end = now;
      if (bbrd.pause_end -  bbrd.pause_begin >= PAUSE_MAX * 100 - bbrd.pause_duration){
	bbrd.pause_duration += bbrd.pause_end - bbrd.pause_begin;
	xTaskCreatePinnedToCore(stop, "stop", STACK_SIZE, NULL, 12, NULL, PRO_CPU);
      }
      break;
    case REPORT:
      bbrd.status = (bbrd.ispeed > 0) ? OFF_STOPPING : OFF_STOPPED;
      if ((now - bbrd.session_end)/100 > 300){ // 300 seconds = 5 minutes
	bbrd.mode = CONF;
	bbrd.start = now;
	xTaskNotify(pltfrm.screen.updater, pdTRUE, eSetValueWithOverwrite);
      }
      break;
    };
    ispeed_recorder(bbrd.start, now);
    if (bbrd.duration > 0)
      bbrd.aspeed =  3.6 * bbrd.distance /(float)bbrd.duration; // Km/h
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
  }
}
  
