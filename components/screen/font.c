#include "font.h"

static uint8_t getCharPtr(uint8_t c) {
  uint16_t tempPtr = 4; // point at first char data

  do {
    fontChar.charCode = cfont.font[tempPtr++];
    if (fontChar.charCode == 0xFF) return 0;
    
    fontChar.adjYOffset = cfont.font[tempPtr++];
    fontChar.width = cfont.font[tempPtr++];
    fontChar.height = cfont.font[tempPtr++];
    fontChar.xOffset = cfont.font[tempPtr++];
    fontChar.xOffset = fontChar.xOffset < 0x80 ? fontChar.xOffset : -(0xFF - fontChar.xOffset);
    fontChar.xDelta = cfont.font[tempPtr++];
    
    if (c != fontChar.charCode && fontChar.charCode != 0xFF) {
      if (fontChar.width != 0) {
        // packed bits
        tempPtr += (((fontChar.width * fontChar.height)-1) / 8) + 1;
      }
    }
  } while ((c != fontChar.charCode) && (fontChar.charCode != 0xFF));
  
  fontChar.dataPtr = tempPtr;

  if (c == fontChar.charCode) {
    if (font_forceFixed > 0) {
      // fix width & offset for forced fixed width
      fontChar.xDelta = cfont.max_x_size;
      fontChar.xOffset = (fontChar.xDelta - fontChar.width) / 2;
    }
  } else return 0;
  
  return 1;
}
int TFT_getStringWidth(char* str)
{
  int strWidth = 0;
  
  if (cfont.bitmap == 2) strWidth = ((_7seg_width()+2) * strlen(str)) - 2;   // 7-segment font
  else if (cfont.x_size != 0) strWidth = strlen(str) * cfont.x_size;	 // fixed width font
  else {
    // calculate the width of the string of proportional characters
    char* tempStrptr = str;
    while (*tempStrptr != 0) {
      if (getCharPtr(*tempStrptr++)) {
	strWidth += (((fontChar.width > fontChar.xDelta) ? fontChar.width : fontChar.xDelta) + 1);
      }
    }
    strWidth--;
  }
  return strWidth;
}

static int printProportionalChar(int x, int y)
{
  uint8_t ch = 0;
  int i, j, char_width;
  
  char_width = ((fontChar.width > fontChar.xDelta) ? fontChar.width : fontChar.xDelta);
  
  if ((font_buffered_char) && (!font_transparent)) {
    int len, bufPos;
    
    // === buffer Glyph data for faster sending ===
    len = char_width * cfont.y_size;
    color_t *color_line = heap_caps_malloc(len*3, MALLOC_CAP_DMA);
    if (color_line) {
      // fill with background color
      for (int n = 0; n < len; n++) {
	color_line[n] = _bg;
      }
      // set character pixels to foreground color
      uint8_t mask = 0x80;
      for (j=0; j < fontChar.height; j++) {
	for (i=0; i < fontChar.width; i++) {
	  if (((i + (j*fontChar.width)) % 8) == 0) {
	    mask = 0x80;
	    ch = cfont.font[fontChar.dataPtr++];
	  }
	  if ((ch & mask) != 0) {
	    // visible pixel
	    bufPos = ((j + fontChar.adjYOffset) * char_width) + (fontChar.xOffset + i);  // bufY + bufX
	    color_line[bufPos] = _fg;
	  }
	  mask >>= 1;
	}
      }
      // send to display in one transaction
      disp_select();
      send_data(x, y, x+char_width-1, y+cfont.y_size-1, len, color_line);
      disp_deselect();
      free(color_line);
      
      return char_width;
    }
  }
  
  int cx, cy;
  
  if (!font_transparent) _fillRect(x, y, char_width+1, cfont.y_size, _bg);
  
  // draw Glyph
  uint8_t mask = 0x80;
  disp_select();
  for (j=0; j < fontChar.height; j++) {
    for (i=0; i < fontChar.width; i++) {
      if (((i + (j*fontChar.width)) % 8) == 0) {
	mask = 0x80;
	ch = cfont.font[fontChar.dataPtr++];
      }
      
      if ((ch & mask) !=0) {
	cx = (uint16_t)(x+fontChar.xOffset+i);
	cy = (uint16_t)(y+j+fontChar.adjYOffset);
	_drawPixel(cx, cy, _fg, 0);
      }
      mask >>= 1;
    }
  }
  disp_deselect();
  
  return char_width;
}

static void printChar(uint8_t c, int x, int y)
{
  uint8_t i, j, ch, fz, mask;
  uint16_t k, temp, cx, cy, len;
  
  // fz = bytes per char row
  fz = cfont.x_size/8;
  if (cfont.x_size % 8) fz++;
  
  // get character position in buffer
  temp = ((c-cfont.offset)*((fz)*cfont.y_size))+4;
  
  if ((font_buffered_char) && (!font_transparent)) {
    // === buffer Glyph data for faster sending ===
    len = cfont.x_size * cfont.y_size;
    color_t *color_line = heap_caps_malloc(len*3, MALLOC_CAP_DMA);
    if (color_line) {
      // fill with background color
      for (int n = 0; n < len; n++) {
	color_line[n] = _bg;
      }
      // set character pixels to foreground color
      for (j=0; j<cfont.y_size; j++) {
	for (k=0; k < fz; k++) {
	  ch = cfont.font[temp+k];
	  mask=0x80;
	  for (i=0; i<8; i++) {
	    if ((ch & mask) !=0) color_line[(j*cfont.x_size) + (i+(k*8))] = _fg;
	    mask >>= 1;
	  }
	}
	temp += (fz);
      }
      // send to display in one transaction
      disp_select();
      send_data(x, y, x+cfont.x_size-1, y+cfont.y_size-1, len, color_line);
      disp_deselect();
      free(color_line);
      
      return;
    }
  }
  
  if (!font_transparent) _fillRect(x, y, cfont.x_size, cfont.y_size, _bg);
  
  disp_select();
  for (j=0; j<cfont.y_size; j++) {
    for (k=0; k < fz; k++) {
      ch = cfont.font[temp+k];
      mask=0x80;
      for (i=0; i<8; i++) {
	if ((ch & mask) !=0) {
	  cx = (uint16_t)(x+i+(k*8));
	  cy = (uint16_t)(y+j);
	  _drawPixel(cx, cy, _fg, 0);
	}
	mask >>= 1;
      }
    }
    temp += (fz);
  }
  disp_deselect();
}



