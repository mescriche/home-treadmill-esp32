#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "freertos/queue.h"
#include "st7735.h"
#include "screen.h"
#include "display.h"
#include "esp_log.h"

static const char *TAG = "SCR";

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
const uint8_t conf_layout_size = 8;
FrameItem conf_layout[] = {
  {.name = "username",   .frame={{ 0,   0}, 80, 28}},
  {.name = "prg_id",     .frame={{ 90,  0}, 50, 28}},
  {.name = "ylabel",     .frame={{  0, 28}, 50, 12}},
  {.name = "max_speed",  .frame={{ 110,28}, 48, 12}},
  {.name = "av_speed",   .frame={{ 50, 28}, 44, 12}},
  {.name = "duration",   .frame={{110,112}, 40, 16}},
  {.name = "speed_prgm", .frame={{ 20, 40},120, 70}},
  {.name = "max_slope",  .frame={{  0,112}, 50, 16}},
};

const uint8_t run_layout_size = 15;
FrameItem run_layout[] = {
  { .name = "heading",     .frame = {{  0,  0},110, 28}},
  { .name = "prg_id",      .frame = {{  0, 30}, 20, 12}},
  { .name = "rspeed",      .frame = {{  0, 48}, 92, 30}},
  { .name = "rspeed1",     .frame = {{ 37, 48}, 56, 30}},
  { .name = "rspeed_next", .frame = {{  0, 48}, 38, 30}},
  { .name = "rspeed_time", .frame = {{ 40, 30}, 46, 12}},
  { .name = "ispeed",      .frame = {{ 10, 78}, 80, 20}},
  { .name = "aspeed",      .frame = {{  5,104}, 80, 18}},
  { .name = "slope",       .frame = {{114, 35}, 40, 12}},
  { .name = "slope_next",  .frame = {{ 96, 35}, 16, 12}},
  { .name = "distance",    .frame = {{ 85,104}, 74, 18}},
  { .name = "duration",    .frame = {{110,  4}, 50, 20}},
  { .name = "temperature", .frame = {{106, 54}, 50, 20}},
  { .name = "humidity",    .frame = {{ 92,78},  70, 20}},
  { .name = "progress",    .frame = {{ 0, 123},  160, 4}}
};

const uint8_t report_layout_size = 7;
FrameItem report_layout[] = {
  {.name = "heading",  .frame = {{  0,  0},160, 28}},
  {.name = "ylabel",   .frame={{  0, 28}, 50, 12}},
  {.name = "aspeed",   .frame = {{ 50, 28}, 44, 12}},
  {.name = "duration", .frame = {{110,112}, 50, 16}},
  {.name = "distance", .frame = {{ 10,112}, 50, 16}},
  {.name = "max_speed",.frame = {{ 110, 28}, 48, 12}}, //85
  {.name = "race",     .frame = {{ 20, 40}, 120, 70}}
};

Frame_t* search_layout(FrameItem* book, uint8_t size, char* key)
{
  for(int i=0; i<size; i++)
    if (strstr(book[i].name, key))
      return &book[i].frame;   
  return &nullframe;
}

enum Lead {MANUAL, PROGRAMMED};
enum Status {ON_READY, ON_RUNNING, OFF_STOPPING, OFF_STOPPED, ANY}; // engine status
enum Mode {WELCOME, CONF, RUN, PAUSE, REPORT};

