/**
 * @file EventList.h
 * @brief AUTO-GENERATED — do not edit by hand.
 *
 * Generated: 2026-06-12 20:06:40
 * Source:     EventConfig*.h files in src/event/configs/
 *
 * This file is the single source of truth for all event IDs.
 * Include it (directly or via event_bus.h) to use EVT_* constants.
 */

#ifndef EVENT_LIST_H
#define EVENT_LIST_H

#include <stdint.h>

/* Event ID type */
typedef uint16_t EventID_t;

/* Sentinel */
#define EVT_INVALID  0x0000U

/* ── AnalogSensor events ── */
#define EVT_ANALOG_READING_READY                 0x0100U
#define EVT_ANALOG_THRESHOLD_HIGH                0x0101U
#define EVT_ANALOG_THRESHOLD_LOW                 0x0102U
#define EVT_ANALOG_SENSOR_FAULT                  0x0103U
#define EVT_ANALOG_SENSOR_OK                     0x0104U
#define EVT_ANALOG_CALIBRATED                    0x0105U

/* ── Relay events ── */
#define EVT_RELAY_ON                             0x0200U
#define EVT_RELAY_OFF                            0x0201U
#define EVT_RELAY_PULSED                         0x0202U
#define EVT_RELAY_FAULT                          0x0203U
#define EVT_RELAY_INTERLOCK                      0x0204U

/* ── LED events ── */
#define EVT_LED_ON                               0x0300U
#define EVT_LED_OFF                              0x0301U
#define EVT_LED_BLINK_START                      0x0302U
#define EVT_LED_BLINK_STOP                       0x0303U
#define EVT_LED_PATTERN_DONE                     0x0304U

/* ── Alarm events ── */
#define EVT_ALARM_RAISED                         0x0400U
#define EVT_ALARM_CLEARED                        0x0401U
#define EVT_ALARM_ACKNOWLEDGED                   0x0402U
#define EVT_ALARM_ESCALATED                      0x0403U
#define EVT_ALARM_INHIBITED                      0x0404U
#define EVT_ALARM_SHELVED                        0x0405U
#define EVT_ALARM_LOG_OVERFLOW                   0x0406U

/* ── DigitalSensor events ── */
#define EVT_DIGITAL_RISING_EDGE                  0x0500U
#define EVT_DIGITAL_FALLING_EDGE                 0x0501U
#define EVT_DIGITAL_STATE_ON                     0x0502U
#define EVT_DIGITAL_STATE_OFF                    0x0503U
#define EVT_DIGITAL_DEBOUNCE_DONE                0x0504U
#define EVT_DIGITAL_FAULT                        0x0505U

/* ── Logging events ── */
#define EVT_LOG_ENTRY_WRITTEN                    0x0600U
#define EVT_LOG_BUFFER_FULL                      0x0601U
#define EVT_LOG_HANDLER_ERROR                    0x0602U
#define EVT_LOG_FLUSH_DONE                       0x0603U

/* ── Buzzer events ── */
#define EVT_BUZZER_ON                            0x0700U
#define EVT_BUZZER_OFF                           0x0701U
#define EVT_BUZZER_BEEP_START                    0x0702U
#define EVT_BUZZER_BEEP_STOP                     0x0703U
#define EVT_BUZZER_PATTERN_DONE                  0x0704U

/* ── Queue events ── */
#define EVT_QUEUE_OVERFLOW                       0x0800U
#define EVT_QUEUE_HIGH_WATER                     0x0801U
#define EVT_QUEUE_EMPTY                          0x0802U

/* ── StateMachine events ── */
#define EVT_SM_TRANSITION                        0x0900U
#define EVT_SM_GUARD_REJECTED                    0x0901U
#define EVT_SM_UNHANDLED_EVENT                   0x0902U

/* ── Diagnostics events ── */
#define EVT_DIAG_SNAPSHOT                        0x0A00U
#define EVT_DIAG_HEALTH_WARNING                  0x0A01U

/* ── TOU events ── */
#define EVT_TOU_WINDOW_START                     0x0B00U
#define EVT_TOU_WINDOW_END                       0x0B01U

#define EVT__COUNT  48U  /* total registered events */

#endif /* EVENT_LIST_H */
