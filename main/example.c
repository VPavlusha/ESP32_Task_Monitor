#include "task_monitor.h"

void app_main(void)
{
    // NOTE: configUSE_TRACE_FACILITY and configGENERATE_RUN_TIME_STATS must be defined as 1
    // in FreeRTOSConfig.h for this API function task_monitor() to be available.
    task_monitor();
}