HeadingItem headings_book[] = {
  {.mode=WELCOME, .status=ANY,          .text=" WELCOME!! "},
  {.mode=CONF,    .status=ANY ,         .text="CONFIGURATION"},
  {.mode=RUN,     .status=ON_READY,     .text="ON-READY"},
  {.mode=RUN,     .status=ON_RUNNING,   .text="ON-RUNNING"},
  {.mode=PAUSE,   .status=ANY,          .text="ON PAUSE"},
  {.mode=REPORT,  .status=OFF_STOPPING, .text="OFF-STOPPING"},
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

void clear_screen()
{
  display_clear(&white);
}
void show_welcome_screen()
{
  Frame_t frm = {{0,40}, 160, 50};
  char *heading = get_heading(WELCOME, ANY);
  setFont(DV24PT);
  displayStringOnFrame(frm, heading, &red);
  //displayFrame(frm, &black);
}
void show_configuration_manual_screen()
{
  char value[20];
  Frame_t frm =  {{0,0}, 160, 30};
  char *heading = get_heading(CONF, ANY);
  setFont(DF12PT);
  displayStringOnFrame(frm, heading , &blue);
  //displayFrame(*frm, &black);
  // manual
  Frame_t frmm = {{0,30}, 160, 78};
  sprintf(value, "MANUAL");
  setFont(DV24PT);
  displayStringOnFrame(frmm, value, &blue);
  // start button
  Frame_t frms = {{110, 100}, 40, 20};
  sprintf(value, "Start");
  setFont(DF12PT);
  displayStringOnFrame(frms, value, &red);
  displayFrame(frms, &black);
  // select program
  Frame_t frmp = {{40,100}, 70, 20};
  sprintf(value, " Programs");
  setFont(DF12PT);
  displayStringOnFrame(frmp, value, &blue);
  displayFrame(frmp, &black);
  // structure
  Pos_t p1 = {0,30};
  displayHLine(p1, 160, &black);
}

void show_configuration_program_screen(char* username, uint8_t programId,
				       float aspeed, float max_speed, uint32_t duration,
				       uint8_t max_slope, float* speed_pgrm)
{
  display_clear(&white);
  Frame_t *frm;
  char value[20];
  // owner
  frm = search_layout(conf_layout, conf_layout_size, "username");
  sprintf(value, "%s", username);
  setFont(UB16PT);
  displayStringOnFrame(*frm, value, &blue);
  //displayFrame(*frm, &black);
  // program Id
  frm = search_layout(conf_layout, conf_layout_size, "prg_id");
  sprintf(value, "Program #%1u", programId+1);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &maroon);
  // ylabel
  frm = search_layout(conf_layout, conf_layout_size, "ylabel");
  setFont(DF12PT);
  displayStringOnFrame(*frm, "Km/h", &blue); 
  // speed program
  frm = search_layout(conf_layout, conf_layout_size, "speed_prgm");
  uint8_t x_scale, y_scale;
  x_scale = (frm->width / duration <= 5) ? frm->width / duration: 5;
  y_scale = frm->height / max_speed;
  displayGraphOnFrame(*frm, x_scale, y_scale, duration, speed_pgrm, &green);
  //
  Pos_t q = frm->pos;
  q.y = frm->pos.y+frm->height-(y_scale * aspeed);
  displayHDashedLine(q, frm->width, &red);
  //displayFrame(*frm, &black);
  // max speed
  frm = search_layout(conf_layout, conf_layout_size, "max_speed");
  setFont(DF12PT);
  sprintf(value, "Mx:%3.1f", max_speed);
  displayStringOnFrame(*frm, value, &red);
  //displayFrame(*frm, &black);
  
  //displayFrame(*frm, &black);
  // average speed
  frm = search_layout(conf_layout, conf_layout_size, "av_speed");
  setFont(DF12PT);
  sprintf(value, "Av:%3.1f", aspeed);
  displayStringOnFrame(*frm, value, &red);
  //displayFrame(frm_av, &black);

    // max slope
  frm = search_layout(conf_layout, conf_layout_size, "max_slope");
  sprintf(value,  "%1u%%&", max_slope);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &blue);
  
  // duration
  frm = search_layout(conf_layout, conf_layout_size, "duration");
  /* int h,m; */
  /* h = duration/60; */
  /* if (h>0) sprintf(value,"%03u",m); */
  sprintf(value, "%02u min", duration);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &blue);
  //displayFrame(*frm, &black);
  
  // structure
  Pos_t p1 = {0,24};
  displayHLine(p1,160, &black);
  /* Pos_t p2 = {140,28}; */
  /* displayVLine(p2,130, &black); */
  /* Pos_t p3 = {0,98}; */
  /* displayHLine(p3,160, &black); */
}

