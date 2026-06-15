/**
 * @file re_engine.c
 * @brief Rule Engine core implementation
 *
 * Deterministic, no dynamic allocation.
 * All tables are externally supplied by generated code (rule_table.c).
 */

#include "re_engine.h"
#include "re_hal.h"
#include "rule_table.h"       /* Generated: sensor/output/rule/schedule tables */
#include "re_scheduler.h"
#include "re_logger.h"

#include <string.h>
#include <stdio.h>

/* =========================================================================
 * Static context — single global instance
 * ========================================================================= */
static RE_Context_t s_ctx;

/* =========================================================================
 * Forward declarations (private helpers)
 * ========================================================================= */
static RE_Status_t  engine_read_all_sensors(void);
static RE_Status_t  engine_evaluate_rules(void);
static bool         engine_eval_condition(const Condition_t *cond);
static bool         engine_eval_rule_conditions(const RuleDescriptor_t *rule);
static bool         engine_check_schedule(uint8_t schedule_id);
static RE_Status_t  engine_apply_action(const RuleDescriptor_t *rule,
                                         const Action_t *action,
                                         uint16_t rule_idx);
static RE_Status_t  engine_tick_pulses(void);
static RE_Status_t  engine_apply_output(uint8_t output_id,
                                         uint8_t state,
                                         uint8_t priority,
                                         uint16_t rule_id);
static void         engine_log(uint16_t rule_id, uint8_t sensor_id,
                                SensorValue_t val, uint8_t output_id,
                                ActionType_t act);

/* =========================================================================
 * RE_Init
 * ========================================================================= */
RE_Status_t RE_Init(void)
{
    RE_Status_t ret;
    uint16_t    i;

    /* Zero the entire context */
    (void)memset(&s_ctx, 0, sizeof(s_ctx));

    /* Initialise HAL */
    ret = HAL_RE_Init();
    if (ret != RE_OK)
    {
        return ret;
    }

    /* Set all outputs to their default (and drive hardware) */
    for (i = 0U; i < RE_NUM_OUTPUTS; i++)
    {
        const OutputDescriptor_t *od = &g_output_table[i];
        s_ctx.output_state[od->id].phys_state =
            (od->default_state != 0U) ? OUTPUT_STATE_ON : OUTPUT_STATE_OFF;
        (void)HAL_WriteOutput(od->id, od->default_state);
    }

    s_ctx.initialized = true;
    RE_LOG_INFO("Rule Engine initialised. Sensors=%u Outputs=%u Rules=%u",
                RE_NUM_SENSORS, RE_NUM_OUTPUTS, RE_NUM_RULES);

    return RE_OK;
}

/* =========================================================================
 * RE_Tick — main evaluation cycle
 * ========================================================================= */
RE_Status_t RE_Tick(void)
{
    RE_Status_t ret;
    uint32_t    t_start;

    if (!s_ctx.initialized)
    {
        return RE_ERR_DISABLED;
    }

    t_start = HAL_GetMicros();

    HAL_KickWatchdog();

    /* 1. Read sensors */
    ret = engine_read_all_sensors();

    /* 2. Snapshot alarm_active into prev_alarm_active, then clear for this tick.
     *    engine_eval_condition reads prev_alarm_active for hysteresis gating and
     *    OR-latches alarm_active; this way the flag persists exactly one tick. */
    {
        uint8_t si;
        for (si = 1U; si <= RE_MAX_SENSORS; si++)
        {
            s_ctx.sensor_state[si].prev_alarm_active = s_ctx.sensor_state[si].alarm_active;
            s_ctx.sensor_state[si].alarm_active      = false;
        }
    }

    /* 3. Evaluate rules (non-fatal if individual sensors fail) */
    (void)engine_evaluate_rules();

    /* 4. Tick pulse timers */
    (void)engine_tick_pulses();

    /* 5. Update diagnostics */
    s_ctx.diag.loop_count++;
    s_ctx.diag.last_loop_duration_us = HAL_GetMicros() - t_start;

    return ret;
}

/* =========================================================================
 * engine_read_all_sensors
 * ========================================================================= */
