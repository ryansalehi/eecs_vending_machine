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
- PC7 SDA
- PA9 INT

UART:
- PA2 UART_TX
- PA3 UART_RX

TODO: assign motor pins

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
- PA2 UART_TX
- PA3 UART_RX

DOOR LATCHES:
- PC2 LATCH1
- PC3 LATCH2
