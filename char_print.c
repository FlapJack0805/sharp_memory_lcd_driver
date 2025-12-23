#include <stdint.h>
#include <stdio.h>
#include "char_print.h"

#define WIDTH 400
#define BYTE_WIDTH (WIDTH >> 3)
#define HEIGHT 240

char sharp_buffer[(WIDTH*HEIGHT) >> 3];



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


static void print_area(uint16_t start_x, uint16_t start_y, uint16_t width, uint16_t height, const unsigned char* data)
{
    // both of these values are relative to the start_x and start_y
    uint16_t curr_x = 0;
    uint16_t curr_y = 0;


    while (curr_y < height && (curr_y + start_y) < HEIGHT)
    {
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

static void init_display(void)
{
    for (uint32_t y = 0; y < HEIGHT; y++)
    {
        if (y == 68)
        {
            for (uint32_t x = 0; x < BYTE_WIDTH; x++)
            {
                sharp_buffer[y * BYTE_WIDTH + x] |= (unsigned char)(0xFF);
            }
        }

        else
        {
            for (uint32_t x = 0; x < BYTE_WIDTH; x++)
            {
                sharp_buffer[y * BYTE_WIDTH + x] &= (unsigned char)0;
            }
        }
    }
}


int display_speed(uint8_t speed)
{
    char speed_str[4]; //max 3 digits + null terminator
    snprintf(speed_str, sizeof(speed_str), "%d",speed);
    return print_string(0, 0, 5, speed_str);
}


int display_soc(uint8_t soc)
{
    char soc_str[5]; // max 3 digits + % + null terminator
    snprintf(soc_str, sizeof(soc_str), "%d%%", soc);
    return print_string(WIDTH-161, 0, 5, soc_str); // maximux of 4 characters with width of 40 bits each and subtract 1 extra because of 0 indexing
}


int main(void)
{

    init_display();

    //print_string(0, 0, 5, "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG");
    
    display_speed(50);
    display_soc(0);

    print_display();
}
