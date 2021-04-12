#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

typedef struct {
  gpio_num_t i2c_sda;
  gpio_num_t i2c_scl;
  TaskHandle_t updater;
} AmbSensor_t;

void ambsensor_init(AmbSensor_t* self);
bool ambsensor_read(float* temp, float* hum);
