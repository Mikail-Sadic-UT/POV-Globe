/**
 * @file Motor_test.c
 * @brief BL2430 Motor test — spins up to full speed then ramps back down.
 */

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/PLL.h"
#include "../Drivers/inc/GPIO.h"
#include "../Drivers/inc/Motor.h"
#include "main.h"

void EnableInterrupts(void);
void DisableInterrupts(void);

static void INITIALIZE(void){
    PLL_Init(Bus80MHz);
    Motor_Init();
}

int main10(void){
    DisableInterrupts();
    INITIALIZE();
    LED_Init();
    EnableInterrupts();

    #define SPEED_STEP_TICKS   100000   /* ~125ms per 1% step */
    #define SPEED_HOLD_TICKS  1600000   /* ~2s hold at 0% or 100% */

    int8_t   speed      = 0;
    int8_t   ramp       = 1;            /* 1 = ramping up, -1 = ramping down */
    uint8_t  motorDir   = MOTOR_FORWARD;
    uint32_t lastStep   = 0;
    uint32_t holdStart  = 0;
    uint8_t  holding    = 1;

    Motor_SetDirection(MOTOR_FORWARD);
    Motor_SetSpeed(0);
    holdStart = delay;

    while(1){
        if(!(delay % 1000000)) LED_HBT();

        uint32_t now = delay;

        if(holding){
            if((uint32_t)(now - holdStart) >= SPEED_HOLD_TICKS){
                /* If stopped, swap direction before resuming */
                if(speed <= 0){
                    motorDir = (motorDir == MOTOR_FORWARD) ? MOTOR_REVERSE : MOTOR_FORWARD;
                    Motor_SetDirection(motorDir);
                }
                holding  = 0;
                lastStep = now;
            }
        } else {
            if((uint32_t)(now - lastStep) >= SPEED_STEP_TICKS){
                lastStep = now;
                Motor_SetSpeed(speed);

                if(ramp > 0){
                    if(speed < 100){
                        speed++;
                    } else {
                        ramp     = -1;
                        holding  = 1;
                        holdStart = now;   /* hold at max */
                    }
                } else {
                    if(speed > 0){
                        speed--;
                    } else {
                        ramp     = 1;
                        holding  = 1;
                        holdStart = now;   /* hold at zero — direction swap on resume */
                    }
                }
            }
        }
        delay++;
    }
}