void show_lead_run_screen(uint8_t prgId, float rspeed, float rspeed_next, uint8_t slope, uint8_t slope_next,
			  uint32_t time_left, uint32_t spent_time, uint32_t total_time)
{
  Frame_t *frm;
  char value[20];
  uint m = time_left/60;
  uint s = time_left - m*60;
  // program Id
  frm = search_layout(run_layout, run_layout_size, "prg_id");
  sprintf(value, "#%1u", prgId+1);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &blue);
  //displayFrame(*frm, &black);
  // next rspeed
  frm = search_layout(run_layout, run_layout_size, "rspeed_next");
  if ((time_left > 0) && (time_left < 30))
    sprintf(value, "[%3.1f]", rspeed_next);
  else value[0] = '\0';
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &olive);
  //displayFrame(*frm, &black);
  frm = search_layout(run_layout, run_layout_size, "rspeed");
  if ((time_left > 0) && (time_left < 30))
    frm = search_layout(run_layout, run_layout_size, "rspeed1");
  sprintf(value, "%3.1f", rspeed);
  setFont(DV24PT);
  displayStringOnFrame(*frm, value, &blue);
  //displayFrame(*frm, &black);
  // next slope
  frm = search_layout(run_layout, run_layout_size, "slope_next");
  value[0] = '\0';
  if ((slope != slope_next) && (time_left > 0) && (time_left < 30))
    sprintf(value, "[%1u]", slope_next);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &olive);
  //displayFrame(*frm, &black);
  // rspeed lasting time
  frm = search_layout(run_layout, run_layout_size, "rspeed_time");
  // ESP_LOGI(TAG, "SCREEN :: rspeed_time_left[sec] = %d", time_left);
  sprintf(value, "%1u:%02u", m,s);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &red);
  //displayFrame(*frm, &black);
  frm =  search_layout(run_layout, run_layout_size, "progress");
  //ESP_LOGI(TAG, "SCREEN :: progress = %f, duration=%d", progress, duration );
  float progress = spent_time / (float) total_time;
  displayHProgressBar(*frm, progress, &red);
}

void show_run_screen(uint8_t lead, uint8_t mode, uint8_t status, float rspeed, float ispeed, float aspeed,
		     uint32_t slope, float distance, uint32_t duration,
		     float temperature, float humidity)
{
  Frame_t *frm;
  char value[20];
  // heading
  frm = search_layout(run_layout, run_layout_size, "heading");
  char *heading = (mode == RUN) ? get_heading(RUN, status) : get_heading(REPORT, status);
  setFont(DF12PT);
  displayStringOnFrame(*frm, heading , &blue);
  //displayFrame(*frm, &black);
  // rspeed
  if (lead == MANUAL){
    frm = search_layout(run_layout, run_layout_size, "rspeed");
    sprintf(value, "%3.1f", rspeed);
    setFont(DV24PT);
    displayStringOnFrame(*frm, value, &blue);
    //displayFrame(*frm, &black);
  }
  // ispeed
  frm = search_layout(run_layout, run_layout_size, "ispeed");
  sprintf(value, "%3.1f Km/h", ispeed);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &blue);
  //displayFrame(*frm, &black);
  // aspeed
  frm = search_layout(run_layout, run_layout_size, "aspeed");
  sprintf(value, "%3.1f Km/h", aspeed);
  setFont(UB16PT);
  displayStringOnFrame(*frm, value, &blue);
  //displayFrame(*frm, &black);
  // slope
  frm = search_layout(run_layout, run_layout_size, "slope");
  sprintf(value,  "%1u %%&", slope);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &red);
  //displayFrame(*frm, &black);
  // distance
  frm = search_layout(run_layout, run_layout_size, "distance");
  if (distance >= 1000) sprintf(value, "%.1f Km", distance/1000.0);
  else sprintf(value, "%3u m", (uint)distance);
  setFont(UB16PT);
  displayStringOnFrame(*frm, value, &red);
  //displayFrame(*frm, &black);
  // duration
  frm = search_layout(run_layout, run_layout_size, "duration");
  int h,m,s; 
  h = duration/3600;
  m = (duration - (3600*h))/60;
  s = duration -(3600*h)-(m*60);
  if (h>9) sprintf(value,"%02u:%02u",h,m);
  else if (h>0) sprintf(value,"%01u:%02u",h,m);
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
  Pos_t p1 = {0, 28}, p2={0,100}, p3={93, 28};
  displayHLine(p1, 160, &black);
  displayHLine(p2, 160, &black);
  displayVLine(p3, 72,  &black);
}

