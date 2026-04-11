/**
 * @date April 10, 2026
 */

#include "statemachine.h"
#include <stdbool.h>

#include "lcd.h"
#include "sender.h"
#include "motors.h"
#include "keypad.h"

typedef struct
{
    char mcard_name[16];
    volatile bool name_set;
    int mcard_level;
    volatile bool level_set;
    char class_string[4];
    int class;
} State_ctx_t;

typedef void (*statePtr)(State_ctx_t*);

extern TIM_HandleTypeDef htim3;

static statePtr next_state;
static State_ctx_t context;

void SM_Run()
{
    memset(&context, 0, sizeof(context));
    next_state = SM_Idle;
    while(true)
    {
        if(next_state)
        {
            next_state(&context);
        }
        else
        {
            // we're cooked, shouldn't get here
            next_state = SM_OhFuck;
        }
    }
}

void SM_SetNewName(char*name)
{
    context.name_set = true;
    strcpy(context.mcard_name, name);
}

void SM_SetLevel(int level)
{
    context.level_set = true;
    context.mcard_level = level;
}

void SM_Idle(State_ctx_t* ctx)
{
    // start a new cycle 
    memset(&context, 0, sizeof(context));

    // print to LCD "Hello, please swipe MCard"
    LCD_BeginFrame();
    LCD_FillRect(0, 71, 480, 249, 228, 228, 228); // fill working area with white
    LCD_DrawText(60, 100, "Welcome to the EECS", 45, 3);
    LCD_DrawText(38, 128, "sticker vending machine!", 45, 3);
    LCD_DrawText(38, 175, "Please swipe your MCard", 45, 3);
    LCD_DrawText(80, 203, "to get started.", 45, 3);
    LCD_DrawText(15, 290, "user: ", 155, 2);
    LCD_DrawText(80, 290, "searching", 155, 2);
    LCD_DrawText(240, 290, "level: ", 155, 2);
    LCD_DrawText(320, 290, "searching", 155, 2);
    LCD_EndFrame();

    // Get MCard Name
    uint32_t start_wait = HAL_GetTick();
    uint32_t max_wait = 5000; // 5s
    while(true)
    {
        if((HAL_GetTick() - start_wait) > max_wait)
        {
            // didn't get response
            next_state = SM_Denied;
            return;
        }
        if(ctx->name_set)
        {
            break;
        }
    }
    // Get MCard Level
    start_wait = HAL_GetTick();
    while(true)
    {
        if((HAL_GetTick() - start_wait) > max_wait)
        {
            // didn't get response
            next_state = SM_Denied;
            return;
        }
        if(ctx->level_set)
        {
            break;
        }
    }

    LCD_BeginFrame();
    LCD_FillRect(0, 71, 480, 25, 228, 228, 228); // fill working area with white
    LCD_DrawText(15, 290, "user: ", 155, 2);
    LCD_DrawText(80, 290, ctx->mcard_name, 155, 2);
    LCD_DrawText(240, 290, "level: ", 155, 2);
    switch(ctx->mcard_level)
    {
        case 1:
            // student
            LCD_DrawText(320, 290, "student", 155, 2);
            next_state = SM_NFCWait;
            break;
        case 2:
            // admin
            LCD_DrawText(320, 290, "admin", 155, 2);
            next_state = SM_UnlockVault;
            break;
        case 3:
            // unathorized / denied
            LCD_DrawText(320, 290, "unrecognized", 155, 2);
            next_state = SM_Denied;
            break;
        default:
            next_state = SM_OhFuck;
            break;
    }
    LCD_EndFrame();
}

void SM_NFCWait(State_ctx_t* ctx)
{
    // print to LCD "waiting for NFC token"
    // if token == bad || timeout == too long' set function pointer to SM_Denied
    // if token == good set function pointer to SM_ClassSelection

    LCD_BeginFrame();
    LCD_FillRect(0, 71, 480, 25, 228, 228, 228); // fill working area with white
    LCD_DrawText(38, 128, "Please insert your token.", 45, 3);
    LCD_DrawText(38, 175, "Machine will time out in 10 seconds.", 45, 3);
    LCD_EndFrame();

    uint8_t token_reading = 0;
    int timer = 10;
    while(timer > 0)
    {
        osDelay(1000);
        --timer;
        switch(token_reading)
        {
            case 1:
                next_state = SM_Denied;
                break;
            case 2:
                next_state = SM_ClassSelection;
                break;
            default:
                next_state = SM_OhFuck;
                break;
        }
    }
    next_state = SM_Denied;
}

