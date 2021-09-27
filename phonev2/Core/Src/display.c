#include "main.h"
#include "dac.h"
#include "dma.h"
#include "rtc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

extern const uint16_t Font7x10 [];
extern const uint16_t Font6x8 [];
extern const uint16_t Font11x18 [];
extern const uint16_t Font16x26 [];

extern SPI_HandleTypeDef hspi1;

uint8_t CurrentX = 0;
uint8_t CurrentY = 0;

// Screenbuffer
static uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];

void DISPLAY_WRITE_CMD(uint8_t cmd, uint16_t size)
{
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // # COMMAND
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET); // CS  
  HAL_SPI_Transmit(&hspi1, &cmd, size, 1000);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET); // CS    
}

void DISPLAY_WRITE_DATA(uint8_t *data, uint16_t size)
{
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); // # DATA
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET); // CS
  HAL_SPI_Transmit(&hspi1, data, size, 1000);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET); // CS  
}

// Fill the whole screen with the given color
void ssd1306_Fill(SSD1306_COLOR color) {
    /* Set memory */
    uint32_t i;

    for(i = 0; i < sizeof(SSD1306_Buffer); i++) {
        SSD1306_Buffer[i] = (color == Black) ? 0x00 : 0xFF;
    }
}

// Write the screenbuffer with changed to the screen
void ssd1306_UpdateScreen(void) {
    // Write data to each page of RAM. Number of pages
    // depends on the screen height:
    //
    //  * 32px   ==  4 pages
    //  * 64px   ==  8 pages
    //  * 128px  ==  16 pages
    for(uint8_t i = 0; i < SSD1306_HEIGHT/8; i++) {
        DISPLAY_WRITE_CMD(0xB0 + i, 1); // Set the current RAM page address.
        DISPLAY_WRITE_CMD(0x00, 1);
        DISPLAY_WRITE_CMD(0x10, 1);
        DISPLAY_WRITE_DATA(&SSD1306_Buffer[SSD1306_WIDTH*i], SSD1306_WIDTH);
    }
}

void ssd1306_SetDisplayOn(const uint8_t on) {
    uint8_t value;
    if (on) {
        value = 0xAF;   // Display on
    } else {
        value = 0xAE;   // Display off
    }
    DISPLAY_WRITE_CMD(value, 1);
}


void DISPLAY_Init(void)
{  
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET); // release CS    
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET); // # LCD POWER OFF

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET); // # RST  
  HAL_Delay(100); //100mS
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET); // # RST
  HAL_Delay(100); //100mS
    
  DISPLAY_WRITE_CMD(0xAE, 1); /*display off*/

  //Set Display Clock Divide Ratio/Oscillator Frequency
  DISPLAY_WRITE_CMD(0xD5, 1); 
  DISPLAY_WRITE_CMD(0x80, 1);

  //Set Multiplex Ratio
  DISPLAY_WRITE_CMD(0xA8, 1); 
  DISPLAY_WRITE_CMD(0x1F, 1); 

  //Set Display Offset
  DISPLAY_WRITE_CMD(0xD3, 1);
  DISPLAY_WRITE_CMD(0x00, 1); 

  // Set Display Start Line
  DISPLAY_WRITE_CMD(0x40, 1);
  
  // Set Charge Pump
  DISPLAY_WRITE_CMD(0x8D, 1);
  DISPLAY_WRITE_CMD(0x14, 1);

  //Set Segment Re-Map
  DISPLAY_WRITE_CMD(0xA1, 1);
  
  //Set COM Output Scan Direction
  DISPLAY_WRITE_CMD(0xC8, 1);
  
  // Set COM Pins Hardware Configuration
  DISPLAY_WRITE_CMD(0xDA, 1);
  DISPLAY_WRITE_CMD(0x00, 1);
  
  //Set Contrast Control
  DISPLAY_WRITE_CMD(0x81, 1);
  DISPLAY_WRITE_CMD(0x8F, 1);
  
  // Set Pre-Charge Period
  DISPLAY_WRITE_CMD(0xD9, 1);
  DISPLAY_WRITE_CMD(0x1F, 1);

  // Set VCOMH Deselect Level
  DISPLAY_WRITE_CMD(0xDB, 1);
  DISPLAY_WRITE_CMD(0x40, 1);

  // Set Entire Display On/Off
  DISPLAY_WRITE_CMD(0xA4, 1);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET); // # LCD POWER  

  ssd1306_Fill(Black);
  // Flush buffer to screen
  ssd1306_UpdateScreen();
    
  DISPLAY_WRITE_CMD(0xAF, 1); /*display ON*/
  HAL_Delay(100); //100mS  
    
  // Set default values for screen object
  CurrentX = 0;
  CurrentY = 0;  
}

//    Draw one pixel in the screenbuffer
//    X => X Coordinate
//    Y => Y Coordinate
//    color => Pixel color
void ssd1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color) {
    if(x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        // Don't write outside the buffer
        return;
    }
        
    // Draw in the right color
    if(color == White) {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
    } else { 
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
    }
}


// Draw 1 char to the screen buffer
// ch       => char om weg te schrijven
// FontHeight
// FontWidth
// color    => Black or White
char ssd1306_WriteChar(char ch, uint8_t FontHeight, uint8_t FontWidth, uint16_t const* fontData, SSD1306_COLOR color) {
    uint32_t i, b, j;
    
    // Check if character is valid
    if (ch < 32 || ch > 126)
        return 0;
    
    // Check remaining space on current line
    if (SSD1306_WIDTH < (CurrentX + FontWidth) ||
        SSD1306_HEIGHT < (CurrentY + FontHeight))
    {
        // Not enough space on current line
        return 0;
    }
    
    // Use the font to write
    for(i = 0; i < FontHeight; i++) {
        b = fontData[(ch - 32) * FontHeight + i];
        for(j = 0; j < FontWidth; j++) {
            if((b << j) & 0x8000)  {
                ssd1306_DrawPixel(CurrentX + j, (CurrentY + i), (SSD1306_COLOR) color);
            } else {
                ssd1306_DrawPixel(CurrentX + j, (CurrentY + i), (SSD1306_COLOR)!color);
            }
        }
    }
    
    // The current space is now taken
    CurrentX += FontWidth;
    
    // Return written char for validation
    return ch;
}

// Write full string to screenbuffer
char ssd1306_WriteString(char* str, uint8_t FontHeight, uint8_t FontWidth, uint16_t const* fontData, SSD1306_COLOR color) {
    // Write until null-byte
    while (*str) {
        if (ssd1306_WriteChar(*str, FontHeight, FontWidth, fontData, color) != *str) {
            // Char could not be written
            return *str;
        }
        
        // Next char
        str++;
    }    
    // Everything ok
    return *str;
}

// Position the cursor
void ssd1306_SetCursor(uint8_t x, uint8_t y) {
    CurrentX = x;
    CurrentY = y;
}

