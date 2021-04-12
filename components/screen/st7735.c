#include <string.h>
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "st7735.h"

// ==== Display commands constants ====
// https://www.displayfuture.com/Display/datasheet/controller/ST7735.pdf
// No read operation considered - MISO is not operative
#define ST7735_NOP      0x00 // No Operation - No Parameter = NP
#define ST7735_SWRESET	0x01 // Sotware reset - NP - await 120 ms before next command
#define ST7735_SLPIN	0x10 // Sleep in & booster off - NP - enter minimum power consumption mode
#define ST7735_SLPOUT	0x11 // Sleep out & booster on - NP - turns off sleep mode - await 120 ms before next command
#define ST7735_PTLON	0x12 // Partial mode on - NP 
#define ST7735_NORON	0x13 // Partial off (=> normal display mode on) - NP
#define ST7735_INVOFF   0x20 // Display inversion off - NP
#define ST7735_INVONN   0x21 // Display inversion on - NP
#define ST7735_GAMMASET 0x26 // Gamma curve select - # 1P (01h, 02h, 04h, 08h) = (G1.0, G2.5, G2.2, G1.8)
#define ST7735_DISPOFF  0x28 // Display off - NP - in this mode, the output from Frame Memory is disabled and blank page inserted
#define ST7735_DISPON   0x29 // Display on - NP - output from frame memory is enabled - await 120 ms after DISPOFF
#define ST7735_CASET	0x2A // Column address set - #4P
#define ST7735_RASET	0x2B // Row address set - #4P
#define ST7735_RAMWR	0x2C // Memory write - #memory range P
#define ST7735_PTLAR    0x30 // Partial Area start/end address set - #4P
#define ST7735_TEOFF    0x34 // Tearing effect line off - NP
#define ST7735_TEON     0x35 // Tearing effect mode set & on - NP
#define ST7735_MADCTL   0x36 // Memory data access control
#define ST7735_IDMOFF   0x38 // idle mode off
#define ST7735_IDMON    0x39 // idle mode on
#define ST7735_COLMOD   0x3A // interface format
#define ST7735_FRMCTR1  0xB1 // set the frame frecuency of the full colors normal mode
#define ST7735_FRMCTR2  0xB2 //Set the frame frequency of the Idle mode
#define ST7735_FRMCTR3  0xB3 // Set the frame frequency of the Partial mode/ full colors.
#define ST7735_INVCTR   0xB4 // Display Inversion mode control
#define ST7735_DISSET5  0xB6 // Display Function set 5
#define ST7735_PWCTR1   0xC0 //  Power Control 1 
#define ST7735_PWCTR2   0xC1 // Power Control 2
#define ST7735_PWCTR3   0xC2 //  Power Control 3 (in Normal mode/ Full colors)
#define ST7735_PWCTR4   0xC3 //  Power Control 4 (in Idle mode/ 8-colors) 
#define ST7735_PWCTR5   0xC4  // Power Control 5 (in Partial mode/ full-colors) 
#define ST7735_VMCTR1   0xC5  // VCOM Control 1 
#define ST7735_VMOFCTR  0xC7  // VCOM Offset Control 
#define ST7735_PWCTR6   0xFC // Power Control 5 (in Partial mode + Idle mode)