void SM_UnlockVault(State_ctx_t* ctx)
{
    LCD_BeginFrame();
    LCD_FillRect(0, 71, 480, 25, 228, 228, 228); // fill working area with white
    LCD_DrawText(38, 128, "Vault unlocked. Open quickly.", 45, 3);
    LCD_DrawText(38, 156, "Combustion Imminent.", 45, 3);
    LCD_EndFrame();
    UART_SendMessage("VAULT:OPEN");
    osDelay(5000);
    next_state = SM_Idle;
}

void SM_Denied(State_ctx_t* ctx)
{
    LCD_BeginFrame();
    LCD_FillRect(0, 71, 480, 25, 228, 228, 228); // fill working area with white
    LCD_DrawText(38, 128, "Access Denied.", 45, 3);
    LCD_EndFrame();
    osDelay(3000);

    next_state = SM_Idle;
}

void SM_ClassSelection(State_ctx_t* ctx)
{

    Class_t class_selection;    
    if(KEYPAD_PromptClassNumber(&class_selection)) 
    {
        switch(class_selection.number_int)
        {
            case 373: 
                // fall through intentional
            case 281:
                // fall through intentional
            case 270:
                // fall through intentional
            case 489: 
                ctx->class = class_selection.number_int;
                memcpy(ctx->class_string, class_selection.number_string, 4);
                next_state = SM_Question;
                break;
            case 1: // 1 means exit
                next_state = SM_Idle;
                break;
            default: 
                next_state = SM_InvalidInput;
                break;
        }
    }
    else
    {
        next_state = SM_InvalidInput;
    }
}

void SM_Question(State_ctx_t* ctx)
{
    // select a question from the question of the class indicated in the context
    // wait for user input 
    // if input is invalid or does not match the correct answer, go to denied state
    // else go to dispense
}

void SM_InvalidInput(State_ctx_t* ctx)
{
    LCD_BeginFrame();
    LCD_FillRect(0, 71, 480, 25, 228, 228, 228); // fill working area with white
    LCD_DrawText(38, 128, "This is the invalid input state :)", 45, 3);
    LCD_DrawText(38, 175, "Don't feel bad. You are cool.", 45, 3);
    LCD_EndFrame();
    osDelay(3000);
    next_state = SM_ClassSelection;
}

void SM_Dispense(State_ctx_t* ctx) 
{
    // print to LCD "Thank you, enjoy your sticker!"
    // turn corresponding motor and dispense sticker
    // return to IDLE after ~10 seconds
    LCD_BeginFrame();
    LCD_FillRect(0, 71, 480, 25, 228, 228, 228); // fill working area with white
    LCD_DrawText(60, 100, "Dispensing sticker...", 45, 3);
    LCD_EndFrame();
    osDelay(500);
    if(ctx->class == 373)
    {
        dispense(1, &htim3);
    }
    else if(ctx->class == 270)
    {
        dispense(2, &htim3);
    }
    else if(ctx->class == 281)
    {
        dispense(3, &htim3);
    }
    else
    {
        dispense(4, &htim3);
    }
    LCD_BeginFrame();
    LCD_FillRect(60, 100, 420, 195, 228, 228, 228);
    LCD_DrawText(60, 100, "Thank you, enjoy your sticker!", 45, 3);
    LCD_EndFrame();
    osDelay(3000);
    next_state = SM_Idle;
} 

void SM_OhFuck(State_ctx_t* ctx)
{
    LCD_BeginFrame();
    LCD_FillRect(0, 71, 480, 25, 228, 228, 228); // fill working area with white
    LCD_DrawText(38, 128, "You fucked up badly!", 45, 3);
    LCD_DrawText(38, 175, "Something broke, go fix", 45, 3);
    LCD_EndFrame();
    osDelay(10000);
    next_state = SM_Idle;
}
