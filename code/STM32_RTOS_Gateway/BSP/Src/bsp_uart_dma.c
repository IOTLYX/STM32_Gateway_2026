#include "bsp_uart_dma.h"

#include "cmsis_os2.h"
#include "ring_buffer.h"
#include "usart.h"

#include <stdbool.h>
#include <stdint.h>

#define UART1_DMA_RX_BUFFER_SIZE   256U
#define APP_PARSER_FLAG_UART_RX    (1U << 0)

#if ((RING_BUFFER_SIZE & (RING_BUFFER_SIZE - 1U)) != 0U)
#error "RING_BUFFER_SIZE must be power of 2"
#endif

#define RING_BUFFER_MASK   (RING_BUFFER_SIZE - 1U)

static uint8_t uart1_dma_rx_buffer[UART1_DMA_RX_BUFFER_SIZE];
static volatile uint16_t s_uart1_dma_last_pos = 0U;
static volatile uint32_t s_uart1_dma_drop_cnt = 0U;

extern osThreadId_t app_parser_taskHandle;

/* ========================= 内部辅助函数 ========================= */

static inline uint16_t ring_buffer_next_index(uint16_t index)
{
    return (uint16_t)((index + 1U) & RING_BUFFER_MASK);
}

/* ISR中压入1字节到环形缓冲区
 * 成功返回true
 * 满了返回false，并累计丢包计数
 */
static bool ring_buffer_push_byte_from_isr(uint8_t data)
{
    uint16_t next_head = ring_buffer_next_index(ring_buffer_head);

    if (next_head == ring_buffer_tail)
    {
        s_uart1_dma_drop_cnt++;
        return false;
    }

    ring_buffer[ring_buffer_head] = data;
    ring_buffer_head = next_head;
    return true;
}

static void uart1_notify_parser_task_from_isr(void)
{
    if (app_parser_taskHandle != NULL)
    {
        (void)osThreadFlagsSet(app_parser_taskHandle, APP_PARSER_FLAG_UART_RX);
    }
}

/* 从DMA环形接收缓冲区，把“新收到的数据”搬运到软件ring_buffer
 *
 * 核心依据：
 * DMA的NDTR寄存器表示“剩余未搬运数”
 * 当前DMA写入位置 = BUF_SIZE - NDTR
 */
static void uart1_dma_fetch_new_data_to_ring_buffer_from_isr(void)
{
    uint16_t dma_pos;
    uint16_t i;

    if (huart1.hdmarx == NULL)
    {
        return;
    }

    dma_pos = (uint16_t)(UART1_DMA_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx));

    if (dma_pos >= UART1_DMA_RX_BUFFER_SIZE)
    {
        dma_pos = 0U;
    }

    /* 没有新数据 */
    if (dma_pos == s_uart1_dma_last_pos)
    {
        return;
    }

    if (dma_pos > s_uart1_dma_last_pos)
    {
        /* 未回卷：直接搬 [last_pos, dma_pos) */
        for (i = s_uart1_dma_last_pos; i < dma_pos; i++)
        {
            (void)ring_buffer_push_byte_from_isr(uart1_dma_rx_buffer[i]);
        }
    }
    else
    {
        /* 已回卷：先搬 [last_pos, end)，再搬 [0, dma_pos) */
        for (i = s_uart1_dma_last_pos; i < UART1_DMA_RX_BUFFER_SIZE; i++)
        {
            (void)ring_buffer_push_byte_from_isr(uart1_dma_rx_buffer[i]);
        }

        for (i = 0U; i < dma_pos; i++)
        {
            (void)ring_buffer_push_byte_from_isr(uart1_dma_rx_buffer[i]);
        }
    }

    s_uart1_dma_last_pos = dma_pos;
}

/* ========================= 对外接口 ========================= */

HAL_StatusTypeDef bsp_uart_dma_init(void)
{
    ring_buffer_head = 0U;
    ring_buffer_tail = 0U;
    s_uart1_dma_last_pos = 0U;
    s_uart1_dma_drop_cnt = 0U;

    if (huart1.hdmarx == NULL)
    {
        return HAL_ERROR;
    }

    __HAL_UART_CLEAR_IDLEFLAG(&huart1);

    /* DMA必须配置为 Circular 模式 */
    if (HAL_UART_Receive_DMA(&huart1, uart1_dma_rx_buffer, UART1_DMA_RX_BUFFER_SIZE) != HAL_OK)
    {
        return HAL_ERROR;
    }

    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    return HAL_OK;
}

HAL_StatusTypeDef bsp_uart_dma_restart(void)
{
    if (huart1.hdmarx == NULL)
    {
        return HAL_ERROR;
    }

    (void)HAL_UART_DMAStop(&huart1);

    s_uart1_dma_last_pos = 0U;
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);

    if (HAL_UART_Receive_DMA(&huart1, uart1_dma_rx_buffer, UART1_DMA_RX_BUFFER_SIZE) != HAL_OK)
    {
        return HAL_ERROR;
    }

    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    return HAL_OK;
}

void bsp_uart_dma_rx_irq_handler(void)
{
    if ((__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != RESET) &&
        (__HAL_UART_GET_IT_SOURCE(&huart1, UART_IT_IDLE) != RESET))
    {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);

        uart1_dma_fetch_new_data_to_ring_buffer_from_isr();
        uart1_notify_parser_task_from_isr();
    }
}

uint32_t bsp_uart_dma_get_drop_count(void)
{
    return s_uart1_dma_drop_cnt;
}

/* ========================= HAL回调 =========================
 * 作用：
 * 1. 即使一直接收不断流，没有IDLE，也能在半满/全满时把数据搬出来
 * 2. 避免DMA缓冲区回卷时数据来不及处理
 *
 * 注意：
 * 如果你工程里别的文件已经实现了这些回调，就把下面逻辑合并过去
 */

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uart1_dma_fetch_new_data_to_ring_buffer_from_isr();
        uart1_notify_parser_task_from_isr();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uart1_dma_fetch_new_data_to_ring_buffer_from_isr();
        uart1_notify_parser_task_from_isr();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        (void)bsp_uart_dma_restart();
    }
}