void displayString(char *st, int x, int y) {
  int stl, i, tmpw, tmph, fh;
  uint8_t ch;
  
  if (cfont.bitmap == 0) return; // wrong font selected
  
  // ** Rotated strings cannot be aligned
  if ((font_rotate != 0) && ((x <= CENTER) || (y <= CENTER))) return;
  
  if ((x < LASTX) || (font_rotate == 0)) TFT_OFFSET = 0;
  
  if ((x >= LASTX) && (x < LASTY)) x = TFT_X + (x-LASTX);
  else if (x > CENTER) x += dispWin.x1;
  
  if (y >= LASTY) y = TFT_Y + (y-LASTY);
  else if (y > CENTER) y += dispWin.y1;
  
  // ** Get number of characters in string to print
  stl = strlen(st);
  
  // ** Calculate CENTER, RIGHT or BOTTOM position
  tmpw = TFT_getStringWidth(st);	// string width in pixels
  fh = cfont.y_size;			// font height
  if ((cfont.x_size != 0) && (cfont.bitmap == 2)) {
    // 7-segment font
    fh = (3 * (2 * cfont.y_size + 1)) + (2 * cfont.x_size);  // 7-seg character height
  }
  
  if (x == RIGHT) x = dispWin.x2 - tmpw + dispWin.x1;
  else if (x == CENTER) x = (((dispWin.x2 - dispWin.x1 + 1) - tmpw) / 2) + dispWin.x1;
  
  if (y == BOTTOM) y = dispWin.y2 - fh + dispWin.y1;
  else if (y==CENTER) y = (((dispWin.y2 - dispWin.y1 + 1) - (fh/2)) / 2) + dispWin.y1;
  
  if (x < dispWin.x1) x = dispWin.x1;
  if (y < dispWin.y1) y = dispWin.y1;
  if ((x > dispWin.x2) || (y > dispWin.y2)) return;
  
  TFT_X = x;
  TFT_Y = y;
  
  // ** Adjust y position
  tmph = cfont.y_size; // font height
  // for non-proportional fonts, char width is the same for all chars
  tmpw = cfont.x_size;
  if (cfont.x_size != 0) {
    if (cfont.bitmap == 2) {	// 7-segment font
      tmpw = _7seg_width();	// character width
      tmph = _7seg_height();	// character height
    }
  }
  else TFT_OFFSET = 0;	// fixed font; offset not needed
  
  if ((TFT_Y + tmph - 1) > dispWin.y2) return;
  
  int offset = TFT_OFFSET;
  
  for (i=0; i<stl; i++) {
    ch = st[i]; // get string character
    
    if (ch == 0x0D) { // === '\r', erase to eol ====
      if ((!font_transparent) && (font_rotate==0)) _fillRect(TFT_X, TFT_Y,  dispWin.x2+1-TFT_X, tmph, _bg);
    }
    
    else if (ch == 0x0A) { // ==== '\n', new line ====
      if (cfont.bitmap == 1) {
	TFT_Y += tmph + font_line_space;
	if (TFT_Y > (dispWin.y2-tmph)) break;
	TFT_X = dispWin.x1;
      }
    }
    
    else { // ==== other characters ====
      if (cfont.x_size == 0) {
	// for proportional font get character data to 'fontChar'
	if (getCharPtr(ch)) tmpw = fontChar.xDelta;
	else continue;
      }
      
      // check if character can be displayed in the current line
      if ((TFT_X+tmpw) > (dispWin.x2)) {
	if (text_wrap == 0) break;
	TFT_Y += tmph + font_line_space;
	if (TFT_Y > (dispWin.y2-tmph)) break;
	TFT_X = dispWin.x1;
      }
      
      // Let's print the character
      if (cfont.x_size == 0) {
	// == proportional font
	if (font_rotate == 0) TFT_X += printProportionalChar(TFT_X, TFT_Y) + 1;
	else {
	  // rotated proportional font
	  offset += rotatePropChar(x, y, offset);
	  TFT_OFFSET = offset;
	}
      }
      else {
	if (cfont.bitmap == 1) {
	  // == fixed font
	  if ((ch < cfont.offset) || ((ch-cfont.offset) > cfont.numchars)) ch = cfont.offset;
	  if (font_rotate == 0) {
	    printChar(ch, TFT_X, TFT_Y);
	    TFT_X += tmpw;
	  }
	  else rotateChar(ch, x, y, i);
	}
	else if (cfont.bitmap == 2) {
	  // == 7-segment font ==
	  _draw7seg(TFT_X, TFT_Y, ch, cfont.y_size, cfont.x_size, _fg);
	  TFT_X += (tmpw + 2);
	}
      }
    }
  }
}
