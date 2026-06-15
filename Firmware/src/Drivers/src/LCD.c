/**
 * @file LCD.c
 * @brief ST7735 LCD menu driver
 *
 * Screens:
 *   MAIN  — Spinup/Spindown, Motor submenu, LED submenu
 *   MOTOR — Speed+, Speed-, Back  (live duty/setpoint/RPM display)
 *   LED   — Mode (picture/animation/mic), Change, Back
 * 
 *  PB1 / PB2 / PB3
 *
 * Basically all of the LCD Menu stuff
 */

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "Drivers/inc/PLL.h"
#include "Drivers/inc/GPIO.h"
#include "main.h"
#include "Drivers/inc/ST7735.h"
#include "Drivers/inc/SysTickInts.h"
#include "Drivers/inc/Motor.h"

static void LCD_RenderScreen(void);

typedef enum { SCREEN_MAIN = 0, SCREEN_MOTOR, SCREEN_LED } Screen_t;

static volatile Screen_t currentScreen = SCREEN_MAIN;
static volatile uint8_t  cursorIdx     = 0;

/* 0 = picture, 1 = animation, 2 = mic */
volatile uint8_t ledModeAnim = 0;

#define COL_ARROW   1
#define COL_TEXT    4
#define ROW_TITLE   1
#define ROW_ITEM0   4
#define ITEM_ROW(i) (ROW_ITEM0 + (i) * 3)

#define COLOR_BG       ST7735_BLACK
#define COLOR_TITLE    ST7735_CYAN
#define COLOR_ARROW    ST7735_YELLOW
#define COLOR_ITEM     ST7735_WHITE
#define COLOR_VALUE    ST7735_GREEN

volatile uint32_t lcd_last_press = 0;
#define DEBOUNCE_MS_LCD 150

volatile uint8_t btn_up     = 0;
volatile uint8_t btn_down   = 0;
volatile uint8_t btn_select = 0;
volatile uint8_t motor_state = 0;
extern volatile MotorRampState_t motorRampState;
volatile uint8_t motorSetpoint = MOTOR_SETPOINT;

/* ─────────────────────────────────────────────────────────────────── */

static void Action_StartStop(void) {
    if (motor_state == 0 && motorRampState == MOTOR_IDLE) {
        Motor_Spinup(MOTOR_SPINUP_MS, motorSetpoint);
        motor_state = 1;
        ST7735_DrawString2(COL_TEXT, ITEM_ROW(0), "Spindown", COLOR_ITEM, 2);
    } else if (motor_state == 1) {
        Motor_Spindown(MOTOR_SPINDOWN_MS);
        motor_state = 0;
        ST7735_DrawString2(COL_TEXT, ITEM_ROW(0), "Spinup  ", COLOR_ITEM, 2);
    }
}

static void Action_SpeedIncrease(void) {
    if (motorRampState != MOTOR_IDLE || Motor_GetDuty() != motorSetpoint) return;
    if (motorSetpoint < MAX_SPEED) motorSetpoint++;
    Motor_SetSpeed(motorSetpoint);
}

static void Action_SpeedDecrease(void) {
    if (motorRampState != MOTOR_IDLE || Motor_GetDuty() != motorSetpoint) return;
    if (motorSetpoint > MIN_STARTSPEED) motorSetpoint--;
    Motor_SetSpeed(motorSetpoint);
}

extern void Globe_ToggleMode(void);
static void Action_LEDModeToggle(void) { Globe_ToggleMode(); }

extern void Globe_ChangeContent(void);
static void Action_LEDChange(void) { Globe_ChangeContent(); }

/* ─────────────────────────────────────────────────────────────────── */

static void ClearArrows(uint8_t n) {
    for (uint8_t i = 0; i < n; i++)
        ST7735_DrawString2(COL_ARROW, ITEM_ROW(i), ">", COLOR_BG, 2);
}

static void DrawArrow(uint8_t i) {
    ST7735_DrawString2(COL_ARROW, ITEM_ROW(i), ">", COLOR_ARROW, 2);
}

/* ─────────────────────────────────────────────────────────────────── */

static void UInt_ToStr(uint32_t n, char *buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[12]; int i = 0;
    while (n) { tmp[i++] = '0' + (n % 10); n /= 10; }
    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

/* ─────────────────────────────────────────────────────────────────── */

static void Render_MainMenu(void) {
    ST7735_FillScreen(COLOR_BG);
    ST7735_DrawString2(-2, ROW_TITLE, "  LED Fan", COLOR_TITLE, 2);
    if (!motor_state) ST7735_DrawString2(COL_TEXT, ITEM_ROW(0), "Spinup  ", COLOR_ITEM, 2);
    else              ST7735_DrawString2(COL_TEXT, ITEM_ROW(0), "Spindown", COLOR_ITEM, 2);
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(1), "Motor", COLOR_ITEM, 2);
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(2), "LED",   COLOR_ITEM, 2);
    DrawArrow(cursorIdx);
}

