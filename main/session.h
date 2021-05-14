#include <stdio.h>
#include <unistd.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
  float speed;
  uint8_t duration;  
  uint8_t slope;
} Order;

typedef struct {
  char* owner;
  float aspeed, max_speed;
  uint8_t max_slope;
  uint32_t duration;
  Order oseq[];
} Program;

void session_init();
void session_set_user(uint8_t userId);
char* session_get_username();
uint session_get_nPrograms();
void session_load_program(uint16_t id);
Program* session_get_program(uint16_t id);
void session_get_speed_program(uint16_t id, float *prg);
void session_get_slope_program(uint16_t id, uint8_t *prg);
void session_get_order(Order* oPtr, Order* nPtr);
void session_end_program();
