#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
  gpio_num_t pspd_pin; //speed plus pin
  gpio_num_t mspd_pin; //speed minus pin
  gpio_num_t sw; //stop/walk = switch
  uint8_t max_speed;
  TaskHandle_t worker;
} SpeedCtrl_t;


typedef struct {
  float rspeed; // Km/h
  uint32_t duration; // seconds
} SpdCtrlRec_t;

void speedctrl_init(SpeedCtrl_t* self);
void speedctrl_do(float rspeed, float ispeed);
void speedctrl_start();
void speedctrl_slowdown(float ispeed);
void speedctrl_stop();
void speedctrl_put(SpdCtrlRec_t* rq); 
BaseType_t speedctrl_get(SpdCtrlRec_t* rq);
/* BaseType_t speedctrl_speedup(float origin, float target); */
/* BaseType_t speedctrl_slowdown(float origin, float target); */
