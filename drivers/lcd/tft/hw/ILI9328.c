/**************************************************************************/
/*! 
    @file     ILI9328.c
    @author   K. Townsend (microBuilder.eu)

    @section  DESCRIPTION

    Driver for ILI9328 240x320 pixel TFT LCD displays.
    
    This driver uses an 8-bit interface and a 16-bit RGB565 colour palette.

    @section  LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2010, microBuilder SARL
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**************************************************************************/
#include "ILI9328.h"
#include "core/systick/systick.h"
#include "drivers/lcd/tft/touchscreen.h"

static volatile lcdOrientation_t lcdOrientation = LCD_ORIENTATION_PORTRAIT;
static lcdProperties_t ili9328Properties = { 240, 320, TRUE, TRUE, TRUE };

/*************************************************/
/* Private Methods                               */
/*************************************************/

/**************************************************************************/
/*! 
    @brief  Causes a brief delay (10 ticks per unit)
*/
/**************************************************************************/
void ili9328Delay(unsigned int t)
{
  unsigned char t1;
  while(t--)
  for ( t1=10; t1 > 0; t1-- )
  {
    __asm("nop");
  }
}

/**************************************************************************/
/*! 
    @brief  Writes the supplied 16-bit command using an 8-bit interface
*/
/**************************************************************************/
void ili9328WriteCmd(uint16_t command) 
{
  // Compiled with -Os on GCC 4.4 this works out to 25 cycles
  // (versus 36 compiled with no optimisations).  I'm not sure it
  // can be improved further, so that means 25 cycles/350nS for
  // continuous writes (cmd, data, data, data, ...) or ~150 cycles/
  // ~2.1uS for a random pixel (Set X [cmd+data], Set Y [cmd+data],
  // Set color [cmd+data]) (times assumes 72MHz clock).

  CLR_CS_CD_SET_RD_WR;  // Saves 18 commands compared to "CLR_CS; CLR_CD; SET_RD; SET_WR;" 
  ILI9328_GPIO2DATA_DATA = (command >> (8 - ILI9328_DATA_OFFSET));
  CLR_WR;
  SET_WR;
  ILI9328_GPIO2DATA_DATA = command << ILI9328_DATA_OFFSET;
  CLR_WR;
  SET_WR_CS;            // Saves 7 commands compared to "SET_WR; SET_CS;"
}

/**************************************************************************/
/*! 
    @brief  Writes the supplied 16-bit data using an 8-bit interface
*/
/**************************************************************************/
void ili9328WriteData(uint16_t data)
{
  CLR_CS_SET_CD_RD_WR;  // Saves 18 commands compared to SET_CD; SET_RD; SET_WR; CLR_CS"
  ILI9328_GPIO2DATA_DATA = (data >> (8 - ILI9328_DATA_OFFSET));
  CLR_WR;
  SET_WR;
  ILI9328_GPIO2DATA_DATA = data << ILI9328_DATA_OFFSET;
  CLR_WR;
  SET_WR_CS;            // Saves 7 commands compared to "SET_WR, SET_CS;"
}

/**************************************************************************/
/*! 
    @brief  Reads a 16-bit value from the 8-bit data bus
*/
/**************************************************************************/
uint16_t ili9328ReadData(void)
{
  // ToDo: Optimise this method!

  uint16_t high, low;
  high = low = 0;
  uint16_t d;

  SET_CD_RD_WR;   // Saves 14 commands compared to "SET_CD; SET_RD; SET_WR"
  CLR_CS;
  
  // set inputs
  ILI9328_GPIO2DATA_SETINPUT;
  CLR_RD;
  ili9328Delay(100);
  high = ILI9328_GPIO2DATA_DATA;  
  high >>= ILI9328_DATA_OFFSET;
  high &= 0xFF;
  SET_RD;
  
  CLR_RD;
  ili9328Delay(100);
  low = ILI9328_GPIO2DATA_DATA;
  low >>= ILI9328_DATA_OFFSET;
  low &=0xFF;
  SET_RD;
  
  SET_CS;
  ILI9328_GPIO2DATA_SETOUTPUT;

  d = high;
  d <<= 8;
  d |= low;
  
  return d;
}

/**************************************************************************/
/*! 
    @brief  Reads a 16-bit value
*/
/**************************************************************************/
uint16_t ili9328Read(uint16_t addr)
{
  ili9328WriteCmd(addr);
  return ili9328ReadData();
}

