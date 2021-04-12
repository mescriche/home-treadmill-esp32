typedef struct {
  uint8_t *font;
  uint8_t x_size;
  uint8_t y_size;
  uint8_t offset;
  uint16_t numchars;
  uint16_t size;
  uint8_t  max_x_size;
  uint8_t  bitmap;
  color_t  color;
} Font;

extern uint8_t DefaultFont[];

Font cfont = {
  .font = DefaultFont,
  .x_size = 0,
  .y_size = 0x0B,
  .offset = 0,
  .numchars = 95,
  .bitmap = 1,
};

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

void displayString(int x, int y, char*st);
