/**
 * @file re_engine.h
 * @brief Rule Engine core — structures, rule table, and evaluation API
 *
 * Rules are statically defined via code-generated rule_table.c/h.
 * No dynamic allocation. Evaluation is fully deterministic.
 */

#ifndef RE_ENGINE_H
#define RE_ENGINE_H

#include "re_types.h"

/* =========================================================================
 * Sensor descriptor (compile-time static table entry)
 * ========================================================================= */
typedef struct
{
    uint8_t         id;
    char            name[RE_MAX_NAME_LEN];
    SensorType_t    type;
    char            unit[RE_MAX_UNIT_LEN];
    SensorValue_t   min_value;      /**< Scaled x100 */
    SensorValue_t   max_value;      /**< Scaled x100 */
    SensorValue_t   threshold_hi;   /**< Upper alarm threshold */
    SensorValue_t   threshold_lo;   /**< Lower alarm threshold */
    SensorValue_t   hysteresis;     /**< Hysteresis band (x100) */
    uint32_t        sample_interval_ms;
    bool            enabled;
} SensorDescriptor_t;

/* =========================================================================
 * Output descriptor (compile-time static table entry)
 * ========================================================================= */
typedef struct
{
    uint8_t         id;
    char            name[RE_MAX_NAME_LEN];
    OutputType_t    type;
    uint8_t         priority;        /**< 0–255, higher = more important */
    uint8_t         default_state;   /**< 0=OFF, 1=ON */
    uint8_t         failsafe_state;  /**< State on watchdog / power loss */
} OutputDescriptor_t;

/* =========================================================================
 * Condition — one clause of the IF part
 * ========================================================================= */
typedef struct
{
    uint8_t         sensor_id;
    CompareOp_t     op;
    SensorValue_t   value_a;    /**< Primary threshold (or low bound) */
    SensorValue_t   value_b;    /**< High bound for BETWEEN / NOT_BETWEEN */
} Condition_t;

/* =========================================================================
 * Action — one element of the THEN part
 * ========================================================================= */
typedef struct
{
    uint8_t         output_id;
    ActionType_t    action;
    uint32_t        duration_ms;    /**< For PULSE: active duration */
} Action_t;

/* =========================================================================
 * Rule descriptor (static, code-generated)
 * ========================================================================= */
typedef struct
{
    uint16_t        id;
    char            name[RE_MAX_NAME_LEN];
    bool            enabled;

    /* Conditions */
    uint8_t         num_conditions;
    Condition_t     conditions[RE_MAX_CONDITIONS_PER_RULE];
    LogicOp_t       condition_logic;    /**< AND / OR applied across conditions */

    /* Delays */
    uint32_t        on_delay_ms;    /**< Condition must hold for this long */
    uint32_t        off_delay_ms;   /**< OFF delay after condition clears  */

    /* Optional TOU schedule gate */
    uint8_t         schedule_id;    /**< 0 = no schedule gate */

    /* Actions */
    uint8_t         num_actions;
    Action_t        actions[RE_MAX_ACTIONS_PER_RULE];

    /* Priority (inherited from highest-priority output) */
    uint8_t         priority;
} RuleDescriptor_t;

/* =========================================================================
 * Runtime rule state (RAM, zeroed at startup)
 * ========================================================================= */
typedef struct
{
    bool            condition_met;      /**< Current evaluation result     */
    bool            prev_condition_met; /**< Previous cycle result         */
    bool            on_delay_active;    /**< ON delay timer running        */
    bool            off_delay_active;   /**< OFF delay timer running       */
    RE_Timestamp_t  condition_true_since;  /**< When condition became true */
    RE_Timestamp_t  condition_false_since; /**< When condition became false*/
    uint32_t        fire_count;         /**< Total times this rule fired   */
    RE_Timestamp_t  last_fired_ts;      /**< Last time this rule fired     */
    bool            runtime_disabled;   /**< Runtime override of rule->enabled */
} RuleState_t;

