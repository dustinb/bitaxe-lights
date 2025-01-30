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
#include "esp_timer.h"

static const char *TAG = "LIGHTS";

static QueueHandle_t lights_queue = NULL;

static void lights_task(void *pvParameters);
static void neopixel_clear();
static void neopixel_price_bar_fall();
static void neopixel_price_bar_rise();
static void neopixel_police(int iterations);
static void neopixel_flash(int start, int end, int iterations, uint32_t colorOn, uint32_t colorOff);
static void neopixel_on(int start, int end, uint32_t colorOn);
static void neopixel_bar(int barSize, int start, int end, uint32_t colorOn, uint32_t colorOff, int delay);

// https://github.com/zorxx/neopixel/tree/main
static tNeopixelContext neopixel;
static uint32_t refreshRate;
static uint32_t taskDelay;
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define PIXEL_COUNT 75
#define NEOPIXEL_PIN GPIO_NUM_12

static const uint32_t COLOR_BITCOIN_ORANGE = NP_RGB(150, 90, 0);
static const uint32_t COLOR_BITCOIN_YELLOW = NP_RGB(130, 96, 0);
static const uint32_t COLOR_WHITE = NP_RGB(120, 120, 120);
static const uint32_t COLOR_RED = NP_RGB(214, 17, 37);
static const uint32_t COLOR_GREEN = NP_RGB(18, 125, 4);
static const uint32_t COLOR_BLUE = NP_RGB(6, 19, 201);
static const uint32_t COLOR_OFF = NP_RGB(0, 0, 0);

// Last price received
static int64_t lastPrice = 0;

// LED positions for each the 6 segments
static const int SEGMENT[][2] = {
    {0, 13},
    {13, 24},
    {24, 37},
    {37, 50},
    {50, 60},
    {61, 74},
};

void lights_init(void)
{
    // Initialize neopixel
    neopixel = neopixel_Init(PIXEL_COUNT, NEOPIXEL_PIN);
    if (NULL == neopixel)
    {
        ESP_LOGE(TAG, "[%s] Initialization failed\n", __func__);
    }

    refreshRate = neopixel_GetRefreshRate(neopixel);
    taskDelay = MAX(1, pdMS_TO_TICKS(1000UL / refreshRate));
    neopixel_clear();
    vTaskDelay(taskDelay);

    lights_queue = xQueueCreate(100, sizeof(blink_event_t));
    xTaskCreate(lights_task, "led_task", 4096, NULL, 5, NULL);
}

void queue_lights_event(const blink_event_t event)
{
    if (lights_queue == NULL)
    {
        ESP_LOGE(TAG, "Event queue not initialized");
        return;
    }

    xQueueSend(lights_queue, &event, 10 / portTICK_PERIOD_MS);
}

// lights_queue worker. Runs effects based on event type
static void lights_task(void *pvParameters)
{
    blink_event_t event;
    while (1)
    {
        if (xQueueReceive(lights_queue, &event, 800 / portTICK_PERIOD_MS) == pdTRUE)
        {
            int segment = event.segment - 1;
            if (strcmp(event.type, "mining.submit") == 0)
            {
                neopixel_on(SEGMENT[segment][0], SEGMENT[segment][1], COLOR_BITCOIN_YELLOW);
                // neopixel_bar(2, SEGMENT[segment][0], SEGMENT[segment][1], COLOR_BITCOIN_YELLOW, COLOR_BITCOIN_YELLOW);
                neopixel_flash(SEGMENT[segment][0], SEGMENT[segment][1], 1, COLOR_BITCOIN_YELLOW, NP_RGB(0, 0, 0));
            }
            else if (strcmp(event.type, "mining.notify") == 0)
            {
                neopixel_flash(SEGMENT[segment][0], SEGMENT[segment][1], 5, NP_RGB(227, 218, 52), COLOR_OFF);
            }
            else if (strcmp(event.type, "tx") == 0)
            {
                neopixel_bar(10, 1, PIXEL_COUNT, COLOR_BITCOIN_ORANGE, COLOR_OFF, 0);
                // neopixel_flash(1, PIXEL_COUNT, 5, COLOR_BITCOIN_ORANGE, COLOR_OFF);
                // neopixel_police(10);
            }
            else if (strcmp(event.type, "price") == 0)
            {
                if (lastPrice > event.value)
                {
                    neopixel_price_bar_fall();
                }
                else
                {
                    neopixel_price_bar_rise();
                }
                lastPrice = event.value;
            }
            else if (strcmp(event.type, "block") == 0)
            {
                neopixel_bar(10, 1, PIXEL_COUNT, COLOR_WHITE, COLOR_OFF, 0);
                neopixel_flash(1, PIXEL_COUNT, 10, COLOR_WHITE, COLOR_OFF);
            }
        }
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
    int length = end - start;
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
        vTaskDelay(taskDelay + 50 / portTICK_PERIOD_MS);

        neopixel_SetPixel(neopixel, pixelsOff, ARRAY_SIZE(pixelsOff));
        vTaskDelay(taskDelay + 50 / portTICK_PERIOD_MS);
    }

    neopixel_SetPixel(neopixel, pixelsClear, ARRAY_SIZE(pixelsClear));
}

