
#include "lcd.h"
#include "header_480_70.h"
#define LCD_CS_LOW()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define LCD_CS_HIGH()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)

#define LCD_DC_CMD()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET)
#define LCD_DC_DATA()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET)

#define LCD_RST_LOW()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET)
#define LCD_RST_HIGH() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET)

#define LCD_WIDTH 480
#define LCD_HEIGHT 320
#define LCD_MAX_TEXT_LEN 64

// Operation types
typedef enum
{
    OP_FILL_RECT,
    OP_BLIT_RGB888,
    OP_TEXT
} op_type_t;

typedef struct
{
    uint16_t x, y, w, h;
} rect_t;

typedef struct
{
    op_type_t type;
    rect_t bounds;
    union
    {
        struct
        {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        } fill;
        struct
        {
            const uint8_t *img; // pointer to RGB888 image in flash
        } blit;
        struct
        {
            char text[LCD_MAX_TEXT_LEN];
            uint16_t color565;
            uint8_t scale;
        } text;
    } u;
} lcd_op_t;

extern SPI_HandleTypeDef hspi1;

static osSemaphoreId_t lcdRenderSem; // The LCD Task waits on this to send to new data
static osMutexId_t lcdFrameMutex; // Other tasks will lock this mutex when pushing a frame
static lcd_op_t requestsArray[10]; // TODO: larger?
static cbuf_t requests;

static void lcd_write_cmd(uint8_t cmd)
{
    LCD_DC_CMD();
    LCD_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    LCD_CS_HIGH();
}

static void lcd_write_data(uint8_t *data, uint16_t size)
{
	LCD_DC_DATA();
	LCD_CS_LOW();
    HAL_SPI_Transmit(&hspi1, data, size, HAL_MAX_DELAY);
    LCD_CS_HIGH();
}

static void lcd_write_data_byte(uint8_t data)
{
	lcd_write_data(&data, 1);
}

static void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    // Column Address Set: x0 to x1
    lcd_write_cmd(0x2A);
    data[0] = x0 >> 8; data[1] = x0 & 0xFF;
    data[2] = x1 >> 8; data[3] = x1 & 0xFF;
    lcd_write_data(data, 4);

    // Row Address Set: y0 to y1
    lcd_write_cmd(0x2B);
    data[0] = y0 >> 8; data[1] = y0 & 0xFF;
    data[2] = y1 >> 8; data[3] = y1 & 0xFF;
    lcd_write_data(data, 4);
}

void LCD_Init(void)
{
    // Reset
	LCD_RST_LOW();
    HAL_Delay(10);
    LCD_RST_HIGH();
    HAL_Delay(120);

    // Sleep Out - turns off sleep mode
    lcd_write_cmd(0x11);
    HAL_Delay(120);

    // Pixel Format: 18-bit
    lcd_write_cmd(0x3A);
    lcd_write_data_byte(0x66);

    // Memory Access Control - Page 191 - defines read/write direction of the frame
    lcd_write_cmd(0x36);
    lcd_write_data_byte(0x28);
    // rotation:
    // 0x28 landscape
    // 0x68 landscape + flip X
    // 0xA8 landscape + flip Y
    // 0xE8 landscape + flip X and Y

    // Initialize threading vars
    lcdRenderSem = osSemaphoreNew(1, 0, NULL);
    configASSERT(lcdRenderSem != NULL);
    lcdFrameMutex = osMutexNew(NULL);
    cbuf_init(
        &requests, // the cbuf to init
        requestsArray, // underlying storage
        sizeof(requestsArray) / sizeof(requestsArray[0]), //  how many elements in the underlying sotrage
        sizeof(requestsArray[0]) // size of an element in the storage
    );

    // Display ON
    lcd_write_cmd(0x29);

    // Start with blank screen
    LCD_FillScreen(228, 228, 228);

    LCD_BeginFrame();
    LCD_DrawImageRGB888(0, 0, 480, 70, header_480_70);
    LCD_EndFrame();
}

static void LCD_Render_Op(lcd_op_t*op)
{
    if(!op)
    {
        // Something went wrong
        return;
    }
    LCD_SetWindow(op->bounds.x, op->bounds.y, op->bounds.x + op->bounds.w - 1, op->bounds.y + op->bounds.h - 1);
    lcd_write_cmd(0x2C); // memory write
    if(op->type == OP_FILL_RECT)
    {
        size_t numPixels = op->bounds.w * op->bounds.h;
        for(size_t i = 0; i < numPixels; i++)
        {
            uint8_t pixel[3] = {op->u.fill.r, op->u.fill.g, op->u.fill.b};
            lcd_write_data(pixel, 3);
        }
    }
    else if(op->type == OP_BLIT_RGB888)
    {
        for(size_t y = 0; y < op->bounds.h; y++)
        {
            lcd_write_data(op->u.blit.img + y * op->bounds.w * 3, op->bounds.w * 3);
        }
    }
    else // OP_TEXT
    {
        
    }
}