/* =========================================================================
 * Schedule descriptor
 * ========================================================================= */
typedef struct
{
    uint8_t         id;
    char            name[RE_MAX_NAME_LEN];
    ScheduleType_t  type;
    uint8_t         num_slots;
    ScheduleSlot_t  slots[RE_MAX_SCHEDULE_SLOTS];
    bool            enabled;
} ScheduleDescriptor_t;

/* =========================================================================
 * Engine context (single global instance, statically allocated)
 * ========================================================================= */
typedef struct
{
    /* Live states — indexed by ID (ID 0 unused, valid IDs: 1–99) */
    SensorState_t   sensor_state[RE_MAX_SENSORS + 1U];
    OutputState_t   output_state[RE_MAX_OUTPUTS + 1U];
    RuleState_t     rule_state[RE_MAX_RULES];

    /* Diagnostics */
    RE_Diagnostics_t diag;

    /* Log ring buffer */
    RE_LogEntry_t   log_buf[RE_MAX_LOG_ENTRIES];
    uint16_t        log_head;
    uint16_t        log_tail;
    uint16_t        log_count;

    bool            initialized;
} RE_Context_t;

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief Initialise the rule engine, HAL, and all subsystems.
 *
 * Call once at startup before the main loop.
 * Sets all outputs to their default_state.
 *
 * @return RE_OK on success
 */
RE_Status_t RE_Init(void);

/**
 * @brief Execute one full evaluation cycle.
 *
 * Sequence:
 *  1. Read all enabled sensors via HAL
 *  2. Check TOU schedules
 *  3. Evaluate all rules (conditions → delays → actions)
 *  4. Apply actions with priority arbitration
 *  5. Drive outputs via HAL
 *  6. Tick pulse timers
 *  7. Update diagnostics
 *
 * Call from main loop or FreeRTOS task.
 *
 * @return RE_OK on success
 */
RE_Status_t RE_Tick(void);

/**
 * @brief Force-read a specific sensor and update internal state.
 */
RE_Status_t RE_ReadSensor(uint8_t sensor_id);

/**
 * @brief Directly set an output (bypasses rule engine, use for manual override).
 */
RE_Status_t RE_ForceOutput(uint8_t output_id, uint8_t state, uint8_t priority);

/**
 * @brief Get current diagnostics snapshot.
 */
RE_Status_t RE_GetDiagnostics(RE_Diagnostics_t *diag_out);

/**
 * @brief Get current sensor state.
 */
RE_Status_t RE_GetSensorState(uint8_t sensor_id, SensorState_t *state_out);

/**
 * @brief Get current output state.
 */
RE_Status_t RE_GetOutputState(uint8_t output_id, OutputState_t *state_out);

/**
 * @brief Reset all diagnostics counters.
 */
void RE_ResetDiagnostics(void);

/**
 * @brief Reset sample timestamps so all sensors are re-read on next tick.
 *
 * Use in test environments where multiple ticks execute within the same
 * wall-clock second and the sample-interval guard would otherwise skip reads.
 */
void RE_InvalidateSensorCache(void);

/**
 * @brief Enable or disable a rule at runtime, overriding the static table.
 * Useful for maintenance modes, commissioning, or remote configuration.
 * @return RE_OK, or RE_ERR_NOT_FOUND if rule_id is not in the table.
 */
RE_Status_t RE_SetRuleEnabled(uint16_t rule_id, bool enabled);

/** @brief Query the effective enable state (static && !runtime_disabled). */
bool RE_IsRuleEnabled(uint16_t rule_id);

/**
 * @brief Print human-readable diagnostics to debug UART / stdout.
 */
void RE_PrintDiagnostics(void);

/**
 * @brief Access global context (for unit tests / diagnostic tools).
 */
RE_Context_t *RE_GetContext(void);

#endif /* RE_ENGINE_H */
