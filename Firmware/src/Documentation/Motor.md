# BL2430 Motor Driver

## Overview

Drives a TEC3650 brushless DC motor through a 6:1 reduction gearbox.
Provides three things:

1. **PWM speed control** with non-blocking spin-up / spin-down ramps
2. **Direction control** via a digital pin
3. **Live RPM measurement** from the hall-effect sensor

The motor sits on Port C:

| Pin | Function | Direction | Notes |
|-----|----------|-----------|-------|
| PC4 | WT0CCP0  | input  | Hall sensor, falling-edge capture |
| PC5 | M0PWM7   | output | Active-low PWM, 20 kHz |
| PC6 | GPIO     | output | Direction (0 = forward) |

---

## PWM Speed Control

PWM uses **PWM Module 0, Generator 3, output B** (M0PWM7 on PC5). The signal is **active-low**: the line idles high (motor off) and pulses low for the active fraction of each period.

```
Period = 80 MHz / 20 kHz = 4000 counts
Duty   = CMPB / LOAD
```

Three special cases short-circuit the normal counter:
- **0%** → GENB forced constant high (motor off)
- **100%** → GENB forced constant low (full speed)
- **1–99%** → normal count-down PWM

### Rate Limiter

`Motor_SetSpeed()` rejects single-call jumps larger than `MAX_STEP` (25%). This protects the driver and motor from sudden current spikes. To change speed by more than 25%, use `Motor_Spinup()` / `Motor_Spindown()` which ramp gradually.

---

## Spin-Up / Spin-Down Ramps

Both functions are **non-blocking** — they store the target state and return. `Motor_Tick()` (called from SysTick at 1 ms) advances the ramp by one step every `motorStepDelay` ms.

```
MOTOR_IDLE
    │
    │  Motor_Spinup(250, 67)
    ▼
MOTOR_SPINNING_UP  ──── increment 1%/250ms ──── reaches target ──▶ MOTOR_IDLE
    │
    │  Motor_Spindown(100)  [preempts spinup]
    ▼
MOTOR_SPINNING_DOWN ── decrement 1%/100ms ──── reaches 5% ──▶ Motor_Stop() ──▶ MOTOR_IDLE
```

Spin-down always preempts spin-up. Spin-up is ignored if a spin-down is already running.

The spin-down ramp stops at 5% rather than 0% and calls `Motor_Stop()` directly — at very low duty cycles the motor behaviour is unpredictable, so it's safer to cut PWM cleanly than to creep down to zero.

### State variables (all volatile)

| Variable | Owner | Purpose |
|----------|-------|---------|
| `motorRampState` | Motor_Tick / Spinup / Spindown | Current ramp phase |
| `motorCurSpeed` | Motor_Tick | Most recently applied duty% |
| `motorTargetSpeed` | Motor_Spinup | Spin-up destination |
| `motorStepDelay` | Spinup / Spindown | ms between 1% steps |
| `motorLastTick` | Motor_Tick | msTick when last step ran |
| `motor_state_changed` | all writers | Flag for main loop to redraw LCD |

---

## Hall Sensor RPM Capture

PC4 receives the hall sensor's open-drain output. We use **Wide Timer 0A in capture mode** — every falling edge latches the timer's current value, generating an interrupt.

The TEC3650 produces **12 hall pulses per motor revolution** (4 pole pairs × 3 phases). One full motor revolution = 12 captured edges.

### ISR Flow (`WideTimer0A_Handler`)

```
edge fires
    │
    ▼
read WTIMER0_TAR_R (latched timestamp)
    │
    ▼
delta = wtPrev - now    (timer counts down from 0xFFFFFFFF)
    │
    ▼
glitch reject: delta < 0.6 × lastValidDelta?  ──▶ ignore
    │
    ▼
stall check: delta > STALL_DELTA (1 sec)?     ──▶ motorRPM = 0, reset
    │
    ▼
update gearPulseCount (mod GEAR_PPR=54)
accumulate wtDeltaSum, increment wtPulse
    │
    ▼
wtPulse == 12?                                ──▶ compute motorRPM,
                                                  save lastRevPeriod,
                                                  reset accumulator
```

### RPM Math

```
RPM = (SYS_CLK × 60) / (HALL_PPR × delta_per_pulse)
    = RPM_SCALE_REV / wtDeltaSum

where RPM_SCALE_REV = 80,000,000 × 60 = 4,800,000,000
and wtDeltaSum = sum of 12 inter-pulse intervals (one full rev)
```

Computing once per revolution rather than per pulse averages out timing jitter and avoids divisions on every interrupt.

### Glitch Rejection

The hall sensor is open-drain with a pull-up. Without a series resistor on the line, fast slew rates can produce double-pulses near the threshold. The ISR rejects any delta less than 60% of the last valid interval, which discards these glitches without affecting legitimate speed changes (motor accel/decel happens on a much slower timescale).

### Stall Detection

`Motor_StallCheck()` is called from SysTick every 1 ms. It reads the **live** timer value (`WTIMER0_TAV_R`) — not the latched capture register — to measure time since the last edge. If more than 1 second has passed, it forces RPM to 0 and resets the capture state. This handles the case where the motor stops without producing a final edge.

---

## Hall-Sync API (for POV column timing)

The POV globe needs columns delivered at a rate that matches the physical rotation. Two getters expose the hall capture state to `main_sd.c`:

| Function | Returns |
|----------|---------|
| `Motor_GetRevPeriod()` | 80 MHz tick count for one full motor revolution. 0 = no measurement yet. |
| `Motor_GetGearPulseCount()` | Current position within one gear rev, [0, 54). |
| `Motor_GetGearAngle()` | Same, but in degrees [0, 360). |

In `main_sd.c`, `HallSync_UpdatePeriod()` reads `Motor_GetRevPeriod()` and computes the column timer period:

```
colPeriod = revPeriod × MOTOR_GEAR_RATIO / SD_NUM_COLUMNS
          = revPeriod × 6 / 140
```

This locks the column rate to actual motor speed — the image stays stable even if RPM drifts. (Only factoring Motor drift, any mechanical drifting down the line, ie gear/belt slippage will not be caught)

---


## Public API Summary

```c
void     Motor_Init(void);
uint8_t  Motor_SetSpeed(int8_t percent);     // rate-limited
uint8_t  Motor_SetDirection(uint8_t dir);    // only when stopped
uint8_t  Motor_Stop(void);
void     Motor_Spinup(uint32_t msPerStep, uint8_t targetDuty);
void     Motor_Spindown(uint32_t msPerStep);
void     Motor_Tick(void);                   // call from SysTick
void     Motor_StallCheck(void);             // call from SysTick

uint32_t Motor_GetRPM(void);                 // motor RPM
uint32_t Motor_GetGearRPM(void);             // = motorRPM / 6
uint8_t  Motor_GetDuty(void);                // current duty %

uint32_t Motor_GetRevPeriod(void);           // for hall-sync
uint8_t  Motor_GetGearPulseCount(void);
uint16_t Motor_GetGearAngle(void);
```
