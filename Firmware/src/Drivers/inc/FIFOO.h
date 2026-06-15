// fifo.h
#ifndef FIFO_H
#define FIFO_H
#include <stdint.h>
#include <stdbool.h>

#define FIFO_SIZE 256       // Has to be ^2 for circular math to work

typedef struct {
  volatile uint16_t head;   // write idx
  volatile uint16_t tail;   // read idx
  char buf[FIFO_SIZE];
} FIFO_t;

void FIFO_Init(FIFO_t *f);
bool FIFO_Put(FIFO_t *f, char data);
bool FIFO_Get(FIFO_t *f, char *data);
bool FIFO_Empty(FIFO_t *f);
bool FIFO_Full(FIFO_t *f);
#endif
