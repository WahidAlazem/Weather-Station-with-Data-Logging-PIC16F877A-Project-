#include <xc.h>
#include <stdio.h>

// PIC Configuration Bits
#pragma config FOSC = HS
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config BOREN = OFF
#pragma config LVP = OFF
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

// Crystal frequency = 20 MHz
#define _XTAL_FREQ 20000000

// LCD pins
#define RS RB2
#define EN RB3
#define D4 RD0
#define D5 RD1
#define D6 RD2
#define D7 RD3

// DHT11 sensor pin
#define DHT11 RA0
#define DHT11_DIR TRISA0

// LED pins
#define LED_GREEN RB1
#define LED_RED   RB0

// Button pin
#define BUTTON RB4

// DS1307 RTC I2C address
#define DS1307_WRITE 0xD0
#define DS1307_READ  0xD1

// Sensor variables
unsigned char temperature = 0;
unsigned char humidity = 0;
unsigned int light = 0;

// RTC time/date variables
unsigned char sec, min, hour;
unsigned char day, date, month, year;

// Screen control variables
unsigned char screen = 0;
unsigned char last_button = 1;

// LCD enable pulse
void LCD_Enable()
{
    EN = 1;
    __delay_us(5);
    EN = 0;
    __delay_us(100);
}

// Send 4 bits to LCD data pins
void LCD_Send4Bit(unsigned char data)
{
    D4 = (data >> 0) & 1;
    D5 = (data >> 1) & 1;
    D6 = (data >> 2) & 1;
    D7 = (data >> 3) & 1;

    LCD_Enable();
}

// Send command to LCD
void LCD_Command(unsigned char cmd)
{
    RS = 0;                  // Command mode

    LCD_Send4Bit(cmd >> 4);  // Send high nibble
    LCD_Send4Bit(cmd & 0x0F);// Send low nibble

    __delay_ms(2);
}

// Send character to LCD
void LCD_Char(unsigned char data)
{
    RS = 1;                  // Data mode

    LCD_Send4Bit(data >> 4);
    LCD_Send4Bit(data & 0x0F);

    __delay_ms(2);
}

// Print string on LCD
void LCD_String(const char *str)
{
    while(*str)
    {
        LCD_Char(*str++);
    }
}

// Set LCD cursor position
void LCD_SetCursor(unsigned char row, unsigned char col)
{
    if(row == 1)
        LCD_Command(0x80 + col - 1);
    else
        LCD_Command(0xC0 + col - 1);
}

// Clear LCD
void LCD_Clear()
{
    LCD_Command(0x01);
    __delay_ms(2);
}

// Initialize LCD in 4-bit mode
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

    LCD_Send4Bit(0x02);     // 4-bit mode

    LCD_Command(0x28);      // 4-bit, 2 lines
    LCD_Command(0x0C);      // Display ON, cursor OFF
    LCD_Command(0x06);      // Auto increment cursor
    LCD_Command(0x01);      // Clear display

    __delay_ms(5);
}

// Initialize UART
void UART_Init()
{
    TRISC6 = 0;             // TX pin output
    TRISC7 = 1;             // RX pin input

    SPBRG = 129;            // Baud rate 9600 for 20 MHz

    TXSTA = 0x24;           // Enable TX, high speed mode
    RCSTA = 0x90;           // Enable serial port and receiver
}

// Send one character by UART
void UART_Write(char data)
{
    while(TXIF == 0);       // Wait until TX buffer is empty

    TXREG = data;
}

// Send string by UART
void UART_String(const char *s)
{
    while(*s)
    {
        UART_Write(*s++);
    }
}

// Initialize ADC on RA1
void ADC_Init_RA1()
{
    ADCON1 = 0x80;          // Right justified, analog input enabled
    ADCON0 = 0x49;          // Select channel RA1 and turn ADC ON

    TRISA1 = 1;             // RA1 input
}

// Read ADC value from RA1
unsigned int ADC_Read_RA1()
{
    ADC_Init_RA1();

    __delay_us(30);         // Acquisition delay

    GO_nDONE = 1;           // Start conversion

    while(GO_nDONE);        // Wait until conversion finishes

    return ((unsigned int)ADRESH << 8) + ADRESL;
}

// Read one byte from DHT11
unsigned char DHT11_ReadByte()
{
    unsigned char i;
    unsigned char data = 0;

    for(i = 0; i < 8; i++)
    {
        while(!DHT11);      // Wait for high signal

        __delay_us(30);     // Check signal length

        if(DHT11)
            data |= (1 << (7 - i));

        while(DHT11);       // Wait until signal becomes low
    }

    return data;
}

// Send start signal to DHT11
void DHT11_Start()
{
    ADCON1 = 0x06;          // Make RA0 digital

    DHT11_DIR = 0;          // RA0 output

    RA0 = 0;                // Pull data line low

    __delay_ms(20);         // Start signal delay

    RA0 = 1;                // Pull data line high

    __delay_us(30);

    DHT11_DIR = 1;          // RA0 input
}

