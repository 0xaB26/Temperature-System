#include <p18f452.h>
#include "max_7221_reg.h"

#define SET_SLAVE_SELECT() (LATAbits.LATA5 = 1)
#define CLEAR_SLAVE_SELECT() (LATAbits.LATA5 = 0)

#pragma config WDT = OFF

#define MAX 3
#define SIZE 2
#define VREF 1.5
#define MAX_RESOLUTION 1023
#define ONE_HUND 100

union type					// CONVERTING TWO BYTES INTO 16 BIT VALUE USING TYPE PUNNING
{	
	unsigned int twoByteResult;
	unsigned char bytes[SIZE];
}TYPE_PUNNING;

struct values				 // THIS STRUCTURE CONTAINS TWO VARIABLES USED TO ENSURE DISPLAYING TEMPERATURE ONLY WHEN IT CHANGES
{
	unsigned char newValue;
	unsigned char oldValue;
}value;


typedef struct stack			// STRUCTRUE FOR SOFTWARE STACK
{
	unsigned char array[MAX];
	signed char TOP;
}STACK;

void function(void);	// CONVERTING A TWO BYTES INTO 16 BIT VALYE
void display(void);
void acquisitionTime(void);			
void convertAdc(void);			
void dataTransmition(unsigned char data);

#pragma interrupt SERVICE
void SERVICE(void)
{
	if(PIR1bits.ADIF)
	{
		PIR1bits.ADIF = 0;
		function();
	}
	acquisitionTime();		
	ADCON0bits.GO = 1;
}

#pragma code VECTOR = 0x00008
void VECTOR(void)
{
	_asm
		GOTO SERVICE
	_endasm
}
#pragma code

void convertAdc(void)
{
	static unsigned char state = 0;	
	unsigned char temp = 0x00;
	if(!state)
	{
		state = 0x01;
		value.oldValue = ((TYPE_PUNNING.twoByteResult * VREF) / MAX_RESOLUTION) * ONE_HUND;	
		display();
	}
	else
	{
		value.newValue = ((TYPE_PUNNING.twoByteResult * VREF) / MAX_RESOLUTION) * ONE_HUND;
		if(value.newValue != value.oldValue)
		{
			value.oldValue = value.newValue;
			display();
		}
	}
}
void display(void)
{
	unsigned char result = value.oldValue;
	STACK stackOne;
	stackOne.TOP = -1;
	while(result != 0)
	{
		stackOne.array[++(stackOne.TOP)] = result % 10;
		result /= 10;
	}
	if(stackOne.TOP == 2)				// IN CASE THE TEMPERATURE IS GREATER THEN 99
	{
		CLEAR_SLAVE_SELECT();
		dataTransmition(DIGIT_TWO_REG);
		dataTransmition(stackOne.array[(stackOne.TOP)--]);
		SET_SLAVE_SELECT();
		CLEAR_SLAVE_SELECT();
		dataTransmition(DIGIT_ONE_REG);
		dataTransmition(stackOne.array[(stackOne.TOP)--]);
		SET_SLAVE_SELECT();
		CLEAR_SLAVE_SELECT();
		dataTransmition(DIGIT_ZERO_REG);
		dataTransmition(stackOne.array[(stackOne.TOP)--]);
		SET_SLAVE_SELECT();	
	}
	else if(stackOne.TOP == 1)	// IN CASE THE TEMPERATURE IS GREATER THEN 9
	{
		CLEAR_SLAVE_SELECT();
		dataTransmition(DIGIT_TWO_REG);
		dataTransmition(0x00);
		SET_SLAVE_SELECT();
		CLEAR_SLAVE_SELECT();
		dataTransmition(DIGIT_ONE_REG);
		dataTransmition(stackOne.array[(stackOne.TOP)--]);
		SET_SLAVE_SELECT();
		CLEAR_SLAVE_SELECT();
		dataTransmition(DIGIT_ZERO_REG);
		dataTransmition(stackOne.array[(stackOne.TOP)--]);
		SET_SLAVE_SELECT();	
	}		
	else
	{
		CLEAR_SLAVE_SELECT();
		dataTransmition(DIGIT_TWO_REG);
		dataTransmition(0x00);
		SET_SLAVE_SELECT();
		CLEAR_SLAVE_SELECT();
		dataTransmition(DIGIT_ONE_REG);
		dataTransmition(0x00);
		SET_SLAVE_SELECT();
		CLEAR_SLAVE_SELECT();
		dataTransmition(DIGIT_ZERO_REG);
		dataTransmition(stackOne.array[(stackOne.TOP)--]);
		SET_SLAVE_SELECT();	
	}
}
void function(void)
{
	TYPE_PUNNING.bytes[0] = ADRESL;				// LITTLE ENDIAN ARCHITECTURE PUT THE LOWEST BYTE AT LOWEST MEMORY LOCATION
	TYPE_PUNNING.bytes[1] = ADRESH;
	convertAdc();
}

void dataTransmition(unsigned char data)		// DATA TRANSMITION VIA SPI 4 WIRE INTERFACE
{
	SSPBUF = data;
	while(!SSPSTATbits.BF);
	SSPSTATbits.BF = 0;
	_asm
		MOVF	SSPBUF,0,0		// PUT THE DUMMY DATA INTO WREG
	_endasm
}

void maxInit(void)				// FUNCTION TO INITIALIZE MAX7221 CONTROL REGISTER 
{
	CLEAR_SLAVE_SELECT();
	dataTransmition(SHUT_DOWN_REG);
	dataTransmition(SHUT_DOWN_VALUE);
	SET_SLAVE_SELECT();
	CLEAR_SLAVE_SELECT();
	dataTransmition(DECODE_MODE_REG);
	dataTransmition(DECODE_MODE_VALUE);
	SET_SLAVE_SELECT();
	CLEAR_SLAVE_SELECT();
	dataTransmition(INTENSITY_MODE_REG);
	dataTransmition(INTENSITY_MODE_VALUE);
	SET_SLAVE_SELECT();
	CLEAR_SLAVE_SELECT();
	dataTransmition(SCAN_LIMIT_REG);
	dataTransmition(SCAN_LIMIT_VALUE);
	SET_SLAVE_SELECT();
	CLEAR_SLAVE_SELECT();
	dataTransmition(DISPLAY_TEST_REG);
	dataTransmition(DISPLAY_TEST_VALUE);
	SET_SLAVE_SELECT();
}

void acquisitionTime(void)
{
	T0CON = 0x48;
	TMR0L = 0xEC;
	INTCONbits.TMR0IF = 0;
	T0CONbits.TMR0ON = 1;
	while(!INTCONbits.TMR0IF);
	INTCONbits.TMR0IF = 0;
	T0CONbits.TMR0ON = 0;
}

void main(void)
{
	TRISC = 0xD7;					
	TRISA = 0x7F;
	TRISA = 0x5F;
	SSPCON1 = 0x22;				// TRANSMITION/RECEIVING AT RISING EDGE, IDLE STATE LOW FOR CLOCK
	SSPSTAT = 0x80;
	maxInit();		
	INTCONbits.GIE = 1;	
	INTCONbits.PEIE = 1;
	PIE1bits.ADIE = 1;			// ENABLING ADC INTERRUPT 
	PIR1bits.ADIF = 0;
	ADCON0 = 0x41;
	ADCON1 = 0x8A;
	acquisitionTime();
	ADCON0bits.GO = 1;
	while(1);
}
