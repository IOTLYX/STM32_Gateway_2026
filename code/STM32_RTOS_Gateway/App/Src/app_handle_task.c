#include "app_handle_task.h"
#include "cmsis_os2.h"

void start_handle_task(void *argument)
{
    for (;;)
    {
        osDelay(10);
    }
}
