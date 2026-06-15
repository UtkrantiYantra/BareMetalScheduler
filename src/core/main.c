/**
 * @file main.c
 * @brief Linux simulation entry point
 *
 * Demonstrates the full framework:
 *   Scheduler  → AnalogSensorMgr → DigitalSensorMgr → RelayMgr
 *   → LEDMgr → AlarmEventMgr → LoggingMgr → RuleEngine → EventBus
 *
 * On bare-metal / RTOS targets, replace main() with app_main() / HAL_Init()
 * chain and call Scheduler_Run() from the superloop.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>   /* usleep */

/* Core */
#include "re_engine.h"
#include "re_hal.h"

/* Framework */
#include "scheduler.h"
#include "event_bus.h"

/* Modules */
#include "relay_mgr.h"
#include "led_mgr.h"
#include "digital_sensor_mgr.h"
#include "analog_sensor_mgr.h"
#include "alarm_event_mgr.h"
#include "logging_mgr.h"
#include "buzzer_mgr.h"
#include "queue_mgr.h"
#include "state_machine.h"
#include "tou_engine.h"
#include "diag_mgr.h"

/* Generated */
#include "config.h"
#include "EventList.h"

/* =========================================================================
 * Linux tick driver — simulates SysTick
 * ========================================================================= */
static uint32_t s_sim_tick = 0U;
static void sim_advance_ticks(uint32_t ms) {
    uint32_t i;
    for (i = 0U; i < ms; i++) {
        Scheduler_Tick();
        LEDMgr_Task(NULL);           /* blink phase transitions              */
        BuzzerMgr_Task(NULL);        /* beep phase transitions               */
        DigitalSensorMgr_Task(NULL); /* debounce advances every ms           */
    }
    s_sim_tick += ms;
}

/* =========================================================================
 * Analog channel registrations (match sensor IDs in industrial_automation.xml)
 * ========================================================================= */
ANALOG_CHANNEL_DEF(g_ch_boiler, 1, "BoilerTemp",
    ANALOG_SRC_INJECT, 0,
    1, 1, 0,   /* scale 1:1 (inject already scaled) */
    1500, 5000, 100U);  /* lo=15°C, hi=50°C matching rule engine threshold */

ANALOG_CHANNEL_DEF(g_ch_room, 2, "RoomTemp",
    ANALOG_SRC_INJECT, 0,
    1, 1, 0,
    1800, 3500, 200U);

ANALOG_CHANNEL_DEF(g_ch_motor, 3, "MotorWindingTemp",
    ANALOG_SRC_INJECT, 0,
    1, 1, 0,
    0, 7000, 200U);

static AnalogChannelState_t s_state_boiler, s_state_room, s_state_motor;

/* =========================================================================
 * Digital sensor registrations
 * ========================================================================= */
DIGITAL_SENSOR_DEF(g_dig_fire1,  60, "FireDetector_Z1", 100U, 20U, true);
DIGITAL_SENSOR_DEF(g_dig_estop,  92, "EStop",            0U,  20U, true);

static DigitalSensorState_t s_dstate_fire1, s_dstate_estop;

/* =========================================================================
 * Relay registrations (IDs match rule engine output IDs)
 * ========================================================================= */
RELAY_DEF(g_relay_pump,   1, "WaterPump",    10U, 0U, 0U, 0U, 1000U);
RELAY_DEF(g_relay_alarm,  2, "GeneralAlarm", 11U, 0U, 0U, 0U, 1000U);
RELAY_DEF(g_relay_fire,   3, "FireAlarm",    12U, 0U, 0U, 0U, 500U);
RELAY_DEF(g_relay_hooter, 4, "Hooter",       13U, 0U, 0U, 0U, 1000U);
RELAY_DEF(g_relay_buzzer, 5, "Buzzer",       14U, 0U, 0U, 0U, 1000U);

static RelayRuntimeState_t s_rstate_pump, s_rstate_alarm, s_rstate_fire,
                            s_rstate_hooter, s_rstate_buzzer;

/* =========================================================================
 * LED registrations
 * ========================================================================= */
