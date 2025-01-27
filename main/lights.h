#ifndef LIGHTS_H
#define LIGHTS_H

#include <stdint.h>

typedef struct
{
    char type[32];
    int segment;
    int64_t value;
} blink_event_t;

void lights_init(void);
void queue_lights_event(const blink_event_t event);

#endif // LIGHTS_H