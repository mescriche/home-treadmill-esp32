#include "driver/spi_master.h"
#include "driver/gpio.h"
#define DMA_CHAN    2
#define TFT_WIDTH  128
#define TFT_HEIGHT 160



void tft_st7735_init(gpio_num_t sda, gpio_num_t scl, gpio_num_t cs, gpio_num_t rst, gpio_num_t dc, gpio_num_t bl);
void display_data(uint8_t xpos, uint8_t ypos, uint8_t w, uint8_t h,  const uint16_t *data);
//IRAM_ATTR void wait_display_data();
