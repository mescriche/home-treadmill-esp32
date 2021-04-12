#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "freertos/queue.h"
#include "st7735.h"
#include "screen.h"
#include "display.h"


extern color_t blue, magenta, white, red, purple, orange, darkcyan;

static char* message[]= {"WELCOME!!","ON-READY", "ON-RUNNING", "ON-PAUSE",  "OFF-STOPPING", "OFF-STOPPED" };

static QueueHandle_t req_queue;

Frame_t layout[] = {
  {{ 0,30}, 90, 50}, {{ 0,75}, 90, 20}, {{5,98}, 80, 30}, //SPEED
  {{ 90,35}, 70, 20}, // SLOPE
  {{ 85,98}, 80, 30}, {{110,0}, 50, 30},  // DISTANCE, DURATION
  {{ 90,55}, 70, 20}, {{90,75}, 70, 20}, // TEMPERATURE, HUMIDITY
  {{ 0,0}, 110, 30} // MESSAGE
};

static float rspeed=0, ispeed=0;
static uint32_t dist = 0;

static void displayPattern()
{
  Pos_t p1 = {0, 30}, p2={0,98}, p3={90, 30};
  displayHLine(p1, 160);
  displayHLine(p2, 160);  
  displayVLine(p3, 68);
}
static void _display_on_screen(Display_req req)
{
  Frame_t frm = layout[req.type];
  char value[20];
  int sec, h,m,s;
  uint8_t fontId;
  switch (req.type){
  case RSPEED:
    rspeed = *((float*)req.dataptr);
    sprintf(value, "%3.1f", rspeed);
    fontId = setFont(DV24PT);
    displayStringOnFrame(frm, value, &blue);
    setFont(fontId);
    //displayFrame(frm);
    free(req.dataptr);
    break;
  case ISPEED:
    ispeed = *((float*)req.dataptr);
    sprintf(value, "%3.1f Km/h", ispeed);
    //    fontId = setFont(UB16PT);
    displayStringOnFrame(frm, value, &blue);
    //setFont(fontId);
    //displayFrame(frm);
    free(req.dataptr);   
    break;
  case ASPEED:
    sprintf(value, "%3.1f Km/h", *((float*)req.dataptr));
    fontId = setFont(UB16PT);
    displayStringOnFrame(frm, value, &blue);
    setFont(fontId);
    //displayFrame(frm);
    free(req.dataptr);
    break;
  case  SLOPE: 
    sprintf(value,  "%1u %%&", *((int*)req.dataptr));
    displayStringOnFrame(frm, value, &red);
    //displayFrame(frm);
    free(req.dataptr);
    break;
  case DISTANCE:
    dist =  *((uint32_t*)req.dataptr); //meters
    if (dist/1000 >= 1) sprintf(value, "%.1f Km", (float)dist/1000);
    else sprintf(value, "%3u m", dist);
    fontId = setFont(UB16PT);
    displayStringOnFrame(frm, value, &red);
    setFont(fontId);
    //displayFrame(frm);
    free(req.dataptr);
    break;
  case DURATION:
    sec =  *((int*)req.dataptr);
    h = sec/3600;
    m = (sec - (3600*h))/60;
    s = sec -(3600*h)-(m*60);
    if (h>9) sprintf(value,"%02u:%02u",h,m);
    else if (h>0) sprintf(value,"%01u:%02u:%02u",h,m,s);
    else sprintf(value, "%02u:%02u", m,s);
    fontId = setFont(UB16PT);
    displayStringOnFrame(frm, value, &blue);
    setFont(fontId);
    //displayFrame(frm);
    free(req.dataptr);
    break;
  case TEMPERATURE:
    sprintf(value, "%3.1f^C", *((float*)req.dataptr));
    displayStringOnFrame(frm, value, &orange);
    //displayFrame(frm);
    free(req.dataptr); 
    break;
  case HUMIDITY:
    sprintf(value, "%2.0f %%HR", *((float*)req.dataptr));
    displayStringOnFrame(frm, value, &magenta);
    //displayFrame(frm);
    free(req.dataptr); 
    break;
    break;
  case MESSAGE:
    //    displayString(pos, req.dataptr, &red);
    displayStringOnFrame(frm, req.dataptr, &red);
    //displayFrame(frm);
    break;
  default:;
  }
  displayPattern();
}

