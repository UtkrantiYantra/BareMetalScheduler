# Requirement Document — AnalogSensorManager

**Module ID:** MOD-ANASEN · **Source:** `src/modules/analog_sensor/` · **Event base:** `0x0100`

## 1. Purpose
Acquire analog channels from any of six data sources, normalise through a common
scaling pipeline, run threshold supervision, and feed values to the rule engine.

## 2. Functional Requirements
| ID | Requirement |
|----|-------------|
| REQ-ANA-01 | Sources per channel: `ADC`, `I2C`, `SPI`, `RS485` (Modbus register), `UART` (frame field), `INJECT` (software/test). |
| REQ-ANA-02 | Registration via `ANALOG_CHANNEL_DEF(id, name, source, hw_addr, scale_num, scale_den, offset, lo_thresh, hi_thresh, sample_ms)`. |
| REQ-ANA-03 | Scaling: `value_x100 = raw × scale_num / scale_den + offset_x100`; INJECT values bypass scaling (already ×100). |
| REQ-ANA-04 | Per-channel sampling period honoured against the scheduler tick. |
| REQ-ANA-05 | Threshold crossings (rising into band) shall publish `EVT_ANALOG_THRESHOLD_HIGH/LOW` exactly once per excursion. |
| REQ-ANA-06 | Read failures shall publish `EVT_ANALOG_SENSOR_FAULT`; recovery shall publish `EVT_ANALOG_SENSOR_OK`. |
| REQ-ANA-07 | Every accepted reading shall be written into the rule engine sensor table and published as `EVT_ANALOG_READING_READY`. |
| REQ-ANA-08 | Platform reads are weak symbols (`platform_adc_read`, `platform_i2c_read`, `platform_spi_read`, `platform_rs485_read`, `platform_uart_read`) overridden per target. |

## 3. Events Published
`EVT_ANALOG_READING_READY` (0x0100) · `EVT_ANALOG_THRESHOLD_HIGH` (0x0101) · `EVT_ANALOG_THRESHOLD_LOW` (0x0102) · `EVT_ANALOG_SENSOR_FAULT` (0x0103) · `EVT_ANALOG_SENSOR_OK` (0x0104) · `EVT_ANALOG_CALIBRATED` (0x0105)

Payloads: `AnalogReadingPayload_t`, `AnalogFaultPayload_t`

## 4. Events Consumed
None.

## 5. Dependencies
`scheduler.h`, `event_bus.h`, `re_hal.h`.