LED_DEF(g_led_red,   6,  "RedLED",   20U, true);
LED_DEF(g_led_green, 8,  "GreenLED", 21U, true);

static LEDRuntimeState_t s_led_red_state, s_led_green_state;

/* =========================================================================
 * Buzzer registrations
 * ========================================================================= */
BUZZER_DEF(g_buzzer_main, 1, "MainBuzzer", 30U, true);
static BuzzerRuntimeState_t s_buzzer_main_state;

/* =========================================================================
 * Queue registrations
 * ========================================================================= */
typedef struct { uint8_t sensor_id; int32_t value; } SensorMsg_t;
QUEUE_MGR_DEF(g_q_sensor, 1, "sensor_readings", sizeof(SensorMsg_t), 8, 75);

/* =========================================================================
 * Motor state machine (HSM): OPERATIONAL{IDLE,RUNNING} + FAULT
 * ========================================================================= */
enum { ST_OPERATIONAL = 0, ST_IDLE, ST_RUNNING, ST_FAULT };
enum { SMEV_START = 1, SMEV_STOP, SMEV_FAULT_DET, SMEV_RESET };

static void sm_on_run_entry(void *ctx)   { (void)ctx; printf("  [SM] >> entered RUNNING\n"); }
static void sm_on_run_exit(void *ctx)    { (void)ctx; printf("  [SM] << exited RUNNING\n"); }
static void sm_on_fault_entry(void *ctx) { (void)ctx; printf("  [SM] !! entered FAULT\n"); }

static const SM_State_t k_motor_states[] = {
    SM_STATE(ST_OPERATIONAL, "operational", SM_NO_PARENT,   NULL, NULL),
    SM_STATE(ST_IDLE,        "idle",        ST_OPERATIONAL, NULL, NULL),
    SM_STATE(ST_RUNNING,     "running",     ST_OPERATIONAL, sm_on_run_entry, sm_on_run_exit),
    SM_STATE(ST_FAULT,       "fault",       SM_NO_PARENT,   sm_on_fault_entry, NULL),
};
static const SM_Transition_t k_motor_trans[] = {
    SM_TRANSITION(ST_IDLE,        SMEV_START,     ST_RUNNING, NULL, NULL),
    SM_TRANSITION(ST_RUNNING,     SMEV_STOP,      ST_IDLE,    NULL, NULL),
    /* HSM: fault transition on the PARENT fires from any child */
    SM_TRANSITION(ST_OPERATIONAL, SMEV_FAULT_DET, ST_FAULT,   NULL, NULL),
    SM_TRANSITION(ST_FAULT,       SMEV_RESET,     ST_IDLE,    NULL, NULL),
};
SM_MACHINE_DEF(g_motor_sm, 1, "motor", k_motor_states, k_motor_trans, ST_IDLE, NULL);

/* =========================================================================
 * TOU schedule: night window 22:00-06:00, every day
 * ========================================================================= */
TOU_SCHEDULE_DEF(g_tou_night, 1, "NightTariff", 22U, 0U, 6U, 0U, 0x7FU);
static TOURuntimeState_t s_tou_night_state;

/* =========================================================================
 * Alarm registrations
 * ========================================================================= */
ALARM_DEF(g_alm_boiler, 1, "BoilerOverTemp",
    EVT_ANALOG_THRESHOLD_HIGH, 1, ALARM_SEV_HIGH, 30000U, true);

ALARM_DEF(g_alm_fire, 2, "FireAlarm",
    EVT_DIGITAL_RISING_EDGE, 60, ALARM_SEV_CRITICAL, 10000U, true);

static AlarmRuntimeState_t s_alm_boiler_state, s_alm_fire_state;

/* =========================================================================
 * Log handlers
 * ========================================================================= */
LOG_HANDLER_DEF(g_uart_log,  LOG_HANDLER_UART,  LOG_SEV_INFO, LOG_TYPE_ALL);
LOG_HANDLER_DEF(g_alarm_log, LOG_HANDLER_UART,  LOG_SEV_HIGH, LOG_TYPE_ALARM | LOG_TYPE_EVENT);

