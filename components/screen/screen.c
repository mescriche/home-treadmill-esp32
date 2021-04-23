#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "freertos/queue.h"
#include "st7735.h"
#include "screen.h"
#include "display.h"


typedef struct {
  uint8_t mode;
  char *name;  
  Frame_t frame;
} FrameItem;

typedef struct {
  uint8_t mode, status;
  char* text;
} HeadingItem;

Frame_t nullframe = {{0,0},0,0};
const uint8_t run_layout_size = 9;
FrameItem run_layout[] = {
  { .name = "heading",     .frame = {{ 0, 0}, 110, 30}},
  { .name = "rspeed",      .frame = {{  0,30}, 90, 50}},
  { .name = "ispeed",      .frame = {{  0,75}, 90, 20}},
  { .name = "aspeed",      .frame = {{  5,98}, 80, 30}},
  { .name = "slope",       .frame = {{ 90,35}, 70, 20}},
  { .name = "distance",    .frame = {{ 85,98}, 80, 30}},
  { .name = "duration",    .frame = {{110, 0}, 50, 30}},
  { .name = "temperature", .frame = {{ 90,55}, 70, 20}},
  { .name = "humidity",    .frame = {{ 90,75}, 70, 20}}
};

const uint8_t report_layout_size = 3;
FrameItem report_layout[] = {
  {.name = "heading", .frame = {{0, 0}, 110, 30}},
  {.name = "aspeed",  .frame = {{0, 0},   0, 0}},
  {.name = "duration", .frame = {{0, 0},   0, 0}},
  {.name = "distance",.frame = {{0, 0},   0, 0}},
  {.name = "race",    .frame = {{0, 0},   0, 0}}
};

Frame_t* search_layout(FrameItem* book, uint8_t size, char* key)
{
  for(int i=0; i<size; i++)
    if (strstr(book[i].name, key))
      return &book[i].frame;   
  return &nullframe;
}

enum Status {ON_READY, ON_RUNNING, OFF_STOPPING, OFF_STOPPED, ANY}; // engine status
enum Mode {WELCOME, CONF, RUN, PAUSE, REPORT};

HeadingItem headings_book[] = {
  {.mode=WELCOME, .status=ANY,          .text="WELCOME!!"},
  {.mode=CONF,    .status=ANY ,         .text="CONFIGURATION"},
  {.mode=RUN,     .status=ON_READY,     .text="READY"},
  {.mode=RUN,     .status=ON_RUNNING,   .text="RUNNING"},
  {.mode=PAUSE,   .status=ANY,          .text="ON PAUSE"},
  {.mode=REPORT,  .status=OFF_STOPPING, .text="STOPPING"},
  {.mode=REPORT,  .status=OFF_STOPPED,  .text="REPORT"}
};

char* get_heading(uint16_t mode, uint16_t status)
{
  uint8_t index = 0;
  
  switch (mode){
  case WELCOME: index=0; break;
  case CONF: index=1; break;
  case RUN:
    if (status == ON_READY) index=2; else index=3;
    break;
  case PAUSE: index = 4; break;
  case REPORT:
    if (status == OFF_STOPPING) index=5; else index = 6;
  }
  return headings_book[index].text;
}

void screen_init(Screen_t* self)
{
  tft_st7735_init(self->sda, self->scl, self->cs, self->rst, self->dc, self->bl);
  display_init(&magenta, &white);
}


void show_welcome_screen()
{
  Frame_t frm = {{0,30}, 90, 50};
  char *heading = get_heading(WELCOME, ANY);
  setFont(DV24PT);
  displayStringOnFrame(frm, heading, &blue);
  //displayFrame(frm);
}

void show_configuration_screen()
{
  Frame_t frm = {{0,30}, 90, 50};
  char *heading = get_heading(CONF, ANY);
  setFont(DF12PT);
  displayStringOnFrame(frm, heading , &blue);
  //displayFrame(*frm);
  
}

