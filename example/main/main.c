#include "task_monitor.h"

void app_main(void)
{
    // NOTE: USE_TRACE_FACILITY and GENERATE_RUN_TIME_STATS must be defined as 1
    // in FreeRTOSConfig.h for this API function task_monitor() to be available.
    task_monitor();
}