static void neopixel_on(int start, int end, uint32_t colorOn)
{
    for (int i = start; i < end + 1; i++)
    {
        tNeopixel pixel[] = {
            {i, colorOn},
        };
        neopixel_SetPixel(neopixel, pixel, ARRAY_SIZE(pixel));
        vTaskDelay(taskDelay);
    }
    vTaskDelay(300 / portTICK_PERIOD_MS);
    neopixel_clear();
}

static void neopixel_price_bar_fall()
{
    int secondPixelIndex = 29;
    for (int i = 81; i > 65; i--)
    {
        tNeopixel pixel[] = {
            {i % 74, COLOR_RED},
            {secondPixelIndex++, COLOR_RED},
        };
        neopixel_SetPixel(neopixel, pixel, ARRAY_SIZE(pixel));
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    neopixel_clear();
}

static void neopixel_price_bar_rise()
{
    int secondPixelIndex = 44;
    for (int i = 66; i < 82; i++)
    {
        tNeopixel pixel[] = {
            {i % 74, COLOR_GREEN},
            {secondPixelIndex--, COLOR_GREEN},
        };
        neopixel_SetPixel(neopixel, pixel, ARRAY_SIZE(pixel));
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    neopixel_clear();
}

static void neopixel_police(int iterations)
{
    tNeopixel pixel[PIXEL_COUNT];
    uint32_t colors[] = {COLOR_RED, COLOR_BLUE};
    for (int j = 0; j < iterations; j++)
    {
        for (int i = 0; i < PIXEL_COUNT; i++)
        {
            pixel[i] = (tNeopixel){i, colors[i % 2]};
        }
        neopixel_SetPixel(neopixel, pixel, ARRAY_SIZE(pixel));
        vTaskDelay(10 + taskDelay);

        for (int i = 0; i < PIXEL_COUNT; i++)
        {
            pixel[i] = (tNeopixel){i, colors[(i + 1) % 2]};
        }
        neopixel_SetPixel(neopixel, pixel, ARRAY_SIZE(pixel));
        vTaskDelay(10 + taskDelay);
    }
    neopixel_clear();
}

static void neopixel_bar(int barSize, int start, int end, uint32_t colorOn, uint32_t colorOff, int delay)
{
    // Turn on the first barSize pixels
    for (int i = start; i < start + barSize; i++)
    {
        tNeopixel pixel[] = {
            {i, colorOn},
        };
        neopixel_SetPixel(neopixel, pixel, ARRAY_SIZE(pixel));
        vTaskDelay(taskDelay + delay / portTICK_PERIOD_MS);
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
        vTaskDelay(taskDelay + delay / portTICK_PERIOD_MS);
    }
    // Turn off the last barSize pixels
    for (int i = end - barSize; i < end; i++)
    {
        tNeopixel pixel[] = {
            {i, colorOff},
        };
        neopixel_SetPixel(neopixel, pixel, ARRAY_SIZE(pixel));
        vTaskDelay(taskDelay + delay / portTICK_PERIOD_MS);
    }
}