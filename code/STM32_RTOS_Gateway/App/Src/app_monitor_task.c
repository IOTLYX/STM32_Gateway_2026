#include "app_monitor_task.h"

#include "cmsis_os2.h"

void start_monitor_task(void *argument)
{
    for (;;)
    {
        osDelay(10);
    }
}
