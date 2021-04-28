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
const uint8_t conf_layout_size = 8;
FrameItem conf_layout[] = {
  {.name = "heading",    .frame={{  0, 0}, 160, 30}},
  {.name = "username",   .frame={{120, 0}, 40, 30}},
  {.name = "prg_id",     .frame={{  0,30}, 80, 30}},
  {.name = "duration",   .frame={{ 80,30}, 80, 30}},
  {.name = "aspeed",     .frame={{  0,60}, 80, 30}},
  {.name = "max_speed",  .frame={{ 80,60}, 80, 30}},
  {.name = "speed_prgm", .frame={{  0,90}, 80, 50}},
  {.name = "slope_prgm", .frame={{ 80,90}, 80, 50}}
};

const uint8_t run_layout_size = 11;
FrameItem run_layout[] = {
  { .name = "heading",     .frame = {{ 0, 0}, 110, 30}},
  { .name = "rspeed",      .frame = {{  0,30}, 90, 50}},
  { .name = "rspeed_next", .frame = {{  0,30}, 40, 20}},
  { .name = "rspeed_time", .frame = {{  0,90}, 40, 20}},
  { .name = "ispeed",      .frame = {{  0,75}, 90, 20}},
  { .name = "aspeed",      .frame = {{  5,98}, 80, 30}},
  { .name = "slope",       .frame = {{ 90,35}, 70, 20}},
  { .name = "distance",    .frame = {{ 85,98}, 80, 30}},
  { .name = "duration",    .frame = {{110, 0}, 50, 30}},
  { .name = "temperature", .frame = {{ 90,55}, 70, 20}},
  { .name = "humidity",    .frame = {{ 90,75}, 70, 20}}
};

const uint8_t report_layout_size = 5;
FrameItem report_layout[] = {
  {.name = "heading",  .frame = {{   0,0},110, 30}},
  {.name = "aspeed",   .frame = {{  0,30}, 53, 40}},
  {.name = "duration", .frame = {{ 53,30}, 53, 40}},
  {.name = "distance", .frame = {{106,30}, 53, 40}},
  {.name = "race",     .frame = {{  0,70},160, 58}}
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
  Frame_t *frm;
  char value[20];
  frm = search_layout(conf_layout, conf_layout_size, "heading");
  char *heading = get_heading(CONF, ANY);
  setFont(DF12PT);
  displayStringOnFrame(*frm, heading , &blue);
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
  Frame_t frmp = {{40,100}, 60, 20};
  sprintf(value, " Program");
  setFont(DF12PT);
  displayStringOnFrame(frmp, value, &blue);
  displayFrame(frmp, &black);
  // structure
  Pos_t p1 = {0,30};
  displayHLine(p1, 160, &black);
}

void show_configuration_program_screen(char* username, uint8_t programId,
				       float aspeed, float max_speed, uint32_t duration,
				       uint8_t* speed_pgrm, uint8_t* slope_pgrm)
{
  // display_clear(&white);
  Frame_t *frm;
  char value[20];
  frm = search_layout(conf_layout, conf_layout_size, "heading");
  char *heading = get_heading(CONF, ANY);
  setFont(DF12PT);
  displayStringOnFrame(*frm, heading , &blue);
  displayFrame(*frm, &black);
  // owner
  frm = search_layout(conf_layout, conf_layout_size, "username");
  sprintf(value, "%s", username);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &blue);
  displayFrame(*frm, &black);
  // program Id
  frm = search_layout(conf_layout, conf_layout_size, "prg_id");
  sprintf(value, "%1u", programId);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &blue);
  displayFrame(*frm, &black);

  // duration
  frm = search_layout(conf_layout, conf_layout_size, "duration");
  int h,m; 
  h = duration/3600;
  m = (duration - (3600*h))/60;
  if (h>0) sprintf(value,"%01u:%02u",h,m);
  else sprintf(value, "00:%02u", m);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &blue);
  // avspeed
  frm = search_layout(conf_layout, conf_layout_size, "aspeed");
  sprintf(value, "Vmed=%3.1f Km/h", aspeed);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &blue);
  // max_speed
  frm = search_layout(conf_layout, conf_layout_size, "max_speed");
  sprintf(value, "Vmax=%3.1f Km/h", max_speed);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &red);
  // speed program
  frm = search_layout(conf_layout, conf_layout_size, "speed_prgm");
  displayGraphOnFrame(*frm, duration, speed_pgrm, &orange);
  // slope program
  frm = search_layout(conf_layout, conf_layout_size, "slope_prgm");
  displayGraphOnFrame(*frm, duration, slope_pgrm, &blue);
  // structure
  Pos_t p1 = {0,30};
  displayHLine(p1, 160, &black);
}

void show_lead_run_screen(float rspeed_next, uint32_t lasting_time)
{
  Frame_t *frm;
  char value[20];
  // next rspeed
  frm = search_layout(run_layout, run_layout_size, "rspeed_next");
  sprintf(value, "%3.1f", rspeed_next);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &blue);
  // rspeed lasting time
  frm = search_layout(run_layout, run_layout_size, "rspeed_time");
  uint m = lasting_time/60;
  uint s = lasting_time - (lasting_time*60);
  sprintf(value, "%1u:%02u", m,s);
  setFont(DF12PT);
  displayStringOnFrame(*frm, value, &red);
}

void show_run_screen(uint8_t mode, uint8_t status, float rspeed, float ispeed, float aspeed,
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
  frm = search_layout(run_layout, run_layout_size, "rspeed");
  sprintf(value, "%3.1f", rspeed); 
  setFont(DV24PT);
  displayStringOnFrame(*frm, value, &blue);
  //displayFrame(*frm, &black);
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
			uint32_t race_size, float* race)
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
  int h,m; 
  h = duration/3600;
  m = (duration - (3600*h))/60;
  if (h>1) sprintf(value,"%01u:%02u",h,m);
  else sprintf(value, "%02u min",m);
  setFont(UB16PT);
  displayStringOnFrame(*frm, value, &blue);
  // race profile
  uint8_t *_race = malloc(race_size * sizeof(uint8_t));
  for(uint i=0; i<race_size; i++) _race[i] = (uint8_t)race[i];
  frm = search_layout(report_layout, report_layout_size, "race");
  displayGraphOnFrame(*frm, race_size, _race, &orange);
  free(_race);
}

