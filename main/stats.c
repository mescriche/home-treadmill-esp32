#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"

#define STATS_TICKS         pdMS_TO_TICKS(2000)
#define ARRAY_SIZE_OFFSET   10 

esp_err_t print_real_time_stats(TickType_t xTicksToWait)
{
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    uint32_t start_run_time, end_run_time;
    esp_err_t ret;

    //Allocate array to store current task states
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array = malloc(sizeof(TaskStatus_t) * start_array_size);
    if (start_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get current task states
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    vTaskDelay(xTicksToWait);

    //Allocate array to store tasks states post delay
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array = malloc(sizeof(TaskStatus_t) * end_array_size);
    if (end_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get post delay task states
    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    //Calculate total_elapsed_time in units of run time stats clock period.
    uint32_t total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto exit;
    }

    printf("| Task | Run Time | Percentage\n");
    //Match each task in start_array to those in the end_array
    for (int i = 0; i < start_array_size; i++) {
        int k = -1;
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {
                k = j; //Mark that task have been matched by overwriting their handles
                start_array[i].xHandle = NULL;
                end_array[j].xHandle = NULL;
                break;
            }
        }
        //Check if matching task found
        if (k >= 0) {
            uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
            uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * portNUM_PROCESSORS);
            printf("| %s | %d | %d%%\n", start_array[i].pcTaskName, task_elapsed_time, percentage_time);
        }
    }

    //Print unmatched tasks
    for (int i = 0; i < start_array_size; i++) {
        if (start_array[i].xHandle != NULL) 
	  printf("| %s | Deleted\n", start_array[i].pcTaskName);
    }
    for (int i = 0; i < end_array_size; i++) {
        if (end_array[i].xHandle != NULL) 
            printf("| %s | Created\n", end_array[i].pcTaskName);
    }
    ret = ESP_OK;

exit:    //Common return path
    printf("ret = %d\n", ret);
    free(start_array);
    free(end_array);
    return ret;
}

void stats_task(void *arg)
{
  //Print real time stats periodically
  while (1) {
    //printf("\n\nGetting real time stats over %d ticks\n", STATS_TICKS);
    if (print_real_time_stats(STATS_TICKS) == ESP_OK) {
      printf("Real time stats obtained\n");
    } else {
      printf("Error getting real time stats\n");
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}



void heart_beat(void *arg)
{
  //  gpio_num_t* led = (gpio_num_t*) arg;
  const gpio_num_t pin = (gpio_num_t)arg;
  //  gpio_reset_pin(pin);
  //gpio_set_direction(pin, GPIO_MODE_OUTPUT);
  gpio_config_t io_conf;
  io_conf.pin_bit_mask = 1ULL << pin;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 0;
  gpio_config(&io_conf);
  gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
  
  static uint8_t status = 0;
  uint32_t _time = 100;
  
  while(1){
    status = (status == 0) ? 1 : 0; 
    gpio_set_level(pin, status);
    vTaskDelay(pdMS_TO_TICKS(_time));
  }
}
