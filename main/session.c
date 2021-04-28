#include "session.h"
#include <string.h>

char* users[] = {"MEV", "MJGG", "MiEG", "MaEG"};
char* usernames[] = {"Manuel-S", "Marichu", "Miguel", "Manuel-J"};
extern Program mev1, mev2, mev3, mev4, maeg1, mieg1, mjgg1;
static Program* book[] = {
  &maeg1,
  &mieg1,
  &mjgg1, 
  &mev1, &mev2, &mev3, &mev4
};
static uint16_t book_size = 0;
static Program** user_book = NULL;
static uint16_t user_book_size = 0;
static uint8_t user_id = 0;
static QueueHandle_t order_queue;

void session_init()
{
  book_size = sizeof(book)/sizeof(Program*);
  
  for(int i=0; i<book_size; i++){
    uint j=0;
    book[i]->duration = 0;
    book[i]->aspeed = 0.0;
    book[i]->max_speed = 0.0;
    while(book[i]->oseq[j].speed!=0){
      book[i]->oseq[j].duration = (book[i]->oseq[j].duration > 5) ? 5 : book[i]->oseq[j].duration;
      book[i]->oseq[j].speed = (book[i]->oseq[j].speed > 10) ? 10 : book[i]->oseq[j].speed; 
      book[i]->duration += book[i]->oseq[j].duration;
      book[i]->aspeed += book[i]->oseq[j].speed * book[i]->oseq[j].duration;
      if (book[i]->oseq[j].speed > book[i]->max_speed)
	book[i]->max_speed = book[i]->oseq[j].speed;
      j++;
    }
    book[i]->aspeed /= (float)book[i]->duration;
    /* printf("owner=%s\n", book[i]->owner); */
    /* printf("duration[min]=%d\n", book[i]->duration); */
    /* printf("aspeed[km/h]=%.1f\n", book[i]->aspeed); */
    /* printf("max speed[km/h]=%.1f\n", book[i]->max_speed); */
  }
  order_queue = xQueueCreate(100, sizeof(Order));
  configASSERT(order_queue != NULL);
}

void session_set_user(uint8_t id)
{
  user_id = id;
  user_book_size = 0;
  char* user = users[id];
  for (uint i=0; i<book_size; i++){
    if (strcmp(user, book[i]->owner) == 0) user_book_size++;
  }
  
  if ( user_book != NULL) free(user_book);
  user_book = malloc(user_book_size * sizeof(Program*));

  for (uint i=0; i<book_size; i++)
    if (strcmp(user,book[i]->owner) == 0)
      *user_book++ = book[i]; 
}

char* session_get_username()
{
  return usernames[user_id];
}

uint session_get_nPrograms()
{
  return user_book_size;
}

Program* session_get_program(uint16_t id)
{
  return user_book[id];
}

void session_get_speed_program(uint16_t id, uint8_t *speed_prgm)
{
  Program* prgm = user_book[id];
  uint16_t index = 0, j=0;
  while(prgm->oseq[j].speed!=0){
    for (uint i=0; i<prgm->oseq[j].duration; i++)
      speed_prgm[index++]=(uint8_t)prgm->oseq[j].speed;
    j++;
  }
}
void session_get_slope_program(uint16_t id, uint8_t *slope_prgm)
{
  Program* prgm = user_book[id];
  uint16_t index = 0, j=0;
  while(prgm->oseq[j].speed!=0){
    for (uint i=0; i<prgm->oseq[j].duration; i++)
      slope_prgm[index++]=prgm->oseq[j].slope;
    j++;
  }
}

BaseType_t session_load_program(uint16_t id)
{
  uint j=0;
  BaseType_t xResult;
  do {
    xResult = xQueueSend(order_queue, &user_book[id]->oseq[j], pdMS_TO_TICKS(1000));
    if (xResult == pdPASS) j++;
  } while(user_book[id]->oseq[j].speed!=0);
  return pdTRUE;
}

void session_get_order(Order* oPtr, Order* nPtr)
{
  xQueueReceive(order_queue, oPtr, portMAX_DELAY);  // wait forever
  xQueuePeek(order_queue, nPtr, 0);
}
