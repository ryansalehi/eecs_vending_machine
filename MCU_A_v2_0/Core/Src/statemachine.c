/**
 * @date April 10, 2026
 */

#include "statemachine.h"
#include <stdbool.h>

#include "lcd.h"
#include "sender.h"
#include "motors.h"
#include "keypad.h"
#include "question.h"

typedef struct
{
    /**
     * MCard Reading
     */
    volatile bool name_set;
    char mcard_name[16];

    volatile bool level_set;
    int mcard_level;

    /**
     * Token Reading
     */
    volatile bool token_set;

    /**
     * Class/keypad Reading
     */
    int class;
    char class_string[4];

    /*
    Current question waiting for the answer
    */
    const Question *q;

    /*
    Whether MCU_B has a valid heartbeat
     */
    bool is_dead;
} State_ctx_t;

typedef void (*statePtr)(State_ctx_t*);

extern TIM_HandleTypeDef htim3;

static statePtr next_state;
static State_ctx_t context;

void SM_IsDead(State_ctx_t* ctx);
void SM_Idle(State_ctx_t* ctx);
void SM_NFCWait(State_ctx_t* ctx);
void SM_UnlockVault(State_ctx_t* ctx);
void SM_Denied(State_ctx_t* ctx);
void SM_ClassSelection(State_ctx_t* ctx);
void SM_Question(State_ctx_t* ctx);
void SM_InvalidInput(State_ctx_t* ctx);
void SM_Dispense(State_ctx_t* ctx);
void SM_OhFuck(State_ctx_t* ctx);

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
    strcpy(context.mcard_name, name);
    context.name_set = true;
}

void SM_SetLevel(int level)
{
    context.mcard_level = level;
    context.level_set = true;
}

void SM_SetToken()
{
    context.token_set = true;
}

void SM_SetDead()
{
    context.is_dead = true;
}

void SM_IsDead(State_ctx_t* ctx)
{
    LCD_BeginFrame();
    LCD_FillRect(0, 71, 480, 249, 228, 228, 228); // fill working area with white
    LCD_DrawText(60, 100, "Missing MCU_B", 45, 3);
    LCD_EndFrame();
    next_state = SM_Idle;
    osDelay(5000);
}

void SM_Idle(State_ctx_t* ctx)
{
    // start a new cycle 
    memset(&context, 0, sizeof(context));
    next_state = SM_OhFuck;

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
    while(true)
    {
        if(ctx->is_dead)
        {
            next_state = SM_IsDead;
            return;
        }
        if(ctx->name_set)
        {
            break;
        }
        // else we stay idling
    }
    // Get MCard Level
    uint32_t start_wait = HAL_GetTick();
    while(true)
    {
        if(ctx->is_dead)
        {
            next_state = SM_IsDead;
            return;
        }
        if((HAL_GetTick() - start_wait) > 5000) // wait 5s max for the level to come in after the name
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
    LCD_BeginFrame();
    LCD_FillRect(0, 71, 480, 25, 228, 228, 228); // fill working area with white
    LCD_DrawText(38, 128, "Please insert your token.", 45, 3);
    LCD_DrawText(38, 175, "Machine will time out in 10 seconds.", 45, 3);
    LCD_EndFrame();

    // Get token reading
    uint32_t start_wait = HAL_GetTick();
    while(true)
    {
        if(ctx->is_dead)
        {
            next_state = SM_IsDead;
            return;
        }
        if((HAL_GetTick() - start_wait) > 10000) // wait 10s max
        {
            // didn't get response
            next_state = SM_Denied;
            return;
        }
        if(ctx->token_set)
        {
            break;
        }
        osDelay(5);
    }

    LCD_BeginFrame();
    LCD_FillRect(0, 71, 480, 25, 228, 228, 228); // fill working area with white
    LCD_DrawText(38, 128, "Valid token received", 45, 3);
    LCD_EndFrame();
    osDelay(1000);
    next_state = SM_ClassSelection;
}

void SM_UnlockVault(State_ctx_t* ctx)
{


    LCD_BeginFrame();
    LCD_FillRect(0, 71, 480, 25, 228, 228, 228); // fill working area with white
    LCD_DrawText(38, 128, "Vault unlocked. Open quickly.", 45, 3);
    LCD_DrawText(38, 156, "Combustion Imminent.", 45, 3);
    LCD_EndFrame();

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
    if(ctx->is_dead)
    {
        next_state = SM_IsDead;
        return;
    }
    Class_t class_selection;    
    if(KEYPAD_PromptClassNumber(&class_selection)) 
    {
        if(ctx->is_dead)
        {
            next_state = SM_IsDead;
            return;
        }
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
    if(ctx->is_dead)
    {
        next_state = SM_IsDead;
        return;
    }
    Course course = get_course(ctx->class);
    if (course == CINV) {
        next_state = SM_InvalidInput;
        return;
    }

    // Assign the address of the question to our pointer
    ctx->q = &questions[course];

   
    LCD_BeginFrame();
    LCD_FillRect(0, 71, 480, 249, 228, 228, 228); 
    // TODO: check if anything is out of the frame
    int y = 100;
    // Print Question lines
    for (int i = 0; i < QUESTION_LINES; i++) {
        if(ctx->q->question[i][0] != '\0') { // Only print non-empty lines
            LCD_DrawText(38, y, ctx->q->question[i], 45, 2);
            y += 35;
        }
    }
    
    y += 10; // Gap between Q and A
    
    // Print Answer choices
    for (int i = 0; i < NUM_ANSWER; i++) {
        LCD_DrawText(38, y, ctx->q->answers[i], 45, 2);
        y += 35;
    }
    LCD_EndFrame();

    
    
    //Next wait on this state until 1. answer is given
    // 2. times out
    //TODO: look at keypad function to ensure correctness
    uint8_t user_choice;
    uint32_t start_wait = HAL_GetTick();
 

    while(true)
    {
        if(ctx->is_dead)
        {
            next_state = SM_IsDead;
            return;
        }
        // 1. Check for Keypad Input (Now non-blocking)
        if (KEYPAD_CheckForAnswer(&user_choice)) {
            if (user_choice == ctx->q->correct_answer) {
                next_state = SM_Dispense;
            } else {
                next_state = SM_Denied; //TODO: add a wrong answer state
            }
            return;
        }

        // 2. Check for Timeout
        if((HAL_GetTick() - start_wait) > 10000) 
        {
            next_state = SM_Denied;
            return;
        }

        // 3. Small delay to let the OS breathe
        osDelay(10); 
    }
}

void SM_InvalidInput(State_ctx_t* ctx)
{
    if(ctx->is_dead)
    {
        next_state = SM_IsDead;
        return;
    }
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
    osDelay(5000);
    next_state = SM_Idle;
}
