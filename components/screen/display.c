#include <string.h>
#include "st7735.h"
#include "display.h"
#include "esp_log.h"

// ==== Color definitions constants ==============
const color_t black       = {   0,   0,   0 };
const color_t navy        = {   0,   0, 0x80 };
const color_t darkgreen   = {   0, 0x80,   0 };
const color_t darkcyan    = {   0, 0x80, 0x80 };
const color_t maroon      = { 0x80,   0,   0 };
const color_t purple      = { 0x80,   0, 0x80 };
const color_t olive       = { 0x80, 0x80,   0 };
const color_t lightgrey   = { 0xC0, 0xC0, 0xC0 };
const color_t darkgrey    = { 0x80, 0x80, 0x80 };
const color_t blue        = {   0,   0, 0xFF };
const color_t green       = {   0, 0xFF,   0 };
const color_t cyan        = {   0, 0xFF, 0xFF };
const color_t red         = { 0xFC,   0,   0 };
const color_t magenta     = { 0xFC,   0, 0xFF };
const color_t yellow      = { 0xFC, 0xFC,   0 };
const color_t white       = { 0xFC, 0xFC, 0xFC };
const color_t orange      = { 0xFC, 0xA4,   0 };
const color_t greenyellow = { 0xAC, 0xFC, 0x2C };
const color_t pink        = { 0xFC, 0xC0, 0xCA };

#define NCOLS 160
#define NROWS 128

extern const char *TAG;

uint16_t _fg;
uint16_t _bg;

extern const unsigned char tft_Dejavu24[];
const unsigned char* pfont24 = tft_Dejavu24;

extern const unsigned char tft_Dejavu18[];
const unsigned char* pfont18 = tft_Dejavu18;

extern const unsigned char tft_Ubuntu16[];
const unsigned char* pfont16 = tft_Ubuntu16;

extern const unsigned char DefaultFont[];
const unsigned char* pfont12 = DefaultFont;

typedef struct {
  enum Font id;
  const unsigned char* pbook;
  uint8_t y_size;
} font_t;

static font_t font;

typedef struct {
      uint8_t charCode;
      int adjYOffset;
      int width;
      int height;
      int xOffset;
      int xDelta;
      uint16_t dataPtr;
} propFont;

static propFont	fontChar;


void display_init(const color_t* fg, const color_t *bg)
{
  _fg = color565(fg);
  _bg = color565(bg);
  display_clear(bg);
  setFont(DF12PT);
  //  font.id=DF12PT; font.pbook=pfont12; font.y_size=12;
}

uint8_t setFont(uint8_t id)
{
  uint8_t prev_id = font.id;
  switch(id){
  case DF12PT:
    font.id=id;  font.pbook=pfont12, font.y_size=font.pbook[1];
    break;
  case UB16PT:
    font.id=id; font.pbook=pfont16; font.y_size=font.pbook[1];
    break; 
  case DV18PT:
    font.id=id; font.pbook=pfont18; font.y_size=font.pbook[1];
    break;
  case DV24PT:
    font.id=id; font.pbook=pfont24; font.y_size=font.pbook[1];
    break;

  }
  return prev_id;
}

static bool getCharPtr(const char c)
{
  uint16_t ptr = 4; // point at first char data
  
  do {
    fontChar.charCode = font.pbook[ptr++];
    if (fontChar.charCode == 0xFF) return 0;
    
    fontChar.adjYOffset = font.pbook[ptr++];
    fontChar.width = font.pbook[ptr++];
    fontChar.height = font.pbook[ptr++];
    fontChar.xOffset = font.pbook[ptr++];
    fontChar.xOffset = fontChar.xOffset < 0x80 ? fontChar.xOffset : -(0xFF - fontChar.xOffset);
    fontChar.xDelta = font.pbook[ptr++];
    
    if (c != fontChar.charCode && fontChar.charCode != 0xFF) 
      if (fontChar.width != 0)          // packed bits
        ptr += (((fontChar.width * fontChar.height)-1) / 8) + 1;
  } while ((c != fontChar.charCode) && (fontChar.charCode != 0xFF));
  
  fontChar.dataPtr = ptr;
  return 1;
}

static uint16_t displayPChar(const Pos_t p, const char c, uint16_t fg)
{
  uint8_t ch = 0;
  int i, j, char_width;

  getCharPtr(c);
  char_width = ((fontChar.width > fontChar.xDelta) ? fontChar.width : fontChar.xDelta);
  
  int len, bufPos;
  
  // === buffer Glyph data for faster sending ===
  len = char_width * font.y_size;
  uint16_t data[len];

  // fill with background color
  for (int n = 0; n < len; n++) data[n] = _bg;
  
  // set character pixels to foreground color
  uint8_t mask = 0x80;
  for (j=0; j < fontChar.height; j++) {
    for (i=0; i < fontChar.width; i++) {
      if (((i + (j*fontChar.width)) % 8) == 0) {
	  mask = 0x80;
	  ch = font.pbook[fontChar.dataPtr++];
      }
      if ((ch & mask) != 0) {
	bufPos = ((j + fontChar.adjYOffset) * char_width) + (fontChar.xOffset + i);  // bufY + bufX
	data[bufPos] = fg;
      }
      mask >>= 1;
    }
  }
  // send to display in one transaction
  display_data(p.x, p.y, char_width, font.y_size, data);
  return char_width;
}

