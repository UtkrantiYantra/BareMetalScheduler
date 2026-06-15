/**
 * @file relay_mgr.h
 * @brief Relay Manager
 *
 * Manages discrete relay outputs.  Each relay has:
 *   - Interlock table: a list of relays that must be OFF before this one can turn ON
 *   - Feedback input: optional digital input that confirms physical state
 *   - Pulse support: turn ON for N ms then auto-OFF
 *   - Priority: matches rule engine priority (higher value = higher priority)
 *
 * Registration macro:
 *   RELAY_DEF(var, id, name, gpio_pin, fb_pin, interlock_mask, prio)
 *
 * All state changes publish RelayPayload_t events.
 */

#ifndef RELAY_MGR_H
#define RELAY_MGR_H

#include <stdint.h>
#include <stdbool.h>
#include "scheduler.h"
#include "event_bus.h"
#include "EventList.h"

typedef enum {
    RELAY_STATE_OFF      = 0U,
    RELAY_STATE_ON       = 1U,
    RELAY_STATE_PULSING  = 2U,
    RELAY_STATE_FAULT    = 3U,
} RelayState_t;

typedef struct {
    uint8_t       id;
    const char   *name;
    uint32_t      gpio_pin;        /**< Output GPIO pin (platform-specific)     */
    uint32_t      feedback_pin;    /**< Feedback input GPIO (0 = none)           */
    uint32_t      interlock_mask;  /**< Bitmask of relay IDs that must be OFF    */
    uint8_t       min_priority;    /**< Minimum command priority accepted        */
    uint32_t      refresh_ms;      /**< Re-assert GPIO every N ms (0 = off).
                                        Recovers stuck/glitched relay drivers.  */
    bool          enabled;
} RelayDesc_t;

typedef struct {
    RelayState_t  state;           /**< LOGICAL state — what firmware commanded */
    uint8_t       actual_state;    /**< ACTUAL state — feedback pin read
                                        (0=OFF, 1=ON, 0xFF=no feedback pin)     */
    uint8_t       active_priority;
    uint16_t      owner_rule_id;
    uint32_t      pulse_end_ms;    /**< Tick at which pulse expires             */
    uint32_t      last_refresh_ms; /**< Last GPIO re-assert tick                */
    uint32_t      on_count;
    uint32_t      fault_count;     /**< Logical/actual mismatch count           */
} RelayRuntimeState_t;

#define RELAY_DEF(var, id_, name_, gpio_, fb_, ilock_, prio_, refresh_ms_) \
    static RelayDesc_t var = {                                                \
        (id_), (name_), (gpio_), (fb_), (ilock_), (prio_),                   \
        (refresh_ms_), true                                                   \
    }

/* API */
void  RelayMgr_Init(void);
bool  RelayMgr_AddRelay(RelayDesc_t *desc, RelayRuntimeState_t *state);
bool  RelayMgr_SetState(uint8_t relay_id, RelayState_t state, uint8_t priority,
                         uint16_t source_rule, uint32_t pulse_ms);
RelayState_t RelayMgr_GetState(uint8_t relay_id);

/**
 * @brief Combined logical + actual status.
 * @param logical_out  Commanded state (RelayState_t).
 * @param actual_out   Feedback-pin state: 0=OFF, 1=ON, 0xFF=no feedback pin.
 * @return true if relay_id found.
 * A logical/actual mismatch indicates a stuck or failed relay — the task
 * publishes EVT_RELAY_FAULT when this is detected.
 */
bool  RelayMgr_GetStatus(uint8_t relay_id, RelayState_t *logical_out,
                          uint8_t *actual_out);

void  RelayMgr_Task(void *arg);    /* register with scheduler:
                                       pulse expiry + periodic refresh +
                                       feedback verification */

#endif /* RELAY_MGR_H */