/**************************************************************************/
/*! 
    @brief  Sends a 16-bit command + 16-bit data
*/
/**************************************************************************/
void ili9328Command(uint16_t command, uint16_t data)
{
  ili9328WriteCmd(command);
  ili9328WriteData(data);
}

/**************************************************************************/
/*! 
    @brief  Returns the 16-bit (4-hexdigit) controller code
*/
/**************************************************************************/
uint16_t ili9328Type(void)
{
  ili9328WriteCmd(ILI9328_COMMANDS_DRIVERCODEREAD);
  return ili9328ReadData();
}

/**************************************************************************/
/*! 
    @brief  Sets the cursor to the specified X/Y position
*/
/**************************************************************************/
void ili9328SetCursor(uint16_t x, uint16_t y)
{
  uint16_t al, ah;
  
  if (lcdOrientation == LCD_ORIENTATION_LANDSCAPE)
  {
    al = y;
    ah = x;
  }
  else
  {
    al = x;
    ah = y;
  }

  ili9328Command(ILI9328_COMMANDS_HORIZONTALGRAMADDRESSSET, al);
  ili9328Command(ILI9328_COMMANDS_VERTICALGRAMADDRESSSET, ah);
}

/**************************************************************************/
/*! 
    @brief  Sends the initialisation sequence to the display controller
*/
/**************************************************************************/
void ili9328InitDisplay(void)
{
  // Clear data line
  GPIO_GPIO2DATA &= ~ILI9328_DATA_MASK;
    
  SET_RD;
  SET_WR;
  SET_CS;
  SET_CD;

  // Reset display
  CLR_RESET;
  ili9328Delay(100);
  SET_RESET;
  ili9328Delay(1000);

  ili9328Command(ILI9328_COMMANDS_DRIVEROUTPUTCONTROL1, 0x0100);  // Driver Output Control Register (R01h)
  ili9328Command(ILI9328_COMMANDS_LCDDRIVINGCONTROL, 0x0700);     // LCD Driving Waveform Control (R02h)
  ili9328Command(ILI9328_COMMANDS_ENTRYMODE, 0x1030);             // Entry Mode (R03h)  
  ili9328Command(ILI9328_COMMANDS_DISPLAYCONTROL2, 0x0302);
  ili9328Command(ILI9328_COMMANDS_DISPLAYCONTROL3, 0x0000);
  ili9328Command(ILI9328_COMMANDS_DISPLAYCONTROL4, 0x0000);       // Fmark On
  ili9328Command(ILI9328_COMMANDS_POWERCONTROL1, 0x0000);         // Power Control 1 (R10h)
  ili9328Command(ILI9328_COMMANDS_POWERCONTROL2, 0x0007);         // Power Control 2 (R11h)
  ili9328Command(ILI9328_COMMANDS_POWERCONTROL3, 0x0000);         // Power Control 3 (R12h)
  ili9328Command(ILI9328_COMMANDS_POWERCONTROL4, 0x0000);         // Power Control 4 (R13h)
  ili9328Delay(1000);  
  ili9328Command(ILI9328_COMMANDS_POWERCONTROL1, 0x14B0);         // Power Control 1 (R10h)  
  ili9328Delay(500);  
  ili9328Command(ILI9328_COMMANDS_POWERCONTROL2, 0x0007);         // Power Control 2 (R11h)  
  ili9328Delay(500);  
  ili9328Command(ILI9328_COMMANDS_POWERCONTROL3, 0x008E);         // Power Control 3 (R12h)
  ili9328Command(ILI9328_COMMANDS_POWERCONTROL4, 0x0C00);         // Power Control 4 (R13h)
  ili9328Command(ILI9328_COMMANDS_POWERCONTROL7, 0x0015);         // NVM read data 2 (R29h)
  ili9328Delay(500);
  ili9328Command(ILI9328_COMMANDS_GAMMACONTROL1, 0x0000);         // Gamma Control 1
  ili9328Command(ILI9328_COMMANDS_GAMMACONTROL2, 0x0107);         // Gamma Control 2
  ili9328Command(ILI9328_COMMANDS_GAMMACONTROL3, 0x0000);         // Gamma Control 3
  ili9328Command(ILI9328_COMMANDS_GAMMACONTROL4, 0x0203);         // Gamma Control 4
  ili9328Command(ILI9328_COMMANDS_GAMMACONTROL5, 0x0402);         // Gamma Control 5
  ili9328Command(ILI9328_COMMANDS_GAMMACONTROL6, 0x0000);         // Gamma Control 6
  ili9328Command(ILI9328_COMMANDS_GAMMACONTROL7, 0x0207);         // Gamma Control 7
  ili9328Command(ILI9328_COMMANDS_GAMMACONTROL8, 0x0000);         // Gamma Control 8
  ili9328Command(ILI9328_COMMANDS_GAMMACONTROL9, 0x0203);         // Gamma Control 9
  ili9328Command(ILI9328_COMMANDS_GAMMACONTROL10, 0x0403);        // Gamma Control 10
  ili9328Command(ILI9328_COMMANDS_HORIZONTALADDRESSSTARTPOSITION, 0x0000);                      // Window Horizontal RAM Address Start (R50h)
  ili9328Command(ILI9328_COMMANDS_HORIZONTALADDRESSENDPOSITION, ili9328Properties.width - 1);   // Window Horizontal RAM Address End (R51h)
  ili9328Command(ILI9328_COMMANDS_VERTICALADDRESSSTARTPOSITION, 0X0000);                        // Window Vertical RAM Address Start (R52h)
  ili9328Command(ILI9328_COMMANDS_VERTICALADDRESSENDPOSITION, ili9328Properties.height - 1);    // Window Vertical RAM Address End (R53h)
  ili9328Command(ILI9328_COMMANDS_DRIVEROUTPUTCONTROL2, 0xa700);    // Driver Output Control (R60h)
  ili9328Command(ILI9328_COMMANDS_BASEIMAGEDISPLAYCONTROL, 0x0003); // Driver Output Control (R61h) - enable VLE
  ili9328Command(ILI9328_COMMANDS_PANELINTERFACECONTROL1, 0X0010);  // Panel Interface Control 1 (R90h)

  // Display On
  ili9328Command(ILI9328_COMMANDS_DISPLAYCONTROL1, 0x0133);     // Display Control (R07h)
  ili9328Delay(500);
  ili9328WriteCmd(ILI9328_COMMANDS_WRITEDATATOGRAM);
}

