#ifndef BSP_UART_DMA_H
#define BSP_UART_DMA_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

HAL_StatusTypeDef bsp_uart_dma_init(void);
HAL_StatusTypeDef bsp_uart_dma_restart(void);
void bsp_uart_dma_rx_irq_handler(void);
uint32_t bsp_uart_dma_get_drop_count(void);

#endif