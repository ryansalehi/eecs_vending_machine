
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
#define FONT5X7_WIDTH 5
#define FONT5X7_HEIGHT 7
#define FONT5X7_ADVANCE 6

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
static void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

static void lcd_write_cmd(uint8_t cmd)
{
    LCD_DC_CMD();
    LCD_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    LCD_CS_HIGH();
}

static void lcd_write_data(const uint8_t *data, uint16_t size)
{
	LCD_DC_DATA();
	LCD_CS_LOW();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)data, size, HAL_MAX_DELAY);
    LCD_CS_HIGH();
}

static void lcd_write_data_byte(uint8_t data)
{
	lcd_write_data(&data, 1);
}

static void lcd_rgb565_to_rgb888(uint16_t color565, uint8_t rgb[3])
{
    uint8_t r5 = (uint8_t)((color565 >> 11) & 0x1F);
    uint8_t g6 = (uint8_t)((color565 >> 5) & 0x3F);
    uint8_t b5 = (uint8_t)(color565 & 0x1F);

    rgb[0] = (uint8_t)((r5 << 3) | (r5 >> 2));
    rgb[1] = (uint8_t)((g6 << 2) | (g6 >> 4));
    rgb[2] = (uint8_t)((b5 << 3) | (b5 >> 2));
}

static void lcd_write_solid_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t pixel[3])
{
    if (w == 0 || h == 0 || x >= LCD_WIDTH || y >= LCD_HEIGHT)
    {
        return;
    }
    if ((uint32_t)x + w > LCD_WIDTH)
    {
        w = (uint16_t)(LCD_WIDTH - x);
    }
    if ((uint32_t)y + h > LCD_HEIGHT)
    {
        h = (uint16_t)(LCD_HEIGHT - y);
    }

    LCD_SetWindow(x, y, x + w - 1, y + h - 1);
    lcd_write_cmd(0x2C);

    size_t numPixels = (size_t)w * h;
    for (size_t i = 0; i < numPixels; i++)
    {
        lcd_write_data(pixel, 3);
    }
}

static void lcd_render_text_op(const lcd_op_t *op)
{
    uint8_t fg[3];
    uint8_t scale = op->u.text.scale == 0 ? 1 : op->u.text.scale;
    uint32_t cursorX = op->bounds.x;
    uint32_t cursorY = op->bounds.y;
    uint32_t startX = op->bounds.x;

    lcd_rgb565_to_rgb888(op->u.text.color565, fg);

    for (size_t i = 0; op->u.text.text[i] != '\0'; i++)
    {
        uint8_t c = (uint8_t)op->u.text.text[i];
        if (c == '\r')
        {
            continue;
        }
        if (c == '\n')
        {
            cursorX = startX;
            cursorY += (FONT5X7_HEIGHT + 1U) * scale;
            continue;
        }
        if (c < 32 || c > 127)
        {
            c = '?';
        }

        const uint8_t *glyph = font5x7[c - 32];
        for (uint8_t row = 0; row < FONT5X7_HEIGHT; row++)
        {
            for (uint8_t sy = 0; sy < scale; sy++)
            {
                uint32_t dstY32 = cursorY + row * scale + sy;
                if (dstY32 >= LCD_HEIGHT)
                {
                    continue;
                }
                uint16_t dstY = (uint16_t)dstY32;

                uint8_t col = 0;
                while (col < FONT5X7_WIDTH)
                {
                    while (col < FONT5X7_WIDTH && ((glyph[col] >> row) & 0x01U) == 0U)
                    {
                        col++;
                    }
                    uint8_t runStart = col;
                    while (col < FONT5X7_WIDTH && ((glyph[col] >> row) & 0x01U) != 0U)
                    {
                        col++;
                    }
                    if (runStart < col)
                    {
                        uint32_t runX32 = cursorX + runStart * scale;
                        if (runX32 >= LCD_WIDTH)
                        {
                            continue;
                        }
                        uint16_t runX = (uint16_t)runX32;
                        uint16_t runW = (uint16_t)((col - runStart) * scale);
                        lcd_write_solid_rect(runX, dstY, runW, 1, fg);
                    }
                }
            }
        }

        cursorX += FONT5X7_ADVANCE * scale;
    }
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
    LCD_DrawText(60, 100, "Welcome to the EECS", 45, 3);
    LCD_DrawText(38, 128, "sticker vending machine!", 45, 3);
    LCD_DrawText(38, 175, "Please swipe your MCard", 45, 3);
    LCD_DrawText(80, 203, "to get started.", 45, 3);
    LCD_EndFrame();

}

static void LCD_Render_Op(lcd_op_t*op)
{
    if(!op)
    {
        // Something went wrong
        return;
    }

    if(op->type == OP_FILL_RECT)
    {
        uint8_t pixel[3] = {op->u.fill.r, op->u.fill.g, op->u.fill.b};
        lcd_write_solid_rect(op->bounds.x, op->bounds.y, op->bounds.w, op->bounds.h, pixel);
    }
    else if(op->type == OP_BLIT_RGB888)
    {
        uint16_t x = op->bounds.x;
        uint16_t y0 = op->bounds.y;
        uint16_t w = op->bounds.w;
        uint16_t h = op->bounds.h;

        if (x >= LCD_WIDTH || y0 >= LCD_HEIGHT || w == 0 || h == 0)
        {
            return;
        }
        if ((uint32_t)x + w > LCD_WIDTH)
        {
            w = (uint16_t)(LCD_WIDTH - x);
        }
        if ((uint32_t)y0 + h > LCD_HEIGHT)
        {
            h = (uint16_t)(LCD_HEIGHT - y0);
        }

        LCD_SetWindow(x, y0, x + w - 1, y0 + h - 1);
        lcd_write_cmd(0x2C); // memory write

        for(size_t row = 0; row < h; row++)
        {
            lcd_write_data(op->u.blit.img + row * op->bounds.w * 3, w * 3);
        }
    }
    else // OP_TEXT
    {
        lcd_render_text_op(op);
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
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT)
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

    if (len == 0)
    {
        return;
    }

    uint32_t textW = (uint32_t)len * FONT5X7_ADVANCE * scale;
    uint32_t textH = (uint32_t)FONT5X7_HEIGHT * scale;
    op.bounds.w = textW > UINT16_MAX ? UINT16_MAX : (uint16_t)textW;
    op.bounds.h = textH > UINT16_MAX ? UINT16_MAX : (uint16_t)textH;

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
