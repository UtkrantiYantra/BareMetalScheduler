#ifndef EVENT_H
#define EVENT_H
#include <stdint.h>
#include <stddef.h>

typedef uint16_t event_id_t;

typedef struct {
    event_id_t id;
    void *payload;
    size_t size;
} event_t;

#endif // EVENT_H
