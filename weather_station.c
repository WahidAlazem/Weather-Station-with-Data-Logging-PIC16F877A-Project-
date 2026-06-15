#include <xc.h>
#include <stdio.h>

#pragma config FOSC = HS
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config BOREN = OFF
#pragma config LVP = OFF
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

#define _XTAL_FREQ 20000000

#define RS RB2
#define EN RB3
#define D4 RD0
#define D5 RD1
#define D6 RD2
#define D7 RD3

#define DHT11 RA0
#define DHT11_DIR TRISA0

#define LED_GREEN RB0
#define LED_RED   RB1

#define BUTTON RB4

#define DS1307_WRITE 0xD0
#define DS1307_READ  0xD1

unsigned char temperature = 0;
unsigned char humidity = 0;
unsigned int light = 0;

unsigned char sec, min, hour;
unsigned char day, date, month, year;

unsigned char screen = 0;
unsigned char last_button = 1;

void LCD_Enable()
{
    EN = 1;
    __delay_us(5);
    EN = 0;
    __delay_us(100);
}

void LCD_Send4Bit(unsigned char data)
{
    D4 = (data >> 0) & 1;
    D5 = (data >> 1) & 1;
    D6 = (data >> 2) & 1;
    D7 = (data >> 3) & 1;

    LCD_Enable();
}

void LCD_Command(unsigned char cmd)
{
    RS = 0;

    LCD_Send4Bit(cmd >> 4);
    LCD_Send4Bit(cmd & 0x0F);

    __delay_ms(2);
}

void LCD_Char(unsigned char data)
{
    RS = 1;

    LCD_Send4Bit(data >> 4);
    LCD_Send4Bit(data & 0x0F);

    __delay_ms(2);
}

void LCD_String(const char *str)
{
    while(*str)
    {
        LCD_Char(*str++);
    }
}

void LCD_SetCursor(unsigned char row, unsigned char col)
{
    if(row == 1)
        LCD_Command(0x80 + col - 1);
    else
        LCD_Command(0xC0 + col - 1);
}

void LCD_Clear()
{
    LCD_Command(0x01);

    __delay_ms(2);
}

void LCD_Init()
{
    TRISB2 = 0;
    TRISB3 = 0;

    TRISD0 = 0;
    TRISD1 = 0;
    TRISD2 = 0;
    TRISD3 = 0;

    __delay_ms(100);

    RS = 0;
    EN = 0;

    LCD_Send4Bit(0x03);
    __delay_ms(5);

    LCD_Send4Bit(0x03);
    __delay_ms(5);

    LCD_Send4Bit(0x03);
    __delay_ms(5);

    LCD_Send4Bit(0x02);

    LCD_Command(0x28);
    LCD_Command(0x0C);
    LCD_Command(0x06);
    LCD_Command(0x01);

    __delay_ms(5);
}

void UART_Init()
{
    TRISC6 = 0;
    TRISC7 = 1;

    SPBRG = 129;

    TXSTA = 0x24;
    RCSTA = 0x90;
}

void UART_Write(char data)
{
    while(TXIF == 0);

    TXREG = data;
}

void UART_String(const char *s)
{
    while(*s)
    {
        UART_Write(*s++);
    }
}

void ADC_Init_RA1()
{
    ADCON1 = 0x80;
    ADCON0 = 0x49;

    TRISA1 = 1;
}

unsigned int ADC_Read_RA1()
{
    ADC_Init_RA1();

    __delay_us(30);

    GO_nDONE = 1;

    while(GO_nDONE);

    return ((unsigned int)ADRESH << 8) + ADRESL;
}

unsigned char DHT11_ReadByte()
{
    unsigned char i;
    unsigned char data = 0;

    for(i = 0; i < 8; i++)
    {
        while(!DHT11);

        __delay_us(30);

        if(DHT11)
            data |= (1 << (7 - i));

        while(DHT11);
    }

    return data;
}

void DHT11_Start()
{
    ADCON1 = 0x06;

    DHT11_DIR = 0;

    RA0 = 0;

    __delay_ms(20);

    RA0 = 1;

    __delay_us(30);

    DHT11_DIR = 1;
}

unsigned char DHT11_CheckResponse()
{
    unsigned char response = 0;

    __delay_us(40);

    if(!DHT11)
    {
        __delay_us(80);

        if(DHT11)
            response = 1;

        while(DHT11);
    }

    return response;
}

void DHT11_ReadData()
{
    unsigned char hum_int, hum_dec;
    unsigned char temp_int, temp_dec;
    unsigned char checksum;

    DHT11_Start();

    if(DHT11_CheckResponse())
    {
        hum_int  = DHT11_ReadByte();
        hum_dec  = DHT11_ReadByte();

        temp_int = DHT11_ReadByte();
        temp_dec = DHT11_ReadByte();

        checksum = DHT11_ReadByte();

        if(checksum == (unsigned char)(hum_int + hum_dec + temp_int + temp_dec))
        {
            humidity = hum_int;
            temperature = temp_int;
        }
    }
}

