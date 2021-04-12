#include <speedmeter.h>
#include "esp_log.h"

#define PCNT_H_LIM_VAL 1 // count N pulses
SpeedMeter_t *pself;
QueueHandle_t speedmeter_xMailbox;

static void speedmeter_intr_handler(void *arg)
{
  SpeedMeter_t* self = (SpeedMeter_t*) arg;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  uint32_t status;
  SpeedRecord_t measurement;
  esp_err_t ret;
  ret = pcnt_get_event_status(self->unit, &status);
  //  ESP_ERROR_CHECK(ret);
  if (status & PCNT_EVT_ZERO){
    measurement.timestamp = xTaskGetTickCount();
    measurement.nstep = PCNT_H_LIM_VAL;
    xQueueSendFromISR(speedmeter_xMailbox, &measurement, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

BaseType_t speedmeter_read(SpeedRecord_t* m)
{
  return xQueueReceive(speedmeter_xMailbox, m, pdMS_TO_TICKS(3000)); //  portMAX_DELAY
}

void speedmeter_reset()
{
  pcnt_counter_pause(pself->unit);
  pcnt_counter_clear(pself->unit);
  pcnt_counter_resume(pself->unit);  
  
}
void speedmeter_stop()
{
  pcnt_counter_pause(pself->unit);
}

void speedmeter_init(SpeedMeter_t* self)
{
  //uint32_t unit = self->unit;
  pself = self;
  pcnt_config_t pcnt_config = {
    .unit = self->unit,
    .channel = self->channel,
    .pulse_gpio_num = self->pin,
    .ctrl_gpio_num = PCNT_PIN_NOT_USED, //-1  not used
    .pos_mode = PCNT_COUNT_INC, // raising edge
    .neg_mode = PCNT_COUNT_DIS,  // disconnected
    .counter_h_lim = PCNT_H_LIM_VAL
  };
  
  esp_err_t ret;
  
  //ESP_ERROR_CHECK(pcnt_unit_config(&pcnt_config));
  ret = pcnt_unit_config(&pcnt_config);
  ESP_ERROR_CHECK(ret);
  
  /* Configure and enable the input filter */
  pcnt_set_filter_value(self->unit, 1023); // 12.7 us max glitch filter - f= 80 MHz 
  pcnt_filter_enable(self->unit);
  /* Set threshold 0 and 1 values and enable events to watch */
  //  pcnt_set_event_value(unit, PCNT_EVT_THRES_1, PCNT_EVT_MAX);
  pcnt_event_disable(self->unit, PCNT_EVT_THRES_1);
  //  pcnt_set_event_value(unit, PCNT_EVT_THRES_0, PCNT_EVT_MAX);
  pcnt_event_disable(self->unit, PCNT_EVT_THRES_0);
  /* Enable events on zero, maximum and minimum limit values */
  pcnt_event_enable(self->unit, PCNT_EVT_ZERO);
  /* Initialize PCNT's counter */
  pcnt_counter_pause(self->unit);
  pcnt_counter_clear(self->unit);
  
  /* Install interrupt service and add isr callback handler */
  pcnt_isr_service_install(0);
  pcnt_isr_handler_add(self->unit, speedmeter_intr_handler, (void *)self);
  
  /* Everything is set up, now go to counting */
  pcnt_counter_resume(self->unit);
  
  speedmeter_xMailbox = xQueueCreate(1, sizeof(SpeedRecord_t));

}