static void Render_MotorMenu(void) {
    ST7735_FillScreen(COLOR_BG);
    ST7735_DrawString2(1, ROW_TITLE, "Motor", COLOR_TITLE, 2);
    ST7735_DrawString(15, 1, "Speed:",    COLOR_VALUE);
    ST7735_DrawString(12, 2, "SetPoint:", COLOR_VALUE);
    ST7735_DrawString(15, 3, "RPM:",      COLOR_VALUE);
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(0), "Speed+", COLOR_ITEM, 2);
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(1), "Speed-", COLOR_ITEM, 2);
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(2), "Back",   COLOR_ITEM, 2);
    DrawArrow(cursorIdx);
}

static void Render_LEDMenu(void) {
    ST7735_FillScreen(COLOR_BG);
    ST7735_DrawString2(1, ROW_TITLE, "LED", COLOR_TITLE, 2);
    if      (ledModeAnim == 2) ST7735_DrawString(10, ROW_TITLE, "Mode:Mic      ", COLOR_VALUE);
    else if (ledModeAnim == 1) ST7735_DrawString(10, ROW_TITLE, "Mode:Animation", COLOR_VALUE);
    else                       ST7735_DrawString(10, ROW_TITLE, "Mode:Picture  ", COLOR_VALUE);
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(0), "Mode",   COLOR_ITEM, 2);
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(1), "Change", COLOR_ITEM, 2);
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(2), "Back",   COLOR_ITEM, 2);
    DrawArrow(cursorIdx);
}

static void LCD_RenderScreen(void) {
    switch (currentScreen) {
        case SCREEN_MAIN:  Render_MainMenu();  break;
        case SCREEN_MOTOR: Render_MotorMenu(); break;
        case SCREEN_LED:   Render_LEDMenu();   break;
    }
}

/* ─────────────────────────────────────────────────────────────────── */

void Menu_Up(void)     { btn_up     = 1; }
void Menu_Down(void)   { btn_down   = 1; }
void Menu_Select(void) { btn_select = 1; }

/* ─────────────────────────────────────────────────────────────────── */

static void Handle_Up(void) {
    uint8_t n = 3;
    cursorIdx = (cursorIdx > 0) ? cursorIdx - 1 : n - 1;
    ClearArrows(n);
    DrawArrow(cursorIdx);
}

static void Handle_Down(void) {
    cursorIdx = (cursorIdx + 1) % 3;
    ClearArrows(3);
    DrawArrow(cursorIdx);
}

static void Handle_Select(void) {
    switch (currentScreen) {

        case SCREEN_MAIN:
            switch (cursorIdx) {
                case 0: Action_StartStop(); break;
                case 1: currentScreen = SCREEN_MOTOR; cursorIdx = 0; LCD_RenderScreen(); break;
                case 2: currentScreen = SCREEN_LED;   cursorIdx = 0; LCD_RenderScreen(); break;
            }
            break;

        case SCREEN_MOTOR:
            switch (cursorIdx) {
                case 0: Action_SpeedIncrease(); break;
                case 1: Action_SpeedDecrease(); break;
                case 2: currentScreen = SCREEN_MAIN; cursorIdx = 1; LCD_RenderScreen(); break;
            }
            break;

        case SCREEN_LED:
            switch (cursorIdx) {
                case 0:
                    Action_LEDModeToggle();
                    if      (ledModeAnim == 2) ST7735_DrawString(10, ROW_TITLE, "Mode:Mic      ", COLOR_VALUE);
                    else if (ledModeAnim == 1) ST7735_DrawString(10, ROW_TITLE, "Mode:Animation", COLOR_VALUE);
                    else                       ST7735_DrawString(10, ROW_TITLE, "Mode:Picture  ", COLOR_VALUE);
                    break;
                case 1: Action_LEDChange(); break;
                case 2: currentScreen = SCREEN_MAIN; cursorIdx = 2; LCD_RenderScreen(); break;
            }
            break;
    }
}

/* ─────────────────────────────────────────────────────────────────── */

void LCD_Menu_Init(void) {
    Output_Init();
    Output_Clear();
    ST7735_SetRotation(3);
    currentScreen = SCREEN_MAIN;
    cursorIdx = 0;
    LCD_RenderScreen();
}

extern volatile uint8_t motor_state_changed;

void buttonHandler(void) {
    if (btn_up)     { btn_up     = 0; Handle_Up();     }
    if (btn_down)   { btn_down   = 0; Handle_Down();   }
    if (btn_select) { btn_select = 0; Handle_Select(); }

    if (currentScreen == SCREEN_MOTOR) {
        char speedStr[16], setStr[16], rpmStr[16];
        UInt_ToStr(Motor_GetDuty(),  speedStr);
        UInt_ToStr(motorSetpoint,    setStr);
        UInt_ToStr(Motor_GetRPM(),   rpmStr);
        ST7735_DrawString(22, ROW_TITLE,   speedStr, COLOR_VALUE);
        ST7735_DrawString(22, ROW_TITLE+1, setStr,   COLOR_VALUE);
        ST7735_DrawString(22, ROW_TITLE+2, rpmStr,   COLOR_VALUE);
    }
    motor_state_changed = 1;
}