/* =========================================================================
 * Scheduler task descriptors
 * period = 0 → every loop; set explicit ms for real modules
 * ========================================================================= */
SCHED_TASK_DEF(g_task_rule,    "rule_engine",    10U,  1U);
SCHED_TASK_DEF(g_task_event,   "event_dispatch",  1U,  0U);
SCHED_TASK_DEF(g_task_analog,  "analog_sensor",  50U,  2U);
SCHED_TASK_DEF(g_task_digital, "digital_sensor", 10U,  2U);
SCHED_TASK_DEF(g_task_relay,   "relay",          10U,  3U);
SCHED_TASK_DEF(g_task_led,     "led",            10U,  3U);
SCHED_TASK_DEF(g_task_alarm,   "alarm",         100U,  4U);
SCHED_TASK_DEF(g_task_log,     "logger",         50U,  5U);

/* =========================================================================
 * Rule engine task wrapper
 * ========================================================================= */
static void rule_engine_task(void *arg) {
    (void)arg;
    RE_HAL_SimForceResample();
    (void)RE_Tick();
}

static void event_dispatch_task(void *arg) {
    (void)arg;
    EventBus_Dispatch(16U);
}

/* =========================================================================
 * Event logger subscriber — log all events to logging module
 * ========================================================================= */
static void on_any_event(EventID_t id, const void *payload,
                         uint8_t len, void *arg) {
    (void)arg;
    LOG_EVENT(LOG_SEV_INFO, id, payload, len);
}

/* =========================================================================
 * Print helpers
 * ========================================================================= */
