/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include <stdint.h>
#include <stdio.h>
#include "char_print.h"
#include "string.h"

#define WIDTH 400
#define BYTE_WIDTH (WIDTH >> 3)
#define HEIGHT 240

#define TOP_DIVIDER_ROW 62
#define TOP_LEFT_DIVIDER_COLUMN WIDTH/3
#define TOP_RIGHT_DIVIDER_COLUMN 2*(WIDTH/3)

#define SPEED_X 0
#define SPEED_Y 0
#define SOC_X WIDTH-122
#define SOC_Y 0
#define TEXT_X 0
#define TEXT_Y (TOP_DIVIDER_ROW + 20)

#define SHARP_CMD_WRITE_LINE 0x80  // "write line" base bit pattern
#define SHARP_CMD_CLEAR   0x20  // "clear screen" bit
#define SHARP_CMD_VCOM    0x00  // "toggle VCOM only" (no write, no clear)
#define SHARP_VCOM_HIGH 0x40    // "bit in command to set to turn VCOM high
#define SHARP_VCOM_LOW 0x00    // "bit in command to set to turn VCOM low
#define SHARP_BYTES_PER_LINE 50


#define SHARP_CS_GPIO_Port GPIOB
#define SHARP_CS_Pin GPIO_PIN_12

static char sharp_buffer[(WIDTH*HEIGHT) >> 3];
static char changed_lines[HEIGHT / 8];

static uint8_t sharp_vcom_bit = 0x40;

static uint8_t bitrev8(uint8_t v)
{
    v = (v & 0xF0) >> 4 | (v & 0x0F) << 4;
    v = (v & 0xCC) >> 2 | (v & 0x33) << 2;
    v = (v & 0xAA) >> 1 | (v & 0x55) << 1;
    return v;
}


static void mark_all_dirty(void)
{
    memset(changed_lines, 0xFF, sizeof(changed_lines));
}


static inline void sharp_toggle_vcom_bit(void)
{
    sharp_vcom_bit = (sharp_vcom_bit ^ SHARP_VCOM_HIGH) & (0b01000000);
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
    HAL_SPI_Transmit(&hspi2, &b, 1, HAL_MAX_DELAY);
}

/*
static void write_line(uint8_t line, const uint8_t b[SHARP_BYTES_PER_LINE])
{
    uint8_t message[4 + SHARP_BYTES_PER_LINE]; // need 1 byte for each line, 2 for the command, and 2 after to allow the screen to update
    message[0] = SHARP_CMD_WRITE_LINE | sharp_vcom_bit;
    message[1] = line;
    memcpy(&message[2], b, SHARP_BYTES_PER_LINE);
	message[52] = 0x00;
	message[53] = 0x00;
    sharp_select();
    HAL_SPI_Transmit(&hspi2, &b, sizeof(message), HAL_MAX_DELAY);
    sharp_deselect();
}
*/

static void sharp_clear(void)
{
    uint8_t cmd = SHARP_CMD_CLEAR | sharp_vcom_bit;

    sharp_select();
    sharp_write8(cmd);     // command + VCOM
    sharp_write8(0x00);    // 8 trailer bits
    sharp_deselect();

    sharp_toggle_vcom_bit();
}

static void sharp_vcom(void)
{
    uint8_t cmd = SHARP_CMD_VCOM | sharp_vcom_bit;
    sharp_select();
    sharp_write8(cmd);
    sharp_write8(0x00);
    sharp_deselect();

    sharp_toggle_vcom_bit();
}

static void write_lines(uint8_t *lines, uint8_t num_lines)
{
    uint8_t message[1 + num_lines * (1 + SHARP_BYTES_PER_LINE + 1) + 1];
    uint16_t idx = 0;

    // Command byte
    message[idx++] = SHARP_CMD_WRITE_LINE | sharp_vcom_bit;

    for (uint8_t i = 0; i < num_lines; i++)
    {
        uint8_t y = lines[i];

        // Line address (1-based, bit-reversed)
        message[idx++] = bitrev8(y + 1);

        // Pixel data (NOT bit-reversed)
        memcpy(&message[idx],
               &sharp_buffer[y * SHARP_BYTES_PER_LINE],
               SHARP_BYTES_PER_LINE);
        idx += SHARP_BYTES_PER_LINE;

        // Line trailer
        message[idx++] = 0x00;
    }

    // End-of-frame trailer
    message[idx++] = 0x00;

    sharp_select();
    HAL_SPI_Transmit(&hspi2, message, idx, HAL_MAX_DELAY);
    sharp_deselect();

    sharp_toggle_vcom_bit();
}

