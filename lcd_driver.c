#include "memory_lcd.h"
#include <stdint.h>
#include <string.h>

#define SHARP_CMD_WRITE_LINE 0x80  // "write line" base bit pattern
#define SHARP_CMD_CLEAR   0x20  // "clear screen" bit
#define SHARP_CMD_VCOM    0x00  // "toggle VCOM only" (no write, no clear)
#define SHARP_VCOM_HIGH 0x40    // "bit in command to set to turn VCOM high
#define SHARP_VCOM_LOW 0x00    // "bit in command to set to turn VCOM low
#define SHARP_BYTES_PER_LINE 50

static uint8_t sharp_vcom = 0x40;

static inline void sharp_toggle_vcom_bit(void)
{
    sharp_vcom = (sharp_vcom ^ SHARP_VCOM_HIGH) & (0b01000000);
}

static void sharp_select(void)
{
    HAL_GPIO_WritePin(SHARP_CS_GPIO_Port, SHARP_CS_Pin, GPIO_PIN_SET); // CS active
}

static void sharp_deselect(void)
{
    HAL_GPIO_WritePin(SHARP_CS_GPIO_Port, SHARP_CS_Pin, GPIO_PIN_RESET); // CS idle
}

static void sharp_write8(uint8_t b)
{
    HAL_SPI_Transmit(&hspi1, &b, 1, HAL_MAX_DELAY);
}

static void write_line(uint8_t line, const uint8_t b[SHARP_BYTES_PER_LINE])
{
    uint8_t message[4 + SHARP_BYTES_PER_LINE]; // need 1 byte for each line, 2 for the command, and 2 after to allow the screen to update
    message[0] = SHARP_CMD_WRITE_LINE | sharp_vcom;
    message[1] = line;
    memcpy(&message[2], b, SHARP_BYTES_PER_LINE);
	message[52] = 0x00;
	message[53] = 0x00;
    sharp_select();
    HAL_SPI_Transmit(&hspi1, &b, sizeof(message), HAL_MAX_DELAY);
    sharp_deselect();
}

static void sharp_clear(void)
{
    uint8_t cmd = SHARP_CMD_CLEAR | sharp_vcom;

    sharp_select();
    sharp_write8(cmd);     // command + VCOM
    sharp_write8(0x00);    // 8 trailer bits
    sharp_deselect();

    sharp_toggle_vcom_bit();
}

static void sharp_vcom(void)
{
    uint8_t cmd = SHARP_CMD_VCOM | sharp_vcom;
    sharp_select();
    sharp_write8(cmd);
    sharp_write8(0x00);
    sharp_deselect();

    sharp_toggle_vcom_bit();
}


static void write_lines(uint8_t *lines, uint16_t num_lines, uint8_t *data)
{
    uint8_t message[2 + num_lines * (SHARP_BYTES_PER_LINE + 2)];

    message[0] = SHARP_CMD_WRITE_LINE | sharp_vcom;
    for (uint8_t line = 0; line < num_lines; line++)
    {
        // Multiply line by bytes per line + 2 because we write in the data and the line bytes and a bytes of trailing 0s
        message[line * (SHARP_BYTES_PER_LINE + 2) + 1] = lines[line];
        memcpy(&message[line * (SHARP_BYTES_PER_LINE + 2) + 2], &data[line * SHARP_BYTES_PER_LINE], SHARP_BYTES_PER_LINE);
        message[(line + 1) * (SHARP_BYTES_PER_LINE + 2)] = 0x00;
    }

    message[num_lines * (SHARP_BYTES_PER_LINE + 2) + 1] = 0x00;

    HAL_SPI_Transmit(&hspi1, &message, sizeof(message), HAL_MAX_DELAY);

	sharp_toggle_vcom_bit();
}



/*
 * I imagine the screen looking something like what I displayed below
┌────────────────────────────────────────────────┐
│  SPEED:  72 mph | MENU TITLE | SOC  50%        │
│                                                │
├────────────────────────────────────────────────┤
│                    MENU DATA                   │
│                                                │
│                                                │
│                                                │
│                                                │
│                                                │
└────────────────────────────────────────────────┘
*/