static RE_Status_t engine_read_all_sensors(void)
{
    uint16_t        i;
    RE_Status_t     status;
    SensorValue_t   val;
    RE_Timestamp_t  now = HAL_GetTimestamp();

    for (i = 0U; i < RE_NUM_SENSORS; i++)
    {
        const SensorDescriptor_t *sd = &g_sensor_table[i];
        SensorState_t            *ss = &s_ctx.sensor_state[sd->id];

        if (!sd->enabled)
        {
            continue;
        }

        /* Respect sampling interval */
        if ((now - ss->last_sampled_ts) < (sd->sample_interval_ms / 1000U))
        {
            continue;
        }

        /* RTC/TOU virtual sensors do not need HAL read */
        if ((sd->type == SENSOR_TYPE_RTC_TIME) ||
            (sd->type == SENSOR_TYPE_TOU_SCHEDULE) ||
            (sd->type == SENSOR_TYPE_VIRTUAL))
        {
            ss->valid = true;
            continue;
        }

        status = HAL_ReadSensor(sd->id, &val);

        if (status == RE_OK)
        {
            ss->prev_value      = ss->value;
            ss->value           = val;
            ss->valid           = true;
            ss->last_sampled_ts = now;
            ss->sample_count++;
            ss->error_count     = 0U;
            s_ctx.diag.sensor_read_ok++;
        }
        else
        {
            ss->valid = false;
            ss->error_count++;
            s_ctx.diag.sensor_read_err++;
        }
    }

    return RE_OK;
}

/* =========================================================================
 * engine_eval_condition — evaluate a single condition clause
 * ========================================================================= */
static bool engine_eval_condition(const Condition_t *cond)
{
    const SensorState_t *ss;
    SensorValue_t        v;
    bool                 result = false;

    if ((cond->sensor_id == 0U) || (cond->sensor_id > RE_MAX_SENSORS))
    {
        return false;
    }

    ss = &s_ctx.sensor_state[cond->sensor_id];

    if (!ss->valid)
    {
        return false;   /* Stale / unread data → condition not met */
    }

    v = ss->value;

    switch (cond->op)
    {
        case OP_EQ:          result = (v == cond->value_a);                   break;
        case OP_NE:          result = (v != cond->value_a);                   break;
        case OP_GT:          result = (v >  cond->value_a);                   break;
        case OP_GTE:         result = (v >= cond->value_a);                   break;
        case OP_LT:          result = (v <  cond->value_a);                   break;
        case OP_LTE:         result = (v <= cond->value_a);                   break;
        case OP_BETWEEN:     result = ((v >= cond->value_a) &&
                                       (v <= cond->value_b));                 break;
        case OP_NOT_BETWEEN: result = ((v <  cond->value_a) ||
                                       (v >  cond->value_b));                 break;
        case OP_ON:          result = (v != 0);                               break;
        case OP_OFF:         result = (v == 0);                               break;
        default:             result = false;                                  break;
    }

    /* Apply hysteresis: once alarm is active, use hysteresis clearance threshold.
     * Overrides primary comparison to prevent chattering on deactivation.
     *   GT/GTE trigger: stays ON until value < (threshold - hysteresis)
     *   LT/LTE trigger: stays ON until value > (threshold + hysteresis)  */
    {
        const SensorDescriptor_t *sd = NULL;
        uint16_t i;
        for (i = 0U; i < RE_NUM_SENSORS; i++)
        {
            if (g_sensor_table[i].id == cond->sensor_id)
            {
                sd = &g_sensor_table[i];
                break;
            }
        }

        if ((sd != NULL) && (sd->hysteresis > 0) && ss->prev_alarm_active)
        {
            /* Already in alarm state: keep ON until hysteresis clear point */
            if ((cond->op == OP_GT) || (cond->op == OP_GTE))
            {
                result = (v > (cond->value_a - sd->hysteresis));
            }
            else if ((cond->op == OP_LT) || (cond->op == OP_LTE))
            {
                result = (v < (cond->value_a + sd->hysteresis));
            }
            /* ON/OFF and BETWEEN operators: no hysteresis adjustment */
        }
    }

    /* Latch alarm_active for this sensor: set on true, cleared per-tick in RE_Tick */
    if (result)
    {
        s_ctx.sensor_state[cond->sensor_id].alarm_active = true;
    }

    return result;
}

/* =========================================================================
 * engine_eval_rule_conditions
 * ========================================================================= */