/*
static void write_lines(uint8_t *lines, uint8_t num_lines)
{
    uint8_t message[2 + num_lines * (SHARP_BYTES_PER_LINE + 2)];

    message[0] = SHARP_CMD_WRITE_LINE | sharp_vcom_bit;
    for (uint8_t line = 0; line < num_lines; line++)
    {
        // Multiply line by bytes per line + 2 because we write in the data and the line bytes and a bytes of trailing 0s
        uint8_t y = lines[line];
        message[line * (SHARP_BYTES_PER_LINE + 2) + 1] = bitrev8((uint8_t)(y + 1));;
        memcpy(&message[line * (SHARP_BYTES_PER_LINE + 2) + 2],
               &sharp_buffer[y * SHARP_BYTES_PER_LINE],
       SHARP_BYTES_PER_LINE);
        message[(line + 1) * (SHARP_BYTES_PER_LINE + 2)] = 0x00;
    }

    message[num_lines * (SHARP_BYTES_PER_LINE + 2) + 1] = 0x00;

    sharp_select();
    HAL_SPI_Transmit(&hspi2, message, sizeof(message), HAL_MAX_DELAY);
    sharp_deselect();

    sharp_toggle_vcom_bit();
}
*/


static void build_pixel_cluster(uint16_t start_x, uint16_t start_y, uint8_t size, unsigned char *data, uint8_t val)
{
    if (val == 1)
    {
        for (uint16_t y = 0; y < (size * 3); y++)
        {
            for (uint16_t x = 0; x < size; x++)
            {
                data[(start_y + y) * size + ((start_x + x) >> 3)] |= (unsigned char)(0x80 >> (x + start_x) % 8);
            }
        }
    }

    else
    {
        for (uint16_t y = 0; y < size * 3; y++)
        {
            for (uint16_t x = 0; x < size; x++)
            {
                    //letter_buf[y + (x >> 3)] &= (unsigned char)~(0x80 >> x);
                data[(start_y + y) * size + ((start_x + x) >> 3)] &= (unsigned char)~(0x80 >> (x + start_x) % 8);
            }
        }
    }
}

static void clear_area(uint16_t start_x, uint16_t start_y, uint16_t width,   uint16_t height)
{
    uint16_t end_x = start_x + width;
    uint16_t end_y = start_y + height;

    if (end_x > WIDTH)  end_x = WIDTH;
    if (end_y > HEIGHT) end_y = HEIGHT;

    uint16_t curr_x = start_x;
    uint16_t curr_y = start_y;

    while (curr_y < end_y)
    {
        changed_lines[curr_y / 8] |= 1 << (curr_y % 8);
        // clear pixel (curr_x, curr_y)
        sharp_buffer[curr_y * BYTE_WIDTH + (curr_x >> 3)] &=
            (unsigned char)~(0x80 >> (curr_x & 7));

        // advance
        curr_x++;
        if (curr_x >= end_x)
        {
            curr_x = start_x;
            curr_y++;
        }
    }
}


static void print_area(uint16_t start_x, uint16_t start_y, uint16_t width, uint16_t height, const unsigned char* data)
{
    // both of these values are relative to the start_x and start_y
    uint16_t curr_x = 0;
    uint16_t curr_y = 0;


    while (curr_y < height && (curr_y + start_y) < HEIGHT)
    {
        changed_lines[(start_y + curr_y) / 8] |= 1 << ((start_y + curr_y) % 8);

        if (curr_y == TOP_DIVIDER_ROW)
        {
            curr_y++;
            continue;
        }

        if (curr_y < TOP_DIVIDER_ROW && (curr_x == TOP_LEFT_DIVIDER_COLUMN || curr_x == TOP_RIGHT_DIVIDER_COLUMN))
        {
            curr_x++;
            if (curr_x >= width)
            {
                curr_x = 0;
                ++curr_y;
            }
            continue;
        }

        if ((data[curr_y * width / 8 + (curr_x >> 3)] & (0x80 >> (curr_x % 8))) == 0)
        {
            sharp_buffer[(curr_y + start_y) * (BYTE_WIDTH) + ((curr_x + start_x) >> 3)] &= ~(unsigned char)(0x80 >> ((start_x + curr_x) % 8));
        }

        else
        {
            sharp_buffer[(curr_y + start_y) * (BYTE_WIDTH) + ((curr_x + start_x) >> 3)] |= (unsigned char)(0x80 >> ((start_x + curr_x) % 8));
        }

        ++curr_x;

        if (curr_x >= width)
        {
            curr_x = 0;
            ++curr_y;
        }
    }
}


