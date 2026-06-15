/**
 * @file EventConfigRelay.h
 * @brief Events published by RelayManager
 *
 * @EVENT_MODULE: Relay
 * @BASE_ID: 0x0200
 */

#ifndef EVENT_CONFIG_RELAY_H
#define EVENT_CONFIG_RELAY_H



/* Event ID definitions — consumed by fw_gen.py to build EventList.h */
#define EVT_RELAY_ON                             0x0200U
#define EVT_RELAY_OFF                            0x0201U
#define EVT_RELAY_PULSED                         0x0202U
#define EVT_RELAY_FAULT                          0x0203U
#define EVT_RELAY_INTERLOCK                      0x0204U

typedef struct {
    uint8_t  relay_id;
    uint8_t  state;          /**< 0=OFF 1=ON */
    uint8_t  priority;
    uint16_t source_rule_id;
    uint32_t timestamp_ms;
} RelayPayload_t;

#endif /* EVENT_CONFIG_RELAY_H */
