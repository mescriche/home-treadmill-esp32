#include <stdio.h>
#include "buzzer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static Buzzer_t* pself;

static void buzzer_timer_callback(void *arg)
{
  //Buzzer_t* self = (Buzzer_t*) arg;
  gpio_num_t* bpin = (gpio_num_t*) arg; 
  static uint8_t status = 0;
  status = (status == 0) ? 1 : 0; 
  gpio_set_level(*bpin, status);
}


void buzzer_init(Buzzer_t* self)
{
  BaseType_t xResult;
  pself = self;
  gpio_config_t io_conf;
  io_conf.pin_bit_mask = 1ULL << self->pin;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_down_en = 1;
  io_conf.pull_up_en = 0;
  gpio_config(&io_conf);
  
  gpio_set_level(self->pin, 0);
  
  esp_timer_init();
  const esp_timer_create_args_t timer_args = {
    .callback = &buzzer_timer_callback,
    .name = "buzzer_timer",
    .arg = &self->pin
  };
  //  esp_timer_handle_t _timer;
  esp_timer_create(&timer_args, &self->timer);
  self->req_queue = xQueueCreate(20, sizeof(Buzzer_req));
  configASSERT(self->req_queue != NULL);
  xResult = xTaskCreate(buzzer_keeper, "buzzer_keeper", 2048, NULL, 5, &self->keeper);
  configASSERT(xResult == pdPASS);
}

  // recommended 0.1 < F < 9.0  kHz ex: F=2.8
  // recomended L =  150 ms
  // example _buzzer_beep (2.8, 150) 
static void _buzzer_beep(Buzzer_req req) 
{
  int T = (int)(1000 / req.frq); // T us
  //  printf("Buzzer Freq=%f, T=%d\n",req.frq, T);
  esp_timer_start_periodic(pself->timer, T); // T = 350 us
  vTaskDelay(req.last / portTICK_PERIOD_MS); //
  esp_timer_stop(pself->timer);
  gpio_set_level(pself->pin, 0);
  vTaskDelay(pdMS_TO_TICKS(300)); // delay before attending next request
}

void buzzer_beep(float frq, uint16_t last)
{
  Buzzer_req req = {.frq = frq, .last=last};
  BaseType_t xResult;
  do xResult = xQueueSend(pself->req_queue, &req, pdMS_TO_TICKS(500));
  while (xResult != pdPASS);
}

void buzzer_keeper(void* pvParameter)
{
  static Buzzer_req req;
  static BaseType_t xResult;
  while(1){
    xResult = xQueueReceive(pself->req_queue, &req ,portMAX_DELAY);
    configASSERT(xResult == pdPASS);
    _buzzer_beep(req);
  }
}

void buzzer_beep_HELLO()
{
  buzzer_beep(2.857, 150);
}
void buzzer_beep_keyOK()
{
  buzzer_beep(2.0, 150);
}

void buzzer_beep_START()
{
  buzzer_beep(2.4, 300);
  buzzer_beep(2.4, 300);
}
void buzzer_beep_STOP()
{
  buzzer_beep(2.4, 300);
  buzzer_beep(2.4, 300);
  buzzer_beep(2.4, 300);
}

void buzzer_beep_keyFAIL()
{
  buzzer_beep(0.5, 150);
}

