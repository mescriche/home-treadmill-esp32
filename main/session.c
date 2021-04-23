#include "session.h"

extern Program mev1, mev2, mev3, mev4;
static Program* book[] = {&mev1, &mev2, &mev3, &mev4};
static uint16_t book_size = 0;
static QueueHandle_t order_queue;

void session_init()
{
  book_size = sizeof(book)/sizeof(Program*);
  
  for(int i=0; i<book_size; i++){
    //printf("name=%s\n",book[i]->name);
    uint j=0;
    book[i]->duration = 0;
    book[i]->aspeed = 0.0;
    while(book[i]->oseq[j].speed!=0){
      book[i]->oseq[j].duration = (book[i]->oseq[j].duration > 5) ? 5 : book[i]->oseq[j].duration;
      book[i]->oseq[j].speed = (book[i]->oseq[j].speed > 10) ? 10 : book[i]->oseq[j].speed; 
      book[i]->duration += book[i]->oseq[j].duration;
      book[i]->aspeed += book[i]->oseq[j].speed * book[i]->oseq[j].duration;
      j++;
    }
    book[i]->aspeed /= (float)book[i]->duration; 
    //printf("duration[min]=%d\n", book[i]->duration);
    //printf("aspeed[km/h]=%.1f\n", book[i]->aspeed);
  }
  order_queue = xQueueCreate(100, sizeof(Order));
  configASSERT(order_queue != NULL);
}

uint session_get_nPrograms()
{
  return book_size;
}

const Program* session_get_program(uint16_t id)
{
  return book[id];
}
BaseType_t session_load_program(uint16_t id)
{
  if (id >= book_size) return pdFALSE;
  uint j=0;
  BaseType_t xResult;
  do {
    xResult = xQueueSend(order_queue, &book[id]->oseq[j], pdMS_TO_TICKS(1000));
    if (xResult == pdPASS) j++;
  } while(book[id]->oseq[j].speed!=0);
  return pdTRUE;
}

BaseType_t session_get_order(Order* optr)
{
  return xQueueReceive(order_queue, &optr, portMAX_DELAY);  // wait forever
}