// char size is the multiplier to the size of each letter. The unmultiplied size is 8x8 pixles and sizes increase exponentially.
// Size 2 is 32x32 and size 3 is 72x72.
// The size squared is the number of pixels multiplied by the origional
// You must null terminate your strings or you will segfault
static int print_string(uint16_t start_x, uint16_t start_y, uint8_t char_size, const char* str)
{
    uint16_t curr_x = start_x;
    uint16_t curr_y = start_y;

    uint16_t letter_side_length_x = char_size << 3;
    uint16_t letter_side_length_y = char_size * 5;

    uint16_t i = 0;
    while (str[i] != '\0')
    {
        if (curr_x == 0 && str[i] == ' ')
        {
            ++i;
            continue;
        }

        unsigned char letter_buf[(letter_side_length_x * letter_side_length_y * 3) >> 3];
        memset(letter_buf, 0, sizeof(letter_buf));

        for (uint16_t y = 0; y < FONT_HEIGHT; ++y)
        {
            for (uint16_t x = 0; x < (FONT_BYTE_WIDTH << 3); ++x)
            {
                if ((FONT[str[i] - LOW_CHAR][y] & (0x80 >> x)) == 0)
                {
                    //letter_buf[y + (x >> 3)] &= (unsigned char)~(0x80 >> x);
                    build_pixel_cluster(x * char_size, y * char_size * 3, char_size, letter_buf, 0);
                }

                else
                {
                    //letter_buf[y + (x >> 3)] |= (unsigned char)(0x80 >> x);
                    build_pixel_cluster(x * char_size, y * char_size * 3, char_size, letter_buf, 1);
                }
            }
        }

        print_area(curr_x, curr_y, letter_side_length_x, letter_side_length_y * 3, letter_buf);

        curr_x += letter_side_length_x;

        if (curr_x + letter_side_length_x >= WIDTH)
        {
            curr_x = 0;
            curr_y += letter_side_length_y * 3;
        }

        if (curr_y >= HEIGHT)
        {
            return -1;
        }
        ++i;
    }

    return 0;
}


/*
static void print_display(void)
{
    for (uint32_t y = 0; y < HEIGHT; y++)
    {
        for (uint32_t x = 0; x < WIDTH; x++)
        {
            //TODO: Make sure this works because it's mega chopped
            if ((sharp_buffer[y * BYTE_WIDTH + (x >> 3)] & (0x80 >> (x % 8))) == 0)
            {
                printf("0");
            }

            else
            {
                printf("1");
            }
        }
        printf("\t %d\n", y);
    }
}
*/

static void init_display(void)
{
    for (uint32_t y = 0; y < HEIGHT; y++)
    {
        for (uint32_t x = 0; x < WIDTH; x++)
        {
            if (y < TOP_DIVIDER_ROW && (x == TOP_LEFT_DIVIDER_COLUMN || x == TOP_RIGHT_DIVIDER_COLUMN) || y == TOP_DIVIDER_ROW)
            {
                sharp_buffer[(y * BYTE_WIDTH) + (x >> 3)] |= (unsigned char)(0x80 >> x % 8);
            }

            else
            {
                sharp_buffer[(y * BYTE_WIDTH) + (x >> 3)] &= ~(unsigned char)(0x80 >> x % 8);
            }
        }
    }
    mark_all_dirty();
}


int display_speed(uint8_t speed)
{
    char speed_str[4]; //max 3 digits + null terminator
    snprintf(speed_str, sizeof(speed_str), "%d",speed);

    clear_area(SPEED_X, SPEED_Y, TOP_LEFT_DIVIDER_COLUMN - 1, TOP_DIVIDER_ROW - 1);
    return print_string(SPEED_X, SPEED_Y, 5, speed_str);
}

