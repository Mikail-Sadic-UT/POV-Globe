/**
 * @file Motor.c
 * @brief TEC3650 BLDC driver
 *
 * Pin assignments (Port C):
 *   PC4 — WT0CCP0  : hall effect input, falling-edge capture
 *   PC5 — M0PWM7   : PWM speed output, active-low, 20 kHz
 *   PC6 — GPIO out : direction (0 = forward, 1 = reverse)
 *
 * RPM is computed from the inter-pulse period of the hall sensor.
 * 12 pulses per motor revolution; gear ratio 6:1.
 * lastRevPeriod holds the 80 MHz tick count for one full motor revolution,
 * used by the POV column timing in main_sd.c.
 */

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/Motor.h"

/* ─────────────────────────────────────────────────────────────────── */

static volatile uint16_t motorRPM       = 0;
static volatile uint16_t gearRPM        = 0;
static volatile uint8_t  gearPulseCount = 0;
static volatile uint16_t gearAngle      = 0;
static volatile uint32_t lastRevPeriod  = 0;  /* 80 MHz counts per motor rev */

static uint32_t wtPrev     = 0;
static uint32_t wtValid    = 0;
static uint32_t wtDeltaSum = 0;
static uint8_t  wtPulse    = 0;

#define GEAR_PPR 54  /* (supposed to be 72, but this worked fine? 12 motor pulses × 6 gear ratio) */

/* ─────────────────────────────────────────────────────────────────── */

volatile MotorRampState_t motorRampState   = MOTOR_IDLE;
static volatile uint8_t   motorCurSpeed    = 0;
static volatile uint8_t   motorTargetSpeed = 0;
static volatile uint32_t  motorStepDelay   = 0;
static volatile uint32_t  motorLastTick    = 0;
static int8_t             currentSpeed     = 0;

#define MIN_MOTOR_TIMESTEP_MS 10

volatile uint8_t motor_state_changed = 1;

extern volatile uint32_t msTick;

/* ─────────────────────────────────────────────────────────────────── */

void Motor_Init(void) {
    volatile uint32_t d;

    SYSCTL_RCGCPWM_R    |= PWM0_CLK;
    SYSCTL_RCGCWTIMER_R |= WT0_CLK;
    SYSCTL_RCGCGPIO_R   |= PORTC_CLK;
    d = SYSCTL_RCGC2_R; (void)d;

    SYSCTL_RCC_R &= ~0x00100000;        /* PWM clock = sysclk */

    /* PC5 — M0PWM7 */
    GPIO_PORTC_AFSEL_R |=  PC5_PWM;
    GPIO_PORTC_DEN_R   |=  PC5_PWM;
    GPIO_PORTC_PCTL_R   = (GPIO_PORTC_PCTL_R & ~PC5_PCTL_CLR) | PC5_PCTL_SET;

    /* PC6 — direction output, default forward */
    GPIO_PORTC_DIR_R  |=  PC6_DIR;
    GPIO_PORTC_DEN_R  |=  PC6_DIR;
    GPIO_PORTC_DATA_R &= ~PC6_DIR;

    /* PC4 — WT0CCP0, pull-up for open-drain hall */
    GPIO_PORTC_DIR_R   &= ~PC4_HALL;
    GPIO_PORTC_AFSEL_R |=  PC4_HALL;
    GPIO_PORTC_DEN_R   |=  PC4_HALL;
    GPIO_PORTC_PUR_R   |=  PC4_HALL;
    GPIO_PORTC_AMSEL_R &= ~PC4_HALL;
    GPIO_PORTC_PCTL_R   = (GPIO_PORTC_PCTL_R & ~0x000F0000) | 0x00070000;

    /* Wide Timer 0A — edge-time capture, falling edge, 80 MHz */
    WTIMER0_CTL_R  &= ~TIMER_CTL_TAEN;
    WTIMER0_CFG_R   =  0x04;
    WTIMER0_TAMR_R  =  TIMER_TAMR_TACMR | TIMER_TAMR_TAMR_CAP;
    WTIMER0_TAILR_R =  0xFFFFFFFF;
    WTIMER0_TAPR_R  =  0;
    WTIMER0_CTL_R   = (WTIMER0_CTL_R & ~0x0C) | 0x04;  /* falling edge */
    WTIMER0_ICR_R  =  TIMER_ICR_CAECINT;
    WTIMER0_IMR_R |=  TIMER_IMR_CAEIM;

    NVIC_PRI23_R = (NVIC_PRI23_R & ~0x00E00000) | 0x00200000;  /* priority 1 */
    NVIC_EN2_R  |= (1UL << 30);
    WTIMER0_CTL_R |= TIMER_CTL_TAEN;

    /* PWM0 Gen 3 — count-down, active-low on PC5 */
    PWM0_3_CTL_R  = 0;
    PWM0_3_GENB_R = 0x0000080C;
    PWM0_3_LOAD_R = MOTOR_PWM_LOAD - 1;
    PWM0_3_CTL_R  = 0x00000001;
    PWM0_ENABLE_R |= 0x00000080;

    Motor_Stop();
}

/* ─────────────────────────────────────────────────────────────────── */

#define MAX_STEP 25

