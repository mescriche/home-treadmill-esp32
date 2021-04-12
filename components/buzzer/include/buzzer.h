#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
  gpio_num_t pin;
  esp_timer_handle_t timer;
  TaskHandle_t keeper;
  QueueHandle_t req_queue;
} Buzzer_t;

typedef struct {
  float frq;
  uint16_t last;
} Buzzer_req;

void buzzer_init(Buzzer_t* self);
void buzzer_keeper(void* pvParameter);
void buzzer_beep(float frq, uint16_t last);
// F[kHz] 0.1 < F < 9.0
// L[ms]  1 < L < 250
void buzzer_beep_HELLO();
void buzzer_beep_keyOK();
void buzzer_beep_STOP();
void buzzer_beep_START();
void buzzer_beep_keyFAIL();

//void heart_beat(void* arg);

