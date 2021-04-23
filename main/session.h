#include <stdio.h>
#include <unistd.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define BOOK_SIZE 20
typedef struct {
  float speed;
  uint32_t duration;  
  uint8_t slope;
} Order;

typedef struct {
  char* name;
  float aspeed;
  uint32_t duration;
  Order oseq[];
} Program;



void session_init();
uint session_get_nPrograms();
BaseType_t session_load_program(uint16_t id);
const Program* session_get_program(uint16_t id);
BaseType_t session_get_order();
