#include "task_monitor.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#if !defined(CONFIG_FREERTOS_USE_TRACE_FACILITY) || !defined(CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS)
    #error "USE_TRACE_FACILITY and GENERATE_RUN_TIME_STATS must be defined!"
#endif

#define COLOR_BLACK   "30"
#define COLOR_RED     "31"
#define COLOR_GREEN   "32"
#define COLOR_YELLOW  "33"
#define COLOR_BLUE    "34"
#define COLOR_PURPLE  "35"
#define COLOR_CYAN    "36"
#define COLOR_WHITE   "37"

#define COLOR(COLOR)  "\033[0;" COLOR "m"
#define RESET_COLOR   "\033[0m"
#define UNDERLINE     "\033[4m"  // TODO: FIX. Does not work!
#define BOLD          "\033[1m"  // TODO: FIX. Does not work!

#define BLACK   COLOR(COLOR_BLACK)
#define RED     COLOR(COLOR_RED)
#define GREEN   COLOR(COLOR_GREEN)
#define YELLOW  COLOR(COLOR_YELLOW)
#define BLUE    COLOR(COLOR_BLUE)
#define PURPLE  COLOR(COLOR_PURPLE)
#define CYAN    COLOR(COLOR_CYAN)
#define WHITE   COLOR(COLOR_WHITE)

static uint32_t get_current_time_ms(void)
{
    return xTaskGetTickCount() * 1000 / configTICK_RATE_HZ;
}


static float get_current_heap_free_percent(void)
{
    size_t current_size = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    size_t total_size = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    return ((float) current_size / total_size) * 100.0;
}

static float get_minimum_heap_free_percent(void)
{
    size_t minimum_size = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    size_t total_size = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    return ((float) minimum_size / total_size) * 100.0;
}

static const char* int_to_string(int number, char *string)
{
    sprintf(string, "%d", number);
    return string;
}

static const char* task_state_to_string(eTaskState state) {
    switch (state) {
        case eRunning:
            return "Running";
        case eReady:
            return "Ready";
        case eBlocked:
            return "Blocked";
        case eSuspended:
            return "Suspended";
        case eDeleted:
            return "Deleted";
        case eInvalid:
            return "Invalid";
        default:
            return "Unknown state";
    }
}

static void sort_tasks_by_runtime(TaskStatus_t *tasks_status_array, size_t number_of_tasks)
{
    for (size_t i = 0; i < number_of_tasks - 1; ++i) {
        for (size_t k = i + 1; k < number_of_tasks; ++k) {
            if (tasks_status_array[k].ulRunTimeCounter > tasks_status_array[i].ulRunTimeCounter) {
                TaskStatus_t temp = tasks_status_array[i];
                tasks_status_array[i] = tasks_status_array[k];
                tasks_status_array[k] = temp;
            }
        }
    }
}

static void task_status_monitor_task(void *params)
{
    while (true) {
        UBaseType_t number_of_tasks = uxTaskGetNumberOfTasks();
        TaskStatus_t *p_tasks_status_array = pvPortMalloc(number_of_tasks * sizeof(TaskStatus_t));

        if (p_tasks_status_array != NULL) {
            uint32_t total_run_time;
            number_of_tasks = uxTaskGetSystemState(p_tasks_status_array, number_of_tasks, &total_run_time);
    
            if (total_run_time > 0) {  // Avoid divide by zero error
                sort_tasks_by_runtime(p_tasks_status_array, number_of_tasks);

                printf("I (%lu) tm: " CYAN "%-18.16s %-11.10s %-7.6s %-8.7s %-11.10s %-12.10s %-15.15s %-s"
                    RESET_COLOR,
                    get_current_time_ms(),
                    "TASK NAME:",
                    "STATE:",
                    "CORE:",
                    "NUMBER:",
                    "PRIORITY:",
                    "STACK_MIN:",
                    "RUNTIME, µs:",
                    "RUNTIME, %:\n"
                );

                for (size_t i = 0; i < number_of_tasks; ++i) {            
                    char string[10];  // 10 - maximum number of characters for int
                    printf("I (%lu) tm: " YELLOW "%-18.16s %-11.10s %-7.6s %-8d %-11d %-12lu %-14lu %-10.3f\n"
                        RESET_COLOR,
                        get_current_time_ms(),
                        p_tasks_status_array[i].pcTaskName,
                        task_state_to_string(p_tasks_status_array[i].eCurrentState),

                        xTaskGetAffinity(p_tasks_status_array[i].xHandle) == tskNO_AFFINITY ?
                            "Any" : int_to_string((int)xTaskGetAffinity(p_tasks_status_array[i].xHandle), string),

                        p_tasks_status_array[i].xTaskNumber,
                        p_tasks_status_array[i].uxCurrentPriority,
                        p_tasks_status_array[i].usStackHighWaterMark,
                        p_tasks_status_array[i].ulRunTimeCounter,
                       (p_tasks_status_array[i].ulRunTimeCounter * 100.0) / total_run_time);
                }

                printf("I (%lu) tm: " YELLOW "Total heap free size:   " GREEN "%d" YELLOW " bytes\n" RESET_COLOR,
                    get_current_time_ms(), heap_caps_get_total_size(MALLOC_CAP_DEFAULT));

                printf("I (%lu) tm: " YELLOW "Current heap free size: " GREEN "%d" YELLOW " bytes (" GREEN "%.2f"
                    YELLOW " %%)\n" RESET_COLOR, get_current_time_ms(), heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
                    get_current_heap_free_percent());

                printf("I (%lu) tm: " YELLOW "Minimum heap free size: " GREEN "%d" YELLOW " bytes (" GREEN "%.2f"
                    YELLOW " %%)\n" RESET_COLOR, get_current_time_ms(),
                    heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT), get_minimum_heap_free_percent());

                printf("I (%lu) tm: " YELLOW "Total RunTime: " GREEN "%lu" YELLOW " µs (" GREEN "%lu" YELLOW
                    " seconds)\n" RESET_COLOR, get_current_time_ms(), total_run_time, total_run_time / 1000000);
                
                uint64_t current_time = esp_timer_get_time();
                printf("I (%lu) tm: " YELLOW "System UpTime: " GREEN "%llu" YELLOW " µs (" GREEN "%llu" YELLOW
                    " seconds)\n\n" RESET_COLOR, get_current_time_ms(), current_time, current_time / 1000000);
            }
            vPortFree(p_tasks_status_array);
        } else {
            printf("I (%lu) tm: " RED "Could not allocate required memory\n" RESET_COLOR, get_current_time_ms());
        }
        vTaskDelay(pdMS_TO_TICKS(30 * 1000));
    }
}

esp_err_t task_monitor(void)
{
    BaseType_t status = xTaskCreatePinnedToCore(task_status_monitor_task, "monitor_task", configMINIMAL_STACK_SIZE * 4,
                                                NULL, tskIDLE_PRIORITY + 1, NULL, tskNO_AFFINITY);
    if (status != pdPASS) {
        printf("I (%lu) tm: task_status_monitor_task(): Task was not created. Could not allocate required memory\n", 
            get_current_time_ms());
        return ESP_ERR_NO_MEM;
    }
    printf("I (%lu) tm: task_status_monitor_task() started successfully\n", get_current_time_ms());
    return ESP_OK;
}