void LCD_Render()
{
    // Is there a render request?
	volatile osStatus_t state = osSemaphoreAcquire(lcdRenderSem, portMAX_DELAY);
    if(osOK == state)
    {
        osMutexAcquire(lcdFrameMutex, portMAX_DELAY);
        lcd_op_t op;
        while(cbuf_pop(&requests, (void*)&op))
        {
            LCD_Render_Op(&op);
        }
        osMutexRelease(lcdFrameMutex);
    }
}

/**
 * Wrap any requests in these
 */
void LCD_BeginFrame()
{
    osMutexAcquire(lcdFrameMutex, portMAX_DELAY);
}
void LCD_EndFrame()
{
    osMutexRelease(lcdFrameMutex);
    osSemaphoreRelease(lcdRenderSem);
}

/**
 * @pre lcdFrameMutex must be owned by caller (obtained through LCD_BeginFrame)
 */
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                  uint8_t r, uint8_t g, uint8_t b)
{
    // out of bounds
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT)
    {
        return;
    }
    // no work
    if (w == 0 || h == 0)
    {
        return;
    }
    lcd_op_t op;
    op.type = OP_FILL_RECT;
    op.bounds.x = x;
    op.bounds.y = y;
    op.bounds.w = w;
    op.bounds.h = h;
    op.u.fill.r = r;
    op.u.fill.g = g;
    op.u.fill.b = b;
    cbuf_push_overwrite(&requests, &op);
}

/**
 * @pre lcdFrameMutex must be owned by caller (obtained through LCD_BeginFrame)
 */
void LCD_DrawImageRGB888(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *img)
{
    // out of bounds
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT)
    {
        return;
    }
    // No work
    if (w == 0 || h == 0)
    {
        return;
    }
    lcd_op_t op;
    op.type = OP_BLIT_RGB888;
    op.bounds.x = x;
    op.bounds.y = y;
    op.bounds.w = w;
    op.bounds.h = h;
    op.u.blit.img = img;
    cbuf_push_overwrite(&requests, &op);
}

/**
 * @pre lcdFrameMutex must be owned by caller (obtained through LCD_BeginFrame)
 */
void LCD_DrawText(uint16_t x, uint16_t y, const char *s, uint16_t color565, uint8_t scale)
{
    if(!s) 
    {
        return;
    }
    if(scale == 0)
    {
        scale = 1;
    }

    lcd_op_t op;
    op.type = OP_TEXT;
    op.bounds.x = x;
    op.bounds.y = y;

    size_t len = strnlen(s, LCD_MAX_TEXT_LEN - 1);
    memcpy(op.u.text.text, s, len);
    op.u.text.text[len] = '\0';

    op.u.text.color565 = color565;
    op.u.text.scale = scale;

    op.bounds.w = (uint16_t)(len * 6 * scale);
    op.bounds.h = (uint16_t)(7 * scale);

    cbuf_push_overwrite(&requests, &op);
}

/**
 * Test/specific use
 */
void LCD_FillScreen(uint8_t r, uint8_t g, uint8_t b)
{
    // Write to the whole screen (x: 0-479, y: 0-319)
    LCD_SetWindow(0, 0, 479, 319);

    // Memory write
    lcd_write_cmd(0x2C);

    // Stream the data
    for (uint32_t i = 0; i < 480UL * 320UL; i++)
    {
        uint8_t pixel[3] = {r, g, b};
        lcd_write_data(pixel, 3);
    }
}
void LCD_DrawTestGradient(void)
{
    // Write to the whole screen
    LCD_SetWindow(0, 0, 479, 319);

    // Memory write
    lcd_write_cmd(0x2C);

    for (uint16_t y = 0; y < 320; y++)
    {
        for (uint16_t x = 0; x < 480; x++)
        {
            uint8_t r = (uint8_t)((x * 255U) / 479U);
            uint8_t g = (uint8_t)((y * 255U) / 319U);
            uint8_t b = 64;

            uint8_t out[3] = { r, g, b };
            lcd_write_data(out, 3);
        }
    }
}