void show_run_screen(uint8_t status, float rspeed, float ispeed, float aspeed,
		     uint32_t slope, float distance, uint32_t duration,
		     float temperature, float humidity)
{
  Frame_t *frm;
  char value[20];
  // heading
  frm = search_layout(run_layout, run_layout_size, "heading");
  char *heading = get_heading(RUN, status);
  setFont(DF12PT);
  displayStringOnFrame(*frm, heading , &blue);
  displayFrame(*frm, &black);
  // rspeed
  frm = search_layout(run_layout, run_layout_size, "rspeed");
  sprintf(value, "%3.1f", rspeed); 
  setFont(DV24PT);
  displayStringOnFrame(*frm, value, &blue);
  displayFrame(*frm, &black);
  // ispeed
  frm = search_layout(run_layout, run_layout_size, "ispeed");
  sprintf(value, "%3.1f Km/h", ispeed);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &blue);
  displayFrame(*frm, &black);
  // aspeed
  frm = search_layout(run_layout, run_layout_size, "aspeed");
  sprintf(value, "%3.1f Km/h", aspeed);
  setFont(UB16PT);
  displayStringOnFrame(*frm, value, &blue);
  displayFrame(*frm, &black);
  // slope
  frm = search_layout(run_layout, run_layout_size, "slope");
  sprintf(value,  "%1u %%&", slope); 
  displayStringOnFrame(*frm, value, &red);
  displayFrame(*frm, &black);
  // distance
  frm = search_layout(run_layout, run_layout_size, "distance");
  if (distance >= 1000) sprintf(value, "%.1f Km", distance/1000.0);
  else sprintf(value, "%3u m", (uint)distance);
  setFont(UB16PT);
  displayStringOnFrame(*frm, value, &red);
  displayFrame(*frm, &black);
  // duration
  frm = search_layout(run_layout, run_layout_size, "duration");
  int h,m,s; 
  h = duration/3600;
  m = (duration - (3600*h))/60;
  s = duration -(3600*h)-(m*60);
  if (h>9) sprintf(value,"%02u:%02u",h,m);
  else if (h>0) sprintf(value,"%01u:%02u:%02u",h,m,s);
  else sprintf(value, "%02u:%02u", m,s);
  setFont(UB16PT);
  displayStringOnFrame(*frm, value, &blue);
  // temperature
  frm = search_layout(run_layout, run_layout_size, "temperature");
  sprintf(value, "%3.1f^C", temperature);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &orange);
  // humidity
  frm = search_layout(run_layout, run_layout_size, "humidity");
  sprintf(value, "%2.0f %%HR", humidity);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &magenta);
  // structure
  Pos_t p1 = {0, 30}, p2={0,98}, p3={90, 30};
  displayHLine(p1, 160, &black);
  displayHLine(p2, 160, &black);
  displayVLine(p3, 68,  &black);
}

void show_pause_screen(uint32_t duration)
{
  Frame_t frm = {{0,30}, 90, 50};
  char *heading = get_heading(PAUSE, ANY);
  setFont(DF12PT);
  displayStringOnFrame(frm, heading , &blue);
  displayFrame(frm, &black);
  // duration
  Frame_t frm1 = {{0, 90}, 30, 30};
  char value[20];
  int m,s; 
  m = duration / 60;
  s = duration - (m*60);
  sprintf(value, "%02u:%02u", m,s);
  setFont(DV24PT);
  displayStringOnFrame(frm1, value, &blue);
}

void show_report_screen(uint8_t status, float aspeed, float distance, uint32_t duration,
			uint32_t begin, uint32_t end, float* race)
{
  Frame_t *frm;
  char value[20];
  // heading
  frm = search_layout(report_layout, report_layout_size, "heading");
  char *heading = get_heading(REPORT, status);
  setFont(DF12PT);
  displayStringOnFrame(*frm, heading , &blue);
  displayFrame(*frm, &black);
  // aspeed
  frm = search_layout(report_layout, report_layout_size, "aspeed");
  sprintf(value, "%3.1f Km/h", aspeed);
  setFont(UB16PT);
  displayStringOnFrame(*frm, value, &blue);
  displayFrame(*frm, &black);
  // distance
  frm = search_layout(report_layout, report_layout_size, "distance");
  if (distance >= 1000) sprintf(value, "%.1f Km", distance/1000.0);
  else sprintf(value, "%3u m", (uint)distance);
  setFont(UB16PT);
  displayStringOnFrame(*frm, value, &red);
  displayFrame(*frm, &black);
  // duration
  frm = search_layout(report_layout, report_layout_size, "duration");
  int h,m,s; 
  h = duration/3600;
  m = (duration - (3600*h))/60;
  s = duration -(3600*h)-(m*60);
  if (h>9) sprintf(value,"%02u:%02u",h,m);
  else if (h>0) sprintf(value,"%01u:%02u:%02u",h,m,s);
  else sprintf(value, "%02u:%02u", m,s);
  setFont(UB16PT);
  displayStringOnFrame(*frm, value, &blue);
  // race profile
  frm = search_layout(report_layout, report_layout_size, "race");
  displayGraphOnFrame(*frm, begin, end, race, &orange);
}

