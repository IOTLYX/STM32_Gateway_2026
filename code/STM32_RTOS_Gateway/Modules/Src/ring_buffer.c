#include "ring_buffer.h"

uint8_t ring_buffer[RING_BUFFER_SIZE];
volatile uint16_t ring_buffer_head = 0U;
volatile uint16_t ring_buffer_tail = 0U;