static void print_separator(const char *title) {
    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  %s\n", title);
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

/* =========================================================================
 * Typed reader callbacks for demo
 * ========================================================================= */
static bool print_sensor_entry(const LogEntry_t *e, void *arg) {
    (void)arg;
    printf("  [%8lu] SENSOR id=%u  val=%.2f\n",
           (unsigned long)e->timestamp_ms,
           e->u.sensor.sensor_id,
           (double)e->u.sensor.value_x100 / 100.0);
    return true;
}

static bool print_alarm_entry(const LogEntry_t *e, void *arg) {
    (void)arg;
    printf("  [%8lu] ALARM  id=%u  sev=%u  val=%.2f\n",
           (unsigned long)e->timestamp_ms,
           e->u.alarm.alarm_id,
           e->u.alarm.severity,
           (double)e->u.alarm.trigger_val / 100.0);
    return true;
}

/* =========================================================================
 * Framework init
 * ========================================================================= */
static void framework_init(void)
{
    /* Scheduler */
    Scheduler_Init();

    /* Event bus */
    EventBus_Init();

    /* Logging module */
    LogMgr_Init();
    LogMgr_AddHandler(&g_uart_log);
    LogMgr_AddHandler(&g_alarm_log);

    /* Rule engine */
    RE_Init();

    /* Analog sensors */
    AnalogSensorMgr_Init();
    AnalogSensorMgr_AddChannel(&g_ch_boiler, &s_state_boiler);
    AnalogSensorMgr_AddChannel(&g_ch_room,   &s_state_room);
    AnalogSensorMgr_AddChannel(&g_ch_motor,  &s_state_motor);

    /* Digital sensors */
    DigitalSensorMgr_Init();
    DigitalSensorMgr_AddSensor(&g_dig_fire1, &s_dstate_fire1);
    DigitalSensorMgr_AddSensor(&g_dig_estop, &s_dstate_estop);

    /* Relays */
    RelayMgr_Init();
    RelayMgr_AddRelay(&g_relay_pump,   &s_rstate_pump);
    RelayMgr_AddRelay(&g_relay_alarm,  &s_rstate_alarm);
    RelayMgr_AddRelay(&g_relay_fire,   &s_rstate_fire);
    RelayMgr_AddRelay(&g_relay_hooter, &s_rstate_hooter);
    RelayMgr_AddRelay(&g_relay_buzzer, &s_rstate_buzzer);

    /* LEDs */
    LEDMgr_Init();
    LEDMgr_AddLED(&g_led_red,   &s_led_red_state);
    LEDMgr_AddLED(&g_led_green, &s_led_green_state);

    /* Buzzers */
    BuzzerMgr_Init();
    BuzzerMgr_AddBuzzer(&g_buzzer_main, &s_buzzer_main_state);

    /* Queue manager */
    QueueMgr_Init();
    QueueMgr_Register(&g_q_sensor);

    /* TOU engine */
    TOU_Init();
    TOU_AddSchedule(&g_tou_night, &s_tou_night_state);

    /* Diagnostics aggregator */
    Diag_Init();

    /* Motor state machine */
    SM_Init(&g_motor_sm);

    /* Alarms */
    AlarmEventMgr_Init();
    AlarmEventMgr_AddAlarm(&g_alm_boiler, &s_alm_boiler_state);
    AlarmEventMgr_AddAlarm(&g_alm_fire,   &s_alm_fire_state);

    /* Subscribe a catch-all logger to all events for the log module */
    EventBus_Subscribe(EVT_ANALOG_THRESHOLD_HIGH, on_any_event, NULL);
    EventBus_Subscribe(EVT_ALARM_RAISED,           on_any_event, NULL);
    EventBus_Subscribe(EVT_RELAY_ON,               on_any_event, NULL);
    EventBus_Subscribe(EVT_RELAY_OFF,              on_any_event, NULL);
    EventBus_Subscribe(EVT_DIGITAL_RISING_EDGE,    on_any_event, NULL);

    /* Register scheduler tasks */
    Scheduler_AddTask(&g_task_event,   event_dispatch_task,        NULL);
    Scheduler_AddTask(&g_task_rule,    rule_engine_task,            NULL);
    Scheduler_AddTask(&g_task_analog,  AnalogSensorMgr_Task,        NULL);
    Scheduler_AddTask(&g_task_digital, DigitalSensorMgr_Task,       NULL);
    Scheduler_AddTask(&g_task_relay,   RelayMgr_Task,               NULL);
    Scheduler_AddTask(&g_task_led,     LEDMgr_Task,                 NULL);
    Scheduler_AddTask(&g_task_alarm,   AlarmEventMgr_Task,          NULL);
    Scheduler_AddTask(&g_task_log,     LogMgr_Task,                 NULL);
}

/* =========================================================================
 * Simulation run loop (replaces Scheduler_Run() for deterministic testing)
 * ========================================================================= */
static void run_tasks_once(void) {
    /* 1. Read all sensors (they inject into rule engine + publish events) */
    AnalogSensorMgr_Task(NULL);
    DigitalSensorMgr_Task(NULL);
    EventBus_Dispatch(32U);          /* dispatch sensor events → alarm subs */

    /* 2. Evaluate rules + actuator tasks */
    rule_engine_task(NULL);
    RelayMgr_Task(NULL);
    LEDMgr_Task(NULL);

    /* 3. Business logic */
    AlarmEventMgr_Task(NULL);
    EventBus_Dispatch(32U);          /* dispatch alarm events → log subs */

    /* 4. Logging (deferred flash/SD) */
    LogMgr_Task(NULL);
    EventBus_Dispatch(32U);          /* final drain */
}

/* =========================================================================
 * main
 * ========================================================================= */
int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Rule Engine + Scheduler + Module Framework  ║\n");
    printf("║  Linux Simulation                            ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    framework_init();
    LOG_TEXT(LOG_SEV_INFO, "MAIN", "Framework initialised. %u event IDs registered.",
             (unsigned)EVT__COUNT);

    /* ── Scenario 1: Normal baseline ────────────────────────────────────── */
    print_separator("Scenario 1: Normal operating conditions");
    AnalogSensorMgr_Inject(1, 2500);   /* Boiler 25.00°C */
    AnalogSensorMgr_Inject(2, 2200);   /* Room   22.00°C */
    AnalogSensorMgr_Inject(3, 3000);   /* Motor  30.00°C */
    sim_advance_ticks(200U);
    run_tasks_once();
    LOG_TEXT(LOG_SEV_INFO, "SIM", "%s", "Baseline stable. No alarms expected.");

    /* ── Scenario 2: Boiler over-temperature ─────────────────────────────── */
    print_separator("Scenario 2: Boiler over-temperature (55°C > threshold 50°C)");
    AnalogSensorMgr_Inject(1, 5500);   /* Boiler 55.00°C */
    sim_advance_ticks(150U);
    run_tasks_once();
    LOG_SENSOR(LOG_SEV_WARN, 1, 5500, ANALOG_SRC_INJECT);
    printf("  → Alarm state: %s\n",
           AlarmEventMgr_GetState(1) == ALARM_STATE_ACTIVE ? "ACTIVE" : "inactive");

    /* ── Scenario 3: Boiler in hysteresis band ───────────────────────────── */
    print_separator("Scenario 3: Boiler drops to 48°C (hysteresis: stays alarmed until 45°C)");
    AnalogSensorMgr_Inject(1, 4800);
    sim_advance_ticks(150U);
    run_tasks_once();
    printf("  → Alarm state: %s (expected: ACTIVE)\n",
           AlarmEventMgr_GetState(1) == ALARM_STATE_ACTIVE ? "ACTIVE" : "inactive");

    /* ── Scenario 4: Fire detector ON → full multi-output response ─────────── */
    print_separator("Scenario 4: Fire detector Zone-1 → full emergency response");
    DigitalSensorMgr_Inject(60, 1);    /* Fire sensor ON */
    sim_advance_ticks(50U);            /* advance past 20ms debounce */
    /* Run digital task manually in the advance loop already handles debounce */
    DigitalSensorMgr_Task(NULL);       /* settle debounce state */
    EventBus_Dispatch(32U);            /* fire DIGITAL_RISING_EDGE → alarm sub */
    AlarmEventMgr_Task(NULL);          /* process any escalation */
    EventBus_Dispatch(32U);
    printf("  → Fire alarm state: %s\n",
           AlarmEventMgr_GetState(2) == ALARM_STATE_ACTIVE ? "ACTIVE" : "inactive");

    /* ── Scenario 5: LED blink pattern ──────────────────────────────────── */
    print_separator("Scenario 5: LED blink pattern (RedLED, 500ms ON / 200ms OFF, 3 times)");
    LEDMgr_SetBlink(6, 500U, 200U, 3U);
    sim_advance_ticks(2500U);
    run_tasks_once();
    printf("  → RedLED mode after sequence: %s\n",
           LEDMgr_GetMode(6) == LED_MODE_OFF ? "OFF (sequence complete)" : "still running");

    /* ── Scenario 6: Acknowledge boiler alarm ────────────────────────────── */
    print_separator("Scenario 6: Operator acknowledges boiler alarm");
    AlarmEventMgr_Acknowledge(1);
    printf("  → Alarm state after ACK: %s\n",
           AlarmEventMgr_GetState(1) == ALARM_STATE_ACKNOWLEDGED ? "ACKNOWLEDGED" : "other");

    /* ── Scenario 7: Buzzer beep patterns ─────────────────────────────────── */
    print_separator("Scenario 7: Buzzer — fast beep x3 (critical pattern)");
    BuzzerMgr_SetBeep(1, 100U, 100U, 3U);
    sim_advance_ticks(900U);
    printf("  → Buzzer mode after pattern: %s\n",
           BuzzerMgr_GetMode(1) == BUZZER_MODE_OFF ? "OFF (complete)" : "still running");

    /* ── Scenario 8: Relay refresh + logical/actual status ──────────────────── */
    print_separator("Scenario 8: Relay logical vs actual state");
    RelayMgr_SetState(2, RELAY_STATE_ON, 100U, 0U, 0U);
    {
        RelayState_t logical; uint8_t actual;
        RelayMgr_GetStatus(2, &logical, &actual);
        printf("  → Relay 2: logical=%s actual=%s\n",
               logical == RELAY_STATE_ON ? "ON" : "OFF",
               actual == 0xFFU ? "n/a (no feedback pin)" : (actual ? "ON" : "OFF"));
    }
    sim_advance_ticks(600U);
    RelayMgr_Task(NULL);   /* periodic refresh re-asserts GPIO */
    RelayMgr_SetState(2, RELAY_STATE_OFF, 100U, 0U, 0U);

    /* ── Scenario 9: Hierarchical state machine ─────────────────────────────── */
    print_separator("Scenario 9: Motor HSM — IDLE -> RUNNING -> FAULT (via parent) -> RESET");
    printf("  Initial state : %s\n", SM_GetStateName(&g_motor_sm));
    SM_Dispatch(&g_motor_sm, SMEV_START);
    printf("  After START   : %s\n", SM_GetStateName(&g_motor_sm));
    /* FAULT_DET is defined on parent OPERATIONAL — bubbles up from RUNNING */
    SM_Dispatch(&g_motor_sm, SMEV_FAULT_DET);
    printf("  After FAULT   : %s (HSM parent transition fired from child)\n",
           SM_GetStateName(&g_motor_sm));
    SM_Dispatch(&g_motor_sm, SMEV_RESET);
    printf("  After RESET   : %s\n", SM_GetStateName(&g_motor_sm));

    /* ── Scenario 10: QueueManager ──────────────────────────────────────────── */
    print_separator("Scenario 10: QueueManager — send/recv + stats");
    {
        SensorMsg_t tx = { 1U, 5500 }, rx;
        QueueMgr_Send(1U, &tx);
        tx.sensor_id = 2U; tx.value = 2200;
        QueueMgr_Send(1U, &tx);
        printf("  Queue count after 2 sends: %u\n", QueueMgr_Count(1U));
        QueueMgr_Recv(1U, &rx);
        printf("  Received: sensor=%u value=%ld\n", rx.sensor_id, (long)rx.value);
        QueueMgr_PrintStats();
    }

    /* ── Scenario 11: TOU window check ──────────────────────────────────────── */
    print_separator("Scenario 11: TOU engine — NightTariff window (22:00-06:00)");
    TOU_Task(NULL);
    printf("  NightTariff active now: %s\n", TOU_IsActive(1U) ? "YES" : "NO");

    /* ── Scenario 12: Diagnostics aggregation ───────────────────────────────── */
    print_separator("Scenario 12: Diagnostics subsystem");
    { DiagSnapshot_t snap; Diag_Collect(&snap); }
    Diag_Print();

    /* ── Typed log reader demo ───────────────────────────────────────────── */
    print_separator("Log Reader: Sensor entries");
    uint32_t n = LogMgr_ReadSensor(1U, print_sensor_entry, NULL);
    printf("  Total sensor log entries for sensor 1: %lu\n", (unsigned long)n);

    print_separator("Log Reader: Alarm entries");
    n = LogMgr_ReadAlarm(0U, print_alarm_entry, NULL);
    printf("  Total alarm log entries: %lu\n", (unsigned long)n);

    print_separator("Log Reader: All log entries");
    printf("  Total entries in log ring: %lu\n", (unsigned long)LogMgr_Count());

    /* ── Scheduler diagnostics ──────────────────────────────────────────── */
    print_separator("Scheduler diagnostics");
    Sched_Diag_t sdiag;
    Scheduler_GetDiag(&sdiag);
    printf("  Tick count      : %lu\n", (unsigned long)sdiag.tick_count);
    printf("  Task dispatches : %lu\n", (unsigned long)sdiag.task_dispatches);
    printf("  Total overruns  : %lu\n", (unsigned long)sdiag.total_overruns);

    /* ── Event bus summary ──────────────────────────────────────────────── */
    printf("\n  Event bus pending: %u\n", EventBus_Pending());

    /* ── Rule engine diagnostics ─────────────────────────────────────────── */
    print_separator("Rule engine diagnostics");
    RE_PrintDiagnostics();

    printf("\n[SIMULATION COMPLETE]\n\n");
    return 0;
}