int display_menu_name(const char *menu_name)
{
	const uint8_t char_size = 5;

	uint16_t char_width = char_size << 3; // 8 * size
	uint16_t str_len = strlen(menu_name);
	uint16_t text_width = str_len * char_width;

	uint16_t middle = (WIDTH) / 2;

	int start_x = middle - (text_width / 2);
//	if (start_x < TOP_LEFT_DIVIDER_COLUMN + 1)
//		start_x = TOP_LEFT_DIVIDER_COLUMN + 1;

	uint16_t start_y = 0;

	clear_area(TOP_LEFT_DIVIDER_COLUMN + 1, 0, TOP_RIGHT_DIVIDER_COLUMN - TOP_LEFT_DIVIDER_COLUMN - 1, TOP_DIVIDER_ROW - 1);

	return print_string(start_x, start_y, char_size, menu_name);
}

int display_soc(uint8_t soc)
{
    char soc_str[5]; // max 3 digits + % + null terminator
    snprintf(soc_str, sizeof(soc_str), "%d%%", soc);

    clear_area(SOC_X, SOC_Y, WIDTH - SOC_X - 1, TOP_DIVIDER_ROW - 1);
    return print_string(SOC_X, SOC_Y, 5, soc_str); // maximux of 4 characters with width of 40 bits each and subtract 1 extra because of 0 indexing
}

int display_cc(const char *cc_status)
{
	const uint8_t char_size = 5;

	uint16_t char_width = char_size << 3; // 8 * size
	uint16_t str_len = strlen(cc_status);
	uint16_t text_width = str_len * char_width;

	int start_x = WIDTH - (text_width + 1);
	uint16_t start_y = TOP_DIVIDER_ROW + 10;



	//clear_area(); // how does this function work
	return print_string(start_x, start_y, char_size, cc_status);
}

int display_cc_speed(uint8_t speed)
{
	char cc_speed_str[5]; // max 3 digits + % + null terminator
	snprintf(cc_speed_str, sizeof(cc_speed_str), "%d",speed);

	return print_string(WIDTH/2+(strlen(cc_speed_str)*5), 150, 5, cc_speed_str); // maximux of 4 characters with width of 40 bits each and subtract 1 extra because of 0 indexing
}

int display_text(const char *string)
{
    clear_area(TEXT_X, TEXT_Y, WIDTH - TEXT_X - 1, HEIGHT - TEXT_Y - 1);
    return print_string(TEXT_X, TEXT_Y, 5, string);
}

int display_menu_num(uint8_t menu_num)
{
    char menu_num_str[3]; // max 2 digits + null terminator
    snprintf(menu_num_str, sizeof(menu_num_str), "%d", menu_num);

    int menu_num_x;
    int menu_num_y = 0;
    const int middle = (TOP_RIGHT_DIVIDER_COLUMN + TOP_LEFT_DIVIDER_COLUMN) / 2;
    if (menu_num >= 100)
    {
        return 0;
    }

    else if (menu_num >= 10) // two chars for menu_num
    {
        menu_num_x = middle - 27;
    }

    else
    {
        menu_num_x = middle - 27 / 2;
    }

    clear_area(TOP_LEFT_DIVIDER_COLUMN + 1, 0, TOP_RIGHT_DIVIDER_COLUMN - TOP_LEFT_DIVIDER_COLUMN - 1, TOP_DIVIDER_ROW - 1);
    return print_string(menu_num_x, menu_num_y, 5, menu_num_str);
}


static void write_sharp(void)
{
    uint8_t lines_to_change[HEIGHT];
    uint8_t num_lines = 0;
    for (int y = 0; y < HEIGHT; ++y)
    {
        if (changed_lines[y / 8] & (1 << (y % 8)))
        {
            lines_to_change[num_lines] = y;
            ++num_lines;
        }

    }

    if (num_lines > 0)
    {
        write_lines(lines_to_change, num_lines);
    }

    else
    {
        sharp_vcom();
    }
    memset(changed_lines, 0, sizeof(changed_lines));
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  init_display();
  display_speed(100);
  display_menu_name("MENU");
  display_soc(99);
  display_cc("CCOFF");
  display_cc_speed(30);
  //print_string(0,HEIGHT/2 , 4, "DIGGIN IN YO BUTT TWIN");


  while (1)
  {
    /* USER CODE END WHILE */
    write_sharp();

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pin : PB12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