/**************************************************************************/
/*! 
    @brief  Sets the cursor to the home position (0,0)
*/
/**************************************************************************/
void ili9328Home(void)
{
  ili9328SetCursor(0, 0);
  ili9328WriteCmd(ILI9328_COMMANDS_WRITEDATATOGRAM);            // Write Data to GRAM (R22h)
}

/**************************************************************************/
/*! 
    @brief  Sets the window confines
*/
/**************************************************************************/
void ili9328SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  ili9328Command(ILI9328_COMMANDS_HORIZONTALADDRESSSTARTPOSITION, x0);
  ili9328Command(ILI9328_COMMANDS_HORIZONTALADDRESSENDPOSITION, x1);
  ili9328Command(ILI9328_COMMANDS_VERTICALADDRESSSTARTPOSITION, y0);
  ili9328Command(ILI9328_COMMANDS_VERTICALADDRESSENDPOSITION, y1);
  ili9328SetCursor(x0, y0);
}

/*************************************************/
/* Public Methods                                */
/*************************************************/

/**************************************************************************/
/*! 
    @brief  Configures any pins or HW and initialises the LCD controller
*/
/**************************************************************************/
void lcdInit(void)
{
  // Set control line pins to output
  gpioSetDir(ILI9328_CS_PORT, ILI9328_CS_PIN, 1);
  gpioSetDir(ILI9328_CD_PORT, ILI9328_CD_PIN, 1);
  gpioSetDir(ILI9328_WR_PORT, ILI9328_WR_PIN, 1);
  gpioSetDir(ILI9328_RD_PORT, ILI9328_RD_PIN, 1);
  
  // Set data port pins to output
  ILI9328_GPIO2DATA_SETOUTPUT;

  // Disable pullups
  ILI9328_DISABLEPULLUPS();
  
  // Set backlight pin to output and turn it on
  gpioSetDir(ILI9328_BL_PORT, ILI9328_BL_PIN, 1);      // set to output
  lcdBacklight(TRUE);

  // Set reset pin to output
  gpioSetDir(ILI9328_RES_PORT, ILI9328_RES_PIN, 1);    // Set to output
  gpioSetValue(ILI9328_RES_PORT, ILI9328_RES_PIN, 0);  // Low to reset
  systickDelay(50);
  gpioSetValue(ILI9328_RES_PORT, ILI9328_RES_PIN, 1);  // High to exit

  // Initialize the display
  ili9328InitDisplay();

  systickDelay(50);

  // Set lcd to default orientation
  lcdSetOrientation(lcdOrientation);

  // Fill black
  lcdFillRGB(COLOR_BLACK);
  
  // Initialise the touch screen (and calibrate if necessary)
  tsInit();
}

