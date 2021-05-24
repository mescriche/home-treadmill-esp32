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
void show_configuration_manual_screen();
void show_configuration_program_screen(char* username, uint8_t programId,
				       float aspeed, float max_speed,
				       uint32_t duration, uint8_t max_slope, float* speed_pgrm);
void show_lead_run_screen(uint8_t prgId, float rspeed, float rspeed_next, uint8_t slope, uint8_t slope_next,
			  uint32_t lasting_time, uint32_t spent_time, uint32_t total_time);
void show_run_screen(uint8_t lead, uint8_t mode, uint8_t status, float rspeed, float ispeed, float aspeed,
		     uint32_t slope, float distance, uint32_t duration,
		     float temperature, float humidity);
void show_pause_screen();
void show_report_screen(uint8_t status, float aspeed, float distance, uint32_t duration,
			uint32_t race_size, float* race);
void clear_screen();


