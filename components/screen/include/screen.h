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

enum DspDataType {RSPEED, ISPEED, ASPEED, SLOPE, DISTANCE, DURATION, TEMPERATURE, HUMIDITY, MESSAGE};
enum Message {WELCOME, READY, RUNNING, MPAUSE, STOPPING, STOPPED};

typedef struct {
  enum DspDataType type;
  void* dataptr;
} Display_req;

void screen_init(Screen_t* self);
//
void show_message(uint16_t msg);
void show_speed(float rspd, float ispd);
void show_aspeed(float spd);
void show_slope(uint16_t level);
void show_distance(uint32_t dist);
void show_duration(uint32_t time);
void show_ambsensor(float temperature, float humidity);
void show_status(uint16_t status);

