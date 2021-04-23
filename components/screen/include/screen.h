#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  gpio_num_t sda;
  gpio_num_t scl;
  gpio_num_t dc;
  gpio_num_t rst;
  gpio_num_t cs;
  gpio_num_t bl;
  TaskHandle_t updater;
  TaskHandle_t server;
} Screen_t;


void screen_init(Screen_t* self);
void show_welcome_screen();
void show_configuration_screen();
void show_run_screen(uint8_t status, float rspeed, float ispeed, float aspeed,
		     uint32_t slope, float distance, uint32_t duration,
		     float temperature, float humidity);
void show_pause_screen();
void show_report_screen(uint8_t status, float aspeed, float distance, uint32_t duration,
			uint32_t begin, uint32_t end, float* race);


