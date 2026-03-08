#include "app_parser_task.h"
#include <stdio.h>
#include "cmsis_os2.h"
#include "ring_buffer.h"

#define APP_PARSER_FLAG_UART_RX    (1U << 0)



static void protocol_parser_input_byte(uint8_t byte)
{
    /* 这里放你的协议状态机解析
       例如 AT响应、MQTT下行命令、私有帧协议解析 */
    (void)byte;
}


void start_parser_task(void *argument)
{
    printf("Parser Task Started\n");
    uint8_t ch;

    for (;;)
    {
        osThreadFlagsWait(APP_PARSER_FLAG_UART_RX,
                                osFlagsWaitAny,
                                osWaitForever);
        while (ring_buffer_tail != ring_buffer_head)
        {
            ch = ring_buffer[ring_buffer_tail];
            ring_buffer_tail = (uint16_t)((ring_buffer_tail + 1U) & (RING_BUFFER_SIZE - 1U));

            protocol_parser_input_byte(ch);
        }
        printf("TEST\n");
    }
}
