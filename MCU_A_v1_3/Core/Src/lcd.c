
#include "lcd.h"

extern SPI_HandleTypeDef hspi1;

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
    lcd_write_data_byte(0x28);  // adjust rotation later

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
