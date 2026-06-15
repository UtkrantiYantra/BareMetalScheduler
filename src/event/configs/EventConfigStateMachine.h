/**
 * @file EventConfigStateMachine.h
 * @brief Events published by StateMachineModule
 *
 * @EVENT_MODULE: StateMachine
 * @BASE_ID: 0x0900
 */

#ifndef EVENT_CONFIG_STATE_MACHINE_H
#define EVENT_CONFIG_STATE_MACHINE_H

#include <stdint.h>

/* Event ID definitions — consumed by fw_gen.py to build EventList.h */
#define EVT_SM_TRANSITION        0x0900U
#define EVT_SM_GUARD_REJECTED    0x0901U
#define EVT_SM_UNHANDLED_EVENT   0x0902U

typedef struct {
    uint8_t  machine_id;
    uint16_t from_state;
    uint16_t to_state;
    uint16_t trigger_event;
    uint32_t timestamp_ms;
} SMTransitionPayload_t;

#endif /* EVENT_CONFIG_STATE_MACHINE_H */