static bool engine_eval_rule_conditions(const RuleDescriptor_t *rule)
{
    uint8_t i;
    bool    result;
    bool    c;

    if (rule->num_conditions == 0U)
    {
        return false;
    }

    /* Evaluate first condition */
    result = engine_eval_condition(&rule->conditions[0]);

    /* Combine remaining conditions using the rule's logic operator */
    for (i = 1U; i < rule->num_conditions; i++)
    {
        c = engine_eval_condition(&rule->conditions[i]);

        switch (rule->condition_logic)
        {
            case LOGIC_AND: result = result && c;  break;
            case LOGIC_OR:  result = result || c;  break;
            case LOGIC_XOR: result = result != c;  break;
            case LOGIC_NOT: result = !result;       break;  /* Unary: applied to first cond result */
            default:        result = result && c;  break;
        }
    }

    /* LOGIC_NOT on single condition */
    if ((rule->num_conditions == 1U) && (rule->condition_logic == LOGIC_NOT))
    {
        result = !engine_eval_condition(&rule->conditions[0]);
    }

    return result;
}

/* =========================================================================
 * engine_check_schedule
 * ========================================================================= */
static bool engine_check_schedule(uint8_t schedule_id)
{
    RE_RTC_t rtc;
    uint16_t i;
    uint16_t s;

    if (schedule_id == 0U)
    {
        return true;    /* No schedule gate — always allowed */
    }

    if (HAL_GetRTC(&rtc) != RE_OK)
    {
        return true;    /* RTC failure → allow rule to proceed */
    }

    for (i = 0U; i < RE_NUM_SCHEDULES; i++)
    {
        const ScheduleDescriptor_t *sd = &g_schedule_table[i];

        if ((sd->id != schedule_id) || !sd->enabled)
        {
            continue;
        }

        for (s = 0U; s < sd->num_slots; s++)
        {
            const ScheduleSlot_t *slot = &sd->slots[s];
            uint16_t now_mins   = (uint16_t)(((uint16_t)rtc.hour * 60U) + (uint16_t)rtc.minute);
            uint16_t start_mins = (uint16_t)(((uint16_t)slot->start_hour * 60U) + (uint16_t)slot->start_min);
            uint16_t end_mins   = (uint16_t)(((uint16_t)slot->end_hour * 60U) + (uint16_t)slot->end_min);

            /* Weekly schedule: check weekday mask */
            if (sd->type == SCHED_WEEKLY)
            {
                if (((slot->weekday_mask >> rtc.weekday) & 1U) == 0U)
                {
                    continue;
                }
            }

            /* Weekend schedule */
            if (sd->type == SCHED_WEEKEND)
            {
                if ((rtc.weekday != 0U) && (rtc.weekday != 6U))
                {
                    continue;
                }
            }

            /* Handle overnight spans (e.g. 22:00 to 06:00) */
            if (end_mins > start_mins)
            {
                if ((now_mins >= start_mins) && (now_mins < end_mins))
                {
                    return true;
                }
            }
            else
            {
                /* Wraps midnight */
                if ((now_mins >= start_mins) || (now_mins < end_mins))
                {
                    return true;
                }
            }
        }

        return false;   /* Schedule found but no active slot */
    }

    return true;    /* Schedule ID not found → allow */
}

/* =========================================================================
 * engine_apply_output — priority arbitration and HAL drive
 * ========================================================================= */
static RE_Status_t engine_apply_output(uint8_t output_id,
                                        uint8_t state,
                                        uint8_t priority,
                                        uint16_t rule_id)
{
    OutputState_t *os;
    RE_Status_t    ret;

    if ((output_id == 0U) || (output_id > RE_MAX_OUTPUTS))
    {
        return RE_ERR_INVALID_ID;
    }

    os = &s_ctx.output_state[output_id];

    /* Priority check: lower priority cannot override higher */
    if (state == 1U)
    {
        if (priority < os->active_priority)
        {
            /* Higher-priority rule already active — skip */
            return RE_OK;
        }
    }
    else
    {
        /* Turning OFF: allow if same owner, OR if priority >= active_priority
         * (enables higher/equal-priority rules and RE_ForceOutput to override) */
        if ((os->owner_rule_id != rule_id) && (priority < os->active_priority))
        {
            return RE_OK;
        }
    }

    /* Check if state already matches */
    if (((state == 1U) && (os->phys_state == OUTPUT_STATE_ON)) ||
        ((state == 0U) && (os->phys_state == OUTPUT_STATE_OFF)))
    {
        return RE_OK;   /* No change needed */
    }

    HAL_EnterCritical();

    os->phys_state      = (state != 0U) ? OUTPUT_STATE_ON : OUTPUT_STATE_OFF;
    os->active_priority = priority;
    os->owner_rule_id   = rule_id;
    os->last_change_ts  = HAL_GetTimestamp();

    if (state != 0U)
    {
        os->on_count++;
    }

    HAL_ExitCritical();

    ret = HAL_WriteOutput(output_id, state);

    if (ret == RE_OK)
    {
        s_ctx.diag.output_write_ok++;
    }
    else
    {
        os->phys_state = OUTPUT_STATE_FAULT;
        s_ctx.diag.output_write_err++;
    }

    return ret;
}

