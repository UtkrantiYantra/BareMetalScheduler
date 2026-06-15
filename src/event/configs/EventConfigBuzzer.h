/**
 * @file EventConfigBuzzer.h
 * @brief Events published by BuzzerManager
 *
 * @EVENT_MODULE: Buzzer
 * @BASE_ID: 0x0700
 */

#ifndef EVENT_CONFIG_BUZZER_H
#define EVENT_CONFIG_BUZZER_H

#include <stdint.h>

/* Event ID definitions — consumed by fw_gen.py to build EventList.h */
#define EVT_BUZZER_ON            0x0700U
#define EVT_BUZZER_OFF           0x0701U
#define EVT_BUZZER_BEEP_START    0x0702U
#define EVT_BUZZER_BEEP_STOP     0x0703U
#define EVT_BUZZER_PATTERN_DONE  0x0704U

typedef struct {
    uint8_t  buzzer_id;
    uint8_t  state;          /**< 0=OFF 1=ON */
    uint16_t beep_on_ms;     /**< ON time per beep (0 = solid)            */
    uint16_t beep_off_ms;    /**< OFF time between beeps                  */
    uint8_t  repeat_count;   /**< 0 = continuous                          */
    uint32_t timestamp_ms;
} BuzzerPayload_t;

#endif /* EVENT_CONFIG_BUZZER_H */