void I2C_Init()
{
    TRISC3 = 1;
    TRISC4 = 1;

    SSPCON = 0x28;
    SSPCON2 = 0x00;

    SSPADD = 49;

    SSPSTAT = 0x00;
}

void I2C_Wait()
{
    while((SSPCON2 & 0x1F) || (SSPSTAT & 0x04));
}

void I2C_Start()
{
    I2C_Wait();

    SEN = 1;

    while(SEN);
}

void I2C_Restart()
{
    I2C_Wait();

    RSEN = 1;

    while(RSEN);
}

void I2C_Stop()
{
    I2C_Wait();

    PEN = 1;

    while(PEN);
}

void I2C_Write(unsigned char data)
{
    I2C_Wait();

    SSPBUF = data;

    while(!SSPIF);

    SSPIF = 0;
}

unsigned char I2C_Read(unsigned char ack)
{
    unsigned char data;

    I2C_Wait();

    RCEN = 1;

    while(!BF);

    data = SSPBUF;

    I2C_Wait();

    ACKDT = ack ? 0 : 1;

    ACKEN = 1;

    while(ACKEN);

    return data;
}

//================ RTC =================

unsigned char BCD_To_Dec(unsigned char val)
{
    return ((val >> 4) * 10) + (val & 0x0F);
}

unsigned char Dec_To_BCD(unsigned char val)
{
    return ((val / 10) << 4) + (val % 10);
}

void DS1307_SetTime()
{
    I2C_Start();

    I2C_Write(DS1307_WRITE);
    I2C_Write(0x00);

    I2C_Write(Dec_To_BCD(0));    // Seconds
    I2C_Write(Dec_To_BCD(30)); 
    I2C_Write(Dec_To_BCD(20));// Hours

    I2C_Write(Dec_To_BCD(1));    // Monday
    I2C_Write(Dec_To_BCD(25));   // Date
    I2C_Write(Dec_To_BCD(5));    // Month
    I2C_Write(Dec_To_BCD(26));   // Year

    I2C_Stop();
}

void DS1307_ReadTime()
{
    I2C_Start();

    I2C_Write(DS1307_WRITE);
    I2C_Write(0x00);

    I2C_Restart();

    I2C_Write(DS1307_READ);

    sec   = BCD_To_Dec(I2C_Read(1) & 0x7F);
    min   = BCD_To_Dec(I2C_Read(1));
    hour  = BCD_To_Dec(I2C_Read(1) & 0x3F);

    day   = BCD_To_Dec(I2C_Read(1));

    date  = BCD_To_Dec(I2C_Read(1));
    month = BCD_To_Dec(I2C_Read(1));
    year  = BCD_To_Dec(I2C_Read(0));

    I2C_Stop();
}

//================ BUTTON =================

void Check_Button()
{
    unsigned char current_button = BUTTON;

    if(last_button == 1 && current_button == 0)
    {
        __delay_ms(40);

        if(BUTTON == 0)
        {
            screen = !screen;

            LCD_Clear();
        }
    }

    last_button = current_button;
}

//================ MAIN =================

void main()
{
    char txt[32];
    unsigned char i;

    ADCON1 = 0x06;

    TRISB0 = 0;
    TRISB1 = 0;

    TRISB4 = 1;

    LED_GREEN = 0;
    LED_RED = 0;

    LCD_Init();

    I2C_Init();

    UART_Init();

    LCD_SetCursor(1,1);
    LCD_String("Weather System");

    LCD_SetCursor(2,1);
    LCD_String("Starting...");

    UART_String("Weather System Started\r\n");

    __delay_ms(2000);

    LCD_Clear();

    while(1)
    {
        DHT11_ReadData();

        light = ADC_Read_RA1();

        DS1307_ReadTime();

        Check_Button();

        if(screen == 0)
        {
            LCD_SetCursor(1,1);

            LCD_String("Weather Station");

            LCD_SetCursor(2,1);

            sprintf(txt,"T:%02u H:%02u L:%04u",
                    temperature,
                    humidity,
                    light);

            LCD_String(txt);
        }
        else
        {
            LCD_SetCursor(1,1);

            sprintf(txt,"Date:%02u/%02u/%02u",
                    date,
                    month,
                    year);

            LCD_String(txt);

            LCD_SetCursor(2,1);

            sprintf(txt,"Time:%02u:%02u:%02u",
                    hour,
                    min,
                    sec);

            LCD_String(txt);
        }

        if(temperature > 30)
        {
            LED_RED = 0;
            LED_GREEN = 1;
        }
        else
        {
            LED_RED = 1;
            LED_GREEN = 0;
        }

        UART_String("Weather Station\r\n");

        sprintf(txt,"T:%02u H:%02u L:%04u\r\n",
                temperature,
                humidity,
                light);

        UART_String(txt);

        sprintf(txt,"Date:%02u/%02u/%02u Time:%02u:%02u:%02u\r\n\r\n",
                date,
                month,
                year,
                hour,
                min,
                sec);

        UART_String(txt);

        for(i = 0; i < 10; i++)
        {
            Check_Button();

            __delay_ms(50);
        }
    }
}