/* =========================================================================
 * engine_apply_action
 * ========================================================================= */
static RE_Status_t engine_apply_action(const RuleDescriptor_t *rule,
                                        const Action_t *action,
                                        uint16_t rule_idx)
{
    OutputState_t  *os;
    RE_Status_t     ret = RE_OK;
    RE_Timestamp_t  now = HAL_GetTimestamp();

    if ((action->output_id == 0U) || (action->output_id > RE_MAX_OUTPUTS))
    {
        return RE_ERR_INVALID_ID;
    }

    os = &s_ctx.output_state[action->output_id];

    switch (action->action)
    {
        case ACTION_ON:
            ret = engine_apply_output(action->output_id, 1U, rule->priority, rule->id);
            break;

        case ACTION_OFF:
            ret = engine_apply_output(action->output_id, 0U, rule->priority, rule->id);
            break;

        case ACTION_TOGGLE:
        {
            uint8_t new_state = (os->phys_state == OUTPUT_STATE_ON) ? (uint8_t)0U : (uint8_t)1U;
            ret = engine_apply_output(action->output_id, new_state, rule->priority, rule->id);
            os->toggle_count++;
            break;
        }

        case ACTION_PULSE:
            /* Turn ON and arm the pulse timer */
            ret = engine_apply_output(action->output_id, 1U, rule->priority, rule->id);
            if (ret == RE_OK)
            {
                HAL_EnterCritical();
                os->phys_state   = OUTPUT_STATE_PULSING;
                os->pulse_end_ts = now + (action->duration_ms / 1000U);
                HAL_ExitCritical();
            }
            break;

        default:
            ret = RE_ERR_INVALID_PARAM;
            break;
    }

    if (ret == RE_OK)
    {
        engine_log(rule->id, rule->conditions[0].sensor_id,
                   s_ctx.sensor_state[rule->conditions[0].sensor_id].value,
                   action->output_id, action->action);
    }

    (void)rule_idx;
    return ret;
}

/* =========================================================================
 * engine_evaluate_rules — main rule loop
 * ========================================================================= */
