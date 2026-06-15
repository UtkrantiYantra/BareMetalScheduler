/**
 * @file EventConfigLED.h
 * @brief Events published by LEDManager
 *
 * @EVENT_MODULE: LED
 * @BASE_ID: 0x0300
 */

#ifndef EVENT_CONFIG_LED_H
#define EVENT_CONFIG_LED_H



/* Event ID definitions — consumed by fw_gen.py to build EventList.h */
#define EVT_LED_ON                               0x0300U
#define EVT_LED_OFF                              0x0301U
#define EVT_LED_BLINK_START                      0x0302U
#define EVT_LED_BLINK_STOP                       0x0303U
#define EVT_LED_PATTERN_DONE                     0x0304U

typedef struct {
    uint8_t  led_id;
    uint8_t  state;          /**< 0=OFF 1=ON */
    uint16_t blink_on_ms;    /**< ON time for blink pattern (0 = solid)  */
    uint16_t blink_off_ms;   /**< OFF time for blink pattern             */
    uint8_t  repeat_count;   /**< 0 = continuous                         */
    uint32_t timestamp_ms;
} LEDPayload_t;

#endif /* EVENT_CONFIG_LED_H */
