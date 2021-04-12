#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/pcnt.h"

typedef struct {
  gpio_num_t pin; // pulse input
  uint8_t unit;
  uint8_t channel;
  //  QueueHandle_t event_queue;
  //  esp_timer_handle_t _timer;
  //  uint32_t sample_time; // ms
  float wheel_diameter; // centimeters
  TaskHandle_t updater;
} SpeedMeter_t;

typedef struct {
  uint32_t nstep; // meters
  TickType_t timestamp;
} SpeedRecord_t;

void speedmeter_init(SpeedMeter_t* self);
void speedmeter_stop();
void speedmeter_reset();
BaseType_t speedmeter_read(SpeedRecord_t* m); // m/s