void displayPString(Pos_t p,const char* ptr, const color_t* fg)
{
  int n = strlen(ptr);
  Pos_t pos = p;
  uint16_t _fg = color565(fg); 
  for (int i=0; i<n; i++)
    pos.x += displayPChar(pos, *ptr++, _fg);
}


static void clearFrame(Frame_t f)
{
  const uint16_t ncols = (f.pos.x + f.width > NCOLS) ? NCOLS - f.pos.x : f.width;
  const uint16_t nrows = (f.pos.y + f.height > NROWS) ? NROWS - f.pos.y : f.height;
  const uint16_t color = 0xFFFF; // white
  uint16_t data[ncols]; // columns
  for (int i=0; i<ncols; i++) data[i] = color;
  for (int i=0; i<nrows; i++) // rows
    display_data(f.pos.x, f.pos.y+i, ncols, 1, data);
}

static uint16_t stringWidth(const char* str)
{
  int strWidth = 0;
  // calculate the width of the string of proportional characters
  const char* ptr = str;
  while (*ptr != 0) 
    if (getCharPtr(*ptr++))  //strWidth += fontChar.xDelta + 1;
      strWidth += (((fontChar.width > fontChar.xDelta) ? fontChar.width : fontChar.xDelta) + 1);
  strWidth--;
  //ESP_LOGI(TAG, "String=%s, width=%d", str, strWidth);
  return strWidth;
}
void displayStringOnFrame(Frame_t f,const char* ptr, const color_t* fg)
{
  uint16_t width = stringWidth(ptr); // n*font_x_size
  clearFrame(f);
  Pos_t p = {f.pos.x + (f.width - width)/2 , f.pos.y + (f.height - font.y_size)/2};
  displayPString(p, ptr, fg);
}

void displayGraphOnFrame(Frame_t f, uint32_t begin, uint32_t end, const float* ptr, const color_t* fg)
{
  Pos_t p = f.pos;
  clearFrame(f);
  uint size = end-begin;
  for(uint i=0; i<size; i++){
    displayVLine(p, ptr[begin+i], fg);
    p.x++;
  }
}

void displayHLine(Pos_t p, uint16_t length, const color_t* fg)
{
  uint16_t data[length]; //black
  uint16_t color = color565(fg);
  for (int i=0; i<length; i++) data[i] = color;
  display_data(p.x, p.y, length, 1, data);
}

void displayHUFrame(Frame_t f, const color_t* fg)
{
  Pos_t p = {f.pos.x, f.pos.y+1};
  displayHLine(p, f.width, fg);
}
void displayHBFrame(Frame_t f, const color_t* fg)
{
  Pos_t p = {f.pos.x, f.pos.y-1 + f.height};
  displayHLine(p,f.width, fg);
}

void displayVLine(Pos_t p, uint16_t length, const color_t* fg)
{
  //  uint16_t data = 0x0000;
  uint16_t data = color565(fg);
  for(int i=0; i<length; i++)
    display_data(p.x, p.y+i, 1, 1, &data);
}

void displayFrame(Frame_t f, const color_t* fg)
{
  const uint16_t ncols = f.width;
  const uint16_t nrows = f.height;
  const uint16_t color = color565(fg);
  uint16_t data[ncols]; // columns
  for (int i=0; i<ncols; i++) data[i] = color;
  display_data(f.pos.x, f.pos.y, ncols, 1, data);
  display_data(f.pos.x, f.pos.y+f.height-1, ncols, 1, data);
  for (int i=0; i<nrows; i ++){
    display_data(f.pos.x, f.pos.y+i, 1, 1, data);
    display_data(f.pos.x+f.width-1, f.pos.y+i, 1, 1,data);
  }
}

void display_clear(const color_t* fg)
{
  const uint16_t nrows = 128;
  const uint16_t ncols = 160;
  const uint16_t color = color565(fg);
  uint16_t data[ncols]; // columns
  for (int i=0; i<ncols; i++) data[i] = color;
  for (int i=0; i<nrows; i++) // rows
    display_data(0, i, ncols, 1, data);
}

uint16_t color565(const color_t *color)
{
  return ((color->blue & 0xF8)<<8) + ((color->red & 0xFC)<<3) + (color->green>>3);
}


/* // used for fixed width font: h w */
/* extern const unsigned char tft_SmallFont[]; */
/* const unsigned char* font = tft_SmallFont; */
/* const uint8_t w=8; */
/* const uint8_t h=12; */
/* static void displayChar(Pos_t p, char c, uint16_t fg) */
/* { */
/*   int size = w*h; */
/*   uint16_t data[size]; */
/*   uint16_t cpos = (c - 0x20)*12 + 4; */
/*   uint8_t mask, m; */
/*   for(int i=0; i<h; i++){ */
/*     m = font[cpos+i]; */
/*     mask = 0x80; */
/*     for(int j=0; j<w; j++){ */
/*       data[i*w+j] = (m & mask) ? fg : _bg; */
/*       mask >>= 1; */
/*      } */
/*   } */
/*   display_data(p.x, p.y, w, h, data); */
/* } */

/* void displayString(Pos_t p, char* ptr, const color_t* fg) */
/* { */
/*   int n = strlen(ptr); */
/*   Pos_t pos = p; */
/*   uint16_t _fg = color565(fg); */
/*   for(int j=0; j<n; j++){ */
/*     displayChar(pos, *ptr++, _fg); */
/*     pos.x += w; */
/*   } */
/* } */