uint8_t Motor_SetSpeed(int8_t percent) {
    if ((int8_t)(percent - currentSpeed) >  MAX_STEP ||
        (int8_t)(percent - currentSpeed) < -MAX_STEP)
        return 0;

    currentSpeed = percent;

    if (percent <= 0)   { PWM0_3_GENB_R = 0x00000C00; return 1; }  /* off */
    if (percent >= 100) { PWM0_3_GENB_R = 0x00000800; return 1; }  /* full */

    PWM0_3_GENB_R = 0x0000080C;
    uint32_t cmp = (MOTOR_PWM_LOAD * (uint32_t)percent) / 100;
    if (cmp == 0)              cmp = 1;
    if (cmp >= MOTOR_PWM_LOAD) cmp = MOTOR_PWM_LOAD - 1;
    PWM0_3_CMPB_R = cmp;
    return 1;
}

uint8_t Motor_SetDirection(uint8_t dir) {
    if (currentSpeed > 0) return 0;
    PC6_DIR_BIT = (dir == MOTOR_REVERSE) ? PC6_DIR : 0;
    return 1;
}

/* ─────────────────────────────────────────────────────────────────── */

uint32_t Motor_GetRPM(void)  { return motorRPM; }
uint8_t  Motor_Stop(void)    { return Motor_SetSpeed(0); }
uint8_t  Motor_GetDuty(void) { return (uint8_t)currentSpeed; }

/* ─────────────────────────────────────────────────────────────────── */

static uint32_t lastValidDelta = 0xFFFFFFFF;

void WideTimer0A_Handler(void) {
    WTIMER0_ICR_R = TIMER_ICR_CAECINT;
    uint32_t now = WTIMER0_TAR_R;

    if (!wtValid) { wtPrev = now; wtValid = 1; return; }

    uint32_t delta = wtPrev - now;

    // Reject glitch pulses, should have prob put a small series resistor on the line..
    if (lastValidDelta != 0xFFFFFFFF && delta < (lastValidDelta * 6 / 10)) return;

    wtPrev = now;
    lastValidDelta = delta;

    if (delta > STALL_DELTA) {
        motorRPM = 0; wtDeltaSum = 0; wtPulse = 0;
        lastValidDelta = 0xFFFFFFFF;
        return;
    }

    gearPulseCount = (gearPulseCount + 1) % GEAR_PPR;
    gearAngle      = (gearPulseCount * 360) / GEAR_PPR;

    wtDeltaSum += delta;
    wtPulse++;

    if (wtPulse >= MOTOR_HALL_PPR) {
        lastRevPeriod = wtDeltaSum;
        motorRPM      = (uint32_t)(RPM_SCALE_REV / wtDeltaSum);
        gearRPM       = motorRPM / 6;
        wtDeltaSum    = 0;
        wtPulse       = 0;
    }
}

/* ─────────────────────────────────────────────────────────────────── */

void Motor_StallCheck(void) {
    if (!wtValid) return;
    uint32_t since = wtPrev - WTIMER0_TAV_R;
    if (since > STALL_DELTA) {
        motorRPM = 0;  gearRPM    = 0; lastRevPeriod = 0;
        wtValid  = 0;  wtDeltaSum = 0; wtPulse       = 0;
    }
}

/* ─────────────────────────────────────────────────────────────────── */

void Motor_Spinup(uint32_t msDelay, uint8_t setSpeed) {
    if (setSpeed > MAX_SPEED)             setSpeed = MAX_SPEED;
    if (msDelay  < MIN_MOTOR_TIMESTEP_MS) msDelay  = MIN_MOTOR_TIMESTEP_MS;

    uint8_t cur = (uint8_t)Motor_GetDuty();
    if (cur < MIN_STARTSPEED) cur = MIN_STARTSPEED;
    if (cur > setSpeed || motorRampState == MOTOR_SPINNING_DOWN) return;

    motorCurSpeed    = cur;
    motorTargetSpeed = setSpeed;
    motorStepDelay   = msDelay;
    motorLastTick    = msTick;
    motorRampState   = MOTOR_SPINNING_UP;
    motor_state_changed = 1;
    Motor_SetSpeed((int8_t)cur);
}

void Motor_Spindown(uint32_t msDelay) {
    uint8_t cur = (uint8_t)Motor_GetDuty();
    if (cur == 0) return;
    if (msDelay < MIN_MOTOR_TIMESTEP_MS) msDelay = MIN_MOTOR_TIMESTEP_MS;

    motorCurSpeed  = cur;
    motorStepDelay = msDelay;
    motorLastTick  = msTick;
    motorRampState = MOTOR_SPINNING_DOWN;
    motor_state_changed = 1;
}

// Done this way to be non-blocking
void Motor_Tick(void) {
    if (motorRampState == MOTOR_IDLE) return;
    if ((msTick - motorLastTick) < motorStepDelay) return;
    motorLastTick = msTick;

    switch (motorRampState) {
        case MOTOR_SPINNING_UP:
            if (motorCurSpeed < motorTargetSpeed) {
                Motor_SetSpeed((int8_t)(++motorCurSpeed));
            } else {
                Motor_SetSpeed((int8_t)motorTargetSpeed);
                motorRampState = MOTOR_IDLE;
                motor_state_changed = 1;
            }
            break;
        case MOTOR_SPINNING_DOWN:
            if (motorCurSpeed > 5) {
                Motor_SetSpeed((int8_t)(--motorCurSpeed));
            } else {
                motorCurSpeed = 0;
                Motor_Stop();
                motorRampState = MOTOR_IDLE;
                motor_state_changed = 1;
            }
            break;
        default: break;
    }
}

/* ─────────────────────────────────────────────────────────────────── */

uint8_t  Motor_GetGearPulseCount(void) { return gearPulseCount; }
uint16_t Motor_GetGearAngle(void)      { return gearAngle;      }
uint32_t Motor_GetRevPeriod(void)      { return lastRevPeriod;  }
