#include <stdio.h>
#include "driver/i2c.h"
#include "ambsensor.h"

#define I2C_MASTER_FREQ_HZ 100000 // 100kHz
#define WRITE_BIT I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ    /*!< I2C master read */
#define ACK_CHECK_EN 0x1            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                 /*!< I2C ack value */
#define NACK_VAL 0x1                /*!< I2C nack value */

AmbSensor_t* pself;
const static int i2c_master_port = 1;
const static uint8_t address = 0x44;
const static uint8_t sht30_cmd[2] = {0x2C, 0x06}; //clock stretching enabled  and high repeatability
const static uint8_t g_polynom = 0x31;

static uint8_t crc8 (uint8_t data[], int len)
{
  // initialization value
  uint8_t crc = 0xff;
  // iterate over all bytes
  for (int i=0; i < len; i++){
    crc ^= data[i];   
    for (int i = 0; i < 8; i++){
      bool xor = crc & 0x80;
      crc = crc << 1;
      crc = xor ? crc ^ g_polynom : crc;
    }
  }
  return crc;
} 

static void get_values (uint8_t* raw_temp, uint8_t* raw_humid, float* temperature, float* humidity)
{
  *temperature = ((((raw_temp[0] * 256.0) + raw_temp[1]) * 175) / 65535.0) - 45;  
  *humidity = ((((raw_humid[0] * 256.0) + raw_humid[1]) * 100) / 65535.0);
}

void ambsensor_init(AmbSensor_t* self)
{
  pself = self;
  i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = self->i2c_sda,         // select GPIO specific to your project
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = self->i2c_scl,         // select GPIO specific to your project
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

bool ambsensor_read(float* temperature, float* humidity)
{
  uint8_t crc1, crc2, raw_temp[2], raw_humid[2];
  esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (address << 1) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write(cmd, sht30_cmd, 2 , ACK_CHECK_EN);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(i2c_master_port, cmd, 1000 / portTICK_RATE_MS);
  if (ret != ESP_OK) return pdFALSE;
  //  assert(ret == ESP_OK);
  i2c_cmd_link_delete(cmd);
  //
  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (address << 1) | READ_BIT, ACK_CHECK_EN);
  i2c_master_read(cmd, raw_temp, 2, ACK_VAL);
  i2c_master_read_byte(cmd, &crc1, ACK_VAL);
  i2c_master_read(cmd, raw_humid, 2, ACK_VAL);
  i2c_master_read_byte(cmd, &crc2, NACK_VAL);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(i2c_master_port, cmd, 1000 / portTICK_RATE_MS);
  if (ret != ESP_OK) return pdFALSE;
  //assert(crc1 ==  crc8(raw_temp, 2));
  if (crc1 !=  crc8(raw_temp, 2)) return pdFALSE;
  if (crc2 !=  crc8(raw_humid, 2)) return pdFALSE;
  i2c_cmd_link_delete(cmd);
  //
  get_values(raw_temp, raw_humid, temperature, humidity);
  //  printf("%.1f, %.0f \n", *temperature, *humidity);
  return pdTRUE;
}
