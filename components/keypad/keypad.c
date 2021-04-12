#include <stdio.h>
#include "driver/i2c.h"
#include "keypad.h"
#include "esp_log.h"
       
#define I2C_MASTER_FREQ_HZ 100000 // 100kHz
#define WRITE_BIT I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ    /*!< I2C master read */
#define ACK_CHECK_EN 0x1            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                 /*!< I2C ack value */
#define NACK_VAL 0x1                /*!< I2C nack value */

extern const char *TAG;
const static int i2c_master_port = 0;
const static uint8_t address = 0x71;
static Keypad_t* pself;

static void IRAM_ATTR keypad_isr_handler(void* arg)
{
  BaseType_t xHigherPriorityTaskWoken;
  xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(pself->reader, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

bool keypad_read(uint8_t *value)
{
  esp_err_t ret;
  uint8_t data;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (address << 0) | READ_BIT, ACK_CHECK_EN);
  i2c_master_read_byte(cmd, &data, NACK_VAL);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(i2c_master_port, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret != ESP_OK) return pdFALSE;
  if (data == 0xFF) return pdFALSE;
  *value = ~data;
  return pdTRUE;
}

void keypad_init(Keypad_t* self)
{
  pself = self;
  gpio_config_t io_conf;
  io_conf.pin_bit_mask = 1ULL << self->intr;
  io_conf.intr_type = GPIO_INTR_NEGEDGE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 1;
  gpio_config(&io_conf);  
  //
  //install gpio isr service
  gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
  //hook isr handler for specific gpio pin
  gpio_isr_handler_add(self->intr, keypad_isr_handler, (void*)NULL);

  i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = self->sda,         // select GPIO specific to your project
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = self->scl,         // select GPIO specific to your project
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = I2C_MASTER_FREQ_HZ  // select frequency specific to your project
    // .clk_flags = 0,         
  };
  esp_err_t ret;
  ret = i2c_param_config(i2c_master_port, &conf);
  ESP_ERROR_CHECK(ret);
  ret = i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
  ESP_ERROR_CHECK(ret);
}
