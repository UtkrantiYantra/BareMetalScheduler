/**
 * @file led_mgr.h
 * @brief LED Manager — solid ON/OFF and blink pattern control
 */

#ifndef LED_MGR_H
#define LED_MGR_H

#include <stdint.h>
#include <stdbool.h>
#include "scheduler.h"
#include "event_bus.h"
#include "EventList.h"

typedef enum {
    LED_MODE_OFF       = 0U,
    LED_MODE_ON        = 1U,
    LED_MODE_BLINK     = 2U,   /* on_ms / off_ms pattern, repeat_count times */
    LED_MODE_ONESHOT   = 3U,   /* blink repeat_count times then stop         */
} LEDMode_t;

typedef struct {
    uint8_t       id;
    const char   *name;
    uint32_t      gpio_pin;
    bool          active_high;   /* true = ON when GPIO high                  */
    bool          enabled;
} LEDDesc_t;

typedef struct {
    LEDMode_t state;
    uint16_t  on_ms;
    uint16_t  off_ms;
    uint8_t   repeat_count;
    uint8_t   repeat_remaining;
    bool      phase_on;         /* current blink phase                        */
    uint32_t  phase_end_ms;
} LEDRuntimeState_t;

#define LED_DEF(var, id_, name_, pin_, active_hi_)          \
    static LEDDesc_t var = { (id_), (name_), (pin_), (active_hi_), true }

/* API */
void     LEDMgr_Init(void);
bool     LEDMgr_AddLED(LEDDesc_t *desc, LEDRuntimeState_t *state);
void     LEDMgr_SetSolid(uint8_t led_id, bool on);
void     LEDMgr_SetBlink(uint8_t led_id, uint16_t on_ms, uint16_t off_ms, uint8_t repeat);

/** Change the blink rate of an already-blinking LED without restarting the
 *  pattern. Different rates encode different meanings, e.g.:
 *    slow  (1000/1000) = idle heartbeat
 *    medium (500/500)  = warning
 *    fast  (100/100)   = critical fault                                    */
void     LEDMgr_SetBlinkRate(uint8_t led_id, uint16_t on_ms, uint16_t off_ms);
void     LEDMgr_Off(uint8_t led_id);
LEDMode_t LEDMgr_GetMode(uint8_t led_id);
void     LEDMgr_Task(void *arg);

#endif /* LED_MGR_H */
