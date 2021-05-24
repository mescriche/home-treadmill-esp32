#include <stdio.h>
#include "esp_log.h"
#include "slopectrl.h"
#define SLP_TMAX  14000 // mseconds to cover all levels

//static const char *TAG = "SLOPE";

static TickType_t level_time;

static SlopeCtrl_t* pself;

void slopectrl_init(SlopeCtrl_t* self)
{
  pself = self;
  self->level = 0;
  gpio_config_t io_conf;
  io_conf.pin_bit_mask = ((1ULL << self->pslp_pin) | (1ULL << self->mslp_pin));
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_down_en = 1;
  io_conf.pull_up_en = 0;
  gpio_config(&io_conf);
  gpio_set_level(self->pslp_pin, 0);
  gpio_set_level(self->mslp_pin, 0);
  level_time = pdMS_TO_TICKS(SLP_TMAX / self->nlevels);
  // 
  gpio_set_level(self->mslp_pin,1);
  vTaskDelay(2*level_time);
  gpio_set_level(self->mslp_pin,0);
  
}

void slopectrl_do(uint8_t rlevel)
{
  //  printf("slopectrl_do rlevel=%d, level=%d\n", rlevel, pself->level);
  while (rlevel > pself->level){
      gpio_set_level(pself->pslp_pin, 1);
      vTaskDelay(level_time);
      gpio_set_level(pself->pslp_pin, 0);
      pself->level++;
  };
  while (rlevel < pself->level){
    gpio_set_level(pself->mslp_pin, 1);
    vTaskDelay(level_time);
    gpio_set_level(pself->mslp_pin, 0);
    pself->level--;
  };
}

void slopectrl_getdown()
{
  gpio_set_level(pself->mslp_pin, 1);
  vTaskDelay(pself->level * level_time);
  gpio_set_level(pself->mslp_pin, 0);
  pself->level = 0;
}
void slopectrl_stop()
{
  while(pself->level > 0){
    gpio_set_level(pself->mslp_pin, 1);
    vTaskDelay(level_time);
    gpio_set_level(pself->mslp_pin, 0);
    pself->level--;
  };
  gpio_set_level(pself->mslp_pin, 1);
  vTaskDelay(2*level_time);
  gpio_set_level(pself->mslp_pin, 0);
}