typedef struct {
  uint8_t cmd;
  uint8_t data[16];
  uint8_t databytes;  //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} ST7735_config_t;

DRAM_ATTR static spi_device_handle_t spi;

DRAM_ATTR static const ST7735_config_t ST7735_config[] = {
  /* Sleep Out */
  {ST7735_SLPOUT, {0}, 0x80}, //await 120 ms
  /* Display Off */
  {ST7735_DISPOFF, {0}, 0}, 
  /* Memory Data Access Control */
  //{ST7735_MADCTL, {0x00}, 1}, //  [MY|MX|MV|ML| |RGB|MH|-|-] = (0000 0000) normal vertical display
  //{ST7735_MADCTL, {0xC0}, 0x01}, //  [MY|MX|MV|ML| |RBG|MH|-|-] = (1100 1000) upside down  display  
  {ST7735_MADCTL, {0xA0}, 0X01}, //  [MY|MX|MV|ML| |RGB|MH|-|-] = (1010 0000) x-y exchange  normal horizontal display
  //{ST7735_MADCTL, {0x60}, 1}, //  [MY|MX|MV|ML| |RGB|MH|-|-] = (0110 0000) x-y exchange  normal horizontal display
  //{ST7735_MADCTL, {0xF4}, 1}, //  [MY|MX|MV|ML| |RGB|MH|-|-] = (1110 0000) horizontal display
  /* Color Mode Interface Format  */
  {ST7735_COLMOD, {0x05}, 0x01},  // 16 bits / pixel 
  // Normal mode
  {ST7735_NORON, {0}, 0x00},
  /* Display On */
  {ST7735_DISPON, {0}, 0x80}, //await 120 ms
  {0, {0}, 0xff}
};

//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.

IRAM_ATTR static void _st7735_dc_signal(spi_transaction_t *t)
{
  //uint32_t   dc  = ((uint32_t)  t->user) & 0x0001;
  //gpio_num_t pin = ((gpio_num_t)t->user) & 0x00FE;
  int dc=(int)t->user;
  static gpio_num_t pin = GPIO_NUM_32;
  gpio_set_level(pin, dc);
}

IRAM_ATTR static void _st7735_cmd(uint8_t cmd)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

IRAM_ATTR static void _st7735_data(const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len==0) return;             //no need to send anything
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=len*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               //Data
    t.user=(void*)1;                //D/C needs to be set to 1
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

IRAM_ATTR void _st7735_init(gpio_num_t rst, gpio_num_t dc, gpio_num_t bl)
{
  const ST7735_config_t* cmd_list = ST7735_config;
  
  // reset display
  gpio_reset_pin(rst);
  gpio_set_direction(rst, GPIO_MODE_OUTPUT);
  gpio_set_level(rst, 0);
  vTaskDelay(1 / portTICK_RATE_MS); // 10 useconds is minimum according to specs
  gpio_set_level(rst, 1);

  gpio_config_t io_conf;
  io_conf.pin_bit_mask = 1ULL << dc | 1ULL << bl;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 1;
  gpio_config(&io_conf);
  
  //Send all the commands to init lcd
  int cmd=0;    
  while (cmd_list[cmd].databytes!=0xff) {
    _st7735_cmd(cmd_list[cmd].cmd);
    _st7735_data(cmd_list[cmd].data, cmd_list[cmd].databytes&0x1F); // 0001 1111
    if (cmd_list[cmd].databytes&0x80)      // 1000 0000
      vTaskDelay(120 / portTICK_RATE_MS); // await 120 ms
    cmd++;
  }
  ///Enable backlight
  gpio_set_level(bl, 1);
}

IRAM_ATTR void _wait_display_data()
{
  spi_transaction_t *rtrans;
  esp_err_t ret;
  //Wait for all 6 transactions to be done and get back the results.
  for (int x=0; x<6; x++) {
    ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
    assert(ret==ESP_OK);
    //We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though.
  }
}

IRAM_ATTR void display_data(uint8_t xpos, uint8_t ypos, uint8_t w, uint8_t h,  const uint16_t *data)
{
  static const uint8_t xoffset=1;
  static const uint8_t yoffset=2;
  static spi_transaction_t trans[6];
  uint8_t x;
  esp_err_t ret;  
  //variables. We allocate them on the stack, so we need to re-init them each call.
  for (x=0; x<6; x++) {
    memset(&trans[x], 0, sizeof(spi_transaction_t));
    if ((x&1)==0) {
      //Even transfers are commands
      trans[x].length=8; // 8 bits long
      trans[x].user=(void*)0;
    } else {
      //Odd transfers are data
      trans[x].length=8*4; // 4 parameters = 32 bits long
      trans[x].user=(void*)1;
    }
    trans[x].flags=SPI_TRANS_USE_TXDATA; // use spi_transaction to store data instead of a buffer since it's 32 bits long
  }
  //
  trans[0].tx_data[0]=ST7735_CASET;   //Column Address Set  CASET
  trans[1].tx_data[0]=0;              //Start Col High
  trans[1].tx_data[1]=(xpos+xoffset)&0xFF;       //Start Col Low
  trans[1].tx_data[2]=0;              //End Col High
  trans[1].tx_data[3]=(xpos+xoffset+w-1)&0xFF;     //End Col Low
  //
  trans[2].tx_data[0]=ST7735_RASET;   //Row address set
  trans[3].tx_data[0]=0;      //Start row high
  trans[3].tx_data[1]=(ypos+yoffset)&0xFF;    //start row low
  trans[3].tx_data[2]=0;    //end row high
  trans[3].tx_data[3]=(ypos+yoffset+h-1)&0xFF;  //end row low
  //
  trans[4].tx_data[0]=ST7735_RAMWR;   //memory write
  trans[5].tx_buffer=data;        //finally send the line data
  trans[5].length=w*h*2*8;          //Data length, in bits ; max 128*160*2 = 40960 bytes
  trans[5].flags=0; //undo SPI_TRANS_USE_TXDATA flag since we're using a buffer 
  
  //Queue all transactions.
  for (x=0; x<6; x++) {
    ret=spi_device_queue_trans(spi, &trans[x], portMAX_DELAY);
    assert(ret==ESP_OK);
  }
  _wait_display_data(); // wait for transactions to end
}

void tft_st7735_init(gpio_num_t sda, gpio_num_t scl, gpio_num_t cs, gpio_num_t rst, gpio_num_t dc, gpio_num_t bl)
{
  //  spi_device_handle_t spi;
  esp_err_t ret; 
  spi_bus_config_t buscfg = { 
    .miso_io_num = GPIO_NUM_NC, // not connected 
    .mosi_io_num = sda, 
    .sclk_io_num = scl, 
    .quadwp_io_num=-1, 
    .quadhd_io_num=-1, 
    //.max_transfer_sz=PARALLEL_LINES*130*2+8 
    .max_transfer_sz=10*160*2+8 
  }; 
  /* //Initialize the SPI bus */
  ret=spi_bus_initialize(VSPI_HOST, &buscfg, DMA_CHAN);
  ESP_ERROR_CHECK(ret); 
    
  spi_device_interface_config_t devcfg = { 
    .clock_speed_hz = 26*1000*1000,
    .mode = 0, 
    .spics_io_num = cs,
    .queue_size = 10,
    .pre_cb = _st7735_dc_signal 
  }; 
  
  ret = spi_bus_add_device (VSPI_HOST, &devcfg, &spi); 
  ESP_ERROR_CHECK(ret); 
  
  ret = spi_device_acquire_bus(spi, portMAX_DELAY);
  ESP_ERROR_CHECK(ret);
  
  _st7735_init(rst, dc, bl);
  
}
