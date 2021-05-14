#include <stdlib.h>
#include <stdio.h>
    
typedef struct __attribute__((__packed__)) {
  uint8_t x;
  uint8_t y;
} Pos_t;    

typedef struct __attribute__((__packed__)) {
  Pos_t pos; //position
  uint8_t width;
  uint8_t height;
} Frame_t;

typedef struct __attribute__((__packed__)) {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} color_t;

enum Font {DF12PT, UB16PT, DV18PT, DV24PT};

void display_init(const color_t* fg, const color_t* bg);
uint8_t setFont(uint8_t id);
void displayPString(Pos_t p,const char* ptr, const color_t* fg);
void displayStringOnFrame(Frame_t f,const char* ptr, const color_t* fg);
void displayGraphOnFrame(Frame_t f, uint8_t x_scale, uint8_t y_scale, uint32_t size,
			 const float* ptr, const color_t* fg);
void displayHProgressBar(Frame_t f, float progress, const color_t* fg);
void displayFrame(Frame_t f, const color_t* fg);
void displayHLine(Pos_t p, uint16_t length, const color_t* fg);
void displayHDashedLine(Pos_t, uint16_t length, const color_t* fg);
void displayVLine(Pos_t p, uint16_t length, const color_t* fg);
void displayXaxis(Pos_t p, uint16_t length, uint8_t step, const color_t* fg);
void displayYaxis(Pos_t p, uint16_t height, uint8_t step, const color_t* fg);
void displayGrid(Frame_t f, uint8_t x_step, uint8_t y_step, const color_t* fg);
void displayBox(Pos_t p, uint8_t width, uint8_t height, const color_t* fg); 
void displayHBFrame(Frame_t f, const color_t* fg);
void displayHUFrame(Frame_t f, const color_t* fg);
void display_clear(const color_t* fg);
uint16_t color565(const color_t *color);


// ==== Color definitions constants ==============
const color_t black;
const color_t navy;
const color_t darkgreen;
const color_t darkcyan;
const color_t maroon;
const color_t purple;
const color_t olive;
const color_t lightgrey;
const color_t darkgrey;
const color_t blue;
const color_t green;
const color_t cyan;
const color_t red;
const color_t magenta;
const color_t yellow;
const color_t white;
const color_t orange;
const color_t greenyellow;
const color_t pink;