void show_pause_screen(uint32_t duration)
{
  Frame_t frm = {{0,0}, 160, 28};
  char *heading = get_heading(PAUSE, ANY);
  setFont(DF12PT);
  displayStringOnFrame(frm, heading , &blue);
  //displayFrame(frm, &black);
  // duration
  Frame_t frm1 = {{0, 30}, 160, 90};
  char value[20];
  int m,s; 
  m = duration / 60;
  s = duration - (m*60);
  sprintf(value, "%02u:%02u", m,s);
  setFont(DV24PT);
  displayStringOnFrame(frm1, value, &blue);
  Pos_t p1 = {0, 24};
  displayHLine(p1, 160, &black);
}

void show_report_screen(uint8_t status, float aspeed, float distance, uint32_t duration,
			uint32_t race_size, float* race)
{
  //ESP_LOGI(TAG, "REPORT SCREEN :: race_size = %d", race_size);
  Frame_t *frm;
  char value[20];
  // heading
  frm = search_layout(report_layout, report_layout_size, "heading");
  char *heading = get_heading(REPORT, status);
  setFont(DF12PT);
  displayStringOnFrame(*frm, heading , &blue);
  //displayFrame(*frm, &black);
  Pos_t p1 = {0, 24};
  displayHLine(p1, 160, &black);
  // ylabel
  frm = search_layout(conf_layout, conf_layout_size, "ylabel");
  setFont(DF12PT);
  displayStringOnFrame(*frm, "Km/h", &blue); 
  // race profile
  if (race_size > 0){
    frm = search_layout(report_layout, report_layout_size, "race");
    float max_speed = 1.0;
    for(uint i=0; i<race_size; i++)
      if (race[i] > max_speed) max_speed = race[i];

    uint8_t x_scale = (frm->width / race_size <= 5)? frm->width / race_size : 5;
    uint8_t y_scale = frm->height / max_speed;
    ESP_LOGI(TAG, "height=%d, max_speed=%3.1f, y_scale=%d",frm->height, max_speed, y_scale);  
    ESP_LOGI(TAG, "width=%d, race_size=%d, x_scale=%d", frm->width, race_size, x_scale);
    displayGraphOnFrame(*frm, x_scale, y_scale, race_size, race, &orange);
    Pos_t q = frm->pos;
    q.y = frm->pos.y+frm->height-(y_scale * aspeed);
    displayHDashedLine(q, frm->width, &blue);
    //displayFrame(*frm, &black);
    // max_speed
    frm = search_layout(report_layout, report_layout_size, "max_speed");
    sprintf(value, "Mx:%3.1f", max_speed);
    setFont(DF12PT);
    displayStringOnFrame(*frm, value, &blue);
  }
  // aspeed
  frm = search_layout(report_layout, report_layout_size, "aspeed");
  sprintf(value, "Av:%3.1f", aspeed);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &blue);
  //displayFrame(*frm, &black);
  // distance
  frm = search_layout(report_layout, report_layout_size, "distance");
  if (distance >= 1000) sprintf(value, "%.1f Km", distance/1000.0);
  else sprintf(value, "%3u m", (uint)distance);
  setFont(UB16PT);
  displayStringOnFrame(*frm, value, &red);
  //displayFrame(*frm, &black);
  // duration
  frm = search_layout(report_layout, report_layout_size, "duration");
  int h,m; 
  h = duration/3600;
  m = (duration - (3600*h))/60;
  if (h>1) sprintf(value,"%01u:%02u",h,m);
  else sprintf(value, "%1u min",m);
  setFont(UB16PT);
  displayStringOnFrame(*frm, value, &blue);
}

