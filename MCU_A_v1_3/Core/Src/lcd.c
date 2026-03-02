
#include "lcd.h"

extern SPI_HandleTypeDef hspi1;
extern const uint8_t image_rgb888[480*320*3];

#define LCD_CS_LOW()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define LCD_CS_HIGH()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)

#define LCD_DC_CMD()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET)
#define LCD_DC_DATA()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET)

#define LCD_RST_LOW()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET)
#define LCD_RST_HIGH() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET)

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

void LCD_Init(void)
{
    // Reset
	LCD_RST_LOW();
    HAL_Delay(10);
    LCD_RST_HIGH();
    HAL_Delay(120);

    // Sleep Out
    lcd_write_cmd(0x11);
    HAL_Delay(120);

    // Pixel Format: 18-bit
    lcd_write_cmd(0x3A);
    lcd_write_data_byte(0x66);

    // Memory Access Control
    lcd_write_cmd(0x36);
    lcd_write_data_byte(0x28);
    // rotation:
    // 0x28 landscape
    // 0x68 landscape + flip X
    // 0xA8 landscape + flip Y
    // 0xE8 landscape + flip X and Y


    // Display ON
    lcd_write_cmd(0x29);
    HAL_Delay(20);
}

void LCD_FillScreen(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t colData[4];
    uint8_t rowData[4];

    // Column address set (0 to 479)
    lcd_write_cmd(0x2A);
    colData[0] = 0x00;
    colData[1] = 0x00;
    colData[2] = 0x01;
    colData[3] = 0xDF;  // 479
    lcd_write_data(colData, 4);

    // Row address set (0 to 319)
    lcd_write_cmd(0x2B);
    rowData[0] = 0x00;
    rowData[1] = 0x00;
    rowData[2] = 0x01;
    rowData[3] = 0x3F;  // 319
    lcd_write_data(rowData, 4);

    // Memory write
    lcd_write_cmd(0x2C);

    LCD_DC_DATA();
    LCD_CS_LOW();

    for (uint32_t i = 0; i < 480UL * 320UL; i++)
    {
        uint8_t pixel[3] = {r, g, b};
        HAL_SPI_Transmit(&hspi1, pixel, 3, HAL_MAX_DELAY);
    }

    LCD_CS_HIGH();
}

void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    lcd_write_cmd(0x2A); // CASET
    data[0] = x0 >> 8; data[1] = x0 & 0xFF;
    data[2] = x1 >> 8; data[3] = x1 & 0xFF;
    lcd_write_data(data, 4);

    lcd_write_cmd(0x2B); // RASET
    data[0] = y0 >> 8; data[1] = y0 & 0xFF;
    data[2] = y1 >> 8; data[3] = y1 & 0xFF;
    lcd_write_data(data, 4);

    lcd_write_cmd(0x2C); // RAMWR
}

void LCD_DrawTestGradient(void)
{
    LCD_SetWindow(0, 0, 479, 319);
    LCD_DC_DATA();
    LCD_CS_LOW();

    for (uint16_t y = 0; y < 320; y++)
    {
        for (uint16_t x = 0; x < 480; x++)
        {
            uint8_t r = (uint8_t)((x * 255U) / 479U);
            uint8_t g = (uint8_t)((y * 255U) / 319U);
            uint8_t b = 64;

            // In 18-bit mode, panel uses the top 6 bits of each color.
            // Sending full 8-bit is fine; low bits are ignored.
            uint8_t out[3] = { r, g, b };
            HAL_SPI_Transmit(&hspi1, out, 3, HAL_MAX_DELAY);
        }
    }

    LCD_CS_HIGH();
}

void LCD_DrawImageRGB888(const uint8_t *img, uint16_t w, uint16_t h)
{
    LCD_SetWindow(0, 0, w-1, h-1);
    LCD_DC_DATA();
    LCD_CS_LOW();

    for (uint16_t y = 0; y < h; y++)
    {
        const uint8_t *line = &img[y * w * 3];
        HAL_SPI_Transmit(&hspi1, (uint8_t*)line, (uint16_t)(w * 3), HAL_MAX_DELAY);
    }

    LCD_CS_HIGH();
}


