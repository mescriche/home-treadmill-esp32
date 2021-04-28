#define MAX_SPEED 12
#define MAX_SLOPE 10
#define PAUSE_MAX 300 // segundos = 5m * 60s = 300 

#define BLUE_LED GPIO_NUM_2

#define LCD_BL  GPIO_NUM_14 
#define LCD_DC  GPIO_NUM_32
#define LCD_RST GPIO_NUM_12 
#define LCD_CS  GPIO_NUM_5
#define LCD_SDA GPIO_NUM_23
#define LCD_SCL GPIO_NUM_18

#define KP_INT GPIO_NUM_35
#define KP_SDA GPIO_NUM_13
#define KP_SCL GPIO_NUM_15

#define BUZZER  GPIO_NUM_25 
#define TM_INCP GPIO_NUM_27   // INC+
#define TM_INCM GPIO_NUM_19   // INC-
#define TM_SPDP GPIO_NUM_4   // SPD+
#define TM_SPDM GPIO_NUM_2   // SPD-
#define TM_SW   GPIO_NUM_26  // Stop/Walk
#define TM_ISPD  GPIO_NUM_33  // input SPD

#define I2C_SDA GPIO_NUM_21
#define I2C_SCL GPIO_NUM_22

#define RX2 GPIO_NUM_16
#define TX2 GPIO_NUM_17

#define MAX_TIME 120 // SPEED RECORD TIME = 2 hours = 120 minutes

// Inicio -> OFF_STOPPED (switch=OFF, ispeed=0) 
// OFF_STOPPED -> ON_READY (switch=ON, ispeed=0) event: switch change 
// ON_READY -> ON_RUNNING (switch=ON, ispeed > 0) event: ispeed increase
// ON -> ON_PAUSE (switch=PAUSE) event: switch -> ON_PAUSE
// ON_RUNNING -> OFF_STOPPING (switch=OFF, ispeed > 0) event: switch change
// OFF_STOPPING -> OFF_READY (switch=OFF, ispeed = 0) event: ispeed became NULL

enum Lead {MANUAL, PROGRAMMED};
enum Mode {WELCOME, CONF, RUN, PAUSE, REPORT};
enum Status {ON_READY, ON_RUNNING, OFF_STOPPING, OFF_STOPPED}; // engine status


typedef struct {
  float speed;
  TickType_t timetick;
} ISpeedRecord;

typedef struct {
  uint32_t min;
  uint16_t sec;
} Time;

typedef struct {
  enum Lead lead; 
  enum Status status;
  enum Mode mode;
  float rspeed, rspeed_backup, rspeed_next; // reference speed
  uint32_t rspeed_time_left;
  float ispeed; // instant speed
  TickType_t ispeed_timetick;
  uint32_t slope, slope_backup; // reference slope
  uint32_t duration; 
  uint32_t nsteps;
  float distance;
  TickType_t start, session_begin, session_end;
  TickType_t pause_begin, pause_end, pause_duration;
  float aspeed; // average speed
  float temperature, humidity;
  int16_t programId;
  float race[MAX_TIME]; //
} Blackboard;

typedef struct {
  Buzzer_t buzzer;
  Screen_t screen;
  Keypad_t keypad;
  SpeedMeter_t speedmeter;
  SpeedCtrl_t speedctrl;
  SlopeCtrl_t slopectrl;
  AmbSensor_t ambsensor;
} Platform;