static void screen_keeper(void* pvParam)
{
  Display_req req;
  BaseType_t xResult;
  while (1) {
    xResult = xQueueReceive(req_queue, &req, portMAX_DELAY);
    //configASSERT(xResult == pdPASS);
    if (xResult != pdPASS) continue;
    _display_on_screen(req);
  }
}

void screen_init(Screen_t* self)
{
  BaseType_t xResult;
  tft_st7735_init(self->sda, self->scl, self->cs, self->rst, self->dc, self->bl);
  display_init(&magenta, &white);
  req_queue = xQueueCreate(20, sizeof(Display_req));
  configASSERT(req_queue != NULL);
  xResult = xTaskCreate(screen_keeper, "screen_keeper", 2048, NULL, 5, &self->server);
  configASSERT(xResult == pdPASS);
}

void show_message(uint16_t msg)
{
  BaseType_t xResult;
  Display_req req = {MESSAGE, message[msg]};
  xResult = xQueueSend(req_queue, &req, 0);
  if (xResult != pdPASS) free(req.dataptr);
  //configASSERT(xResult == pdPASS);
}

void show_speed(float rspd, float ispd)
{
  BaseType_t xResult;
  Display_req req = {RSPEED, NULL};
  req.dataptr = malloc(sizeof(float));
  *(float*)req.dataptr = rspd;
  xResult = xQueueSend(req_queue, &req, 0);
  if (xResult != pdPASS) free(req.dataptr);
  //configASSERT(xResult == pdPASS);

  Display_req req1 = {ISPEED, NULL};
  req1.dataptr = malloc(sizeof(float));
  *(float*)req1.dataptr = ispd;
  xResult = xQueueSend(req_queue, &req1, 0);
  if (xResult != pdPASS) free(req1.dataptr);
  //configASSERT(xResult == pdPASS);
}

void show_ambsensor(float temperature, float humidity)
{
  BaseType_t xResult;
  Display_req req = {TEMPERATURE, NULL};
  req.dataptr = malloc(sizeof(float));
  *(float*)req.dataptr = temperature;
  xResult = xQueueSend(req_queue, &req, 0);
  if (xResult != pdPASS) free(req.dataptr);
  //configASSERT(xResult == pdPASS);

  req.type = HUMIDITY;
  req.dataptr = malloc(sizeof(float));
  *(float*)req.dataptr = humidity;
  xResult = xQueueSend(req_queue, &req, 0);
  if (xResult != pdPASS) free(req.dataptr);
  //configASSERT(xResult == pdPASS);
}

void show_aspeed(float spd)
{
  BaseType_t xResult;
  Display_req req = {ASPEED, NULL};
  req.dataptr = malloc(sizeof(float));
  *(float*)req.dataptr = spd;
  xResult = xQueueSend(req_queue, &req, 0);
  if (xResult != pdPASS) free(req.dataptr);
  //configASSERT(xResult == pdPASS);
}


void show_slope(uint16_t level)
{
  BaseType_t xResult;
  Display_req req = {SLOPE, NULL};
  req.dataptr = malloc(sizeof(int));
  *(int*)req.dataptr = level;
  xResult = xQueueSend(req_queue, &req, 0);
  if (xResult != pdPASS) free(req.dataptr);
  //configASSERT(xResult == pdPASS);
}

void show_duration(uint32_t time)
{
  BaseType_t xResult;
  Display_req req = {DURATION, NULL};
  req.dataptr = malloc(sizeof(uint32_t));
  *(int*)req.dataptr = time;
  xResult = xQueueSend(req_queue, &req, 0);
  if (xResult != pdPASS) free(req.dataptr);
}

void show_distance(uint32_t dist)
{
  BaseType_t xResult;
  Display_req req = {DISTANCE, NULL};
  req.dataptr = malloc(sizeof(uint32_t));
  *(uint32_t*)req.dataptr = dist;
  xResult = xQueueSend(req_queue, &req, 0);
  if (xResult != pdPASS) free(req.dataptr);
  //configASSERT(xResult == pdPASS);
}

void show_status(uint16_t status)
{
  switch(status){
  case 0:  //ON_READY
    show_message(READY);
    break;
  case 1: // ON_RUNNING:
    show_message(RUNNING);
    break;
  case 2: // ON_PAUSE
    show_message(MPAUSE);
    break;
  case 3: //OFF_STOPPING:
    show_message(STOPPING);
    break;
  case 4: // OFF_STOPPED:
    show_message(STOPPED);
    break;
  }
}