/**************************************************************************/
/*! 
    @brief  Enables or disables the LCD backlight
*/
/**************************************************************************/
void lcdBacklight(bool state)
{
  // Set the backlight
  gpioSetValue(ILI9328_BL_PORT, ILI9328_BL_PIN, state ? 0 : 1);
}

/**************************************************************************/
/*! 
    @brief  Renders a simple test pattern on the LCD
*/
/**************************************************************************/
void lcdTest(void)
{
  uint32_t i,j;
  ili9328Home();
  
  for(i=0;i<320;i++)
  {
    for(j=0;j<240;j++)
    {
      if(i>279)ili9328WriteData(COLOR_WHITE);
      else if(i>239)ili9328WriteData(COLOR_BLUE);
      else if(i>199)ili9328WriteData(COLOR_GREEN);
      else if(i>159)ili9328WriteData(COLOR_CYAN);
      else if(i>119)ili9328WriteData(COLOR_RED);
      else if(i>79)ili9328WriteData(COLOR_MAGENTA);
      else if(i>39)ili9328WriteData(COLOR_YELLOW);
      else ili9328WriteData(COLOR_BLACK);
    }
  }
}

/**************************************************************************/
/*! 
    @brief  Fills the LCD with the specified 16-bit color
*/
/**************************************************************************/
void lcdFillRGB(uint16_t data)
{
  unsigned int i;
  ili9328Home();
  
  uint32_t pixels = 320*240;
  for ( i=0; i < pixels; i++ )
  {
    ili9328WriteData(data);
  } 
}

/**************************************************************************/
/*! 
    @brief  Draws a single pixel at the specified X/Y location
*/
/**************************************************************************/
void lcdDrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
  ili9328SetCursor(x, y);
  ili9328WriteCmd(ILI9328_COMMANDS_WRITEDATATOGRAM);  // Write Data to GRAM (R22h)
  ili9328WriteData(color);
}

/**************************************************************************/
/*! 
    @brief  Draws an array of consecutive RGB565 pixels (much
            faster than addressing each pixel individually)
*/
/**************************************************************************/
void lcdDrawPixels(uint16_t x, uint16_t y, uint16_t *data, uint32_t len)
{
  uint32_t i = 0;
  ili9328SetCursor(x, y);
  ili9328WriteCmd(ILI9328_COMMANDS_WRITEDATATOGRAM);
  do
  {
    ili9328WriteData(data[i]);
    i++;
  } while (i<len);
}

/**************************************************************************/
/*! 
    @brief  Optimised routine to draw a horizontal line faster than
            setting individual pixels
*/
/**************************************************************************/
void lcdDrawHLine(uint16_t x0, uint16_t x1, uint16_t y, uint16_t color)
{
  // Allows for slightly better performance than setting individual pixels
  uint16_t x, pixels;

  if (x1 < x0)
  {
    // Switch x1 and x0
    x = x1;
    x1 = x0;
    x0 = x;
  }

  // Check limits
  if (x1 >= lcdGetWidth())
  {
    x1 = lcdGetWidth() - 1;
  }
  if (x0 >= lcdGetWidth())
  {
    x0 = lcdGetWidth() - 1;
  }

  ili9328SetCursor(x0, y);
  ili9328WriteCmd(ILI9328_COMMANDS_WRITEDATATOGRAM);  // Write Data to GRAM (R22h)
  for (pixels = 0; pixels < x1 - x0 + 1; pixels++)
  {
    ili9328WriteData(color);
  }
}

