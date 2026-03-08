#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

#define RING_BUFFER_SIZE 1024U

extern uint8_t ring_buffer[RING_BUFFER_SIZE];
extern volatile uint16_t ring_buffer_head;
extern volatile uint16_t ring_buffer_tail;

#endif