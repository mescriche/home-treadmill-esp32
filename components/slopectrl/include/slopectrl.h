#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
  gpio_num_t pslp_pin; // plus pin
  gpio_num_t mslp_pin; // minus pin
  float      height;
  float      lenght;
  uint16_t   nlevels;
  uint8_t   level;
  TaskHandle_t worker;
} SlopeCtrl_t;


void slopectrl_init(SlopeCtrl_t* self);
void slopectrl_do(uint8_t level);
void slopectrl_getdown();
void slopectrl_stop();
