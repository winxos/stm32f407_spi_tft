/* USER CODE END 0 */
#include "st7735s.h"
#include "stm32f4xx_hal.h"
#include "spi.h"
#include "font.h"
struct ST7735_cmdBuf {
  uint8_t command;   // ST7735 command byte
  uint8_t delay;     // ms delay after
  uint8_t len;       // length of parameter data
  uint8_t data[16];  // parameter data
};
#define W 128
#define H 160
#define WxH 20480
u16 color=0x00f0;
u16 sbuf[WxH]={0};
static const struct ST7735_cmdBuf initializers[] = {
  // SWRESET Software reset 
  { 0x01, 150, 0, 0},
  // SLPOUT Leave sleep mode
  { 0x11,  150, 0, 0},
  // FRMCTR1, FRMCTR2 Frame Rate configuration -- Normal mode, idle
  // frame rate = fosc / (1 x 2 + 40) * (LINE + 2C + 2D) 
  { 0xB1, 0, 3, { 0x01, 0x2C, 0x2D }},
  { 0xB2, 0, 3, { 0x01, 0x2C, 0x2D }},
  // FRMCTR3 Frame Rate configureation -- partial mode
  { 0xB3, 0, 6, { 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D }},
  // INVCTR Display inversion (no inversion)
  { 0xB4,  0, 1, { 0x07 }},
  // PWCTR1 Power control -4.6V, Auto mode
  { 0xC0,  0, 3, { 0xA2, 0x02, 0x84}},
  // PWCTR2 Power control VGH25 2.4C, VGSEL -10, VGH = 3 * AVDD
  { 0xC1,  0, 1, { 0xC5}},
  // PWCTR3 Power control, opamp current smal, boost frequency
  { 0xC2,  0, 2, { 0x0A, 0x00 }},
  // PWCTR4 Power control, BLK/2, opamp current small and medium low
  { 0xC3,  0, 2, { 0x8A, 0x2A}},
  // PWRCTR5, VMCTR1 Power control
  { 0xC4,  0, 2, { 0x8A, 0xEE}},
  { 0xC5,  0, 1, { 0x0E }},
  // INVOFF Don't invert display
  { 0x20,  0, 0, 0},
  // Memory access directions. row address/col address, bottom to top refesh (10.1.27)
  { 0x36,  0, 1, {0x00}},
  // Color mode 16 bit (10.1.30
  { 0x3a,   0, 1, {0x05}},
  // Column address set 0..127 
  { 0x2a,   0, 4, {0x00, 0x00, 0x00, 0x7F }},
  // Row address set 0..159
  { 0x2b,   0, 4, {0x00, 0x00, 0x00, 0x9F }},
  // GMCTRP1 Gamma correction
  { 0xE0, 0, 16, {0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
			    0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10 }},
  // GMCTRP2 Gamma Polarity corrction
  { 0xE1, 0, 16, {0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
			    0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10 }},
  // DISPON Display on
  { 0x29, 100, 0, 0},
  // NORON Normal on
  { 0x13,  10, 0, 0},
  // End
  { 0, 0, 0, 0}
};

void lcd_write(u8 dc, u8 *data, int n)
{
	HAL_GPIO_WritePin(TFT_A0_GPIO_Port,TFT_A0_Pin,(GPIO_PinState)dc);
	HAL_GPIO_WritePin(SPI2_CS_GPIO_Port,SPI2_CS_Pin,GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2,data,n,50000);
	//HAL_SPI_Transmit_DMA(&hspi2,data,n);
	HAL_GPIO_WritePin(SPI2_CS_GPIO_Port,SPI2_CS_Pin,GPIO_PIN_SET);
}
void lcd_set_window(u16 xs, u16 ys,u16 xe,u16 ye)
{
	u8 xram[]={0x2a,xs>>8,xs&0xff,xe>>8,xe&0xff};
	u8 yram[]={0x2b,ys>>8,ys&0xff,ye>>8,ye&0xff};
	lcd_write(0,&xram[0],1); 
	lcd_write(1,(u8*)&xram[1],4); 
	lcd_write(0,&yram[0],1); 
	lcd_write(1,(u8*)&yram[1],4); //set window
}
void lcd_show()
{
	u8 gram=0x2c;
	lcd_write(0,&gram,1); 
	lcd_write(1,(u8*)&sbuf,WxH*2);
}

void set_color(u16 c)
{
	color=c;
}
void lcd_fill(u16 color)
{
	for(u32 i=0;i<WxH;i++)
	{
		sbuf[i]=color;
	}
}
void lcd_point(u16 x,u16 y)
{
	sbuf[x+y*W]=color;
}
void lcd_clear()
{
	lcd_fill(0);
}
void lcd_init()
{
	HAL_GPIO_WritePin(SPI2_CS_GPIO_Port,SPI2_CS_Pin,GPIO_PIN_SET);
	HAL_GPIO_WritePin(TFT_RESET_GPIO_Port,TFT_RESET_Pin,GPIO_PIN_SET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(TFT_RESET_GPIO_Port,TFT_RESET_Pin,GPIO_PIN_RESET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(TFT_RESET_GPIO_Port,TFT_RESET_Pin,GPIO_PIN_SET);
	const struct ST7735_cmdBuf *cmd;
	for (cmd = initializers; cmd->command; cmd++)
	{
		lcd_write(0, (u8*)&(cmd->command), 1);
		if (cmd->len)
		{
			lcd_write(1, (u8*)cmd->data, cmd->len);
		}
		if (cmd->delay)
		{
			HAL_Delay(cmd->delay);
		}
	}
	lcd_clear();
}
#define font font1608
void lcd_char(char cc,u16 x,u16 y)
{
  for(u8 r=0;r<16;r++)
	{
		for(u8 c=0;c<8;c++)
		{
			if(font[cc*16+r]&(1<<(7-c)))
			{
				set_color(0x000f);
			}
			else
			{
				set_color(0);
			}
			lcd_point(x+c,y+r);
		}
	}
}
void lcd_str(char * str,u16 x,u16 y)
{
	u16 r=0;
	while(*str)
	{
		lcd_char(*str++,x+r,y);
		r+=8;
	}
}
void lcd_num(long n,u16 x,u16 y)
{
	char buf[12];
	sprintf(buf,"%ld",n);
	lcd_str(buf,x,y);
}
void lcd_box(u8 x,u8 y,u8 w,u8 h)
{
	for(u8 i=x;i<x+w;i++)
	{
		for(u8 j=y;j<y+h;j++)
		{
			lcd_point(i,j);
		}
	}
}