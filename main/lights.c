#include "lights.h"
#include "esp_log.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <string.h>
#include "neopixel.h"

static const char *TAG = "LIGHTS";

#define LED_GPIO GPIO_NUM_2

typedef struct
{
    char type[32];
    int segment;
    int64_t value;
} blink_event_t;

static QueueHandle_t lights_queue = NULL;

static void neopixel_clear();

// https://github.com/zorxx/neopixel/tree/main
static tNeopixelContext neopixel;
static uint32_t refreshRate;
static uint32_t taskDelay;
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define PIXEL_COUNT 40
#define NEOPIXEL_PIN GPIO_NUM_12

void queue_led_event(const char *json)
{
    cJSON *root = cJSON_Parse((char *)json);
    if (root)
    {
        blink_event_t event = {0};
        cJSON *type = cJSON_GetObjectItem(root, "type");
        cJSON *segment = cJSON_GetObjectItem(root, "segment");
        cJSON *value = cJSON_GetObjectItem(root, "value");

        if (type && segment)
        {
            ESP_LOGW(TAG, "Type: %s, Segment: %d, Value: %lld",
                     type->valuestring, segment->valueint,
                     value ? (int64_t)value->valuedouble : 0);

            strncpy(event.type, type->valuestring, sizeof(event.type) - 1);
            event.segment = segment->valueint;
            event.value = value ? (int64_t)value->valuedouble : 0;

            if (lights_queue != NULL)
            {
                xQueueSend(lights_queue, &event, 0);
            }
        }
        cJSON_Delete(root);
    }
}

static void blink_led(int count, int delay)
{
    for (int i = 0; i < count; i++)
    {
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(delay / portTICK_PERIOD_MS);
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

static void neopixel_clear()
{
    tNeopixel pixel[PIXEL_COUNT];
    for (int i = 0; i < PIXEL_COUNT; i++)
    {
        pixel[i] = (tNeopixel){i, NP_RGB(0, 0, 0)};
    }
    neopixel_SetPixel(neopixel, pixel, ARRAY_SIZE(pixel));
    vTaskDelay(taskDelay);
}

static void neopixel_flash(int start, int end, int iterations, uint32_t colorOn, uint32_t colorOff)
{
    start -= 1;
    end -= 1;
    int length = end - start + 1;
    tNeopixel pixelsOn[length];
    tNeopixel pixelsOff[length];
    tNeopixel pixelsClear[length];

    for (int i = 0; i < length; i++)
    {
        pixelsOn[i] = (tNeopixel){start + i, colorOn};
        pixelsOff[i] = (tNeopixel){start + i, colorOff};
        pixelsClear[i] = (tNeopixel){start + i, NP_RGB(0, 0, 0)};
    }

    for (int i = 0; i < iterations; i++)
    {
        neopixel_SetPixel(neopixel, pixelsOn, ARRAY_SIZE(pixelsOn));
        vTaskDelay(taskDelay + 5);

        neopixel_SetPixel(neopixel, pixelsOff, ARRAY_SIZE(pixelsOff));
        vTaskDelay(taskDelay + 5);
    }

    neopixel_SetPixel(neopixel, pixelsClear, ARRAY_SIZE(pixelsClear));
    vTaskDelay(taskDelay);
}

static void neopixel_bar(int barSize, int start, int end, uint32_t colorOn, uint32_t colorOff)
{
    start -= 1;
    end -= 1;

    // Turn on the first barSize pixels
    for (int i = start; i < start + barSize; i++)
    {
        tNeopixel pixel[] = {
            {i, colorOn},
        };
        neopixel_SetPixel(neopixel, pixel, ARRAY_SIZE(pixel));
        vTaskDelay(taskDelay);
    }
    // Move the bar
    for (int i = start + barSize; i < end; i++)
    {
        tNeopixel pixel[] =
            {
                {(i - barSize), colorOff},
                {i, colorOn},
            };
        neopixel_SetPixel(neopixel, pixel, ARRAY_SIZE(pixel));
        vTaskDelay(taskDelay);
    }
    // Turn off the last barSize pixels
    for (int i = end - barSize; i < end; i++)
    {
        tNeopixel pixel[] = {
            {i, colorOff},
        };
        neopixel_SetPixel(neopixel, pixel, ARRAY_SIZE(pixel));
        vTaskDelay(taskDelay);
    }
}

static void lights_task(void *pvParameters)
{
    blink_event_t event;
    while (1)
    {
        if (xQueueReceive(lights_queue, &event, portMAX_DELAY) == pdTRUE)
        {
            if (strcmp(event.type, "mining.submit") == 0)
            {
                neopixel_bar(event.segment, 1, 20, NP_RGB(0, 50, 0), NP_RGB(0, 0, 0));
                // blink_led(1, 100);
            }
            else if (strcmp(event.type, "mining.notify") == 0)
            {
                neopixel_flash(1, event.segment, 5, NP_RGB(227, 218, 52), NP_RGB(0, 0, 0));
            }
            else if (strcmp(event.type, "tx") == 0)
            {
                neopixel_flash(3, 20, 5, NP_RGB(255, 153, 0), NP_RGB(0, 0, 0));
            }
            else if (strcmp(event.type, "block") == 0)
            {
                neopixel_bar(10, 1, PIXEL_COUNT, NP_RGB(120, 120, 120), NP_RGB(0, 0, 0));
                neopixel_flash(1, PIXEL_COUNT, 10, NP_RGB(120, 120, 120), NP_RGB(0, 0, 0));
            }
        }
    }
}

void lights_init(void)
{
    // Initialize GPIO
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    // Initialize neopixel
    neopixel = neopixel_Init(PIXEL_COUNT, NEOPIXEL_PIN);
    if (NULL == neopixel)
    {
        ESP_LOGE(TAG, "[%s] Initialization failed\n", __func__);
    }
    // Turn off
    refreshRate = neopixel_GetRefreshRate(neopixel);
    taskDelay = MAX(1, pdMS_TO_TICKS(1000UL / refreshRate));
    neopixel_clear();
    vTaskDelay(taskDelay);

    lights_queue = xQueueCreate(100, sizeof(blink_event_t));
    xTaskCreate(lights_task, "led_task", 4096, NULL, 5, NULL);
}