static RE_Status_t engine_evaluate_rules(void)
{
    uint16_t        i;
    uint8_t         j;
    bool            cond_met;
    bool            fire;
    RE_Timestamp_t  now = HAL_GetTimestamp();

    for (i = 0U; i < RE_NUM_RULES; i++)
    {
        const RuleDescriptor_t *rule = &g_rule_table[i];
        RuleState_t            *rs   = &s_ctx.rule_state[i];

        if (!rule->enabled || rs->runtime_disabled)
        {
            continue;
        }

        s_ctx.diag.rule_eval_count++;

        /* Check schedule gate */
        if (!engine_check_schedule(rule->schedule_id))
        {
            /* Schedule inactive: turn off all rule outputs */
            for (j = 0U; j < rule->num_actions; j++)
            {
                if ((rule->actions[j].action == ACTION_ON) ||
                    (rule->actions[j].action == ACTION_PULSE))
                {
                    (void)engine_apply_output(rule->actions[j].output_id,
                                               0U, rule->priority, rule->id);
                }
            }
            continue;
        }

        /* Evaluate conditions */
        cond_met = engine_eval_rule_conditions(rule);

        /* ---- ON Delay handling ---- */
        if (cond_met && !rs->prev_condition_met)
        {
            /* Rising edge: start ON delay timer */
            rs->condition_true_since = now;
            rs->on_delay_active      = (rule->on_delay_ms > 0U);
        }

        if (rs->on_delay_active)
        {
            uint32_t elapsed_ms = (now - rs->condition_true_since) * 1000U;
            if (elapsed_ms >= rule->on_delay_ms)
            {
                rs->on_delay_active = false;
            }
            else
            {
                /* Still waiting on delay */
                rs->condition_met      = false;
                rs->prev_condition_met = cond_met;
                continue;
            }
        }

        /* ---- OFF Delay handling ---- */
        if (!cond_met && rs->prev_condition_met)
        {
            rs->condition_false_since = now;
            rs->off_delay_active      = (rule->off_delay_ms > 0U);
        }

        if (rs->off_delay_active)
        {
            uint32_t elapsed_ms = (now - rs->condition_false_since) * 1000U;
            if (elapsed_ms >= rule->off_delay_ms)
            {
                rs->off_delay_active = false;
            }
            else
            {
                /* Maintain previous state during OFF delay */
                cond_met = true;
            }
        }

        /* ---- Determine if rule should fire ---- */
        fire = (cond_met && !rs->condition_met);   /* Rising edge */

        rs->condition_met      = cond_met;
        rs->prev_condition_met = cond_met;

        if (fire)
        {
            /* Execute all actions */
            for (j = 0U; j < rule->num_actions; j++)
            {
                (void)engine_apply_action(rule, &rule->actions[j], i);
            }
            rs->fire_count++;
            rs->last_fired_ts           = now;
            s_ctx.diag.rule_fire_count++;
            s_ctx.diag.last_rule_fired  = rule->id;
            s_ctx.diag.last_rule_time   = now;
        }
        else if (!cond_met && rs->fire_count > 0U)
        {
            /* Condition cleared: execute OFF actions (turn OFF for ON actions) */
            for (j = 0U; j < rule->num_actions; j++)
            {
                const Action_t *a = &rule->actions[j];
                if (a->action == ACTION_ON)
                {
                    (void)engine_apply_output(a->output_id, 0U,
                                               rule->priority, rule->id);
                }
            }
        }
    }

    return RE_OK;
}

/* =========================================================================
 * engine_tick_pulses — expire pulse timers
 * ========================================================================= */
static RE_Status_t engine_tick_pulses(void)
{
    uint16_t       i;
    RE_Timestamp_t now = HAL_GetTimestamp();

    for (i = 1U; i <= RE_MAX_OUTPUTS; i++)
    {
        OutputState_t *os = &s_ctx.output_state[i];

        if (os->phys_state == OUTPUT_STATE_PULSING)
        {
            if (now >= os->pulse_end_ts)
            {
                os->phys_state      = OUTPUT_STATE_OFF;
                os->active_priority = 0U;
                (void)HAL_WriteOutput((uint8_t)i, 0U);
            }
        }
    }

    return RE_OK;
}

/* =========================================================================
 * engine_log — write log entry to ring buffer and HAL
 * ========================================================================= */
static void engine_log(uint16_t rule_id, uint8_t sensor_id,
                        SensorValue_t val, uint8_t output_id,
                        ActionType_t act)
{
    RE_LogEntry_t entry;

    entry.timestamp   = HAL_GetTimestamp();
    entry.rule_id     = rule_id;
    entry.sensor_id   = sensor_id;
    entry.sensor_value = val;
    entry.output_id   = output_id;
    entry.action      = act;

    /* RAM ring buffer */
    HAL_EnterCritical();
    s_ctx.log_buf[s_ctx.log_head] = entry;
    s_ctx.log_head = (uint16_t)((s_ctx.log_head + 1U) % RE_MAX_LOG_ENTRIES);
    if (s_ctx.log_count < RE_MAX_LOG_ENTRIES)
    {
        s_ctx.log_count++;
    }
    else
    {
        s_ctx.log_tail = (uint16_t)((s_ctx.log_tail + 1U) % RE_MAX_LOG_ENTRIES);
    }
    HAL_ExitCritical();

    /* Persist to flash/SD */
    (void)HAL_LogEvent(&entry);
}

/* =========================================================================
 * Public API implementations
 * ========================================================================= */

