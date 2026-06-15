#include "../inc/FIFOO.h"

// Check .h for struct

void FIFO_Init(FIFO_t *f){ f->head = 0; f->tail = 0; }          // Init FIFO

uint16_t next(uint16_t idx){ return (idx+1) & (FIFO_SIZE-1);  } // Circular

bool FIFO_Empty(FIFO_t *f){  return (f->head == f->tail);     } // If H=T, empty

bool FIFO_Full(FIFO_t *f){   return next(f->head) == f->tail; } // If N(H)=T, then wrapped around and full


bool FIFO_Put(FIFO_t *f, char data){
    if(FIFO_Full(f)) return false;  // failure if full
    f->buf[f->head] = data;         // place data
    f->head = next(f->head);        // set head to next
    return true;                    // success
}

bool FIFO_Get(FIFO_t *f, char *data){
    if(FIFO_Empty(f)) return false; // failure if empty
    *data = f->buf[f->tail];        // grab bottom of stack
    f->tail = next(f->tail);        // set tail to next
    return true;                    // success
}
