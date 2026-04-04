# EECS 373 Project: EECS Sticker Vending Machine

![Header image of EECS Stickers, displayed on the top of the LCD](Project_Images/header.png)

Luqin Chang, Matteo Greco, Jacob Pickos, Ryan Salehi, Asher Strayhorn

Spring 2026

TODO: Put revised proposal here

# MCU_A Pinout
LCD:
- PA4 CS
- PB0 RESET
- PB1 DC
- PA7 MOSI
- PA6 MISO
- PB3 SCK
- 3.3V LED

Keypad:
- PB6 SCL
- PB9 SDA
- PA9 INT

UART:
- PC10 UART_TX
- PC11 UART_RX

Motors:
- PC8 Step
- PC9 Dir
- PB4 EN1
- PB5 EN2
- PB13 EN3
- PB14 EN4

////////////////////////////

# MCU_B Pinout
Token NFC:
- PB8 SCL
- PB9 SDA
- PC13 IRQ
- PA8 RESET

PS/2 Magstripe Reader:
- PC0 CLOCK
- PC1 DATA

UART:
- PC10 UART_TX
- PC11 UART_RX

DOOR LATCHES:
- PC2 LATCH1
- PC3 LATCH2

# Keypad Pinout

With pin 0 as the pin closest to the * button:
- Pin 0: Row 1
- Pin 1: Row 2
- Pin 2: Col 0
- Pin 3: Row 3
- Pin 4: Col 1
- Pin 5: Col 2
- Pin 6: Col 3
- Pin 7: Row 0

When read, column value is 1-based and row value is 0-based
