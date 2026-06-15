//hi
//helow

/*
#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "../Drivers/inc/PLL.h"
#include "../Drivers/inc/GPIO.h"
#include "main.h"

#include "../Drivers/inc/ST7735.h"
#include "../Drivers/inc/SysTickInts.h"

*/

/*


void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
void WaitForInterrupt(void);  // low power mode
static void INITIALIZE(void);

static void LCD_Menu_Init(void);
static void LCD_RenderScreen(void);
 
// debounce from chat lol
// yeah thats the way ur supposed to do it
volatile uint32_t lcd_last_press = 0;
#define DEBOUNCE_MS_LCD 150

volatile uint8_t btn_up = 0;
volatile uint8_t btn_down = 0;
volatile uint8_t btn_select = 0;

// Called when Start/Stop is selected from main menu
static void Action_StartStop(void) {
    // implement motor start/stop toggle
}
 
// Called when Speed+ is selected in Motor menu
static void Action_SpeedIncrease(void) {
    // increase motor speed
}
 
// Called when Speed- is selected in Motor menu
static void Action_SpeedDecrease(void) {
    // decrease motor speed
}
 
// Called when Mode is selected in LED menu (cycle picture <-> animation)
static void Action_LEDModeToggle(void) {
    // toggle LED mode between picture and animation

    //ledModeAnim = !ledModeAnim;   // forward ref — defined below

}

// Called when Change is selected in LED menu
static void Action_LEDChange(void) {
    // change the current picture or animation
}

// scrrens
typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_MOTOR,
    SCREEN_LED
} Screen_t;
 
static volatile Screen_t currentScreen = SCREEN_MAIN;
// which cursors
static volatile uint8_t cursorIdx = 0;
 
// Simulated state values shown on sub-menus
// in motor menu header
static volatile uint32_t motorSpeed = 0;
// 0 means pciture, 1 means animation
static volatile uint8_t ledModeAnim = 0;

#define COL_ARROW 1  
#define COL_TEXT 4   
#define ROW_TITLE 1   
#define ROW_DIVIDER 10  
#define ROW_ITEM0 4   

#define ITEM_ROW(i) (ROW_ITEM0 + (i) * 3)
 
#define DIV_Y_PX    36
 
#define COLOR_BG ST7735_BLACK
#define COLOR_TITLE ST7735_CYAN
#define COLOR_ARROW ST7735_YELLOW
#define COLOR_ITEM ST7735_WHITE
#define COLOR_DIM ST7735_DARKGREY
#define COLOR_DIVIDER ST7735_DARKGREY
#define COLOR_VALUE ST7735_GREEN

static void DrawDivider(void) { // turned it off for now it was pmo
    //ST7735_DrawFastHLine(0, DIV_Y_PX, 160, COLOR_DIVIDER);
}

// clear the arrow cursor
static void ClearArrows(uint8_t numItems) {
    for (uint8_t i = 0; i < numItems; i++) {
        ST7735_DrawString2(COL_ARROW, ITEM_ROW(i), ">", COLOR_BG, 2);
    }
}
 
// arrow cursor
static void DrawArrow(uint8_t itemIdx) {
    ST7735_DrawString2(COL_ARROW, ITEM_ROW(itemIdx), ">", COLOR_ARROW, 2);
}

// render main menu
static void Render_MainMenu(void) {
    ST7735_FillScreen(COLOR_BG);
 
    // main title
    ST7735_DrawString2(-2, ROW_TITLE, "  LED Fan", COLOR_TITLE, 2); //idk why this wants -2 lol
    DrawDivider();

    // menu items
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(0), "Start/Stop", COLOR_ITEM, 2);
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(1), "Motor",      COLOR_ITEM, 2);
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(2), "LED",        COLOR_ITEM, 2);

    // curseor
    DrawArrow(cursorIdx);
}

// Build a small integer decimal string (for motor speed display)
// extra QOL chat stuff
static void UInt_ToStr(uint32_t n, char *buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[12]; int i = 0;
    while (n) { tmp[i++] = '0' + (n % 10); n /= 10; }
    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

// mtor menu
static void Render_MotorMenu(void) {
    ST7735_FillScreen(COLOR_BG);
 
    // title
    ST7735_DrawString2(1, ROW_TITLE, "Motor", COLOR_TITLE, 2);
 
    // speed + values
    char speedStr[16];
    UInt_ToStr(motorSpeed, speedStr);
    ST7735_DrawString(16, 1, "Speed:", COLOR_VALUE);
    ST7735_DrawString(23, 1, speedStr, COLOR_VALUE);
 
    DrawDivider();
 
    // menu items
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(0), "Speed+", COLOR_ITEM, 2);
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(1), "Speed-", COLOR_ITEM, 2);
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(2), "Back",   COLOR_ITEM, 2);
 
    DrawArrow(cursorIdx);
}

// led menu
static void Render_LEDMenu(void) {
    ST7735_FillScreen(COLOR_BG);
 
    // title + current mode
    ST7735_DrawString2(1, ROW_TITLE, "LED", COLOR_TITLE, 2);
    if (ledModeAnim) ST7735_DrawString(10, ROW_TITLE, "Mode:Animation", COLOR_VALUE);
    else             ST7735_DrawString(10, ROW_TITLE, "Mode:Picture  ", COLOR_VALUE);
 
    DrawDivider();
 
    // menu items
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(0), "Mode",   COLOR_ITEM, 2);
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(1), "Change", COLOR_ITEM, 2);
    ST7735_DrawString2(COL_TEXT, ITEM_ROW(2), "Back",   COLOR_ITEM, 2);
 
    DrawArrow(cursorIdx);
}
 
// which screen
static void LCD_RenderScreen(void) {
    switch (currentScreen) {
        case SCREEN_MAIN:  Render_MainMenu();  break;
        case SCREEN_MOTOR: Render_MotorMenu(); break;
        case SCREEN_LED:   Render_LEDMenu();   break;
    }
}
 

void Menu_Up(void) { btn_up = 1; }
void Menu_Down(void) { btn_down = 1; }
void Menu_Select(void) { btn_select = 1; }
 
// ============================================================
//  Foreground menu handlers — safe to call ST7735 here             // hello johnathanGPT
// ============================================================
 
static void Handle_Up(void) {
    uint8_t numItems = 3;
    if (cursorIdx > 0) cursorIdx--;
    else  cursorIdx = numItems - 1;
    
    ClearArrows(numItems);
    DrawArrow(cursorIdx);
}
 
static void Handle_Down(void) {
    uint8_t numItems = 3;
    cursorIdx = (cursorIdx + 1) % numItems;
    ClearArrows(numItems);
    DrawArrow(cursorIdx);
}
 
static void Handle_Select(void) {
    char speedStr[16];
 
    switch (currentScreen) {
 
        case SCREEN_MAIN:
            switch (cursorIdx) {
                case 0: Action_StartStop(); break;
                case 1:
                    currentScreen = SCREEN_MOTOR;
                    cursorIdx = 0;
                    LCD_RenderScreen();
                    break;
                case 2:
                    currentScreen = SCREEN_LED;
                    cursorIdx = 0;
                    LCD_RenderScreen();
                    break;
            }
            break;
 
        case SCREEN_MOTOR:
            switch (cursorIdx) {
                case 0:
                    Action_SpeedIncrease();
                    UInt_ToStr(motorSpeed, speedStr);
                    ST7735_DrawString(14, ROW_TITLE, "        ", COLOR_BG);
                    ST7735_DrawString(14, ROW_TITLE, speedStr,   COLOR_VALUE);
                    break;
                case 1:
                    Action_SpeedDecrease();
                    UInt_ToStr(motorSpeed, speedStr);
                    ST7735_DrawString(14, ROW_TITLE, "        ", COLOR_BG);
                    ST7735_DrawString(14, ROW_TITLE, speedStr,   COLOR_VALUE);
                    break;
                case 2:
                    currentScreen = SCREEN_MAIN;
                    cursorIdx = 1;
                    LCD_RenderScreen();
                    break;
            }
            break;
 
        case SCREEN_LED:
            switch (cursorIdx) {
                case 0:
                    Action_LEDModeToggle();
                    if (ledModeAnim) ST7735_DrawString(10, ROW_TITLE, "Mode:Animation", COLOR_VALUE);
                    else             ST7735_DrawString(10, ROW_TITLE, "Mode:Picture  ", COLOR_VALUE);
                    break;
                case 1: Action_LEDChange(); break;
                case 2:
                    currentScreen = SCREEN_MAIN;
                    cursorIdx = 2;
                    LCD_RenderScreen();
                    break;
            }
            break;
    }
}

// menu init
static void LCD_Menu_Init(void) {
    Output_Init(); 
    Output_Clear();
 
    ST7735_SetRotation(1);
 
    currentScreen = SCREEN_MAIN;
    cursorIdx = 0;
    LCD_RenderScreen();
}

static void INITIALIZE(){

    PLL_Init(Bus80MHz);         // 1) clock to 80 MHz — must be first
    SysTick_Init(80000);        // 2) 1 ms tick (80 MHz / 80000 = 1 kHz)
    PB_Init();                  // 3) Port B buttons (IRQ1)
    LED_Init();
    LCD_Menu_Init();            // 4) ST7735 display + initial render

}

static void buttonHandler(){
    if (btn_up) {
        btn_up = 0;
        Handle_Up();
    }
    if (btn_down) {
        btn_down = 0;
        Handle_Down();
    }
    if (btn_select) {
        btn_select = 0;
        Handle_Select();
    }
}

//////////

int main00(void){
	DisableInterrupts();
	INITIALIZE();
	EnableInterrupts();

	while(1){
        buttonHandler();
        WaitForInterrupt();
	}
}

////////

*/