RE_Status_t RE_ReadSensor(uint8_t sensor_id)
{
    SensorValue_t val;
    RE_Status_t   ret;

    if ((sensor_id == 0U) || (sensor_id > RE_MAX_SENSORS))
    {
        return RE_ERR_INVALID_ID;
    }

    ret = HAL_ReadSensor(sensor_id, &val);
    if (ret == RE_OK)
    {
        s_ctx.sensor_state[sensor_id].prev_value = s_ctx.sensor_state[sensor_id].value;
        s_ctx.sensor_state[sensor_id].value      = val;
        s_ctx.sensor_state[sensor_id].valid      = true;
        s_ctx.sensor_state[sensor_id].last_sampled_ts = HAL_GetTimestamp();
    }

    return ret;
}

RE_Status_t RE_ForceOutput(uint8_t output_id, uint8_t state, uint8_t priority)
{
    return engine_apply_output(output_id, state, priority, 0xFFFFU);
}

RE_Status_t RE_GetDiagnostics(RE_Diagnostics_t *diag_out)
{
    if (diag_out == NULL)
    {
        return RE_ERR_NULL_PTR;
    }
    HAL_EnterCritical();
    *diag_out = s_ctx.diag;
    HAL_ExitCritical();
    return RE_OK;
}

RE_Status_t RE_GetSensorState(uint8_t sensor_id, SensorState_t *state_out)
{
    if ((sensor_id == 0U) || (sensor_id > RE_MAX_SENSORS) || (state_out == NULL))
    {
        return RE_ERR_INVALID_ID;
    }
    HAL_EnterCritical();
    *state_out = s_ctx.sensor_state[sensor_id];
    HAL_ExitCritical();
    return RE_OK;
}

RE_Status_t RE_GetOutputState(uint8_t output_id, OutputState_t *state_out)
{
    if ((output_id == 0U) || (output_id > RE_MAX_OUTPUTS) || (state_out == NULL))
    {
        return RE_ERR_INVALID_ID;
    }
    HAL_EnterCritical();
    *state_out = s_ctx.output_state[output_id];
    HAL_ExitCritical();
    return RE_OK;
}

void RE_ResetDiagnostics(void)
{
    HAL_EnterCritical();
    (void)memset(&s_ctx.diag, 0, sizeof(s_ctx.diag));
    HAL_ExitCritical();
}

RE_Status_t RE_SetRuleEnabled(uint16_t rule_id, bool enabled)
{
    uint16_t i;
    for (i = 0U; i < RE_NUM_RULES; i++)
    {
        if (g_rule_table[i].id == rule_id)
        {
            s_ctx.rule_state[i].runtime_disabled = !enabled;
            return RE_OK;
        }
    }
    return RE_ERR_NOT_FOUND;
}

bool RE_IsRuleEnabled(uint16_t rule_id)
{
    uint16_t i;
    for (i = 0U; i < RE_NUM_RULES; i++)
    {
        if (g_rule_table[i].id == rule_id)
        {
            return g_rule_table[i].enabled &&
                   !s_ctx.rule_state[i].runtime_disabled;
        }
    }
    return false;
}

void RE_InvalidateSensorCache(void)
{
    uint8_t i;
    HAL_EnterCritical();
    for (i = 1U; i <= RE_MAX_SENSORS; i++)
    {
        s_ctx.sensor_state[i].last_sampled_ts = 0U;
    }
    HAL_ExitCritical();
}

void RE_PrintDiagnostics(void)
{
    RE_Diagnostics_t d;
    (void)RE_GetDiagnostics(&d);
    printf("=== Rule Engine Diagnostics ===\n");
    printf("  Loop count          : %lu\n", (unsigned long)d.loop_count);
    printf("  Last loop (us)      : %lu\n", (unsigned long)d.last_loop_duration_us);
    printf("  Rules evaluated     : %lu\n", (unsigned long)d.rule_eval_count);
    printf("  Rules fired         : %lu\n", (unsigned long)d.rule_fire_count);
    printf("  Sensor reads OK     : %lu\n", (unsigned long)d.sensor_read_ok);
    printf("  Sensor read errors  : %lu\n", (unsigned long)d.sensor_read_err);
    printf("  Output writes OK    : %lu\n", (unsigned long)d.output_write_ok);
    printf("  Output write errors : %lu\n", (unsigned long)d.output_write_err);
    printf("  Last rule fired     : %u\n",  d.last_rule_fired);
    printf("  Last rule time      : %lu\n", (unsigned long)d.last_rule_time);
    printf("===============================\n");
}

RE_Context_t *RE_GetContext(void)
{
    return &s_ctx;
}
