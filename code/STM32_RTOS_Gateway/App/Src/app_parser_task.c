#include "app_parser_task.h"
#include "cmsis_os2.h"
#include "main.h"
#include "stm32f4xx_hal_gpio.h"


void start_parser_task(void *argument)
{
    for (;;)
    {
        HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
        osDelay(500);
    }
}