// Check DHT11 response
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

// Read temperature and humidity from DHT11
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

        // Check if received data is correct
        if(checksum == (unsigned char)(hum_int + hum_dec + temp_int + temp_dec))
        {
            humidity = hum_int;
            temperature = temp_int;
        }
    }
}

// Initialize I2C module
void I2C_Init()
{
    TRISC3 = 1;             // SCL input
    TRISC4 = 1;             // SDA input

    SSPCON = 0x28;          // Enable MSSP in I2C master mode
    SSPCON2 = 0x00;

    SSPADD = 49;            // 100 kHz I2C speed with 20 MHz

    SSPSTAT = 0x00;
}

// Wait until I2C is idle
void I2C_Wait()
{
    while((SSPCON2 & 0x1F) || (SSPSTAT & 0x04));
}

// I2C start condition
void I2C_Start()
{
    I2C_Wait();

    SEN = 1;

    while(SEN);
}

// I2C repeated start condition
void I2C_Restart()
{
    I2C_Wait();

    RSEN = 1;

    while(RSEN);
}

// I2C stop condition
void I2C_Stop()
{
    I2C_Wait();

    PEN = 1;

    while(PEN);
}

// Write byte to I2C bus
void I2C_Write(unsigned char data)
{
    I2C_Wait();

    SSPBUF = data;

    while(!SSPIF);

    SSPIF = 0;
}

// Read byte from I2C bus
unsigned char I2C_Read(unsigned char ack)
{
    unsigned char data;

    I2C_Wait();

    RCEN = 1;               // Enable receive mode

    while(!BF);             // Wait until buffer is full

    data = SSPBUF;

    I2C_Wait();

    ACKDT = ack ? 0 : 1;    // ACK if ack=1, NACK if ack=0

    ACKEN = 1;              // Send ACK/NACK

    while(ACKEN);

    return data;
}

// Convert BCD value to decimal
unsigned char BCD_To_Dec(unsigned char val)
{
    return ((val >> 4) * 10) + (val & 0x0F);
}

// Convert decimal value to BCD
unsigned char Dec_To_BCD(unsigned char val)
{
    return ((val / 10) << 4) + (val % 10);
}

// Set time and date in DS1307 RTC
void DS1307_SetTime()
{
    I2C_Start();

    I2C_Write(DS1307_WRITE);
    I2C_Write(0x00);

    I2C_Write(Dec_To_BCD(0));    // Seconds
    I2C_Write(Dec_To_BCD(30));   // Minutes
    I2C_Write(Dec_To_BCD(20));   // Hours

    I2C_Write(Dec_To_BCD(1));    // Day
    I2C_Write(Dec_To_BCD(25));   // Date
    I2C_Write(Dec_To_BCD(5));    // Month
    I2C_Write(Dec_To_BCD(26));   // Year

    I2C_Stop();
}

// Read time and date from DS1307 RTC
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
    year  = BCD_To_Dec(I2C_Read(0)); // Last byte, send NACK

    I2C_Stop();
}

// Check button press and switch LCD screen
void Check_Button()
{
    unsigned char current_button = BUTTON;

    // Detect falling edge: button changed from 1 to 0
    if(last_button == 1 && current_button == 0)
    {
        __delay_ms(40);     // Debounce delay

        if(BUTTON == 0)
        {
            screen = !screen; // Change screen

            LCD_Clear();
        }
    }

    last_button = current_button;
}

// Main function
void main()
{
    char txt[32];
    unsigned char i;

    ADCON1 = 0x06;          // Make pins digital first

    TRISB0 = 0;             // Red LED output
    TRISB1 = 0;             // Green LED output

    TRISB4 = 1;             // Button input

    LED_GREEN = 0;
    LED_RED = 0;

    LCD_Init();             // Initialize LCD

    I2C_Init();             // Initialize I2C

    UART_Init();            // Initialize UART

    // Startup message on LCD
    LCD_SetCursor(1,1);
    LCD_String("Weather System");

    LCD_SetCursor(2,1);
    LCD_String("Starting...");

    // Startup message on serial monitor
    UART_String("Weather System Started\r\n");

    __delay_ms(2000);

    LCD_Clear();

    while(1)
    {
        DHT11_ReadData();   // Read temperature and humidity

        light = ADC_Read_RA1(); // Read light sensor value

        DS1307_ReadTime();  // Read date and time

        Check_Button();     // Check if button is pressed

        if(screen == 0)
        {
            // Weather screen
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
            // Date and time screen
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

        // LED control based on temperature
        if(temperature > 30)
        {
            LED_RED = 1;    // Hot condition
            LED_GREEN = 0;
        }
        else
        {
            LED_RED = 0;
            LED_GREEN = 1;  // Normal condition
        }

        // Send data to UART
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

        // Small delay while still checking button
        for(i = 0; i < 10; i++)
        {
            Check_Button();

            __delay_ms(50);
        }
    }
}
