/**
 * @file buzzer_mgr.h
 * @brief Buzzer Manager — ON / OFF / BEEP pattern control
 *
 * Mirrors LEDManager semantics for audible indicators:
 *   BUZZER_MODE_OFF     — silent
 *   BUZZER_MODE_ON      — continuous tone
 *   BUZZER_MODE_BEEP    — on_ms/off_ms pattern, continuous
 *   BUZZER_MODE_ONESHOT — beep pattern N times then auto-OFF
 *
 * Different beep rates encode different meanings (e.g. slow beep = warning,
 * fast beep = critical, double-beep = acknowledge).
 */

#ifndef BUZZER_MGR_H
#define BUZZER_MGR_H

#include <stdint.h>
#include <stdbool.h>
#include "scheduler.h"
#include "event_bus.h"
#include "EventList.h"

typedef enum {
    BUZZER_MODE_OFF     = 0U,
    BUZZER_MODE_ON      = 1U,
    BUZZER_MODE_BEEP    = 2U,   /* continuous pattern                       */
    BUZZER_MODE_ONESHOT = 3U,   /* pattern repeat_count times then stop     */
} BuzzerMode_t;

typedef struct {
    uint8_t       id;
    const char   *name;
    uint32_t      gpio_pin;
    bool          active_high;
    bool          enabled;
} BuzzerDesc_t;

typedef struct {
    BuzzerMode_t mode;
    uint16_t     on_ms;
    uint16_t     off_ms;
    uint8_t      repeat_count;
    uint8_t      repeat_remaining;
    bool         phase_on;
    uint32_t     phase_end_ms;
} BuzzerRuntimeState_t;

#define BUZZER_DEF(var, id_, name_, pin_, active_hi_)        \
    static BuzzerDesc_t var = { (id_), (name_), (pin_), (active_hi_), true }

/* API */
void         BuzzerMgr_Init(void);
bool         BuzzerMgr_AddBuzzer(BuzzerDesc_t *desc, BuzzerRuntimeState_t *state);
void         BuzzerMgr_SetSolid(uint8_t buzzer_id, bool on);
void         BuzzerMgr_SetBeep(uint8_t buzzer_id, uint16_t on_ms, uint16_t off_ms,
                                uint8_t repeat /* 0 = continuous */);
void         BuzzerMgr_Off(uint8_t buzzer_id);
BuzzerMode_t BuzzerMgr_GetMode(uint8_t buzzer_id);
void         BuzzerMgr_Task(void *arg);

#endif /* BUZZER_MGR_H */
