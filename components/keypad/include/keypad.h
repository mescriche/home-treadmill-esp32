#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
  gpio_num_t intr;
  gpio_num_t sda;
  gpio_num_t scl;
  TaskHandle_t reader;
} Keypad_t;

void keypad_init(Keypad_t* self);
bool keypad_read(uint8_t* value);