/**************************************************************************/
/*! 
    @brief  Optimised routine to draw a vertical line faster than
            setting individual pixels
*/
/**************************************************************************/
void lcdDrawVLine(uint16_t x, uint16_t y0, uint16_t y1, uint16_t color)
{
  lcdOrientation_t orientation = lcdOrientation;

  // Switch orientation
  lcdSetOrientation(orientation == LCD_ORIENTATION_PORTRAIT ? LCD_ORIENTATION_LANDSCAPE : LCD_ORIENTATION_PORTRAIT);

  // Draw horizontal line like usual
  lcdDrawHLine(y0, y1, lcdGetHeight() - (x + 1), color);

  // Switch orientation back
  lcdSetOrientation(orientation);
}

/**************************************************************************/
/*! 
    @brief  Gets the 16-bit color of the pixel at the specified location
*/
/**************************************************************************/
uint16_t lcdGetPixel(uint16_t x, uint16_t y)
{
  uint16_t preFetch = 0;

  ili9328SetCursor(x, y);
  ili9328WriteCmd(ILI9328_COMMANDS_WRITEDATATOGRAM);
  preFetch = ili9328ReadData();

  // Eeek ... why does this need to be done twice for a proper value?!?
  ili9328SetCursor(x, y);
  ili9328WriteCmd(ILI9328_COMMANDS_WRITEDATATOGRAM);
  return ili9328ReadData();
}

/**************************************************************************/
/*! 
    @brief  Sets the LCD orientation to horizontal and vertical
*/
/**************************************************************************/
void lcdSetOrientation(lcdOrientation_t orientation)
{
  uint16_t entryMode = 0x1030;
  uint16_t outputControl = 0x0100;

  switch (orientation)
  {
    case LCD_ORIENTATION_PORTRAIT:
      entryMode = 0x1030;
      outputControl = 0x0100;
      break;
    case LCD_ORIENTATION_LANDSCAPE:
      entryMode = 0x1028;
      outputControl = 0x0000;
      break;
  }

  ili9328Command(ILI9328_COMMANDS_ENTRYMODE, entryMode);
  ili9328Command(ILI9328_COMMANDS_DRIVEROUTPUTCONTROL1, outputControl);
  lcdOrientation = orientation;

  ili9328SetCursor(0, 0);
}

/**************************************************************************/
/*! 
    @brief  Gets the current screen orientation (horizontal or vertical)
*/
/**************************************************************************/
lcdOrientation_t lcdGetOrientation(void)
{
  return lcdOrientation;
}

/**************************************************************************/
/*! 
    @brief  Gets the width in pixels of the LCD screen (varies depending
            on the current screen orientation)
*/
/**************************************************************************/
uint16_t lcdGetWidth(void)
{
  switch (lcdOrientation) 
  {
    case LCD_ORIENTATION_PORTRAIT:
      return ili9328Properties.width;
      break;
    case LCD_ORIENTATION_LANDSCAPE:
    default:
      return ili9328Properties.height;
  }
}

/**************************************************************************/
/*! 
    @brief  Gets the height in pixels of the LCD screen (varies depending
            on the current screen orientation)
*/
/**************************************************************************/
uint16_t lcdGetHeight(void)
{
  switch (lcdOrientation) 
  {
    case LCD_ORIENTATION_PORTRAIT:
      return ili9328Properties.height;
      break;
    case LCD_ORIENTATION_LANDSCAPE:
    default:
      return ili9328Properties.width;
  }
}

/**************************************************************************/
/*! 
    @brief  Scrolls the contents of the LCD screen vertically the
            specified number of pixels using a HW optimised routine
*/
/**************************************************************************/
void lcdScroll(int16_t pixels, uint16_t fillColor)
{
  int16_t y = pixels;
  while (y < 0)
    y += 320;
  while (y >= 320)
    y -= 320;
  ili9328WriteCmd(ILI9328_COMMANDS_VERTICALSCROLLCONTROL);
  ili9328WriteData(y);
}

/**************************************************************************/
/*! 
    @brief  Gets the controller's 16-bit (4 hexdigit) ID
*/
/**************************************************************************/
uint16_t lcdGetControllerID(void)
{
  return ili9328Type();
}

/**************************************************************************/
/*! 
    @brief  Returns the LCDs 'lcdProperties_t' that describes the LCDs
            generic capabilities and dimensions
*/
/**************************************************************************/
lcdProperties_t lcdGetProperties(void)
{
    return ili9328Properties;
}
