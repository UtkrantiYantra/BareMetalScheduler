/**
 * @file EventConfigTOU.h
 * @brief Events published by TOU (Time-Of-Use) engine
 *
 * @EVENT_MODULE: TOU
 * @BASE_ID: 0x0B00
 */

#ifndef EVENT_CONFIG_TOU_H
#define EVENT_CONFIG_TOU_H

#include <stdint.h>

/* Event ID definitions — consumed by fw_gen.py to build EventList.h */
#define EVT_TOU_WINDOW_START     0x0B00U
#define EVT_TOU_WINDOW_END       0x0B01U

typedef struct {
    uint8_t  schedule_id;
    uint8_t  active;         /**< 1 = window started, 0 = ended           */
    uint32_t timestamp_ms;
} TOUPayload_t;

#endif /* EVENT_CONFIG_TOU_H */
