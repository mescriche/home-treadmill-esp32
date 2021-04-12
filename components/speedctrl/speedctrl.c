#include <stdio.h>
#include <math.h>
#include "esp_log.h"
#include <speedctrl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static SpeedCtrl_t* pself;
static const char *TAG = "SPDCTRL";
static QueueHandle_t rspeed_queue;
const float speed_threshold = 0.5;

void speedctrl_init(SpeedCtrl_t* self)
{
  pself = self;
  rspeed_queue = xQueueCreate(50, sizeof(SpdCtrlRec_t));
  configASSERT(rspeed_queue != NULL);
  //
  gpio_reset_pin(self->sw);
  gpio_set_direction(self->sw, GPIO_MODE_OUTPUT);
  gpio_set_level(self->sw, 0);
  
  gpio_config_t io_conf;
  io_conf.pin_bit_mask = 1ULL << self->pspd_pin | 1ULL << self->mspd_pin;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 0;
  gpio_config(&io_conf); 
  gpio_set_level(self->pspd_pin, 0);
  gpio_set_level(self->mspd_pin, 0);
  ESP_LOGI(TAG, "SPEEDCTRL :: INIT");
  
}

void speedctrl_start()
{
  ESP_LOGI(TAG, "SPEEDCTRL :: START");
  gpio_set_level(pself->sw, 1);
  gpio_set_level(pself->mspd_pin, 0);
  gpio_set_level(pself->pspd_pin, 0);
  
}

void speedctrl_stop()
{
  ESP_LOGI(TAG, "SPEEDCTRL :: STOP");
  gpio_set_level(pself->mspd_pin, 0);
  gpio_set_level(pself->pspd_pin, 0);
  gpio_set_level(pself->sw, 0);
}

void speedctrl_slowdown(float ispeed)
{
  gpio_set_level(pself->pspd_pin, 0);
  gpio_set_level(pself->mspd_pin, 1);
  if (ispeed < 2.0){
    gpio_set_level(pself->sw, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(pself->sw, 1);
  }
}

void speedctrl_do(float rspeed, float ispeed)
{
  static float error;
  error = rspeed - ispeed;
  if (fabs(error) > speed_threshold){
    if (error > 0) { // speed up
      gpio_set_level(pself->mspd_pin, 0);
      gpio_set_level(pself->pspd_pin, 1);
    } else { // slow down
      gpio_set_level(pself->pspd_pin, 0);
      gpio_set_level(pself->mspd_pin, 1);
    }
  } else { // keep up
    gpio_set_level(pself->pspd_pin, 0);
    gpio_set_level(pself->mspd_pin, 0);
  }
}

void speedctrl_put(SpdCtrlRec_t* req_ptr)
{
  ESP_LOGI(TAG, "SPEEDCTRL :: QUEUE request={%.1f,%d }", req_ptr->rspeed, req_ptr->duration);
  xQueueSend(rspeed_queue, req_ptr, pdMS_TO_TICKS(500));
}

BaseType_t speedctrl_get(SpdCtrlRec_t* req_ptr)
{
   BaseType_t xResult;
   xResult = xQueueReceive(rspeed_queue, req_ptr, 0); // no wait
   return xResult;
}

/*   //                       speed     0, 1,  2,  3,  4,  5,  6,   7,  */
/* const  uint32_t speedup_time[]  = {1, 2,  4,  5, 15, 30, 60,  120}; */
/* BaseType_t speedctrl_speedup(float origin, float target) */
/* { */
/*   uint32_t runner; */
/*   SpdCtrlRec_t request; */
/*   if (target > 10) target = 10; */
/*   if (origin < 0 ) return pdFALSE; */
/*   if (target - origin <= 2) return pdFALSE; */
/*   // speed up */
/*   xQueueReset(rspeed_queue); //clear queue */
/*   runner = (uint8_t)origin+1; */
/*   do { // speed up */
/*     request.duration = (runner <= 7) ? speedup_time[runner] : 120; */
/*     request.rspeed = runner++; */
/*     speedctrl_put(&request); */
/*   } while (runner < target); */
/*   // finish sequence with short order  */
/*   request.rspeed = target; */
/*   request.duration = 5; */
/*   speedctrl_put(&request); */
/*   return pdTRUE; */
/* } */

/* //                speed                   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10  */
/* const  static uint32_t slowdown_time[] = {1, 1, 1, 1, 1, 1, 2, 2, 2, 2,  3}; */
/* BaseType_t speedctrl_slowdown(float origin, float target) */
/* { */
/*   uint8_t runner; */
/*   SpdCtrlRec_t request; */
/*   if (target < 0) target = 0; */
/*   if (origin - target <= 1) return pdFALSE; */
/*   // slow down */
/*   xQueueReset(rspeed_queue); //clear queue  */
/*   runner = (uint8_t)origin-1; */
/*   do { // slowdown */
/*     request.duration = (runner <= 10) ? slowdown_time[runner] : 3; */
/*     request.rspeed = runner--; */
/*     speedctrl_put(&request); */
/*   } while (runner > target); */
/*   // finish sequence with short order  */
/*   request.rspeed = target; */
/*   request.duration = 5; */
/*   speedctrl_put(&request); */
/*   return pdTRUE; */
/* } */
