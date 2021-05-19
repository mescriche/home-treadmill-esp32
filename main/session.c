#include "session.h"
#include <string.h>
#include "esp_log.h"

char* users[] = {"MEV", "MJGG", "MiEG", "MaEG"};
char* usernames[] = {"Manuel", "Marichu", "Miguel", "Manu"};
extern Program test, mev1, mev2, mev3, mev4, maeg1, maeg2, maeg3, mieg1, mjgg1, mjgg2, mjgg3;
static Program* book[] = {
  &maeg1, &maeg2, &maeg3,
  &mieg1,
  &mjgg1, &mjgg2, &mjgg3,
  &mev1, &mev2, &mev3, &mev4,
  &test,
};
static uint16_t book_size = 0;
static Program** user_book = NULL;
static uint16_t user_book_size = 0;
static uint8_t user_id = 0;
static QueueHandle_t order_queue;

//static const char *TAG = "SES";

void session_init()
{
  //  ESP_LOGI(TAG, "INIT");
  book_size = sizeof(book)/sizeof(Program*);
  
  for(int i=0; i<book_size; i++){
    uint j=0;
    book[i]->duration = 0;
    book[i]->aspeed = 0.0;
    book[i]->max_speed = 1.0;
    book[i]->max_slope = 0;
    while(book[i]->oseq[j].speed!=0){
      //book[i]->oseq[j].duration = (book[i]->oseq[j].duration > 5) ? 5 : book[i]->oseq[j].duration;
      book[i]->oseq[j].speed = (book[i]->oseq[j].speed > 12) ? 12 : book[i]->oseq[j].speed; 
      book[i]->duration += book[i]->oseq[j].duration;
      book[i]->aspeed += book[i]->oseq[j].speed * book[i]->oseq[j].duration;
      if (book[i]->oseq[j].speed > book[i]->max_speed)
	book[i]->max_speed = book[i]->oseq[j].speed;
      if (book[i]->oseq[j].slope > book[i]->max_slope)
	book[i]->max_slope = book[i]->oseq[i].slope;
      j++;
    }
    book[i]->aspeed /= (float)book[i]->duration;
    /* printf("owner=%s\n", book[i]->owner); */
    /* printf("duration[min]=%d\n", book[i]->duration); */
    /* printf("aspeed[km/h]=%.1f\n", book[i]->aspeed); */
    /* printf("max speed[km/h]=%.1f\n", book[i]->max_speed); */
  }
  session_set_user(0);
  order_queue = xQueueCreate(100, sizeof(Order));
  configASSERT(order_queue != NULL);
}

void session_set_user(uint8_t id)
{
  //  ESP_LOGI(TAG, "SET_USER = %d", id);
  user_id = id;
  user_book_size = 0;
  char* user = users[id];
  for (uint i=0; i<book_size; i++)
    if (strcmp(user, book[i]->owner) == 0) user_book_size++;
  //ESP_LOGI(TAG, "SET_USER = user_book_size = %d", user_book_size);  
  if ( user_book != NULL) free(user_book);
  user_book = malloc(user_book_size * sizeof(Program*));
  uint j=0;
  for (uint i=0; i<book_size; i++)
    if (strcmp(user,book[i]->owner) == 0)
      user_book[j++] = book[i];
  /* for (uint i=0; i<user_book_size; i++) */
  /*   ESP_LOGI(TAG, "SET_USER = BOOK[%d]: owner=%s, aspeed=%.1f, max_speed=%.1f, duration=%d" */
  /* 	     , i, */
  /* 	     user_book[i]->owner, */
  /* 	     user_book[i]->aspeed, */
  /* 	     user_book[i]->max_speed, */
  /* 	     user_book[i]->duration); */
}

char* session_get_username()
{
  //  ESP_LOGI(TAG, "GET_USER_NAME = %s", usernames[user_id]);
  return usernames[user_id];
}

uint session_get_nPrograms()
{
  // ESP_LOGI(TAG, "GET_NPROGRAMS=%d", user_book_size);
  return user_book_size;
}

Program* session_get_program(uint16_t id)
{
  // ESP_LOGI(TAG, "GET_PROGRAM=%d", id);
  return user_book[id];
}

void session_get_speed_program(uint16_t id, float *speed_prgm)
{
  //  ESP_LOGI(TAG, "GET_SPEED_PROGRAM=%d", id);
  Program* prgm = user_book[id];
  uint16_t index = 0, j=0;
  while(prgm->oseq[j].speed!=0){
    for (uint i=0; i<prgm->oseq[j].duration; i++)
      speed_prgm[index++]=prgm->oseq[j].speed;
    j++;
  }
}
void session_get_slope_program(uint16_t id, uint8_t *slope_prgm)
{
  //  ESP_LOGI(TAG, "GET_SLOPE_PROGRAM=%d", id);
  Program* prgm = user_book[id];
  uint16_t index = 0, j=0;
  while(prgm->oseq[j].speed!=0){
    for (uint i=0; i<prgm->oseq[j].duration; i++)
      slope_prgm[index++]=prgm->oseq[j].slope;
    j++;
  }
}

void session_end_program()
{
   xQueueReset(order_queue);
}

void session_load_program(uint16_t id)
{
  //ESP_LOGI(TAG, "LOAD_PROGRAM=%d", id);
  uint j=0;
  BaseType_t xResult;
  xQueueReset(order_queue);
  do {
    xResult = xQueueSend(order_queue, &user_book[id]->oseq[j], pdMS_TO_TICKS(1000));
    if (xResult == pdPASS) j++;
  } while(user_book[id]->oseq[j].speed!=0);
  xQueueSend(order_queue,  &user_book[id]->oseq[j], pdMS_TO_TICKS(1000));
  //  return pdTRUE;
}

void session_get_order(Order* oPtr, Order* nPtr)
{
  //BaseType_t xResult;
  xQueueReceive(order_queue, oPtr, portMAX_DELAY);  // wait forever
  //ESP_LOGI(TAG, "GET_ORDER, rspeed=%.1f",oPtr->speed);
  xQueuePeek(order_queue, nPtr, pdMS_TO_TICKS(100));
  //configASSERT(xResult == pdTRUE);
    //ESP_LOGI(TAG, "GET_ORDER, rspeed=%.1f, next_rspeed=%.1f",oPtr->speed, nPtr->speed);

}
