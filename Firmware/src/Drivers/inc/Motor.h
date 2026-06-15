/**
 * @file Motor.h
 * @brief TEC3650 brushless motor driver — PWM speed, direction, hall RPM
 *
 * Pin assignments (Port C):
 *   PC5 — M0PWM7  : PWM speed output, active-low, 20kHz
 *   PC4 — GPIO in : Hall effect feedback, falling-edge interrupt
 *   PC6 — GPIO out: Direction control (0 = forward, 1 = reverse)
 *
 * Active-low PWM.
 */

#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

/* ─────────────────────────────────────────────────────────────────── */

typedef enum {
    MOTOR_IDLE = 0,
    MOTOR_SPINNING_UP,
    MOTOR_SPINNING_DOWN
} MotorRampState_t;

#define MOTOR_SPINDOWN_MS 100   // 100ms/1%
#define MOTOR_SPINUP_MS   250   // 250ms/1%
#define MOTOR_SETPOINT    67    // duty%
#define MIN_STARTSPEED    20    // to overcome friction
#define MAX_SPEED         90    // Safeguard

#define MOTOR_PWM_HZ        20000U
#define MOTOR_PWM_LOAD      (80000000U / MOTOR_PWM_HZ)

/** TEC3650: 4 pole pairs = 12 hall pulses/rev */
#define MOTOR_HALL_PPR      12

/* ── RPM formula constants ──────────────────────────────────────────────────
 * RPM_SCALE  = (SYS_CLK * 60) / MOTOR_HALL_PPR = (80e6 * 60) / 6
 * STALL_DELTA = 1 second at 80 MHz
 * --------------------------------------------------------------------------- */
#define RPM_SCALE     800000000UL
#define RPM_SCALE_REV 4800000000ULL
#define STALL_DELTA   8000000UL

/* ─────────────────────────────────────────────────────────────────── */

#define PORTC_CLK           0x04
#define PWM0_CLK            0x01
#define WT0_CLK             0x01

#define PC4_HALL            0x10    ///< PC4 — hall input
#define PC5_PWM             0x20    ///< PC5 — PWM output
#define PC6_DIR             0x40    ///< PC6 — direction output
#define PC6_DIR_BIT     (*((volatile uint32_t *)(0x40006000 + (PC6_DIR << 2))))

#define PC5_PCTL_SET        0x00400000  ///< PC5 PMC = 4 (M0PWM7)
#define PC5_PCTL_CLR        0x00F00000

/* NVIC — GPIO Port C = IRQ 4 */
#define PORTC_IRQ_EN        (1 << 2)
#define PORTC_IRQ_PRI_MASK  (7U << 21)
#define PORTC_IRQ_PRI2      (2 << 21)

#define MOTOR_FORWARD   0
#define MOTOR_REVERSE   1

/* ─────────────────────────────────────────────────────────────────── */

/**
 * @brief Initialise PWM, GPIO direction, and hall interrupt.
 *
 * PC5 -> M0PWM7 at 20kHz, output held high (motor off).
 * PC6 -> output low (forward).
 * PC4 -> input with pull-up, falling-edge interrupt.
 */
void Motor_Init(void);

/**
 * @brief Set motor speed as a duty cycle percentage.
 *
 * <= 0   -> output permanently high (motor off)
 * >= 100 -> output permanently low  (full speed)
 * 1–99   -> active-low PWM, pulse width scales with percent
 *
 * @param percent  Target speed 0–100.
 * @return         1 on success, 0 ielse.
 */
uint8_t Motor_SetSpeed(int8_t percent);

/**
 * @brief Set motor direction via PC6.
 *
 * Rejected if motor is not fully stopped (currentSpeed > 0)
 *
 * @param dir   MOTOR_FORWARD or MOTOR_REVERSE.
 * @return      1 on success, 0 else
 */
uint8_t Motor_SetDirection(uint8_t dir);

/** @brief Stop motor. */
uint8_t Motor_Stop(void);

/**
 * @brief Return most recently computed RPM.
 *
 * Updated in GPIOPortC_Handler every MOTOR_HALL_PPR pulses (one revolution).
 * Returns last known value.
 */
uint32_t Motor_GetRPM(void);

/** @brief Returns set Duty cycle % */
uint8_t Motor_GetDuty(void);

/** @brief Spins down motor gracefully */
void Motor_Spindown(uint32_t msDelay);

/** @brief RSpins up motor to set point */
void Motor_Spinup(uint32_t msDelay, uint8_t setSpeed);

/** @brief ISR tick */
void Motor_Tick(void);

/** @brief checks for stall */
void Motor_StallCheck(void);

/** @brief Returns gear rpm (globe rpm) */
uint32_t Motor_GetGearRPM(void);

/* ---------------------------------------------------------------------------
 * Hall-sync: locks col timing to hall pulses
 *
 * Hall ISR tracks gear position (0 to GEAR_PPR-1).
 * Motor_GetGearPulseCount() returns the current position within one
 * full gear revolution. Motor_GetRevPeriod() returns the most recent
 * full motor-revolution period in 80 MHz timer counts (sum of
 * MOTOR_HALL_PPR inter-pulse intervals). Returns 0 if no complete
 * revolution has been measured yet.
 * --------------------------------------------------------------------------- */

/** @brief Current gear pulse position [0, GEAR_PPR). */
uint8_t  Motor_GetGearPulseCount(void);

/** @brief Gear angle in degrees [0, 360). */
uint16_t Motor_GetGearAngle(void);

/** @brief Last measured motor revolution period in 80 MHz counts. 0 = unknown. */
uint32_t Motor_GetRevPeriod(void);

/** @brief Gear ratio — motor revs per gear rev */
#define MOTOR_GEAR_RATIO  6

#endif /* MOTOR_H */
