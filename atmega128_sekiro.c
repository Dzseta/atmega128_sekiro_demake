#undef F_CPU
#define F_CPU 16000000
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega128");

#define	__AVR_ATmega128__	1
#include <avr/io.h>


// GENERAL INIT - USED BY ALMOST EVERYTHING ----------------------------------

static void port_init() {
    PORTA = 0b00011111;	DDRA = 0b01000000; // buttons & led
    PORTB = 0b00000000;	DDRB = 0b00000000;
    PORTC = 0b00000000;	DDRC = 0b11110111; // lcd
    PORTD = 0b11000000;	DDRD = 0b00001000;
    PORTE = 0b00100000;	DDRE = 0b00110000; // buzzer
    PORTF = 0b00000000;	DDRF = 0b00000000;
    PORTG = 0b00000000;	DDRG = 0b00000000;
}

// TIMER-BASED RANDOM NUMBER GENERATOR ---------------------------------------

static void rnd_init() {
	TCCR0 |= (1  << CS00);	// Timer 0 no prescaling (@FCPU)
	TCNT0 = 0; 				// init counter
}

// generate a value between 0 and max
static int rnd_gen(int max) {
	return TCNT0 % max;
}

// BUTTON HANDLING -----------------------------------------------------------

#define BUTTON_NONE		0
#define BUTTON_UP		1
#define BUTTON_LEFT		2
#define BUTTON_CENTER	3
#define BUTTON_RIGHT	4
#define BUTTON_DOWN		5
static int button_accept = 1;

static int button_pressed() {
	// up
	if (!(PINA & 0b00000001) & button_accept) { // check state of button 1 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_UP;
	}

	// left
	if (!(PINA & 0b00000010) & button_accept) { // check state of button 2 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_LEFT;
	}

	// center
	if (!(PINA & 0b00000100) & button_accept) { // check state of button 3 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_CENTER;
	}

	// right
	if (!(PINA & 0b00001000) & button_accept) { // check state of button 4 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_RIGHT;
	}
	
	// down
	if (!(PINA & 0b00010000) & button_accept) { // check state of button 5 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_DOWN;
	}

	return BUTTON_NONE;
}

static void button_unlock() {
	//check state of all buttons
	if (
		((PINA & 0b00000001)
		|(PINA & 0b00000010)
		|(PINA & 0b00000100)
		|(PINA & 0b00001000)
		|(PINA & 0b00010000)) == 31)
	button_accept = 1; //if all buttons are released button_accept gets value 1
}

// LCD HELPERS ---------------------------------------------------------------

#define    CLR_DISP	        0x00000001
#define    DISP_ON		    0x0000000C
#define    DISP_OFF	        0x00000008
#define    CUR_HOME         0x00000002
#define    CUR_OFF 	        0x0000000C
#define    CUR_ON_UNDER     0x0000000E
#define    CUR_ON_BLINK     0x0000000F
#define    CUR_LEFT         0x00000010
#define    CUR_RIGHT        0x00000014
#define    CG_RAM_ADDR		0x00000040
#define    DD_RAM_ADDR	    0x00000080
#define    DD_RAM_ADDR2     0x000000C0

//#define		ENTRY_INC	    0x00000007	//LCD increment
//#define		ENTRY_DEC	    0x00000005	//LCD decrement
//#define		SH_LCD_LEFT	    0x00000010	//LCD shift left
//#define		SH_LCD_RIGHT	0x00000014	//LCD shift right
//#define		MV_LCD_LEFT	    0x00000018	//LCD move left
//#define		MV_LCD_RIGHT	0x0000001C	//LCD move right

static void lcd_delay(unsigned int b) {
	volatile unsigned int a = b;
	while (a)
		a--;
}

static void lcd_pulse() {
	PORTC = PORTC | 0b00000100;	//set E to high
	lcd_delay(1400); 			//delay ~110ms
	PORTC = PORTC & 0b11111011;	//set E to low
}

static void lcd_send(int command, unsigned char a) {
	unsigned char data;

	data = 0b00001111 | a;					//get high 4 bits
	PORTC = (PORTC | 0b11110000) & data;	//set D4-D7
	if (command)
		PORTC = PORTC & 0b11111110;			//set RS port to 0 -> display set to command mode
	else
		PORTC = PORTC | 0b00000001;			//set RS port to 1 -> display set to data mode
	lcd_pulse();							//pulse to set D4-D7 bits

	data = a<<4;							//get low 4 bits
	PORTC = (PORTC & 0b00001111) | data;	//set D4-D7
	if (command)
		PORTC = PORTC & 0b11111110;			//set RS port to 0 -> display set to command mode
	else
		PORTC = PORTC | 0b00000001;			//set RS port to 1 -> display set to data mode
	lcd_pulse();							//pulse to set d4-d7 bits
}

static void lcd_send_command(unsigned char a) {
	lcd_send(1, a);
}

static void lcd_send_data(unsigned char a) {
	lcd_send(0, a);
}

static void lcd_init() {
	//LCD initialization
	//step by step (from Gosho) - from DATASHEET

	PORTC = PORTC & 0b11111110;

	lcd_delay(10000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00100000;				//set D4 to 0, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)

	lcd_send_command(0x28);         // function set: 4 bits interface, 2 display lines, 5x8 font
	lcd_send_command(DISP_OFF);     // display off, cursor off, blinking off
	lcd_send_command(CLR_DISP);     // clear display
	lcd_send_command(0x06);         // entry mode set: cursor increments, display does not shift

	lcd_send_command(DISP_ON);		// Turn ON Display
	lcd_send_command(CLR_DISP);		// Clear Display
}

static void lcd_send_text(char *str) {
	while (*str)
		lcd_send_data(*str++);
}

static void lcd_send_line1(char *str) {
	lcd_send_command(DD_RAM_ADDR);
	lcd_send_text(str);
}

static void lcd_send_line2(char *str) {
	lcd_send_command(DD_RAM_ADDR2);
	lcd_send_text(str);
}

// method to change a custom character
static void lcd_send_custom_char(unsigned char *str, int char_index, int array_index) {
	lcd_send_command(CG_RAM_ADDR + char_index * 8);
	for (int i = 0; i < 8; i++){
		lcd_send_data(str[i + array_index * 8]);
	}
}

// method to change a character in the first line
static void lcd_change_char_line1(unsigned char a, int char_index) {
	lcd_send_command(DD_RAM_ADDR + char_index);
	lcd_send_data(a);
}

// method to change a character in the second line
static void lcd_change_char_line2(unsigned char a, int char_index) {
	lcd_send_command(DD_RAM_ADDR2 + char_index);
	lcd_send_data(a);
}
// ########################################################################################################
// ########################################################################################################
// ########################################################################################################

// ARRAYS ==================================================================================

// starting screen
static char start_screen1[] = {255, 43, 252, 255, 32, 32, 32, 32, 32, 32, 32, 32, 255, 252, 43, 255};
static char start_screen2[] = {255, 32, 32, 255, 32, 32, 32, 32, 32, 32, 32, 32, 255, 32, 32, 255};
// empty screens
static char empty_screen[] = {32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32};

// CUSTOM CHARACTERS ============================================================
// IDLE =========================================================================

static unsigned char player_idle[] = {
	0b00000,
	0b00100,
	0b01110,
	0b00100,
	0b01111,
	0b01110,
	0b01110,
	0b01010,
	
	0b00000,
	0b01000,
	0b11100,
	0b01000,
	0b11111,
	0b11100,
	0b11100,
	0b10100,
};

static unsigned char sword_enemy_idle[] = {
	0b00000,
	0b00100,
	0b01110,
	0b00100,
	0b11110,
	0b01110,
	0b01110,
	0b01010,
	
	0b00000,
	0b00010,
	0b00111,
	0b00010,
	0b11111,
	0b00111,
	0b00111,
	0b00101,
};

static unsigned char spear_enemy_idle[] = {
	0b00000,
	0b00100,
	0b01110,
	0b00100,
	0b01110,
	0b11111,
	0b01110,
	0b01010,
	
	0b00000,
	0b01000,
	0b11100,
	0b01000,
	0b11100,
	0b11110,
	0b11100,
	0b10100,
};

static unsigned char fat_enemy_idle[] = {
	0b00000,
	0b01110,
	0b01110,
	0b00110,
	0b11111,
	0b01111,
	0b01111,
	0b01001,
	
	0b00000,
	0b11100,
	0b11100,
	0b01100,
	0b11110,
	0b11110,
	0b11110,
	0b10010,
};

static unsigned char samurai_enemy_idle[] = {
	0b00010,
	0b01100,
	0b01100,
	0b00100,
	0b11110,
	0b01110,
	0b01110,
	0b01010,
	
	0b00001,
	0b00110,
	0b00110,
	0b00010,
	0b11111,
	0b00111,
	0b00111,
	0b00101,
};

static unsigned char ronin_enemy_idle[] = {
	0b00010,
	0b01100,
	0b01100,
	0b00100,
	0b11110,
	0b11110,
	0b01110,
	0b01010,
	
	0b00001,
	0b00110,
	0b00110,
	0b00010,
	0b11111,
	0b10111,
	0b01111,
	0b00101,
};

// MIKIRI ====================================================================

static unsigned char player_mikiri[] = {
	0b00001,
	0b01001,
	0b11101,
	0b01001,
	0b11110,
	0b11100,
	0b11100,
	0b10100,

	0b00001,
	0b01001,
	0b11101,
	0b01001,
	0b11110,
	0b11100,
	0b11111,
	0b10000,

	0b00001,
	0b01001,
	0b11101,
	0b01001,
	0b11110,
	0b11100,
	0b11110,
	0b10010
};

// PARRY ======================================================================

static unsigned char player_parry[] = {
	0b00001,
	0b01001,
	0b11101,
	0b01001,
	0b11110,
	0b11100,
	0b11100,
	0b10100,
};

static unsigned char sword_enemy_parry[] = {
	0b10000,
	0b10010,
	0b10111,
	0b10010,
	0b01111,
	0b00111,
	0b00111,
	0b00101,
};

static unsigned char spear_enemy_parry[] = {
	0b00000,
	0b00010,
	0b00111,
	0b00010,
	0b11111,
	0b11111,
	0b00111,
	0b00101,

	0b00000,
	0b00010,
	0b00111,
	0b00010,
	0b11111,
	0b00111,
	0b10111,
	0b01101,

	0b00000,
	0b00010,
	0b00111,
	0b00010,
	0b11111,
	0b00111,
	0b10111,
	0b10101,

	0b10000,
	0b10010,
	0b10111,
	0b10010,
	0b11111,
	0b10111,
	0b10111,
	0b10101,
};

// ATTACK =========================================================================

static unsigned char player_attack[] = {
	0b00000,
	0b00010,
	0b00111,
	0b00010,
	0b00111,
	0b00111,
	0b00111,
	0b00101,
};

static unsigned char sword_enemy_attack[] = {
	0b00000,
	0b01000,
	0b11100,
	0b01000,
	0b11100,
	0b11100,
	0b11100,
	0b10100,
};

static unsigned char spear_enemy_attack[] = {
	0b00000,
	0b01000,
	0b11100,
	0b01000,
	0b11100,
	0b11111,
	0b11100,
	0b10100,
};

static unsigned char spear_light_enemy_attack[] = {
	0b00000,
	0b01000,
	0b11100,
	0b01000,
	0b11100,
	0b11111,
	0b11100,
	0b10100,
};

// DODGE ========================================================================

static unsigned char player_dodge[] = {
	0b00000,
	0b00000,
	0b00001,
	0b00000,
	0b00001,
	0b00001,
	0b00001,
	0b00001,

	0b00000,
	0b00001,
	0b00011,
	0b00001,
	0b00011,
	0b00011,
	0b00011,
	0b00010,

	0b00000,
	0b00010,
	0b00111,
	0b00010,
	0b00111,
	0b00111,
	0b00111,
	0b00101,

	0b00000,
	0b10000,
	0b11001,
	0b10010,
	0b11100,
	0b11000,
	0b11000,
	0b01000,

	0b00000,
	0b00001,
	0b10010,
	0b00100,
	0b11000,
	0b10000,
	0b10000,
	0b10000,

	0b00001,
	0b00010,
	0b00100,
	0b01000,
	0b10000,
	0b00000,
	0b00000,
	0b00000,
};

// STAGGARED ========================================================================

static unsigned char player_staggared[] = {
	0b00000,
	0b01000,
	0b11100,
	0b01000,
	0b11111,
	0b11101,
	0b11101,
	0b10101,

	0b00000,
	0b00000,
	0b01000,
	0b11100,
	0b01000,
	0b11111,
	0b11101,
	0b10101
};

static unsigned char sword_enemy_staggared[] = {
	0b00000,
	0b00010,
	0b00111,
	0b00010,
	0b11111,
	0b10111,
	0b10111,
	0b10101,

	0b00000,
	0b00000,
	0b00010,
	0b00111,
	0b00010,
	0b11111,
	0b10111,
	0b10101
};

static unsigned char spear_enemy_staggared[] = {
	0b00000,
	0b00010,
	0b00111,
	0b00010,
	0b00111,
	0b11111,
	0b00111,
	0b00101,

	0b00000,
	0b00010,
	0b00111,
	0b00010,
	0b00111,
	0b10111,
	0b00111,
	0b00101,

	0b00000,
	0b00010,
	0b00111,
	0b00010,
	0b00111,
	0b00111,
	0b10111,
	0b00101,

	0b00000,
	0b00010,
	0b00111,
	0b00010,
	0b00111,
	0b00111,
	0b00111,
	0b10101,

	0b00000,
	0b00000,
	0b00010,
	0b00111,
	0b00010,
	0b00111,
	0b00111,
	0b10101,
};

// JUMP ========================================================================

static unsigned char player_jump[] = {
	0b00000,
	0b00000,
	0b00100,
	0b01110,
	0b00100,
	0b01111,
	0b01110,
	0b01010,

	0b00100,
	0b01110,
	0b00100,
	0b01111,
	0b01110,
	0b01110,
	0b01010,
	0b00000,

	0b00100,
	0b01111,
	0b01110,
	0b01110,
	0b01010,
	0b00000,
	0b00000,
	0b00000,

	0b01110,
	0b01110,
	0b01010,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,

	0b01010,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00100,
	0b01110,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00100,
	0b01110,
	0b00100,

	0b00000,
	0b00000,
	0b00000,
	0b00100,
	0b01110,
	0b00100,
	0b01111,
	0b01110,

	0b00000,
	0b00100,
	0b01110,
	0b00100,
	0b01111,
	0b01110,
	0b01010,
	0b00000,
};

// DEAD =========================================================================

static unsigned char player_dead[] = {
	0b00000,
	0b00000,
	0b00100,
	0b01110,
	0b00100,
	0b01111,
	0b00111,
	0b00010,

	0b00000,
	0b00000,
	0b00000,
	0b01110,
	0b01100,
	0b01011,
	0b00011,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00010,
	0b01011,
	0b11111,
	0b01011,
	0b00001,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00010,
	0b01011,
	0b11111,
	0b01011,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b01011,
	0b11111,
	0b01011,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b10000,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b11000,
	0b10000,
	0b11000,
	
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b11100,
	0b10000,
	0b11100
};

static unsigned char sword_enemy_dead[] = {
	0b00000,
	0b00000,
	0b00100,
	0b01110,
	0b00100,
	0b11110,
	0b11100,
	0b00010,

	0b00000,
	0b00000,
	0b00000,
	0b01110,
	0b00110,
	0b11010,
	0b11000,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b01000,
	0b11010,
	0b11111,
	0b11010,
	0b10000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b01000,
	0b11010,
	0b11111,
	0b11010,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b11010,
	0b11111,
	0b11010,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00001,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00011,
	0b00001,
	0b00011,
	
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00111,
	0b00001,
	0b00111
};

// WEAPONS ====================================================================

static unsigned char player_sword[] = {
	0b00010,
	0b00100,
	0b01000,
	0b10000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,

	0b10000,
	0b10000,
	0b10000,
	0b10000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b11110,
	0b00000,
	0b00000,
	0b00000,

	0b01000,
	0b01000,
	0b10000,
	0b10000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,

	0b00000,
	0b00000,
	0b00110,
	0b11000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b11000,
	0b00110,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b10000,
	0b01000,
	0b00100,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b10000,
	0b10000,
	0b10000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b11110,

	0b00000,
	0b01000,
	0b01000,
	0b10000,
	0b10000,
	0b00000,
	0b00000,
	0b00000,

	0b01000,
	0b10000,
	0b10000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,

	0b10000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b01000,
	0b01000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b01000,
	0b01000,
	0b10000,
	0b10000,

	0b00000,
	0b00000,
	0b01000,
	0b01000,
	0b10000,
	0b10000,
	0b00000,
	0b00000,

	0b01000,
	0b10000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
};

static unsigned char enemy_sword[] = {
	0b01000,
	0b00100,
	0b00010,
	0b00001,
	0b00000,
	0b00000,
	0b00000,
	0b00000,

	0b00001,
	0b00001,
	0b00001,
	0b00001,
	0b00000,
	0b00000,
	0b00000,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b01111,
	0b00000,
	0b00000,
	0b00000,

	0b00010,
	0b00010,
	0b00001,
	0b00001,
	0b00000,
	0b00000,
	0b00000,
	0b00000,

	0b00000,
	0b00000,
	0b01100,
	0b00011,
	0b00000,
	0b00000,
	0b00000,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00011,
	0b01100,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00001,
	0b00010,
	0b00100,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00001,
	0b00001,
	0b00001,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b01111,
};

static unsigned char enemy_spear[] = {
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00111,
	0b00000,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b01111,
	0b00000,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b11100,
	0b00011,
	0b00000,
	0b00000,
	0b00000,

	0b00000,
	0b01000,
	0b00100,
	0b00010,
	0b00001,
	0b00000,
	0b00000,
	0b00000,

	0b00010,
	0b00010,
	0b00001,
	0b00001,
	0b00001,
	0b00001,
	0b00000,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b11111,
	0b00000,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00011,
	0b11100,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00011,
	0b11100,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b11111,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b11111,

	0b10000,
	0b01000,
	0b00100,
	0b00010,
	0b00001,
	0b00000,
	0b00000,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00001,
	0b00110,
	0b11000,
};

static unsigned char enemy_axe[] = {
	0b01000,
	0b11100,
	0b11010,
	0b00001,
	0b00000,
	0b00000,
	0b00000,
	0b00000,

	0b00111,
	0b00111,
	0b00001,
	0b00001,
	0b00001,
	0b00000,
	0b00000,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b11111,
	0b11000,
	0b11000,
	0b00000,
};

static unsigned char enemy_naginata[] = {
	0b10000,
	0b11000,
	0b01100,
	0b00010,
	0b00001,
	0b00000,
	0b00000,
	0b00000,

	0b00001,
	0b00011,
	0b00011,
	0b00001,
	0b00001,
	0b00001,
	0b00001,
	0b00000,

	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b11111,
	0b01100,
	0b00000,
	0b00000,
};

// THE PROGRAM ==================================================================

int main() {
	port_init();
	lcd_init();
	rnd_init();

	// variables
	int health = 0;
	int posture = 0;
	int enemy_health = 0;
	int enemy_posture = 0;
	int position = 0;
	int position_length = 0;
	int enemy_position = 0;
	int enemy_position_length = 0;
	int random = 0;
	int end = 0;
	int enemy_number = 0;

	// main menu
	lcd_send_line1("     SEKIRO     ");
	lcd_send_line2("    > play <    ");
	// main menu, wait for start
	while (button_pressed() != BUTTON_CENTER) button_unlock();

	// loop of the whole program
	while(1) {
		random = 0;

		// enemy picker menu
		lcd_send_line1(empty_screen);
		lcd_send_line2(empty_screen);
		lcd_change_char_line2(60, 6);
		lcd_change_char_line2(2, 7);
		lcd_change_char_line2(3, 8);
		lcd_change_char_line2(62, 9);
		lcd_send_custom_char(enemy_sword, 2, 0);
		lcd_send_custom_char(sword_enemy_idle, 3, 0);

		int button = button_pressed();
		while(random == 0) {
			// get the button
			button = button_pressed();
			if(button == BUTTON_LEFT && enemy_number > 0) { 
				enemy_number--;
			} else if(button == BUTTON_RIGHT && enemy_number < 4) {
				enemy_number++;
			} else if (button == BUTTON_CENTER) {
				break;
			}
			if(enemy_number == 0) {
				lcd_send_line1("Odachi Ashigaru ");
				lcd_send_custom_char(enemy_sword, 2, 0);
				lcd_send_custom_char(sword_enemy_idle, 3, 0);
				enemy_health = 3;
				enemy_posture = 3;
			} else if(enemy_number == 1) {
				lcd_send_line1("Yari Ashigaru   ");
				lcd_send_custom_char(enemy_spear, 2, 0);
				lcd_send_custom_char(spear_enemy_idle, 3, 0);
				enemy_health = 5;
				enemy_posture = 3;
			} else if(enemy_number == 2) {
				lcd_send_line1("Ono Samurai     ");
				lcd_send_custom_char(enemy_axe, 2, 0);
				lcd_send_custom_char(fat_enemy_idle, 3, 0);
				enemy_health = 9;
				enemy_posture = 3;
			} else if(enemy_number == 3) {
				lcd_send_line1("Katana Samurai  ");
				lcd_send_custom_char(enemy_sword, 2, 0);
				lcd_send_custom_char(samurai_enemy_idle, 3, 0);
				enemy_health = 3;
				enemy_posture = 9;
			} else if(enemy_number == 4) {
				lcd_send_line1("Naginata Ronin  ");
				lcd_send_custom_char(enemy_naginata, 2, 0);
				lcd_send_custom_char(ronin_enemy_idle, 3, 0);
				enemy_health = 9;
				enemy_posture = 9;
			}
			for(int k = 0; k<100; k++) for(int l=0; l<10000; l++);
			button_unlock();
		}

		// initialize the health and posture
		health = 3;
		posture = 5;
		end = 0;
		position = 0;
		position_length = -1;
		enemy_position = 0;
		enemy_position_length = -1;

		// start of battle
		lcd_send_line1(start_screen1);
		lcd_send_line2(start_screen2);

		// show health and posture
		lcd_change_char_line2(health+48, 1);
		lcd_change_char_line2(posture+48, 2);
		lcd_change_char_line2(enemy_posture+48, 13);
		lcd_change_char_line2(enemy_health+48, 14);

		// initialize custom chars for the player
		lcd_send_custom_char(player_idle, 0, 0);
		lcd_send_custom_char(player_sword, 1, 0);

		// initialize custom chars for the enemy
		if(enemy_number == 0) {
			lcd_send_custom_char(sword_enemy_idle, 2, 0);
			lcd_send_custom_char(enemy_sword, 3, 0);
		} else if(enemy_number == 1) {
			lcd_send_custom_char(spear_enemy_idle, 2, 0);
			lcd_send_custom_char(enemy_spear, 3, 0);
		} else if(enemy_number == 2) {
			lcd_send_custom_char(fat_enemy_idle, 2, 0);
			lcd_send_custom_char(enemy_axe, 3, 0);
		} else if(enemy_number == 3) {
			lcd_send_custom_char(samurai_enemy_idle, 2, 0);
			lcd_send_custom_char(enemy_sword, 3, 0);
		} else if(enemy_number == 4) {
			lcd_send_custom_char(ronin_enemy_idle, 2, 0);
			lcd_send_custom_char(enemy_naginata, 3, 0);
		}

		// player character
		lcd_change_char_line2(0, 6);
		lcd_change_char_line2(1, 7);

		// enemy character
		lcd_change_char_line2(2, 9);
		lcd_change_char_line2(3, 8);

		// loop for animation
		int loop = 0;
		// loop of the game
		while(1) {
			// get the button
			int button = button_pressed();
			// loop
			if(loop == 19) loop = 0;
			else loop++;
			// THE PLAYER ==================================================================
			// get the input from the button
			if(end == 1) {
				if(button == BUTTON_CENTER && (position_length == -2 || enemy_position_length == -2)) break;
			} if(button == BUTTON_UP) {
				position = 1;
				position_length = 20;
			} else if(button == BUTTON_LEFT) { 
				position = 2;
				position_length = 20;
			} else if(button == BUTTON_CENTER) { 
				position = 3;
				position_length = 25;
			} else if(button == BUTTON_RIGHT) {
				position = 4;
				position_length = 25;
			} else if(button == BUTTON_DOWN) {
				position = 5;
				position_length = 20;
			}

			// animate based on pose
			if(position == 0) { // idle pose
				// idle animation
				if(loop == 0) {
					lcd_send_custom_char(player_idle, 0, 1);
				} else if (loop == 10) {
					lcd_send_custom_char(player_idle, 0, 0);
				}
			} else if(position == 1) { // jump
				// write symbol
				lcd_change_char_line1(94, 4);
				// animate player
				if (position_length == 20) {
					lcd_send_custom_char(player_idle, 0, 0);
					lcd_send_custom_char(player_sword, 1, 0);
				} else if (position_length == 19) {
					lcd_send_custom_char(player_jump, 0, 0);
					lcd_send_custom_char(player_sword, 1, 9);
				} else if (position_length == 16) {
					lcd_send_custom_char(player_jump, 0, 1);
					lcd_send_custom_char(player_sword, 1, 10);
				} else if (position_length == 15) {
					lcd_send_custom_char(player_jump, 5, 5);
					lcd_send_custom_char(player_sword, 4, 12);
					lcd_send_custom_char(player_jump, 0, 2);
					lcd_send_custom_char(player_sword, 1, 11);
					lcd_change_char_line1(4, 7);
					lcd_change_char_line1(5, 6);
				} else if (position_length == 14) {
					lcd_send_custom_char(player_jump, 5, 6);
					lcd_send_custom_char(player_sword, 4, 13);
					lcd_send_custom_char(player_jump, 0, 3);
					lcd_change_char_line2(32, 7);
				} else if (position_length == 13) {
					lcd_send_custom_char(player_jump, 5, 7);
					lcd_send_custom_char(player_sword, 4, 14);
					lcd_send_custom_char(player_jump, 0, 4);
				} else if (position_length == 12) {
					lcd_send_custom_char(player_jump, 5, 8);
					lcd_send_custom_char(player_sword, 4, 3);
					lcd_change_char_line2(32, 6);
				} else if (position_length == 8) {
					lcd_send_custom_char(player_jump, 5, 7);
					lcd_send_custom_char(player_sword, 4, 14);
					lcd_send_custom_char(player_jump, 0, 4);
					lcd_change_char_line2(0, 6);
				} else if (position_length == 7) {
					lcd_send_custom_char(player_jump, 5, 6);
					lcd_send_custom_char(player_sword, 4, 13);
					lcd_send_custom_char(player_jump, 0, 3);
				} else if (position_length == 6) {
					lcd_change_char_line2(1, 7);
					lcd_send_custom_char(player_jump, 5, 5);
					lcd_send_custom_char(player_sword, 4, 12);
					lcd_send_custom_char(player_jump, 0, 2);
					lcd_send_custom_char(player_sword, 1, 11);
				} else if (position_length == 5) {
					lcd_send_custom_char(player_jump, 0, 1);
					lcd_send_custom_char(player_sword, 1, 10);
					lcd_change_char_line1(32, 6);
					lcd_change_char_line1(32, 7);
				} else if (position_length == 4) {
					lcd_send_custom_char(player_jump, 0, 0);
					lcd_send_custom_char(player_sword, 1, 9);
				} else if (position_length == 1) {
					lcd_send_custom_char(player_idle, 0, 0);
					lcd_send_custom_char(player_sword, 1, 0);
				}
			} else if(position == 2) { // dodge backwards
				// write symbol
				lcd_change_char_line1(60, 4);
				// show player one block back
				if (position_length == 20) {
					lcd_send_custom_char(player_idle, 0, 0);
					lcd_send_custom_char(player_sword, 1, 0);
				} else if (position_length == 19) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 0);
				} else if (position_length == 18) {
					lcd_send_custom_char(player_dodge, 4, 0);
					lcd_send_custom_char(player_dodge, 0, 3);
					lcd_send_custom_char(player_sword, 1, 15);
					lcd_change_char_line2(4, 5);
				} else if (position_length == 17) {
					lcd_send_custom_char(player_dodge, 4, 1);
					lcd_send_custom_char(player_dodge, 0, 4);
					lcd_send_custom_char(player_sword, 1, 11);
				} else if (position_length == 16) {
					lcd_send_custom_char(player_dodge, 4, 2);
					lcd_send_custom_char(player_dodge, 0, 5);
					lcd_change_char_line2(32, 7);
				} else if (position_length == 15) {
					lcd_send_custom_char(player_idle, 4, 0);
					lcd_send_custom_char(player_sword, 0, 0);
				} else if (position_length == 14) {
					lcd_send_custom_char(player_idle, 4, 1);
					lcd_send_custom_char(player_sword, 0, 0);
				} else if (position_length == 6) {
					lcd_send_custom_char(player_idle, 4, 0);
					lcd_send_custom_char(player_sword, 0, 0);
				} else if (position_length == 5) {
					lcd_send_custom_char(player_dodge, 4, 2);
					lcd_send_custom_char(player_dodge, 0, 5);
				} else if (position_length == 4) {
					lcd_change_char_line2(1, 7);
					lcd_send_custom_char(player_dodge, 4, 1);
					lcd_send_custom_char(player_dodge, 0, 4);
					lcd_send_custom_char(player_sword, 1, 11);
				} else if (position_length == 3) {
					lcd_send_custom_char(player_dodge, 4, 0);
					lcd_send_custom_char(player_dodge, 0, 3);
					lcd_send_custom_char(player_sword, 1, 15);
				} else if (position_length == 2) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 0);
					lcd_change_char_line2(32, 5);
				} else if (position_length == 1) {
					lcd_send_custom_char(player_idle, 0, 0);
					lcd_send_custom_char(player_sword, 1, 0);
				}
			} else if(position == 3) { // parry
				// write symbol
				lcd_change_char_line1(79, 4);
				// move sword up
				if (position_length == 25) {
					lcd_send_custom_char(player_idle, 0, 0);
					lcd_send_custom_char(player_sword, 1, 0);
				} else if (position_length == 24) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 0);
				} else if (position_length == 23) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 3);
				} else if (position_length == 22) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 1);
				} else if (position_length == 21) {
					lcd_send_custom_char(player_parry, 0, 0);
					lcd_change_char_line2(32, 7);
				} else if (position_length == 4) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 1);
					lcd_change_char_line2(1, 7);
				} else if (position_length == 3) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 3);
				} else if (position_length == 2) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 0);
				} else if (position_length == 1) {
					lcd_send_custom_char(player_idle, 0, 0);
					lcd_send_custom_char(player_sword, 1, 0);
				}
			} else if(position == 4) { // attack
				// write symbol
				lcd_change_char_line1(62, 4);
				// move sword to attack position
				if (position_length == 25) {
					lcd_send_custom_char(player_idle, 0, 0);
					lcd_send_custom_char(player_sword, 1, 0);
				} else if (position_length == 24) {
					lcd_send_custom_char(player_attack, 0, 0);
					lcd_send_custom_char(player_sword, 1, 0);
				} else if (position_length == 23) {
					lcd_send_custom_char(player_sword, 1, 3);
				} else if (position_length == 22) {
					lcd_send_custom_char(player_sword, 1, 1);
				} else if (position_length == 11) {
					lcd_send_custom_char(player_sword, 1, 3);
				} else if (position_length == 10) {
					lcd_send_custom_char(player_sword, 1, 0);
				} else if (position_length == 9) {
					lcd_send_custom_char(player_sword, 1, 4);
				} else if (position_length == 8) {
					lcd_send_custom_char(player_sword, 1, 2);
					if(enemy_position == 3 && enemy_position_length>4 && enemy_position_length<27) {
						posture = posture - 1;
						lcd_change_char_line2(posture+48, 2);
						if(posture == 0) {
							position = 6;
							position_length = 8;
						}
					} else if(enemy_position == 6) {
						enemy_health = 0;
						lcd_change_char_line2(enemy_health+48, 14);
					} else {
						enemy_health = enemy_health - 1;
						lcd_change_char_line2(enemy_health+48, 14);
					}
				} else if (position_length == 3) {
					lcd_send_custom_char(player_sword, 1, 4);
				} else if (position_length == 2) {
					lcd_send_custom_char(player_sword, 1, 0);
				} else if (position_length == 1) {
					lcd_send_custom_char(player_idle, 0, 0);
					lcd_send_custom_char(player_sword, 1, 0);
				}
			} else if(position == 5) { // mikiri counter
				// write symbol
				lcd_change_char_line1(118, 4);
				// show player in mikiri pose
				if (position_length == 20) {
					lcd_send_custom_char(player_idle, 0, 0);
					lcd_send_custom_char(player_sword, 1, 0);
				} else if (position_length == 19) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 0);
				} else if (position_length == 18) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 3);
				} else if (position_length == 17) {
					lcd_send_custom_char(player_mikiri, 0, 0);
					lcd_change_char_line2(32, 7);
				} else if (position_length == 16) {
					lcd_send_custom_char(player_mikiri, 0, 1);
				} else if (position_length == 15) {
					lcd_send_custom_char(player_mikiri, 0, 2);
				} else if (position_length == 5) {
					lcd_send_custom_char(player_mikiri, 0, 1);
				} else if (position_length == 4) {
					lcd_send_custom_char(player_mikiri, 0, 0);
				} else if (position_length == 3) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_change_char_line2(1, 7);
					lcd_send_custom_char(player_sword, 1, 3);
				} else if (position_length == 2) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 0);
				} else if (position_length == 1) {
					lcd_send_custom_char(player_idle, 0, 0);
					lcd_send_custom_char(player_sword, 1, 0);
				}
			} else if(position == 6) { // staggared
				// write symbol
				lcd_change_char_line1(32, 4);
				// show player in staggared pose
				if(position_length == 8) {
					lcd_send_custom_char(player_idle, 0, 0);
					lcd_send_custom_char(player_sword, 1, 0);
				} else if(position_length == 7) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 0);
				} else if(position_length == 6) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 4);
				} else if(position_length == 5) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 2);
				} else if(position_length == 4) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 5);
				} else if(position_length == 3) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 6);
				} else if(position_length == 2) {
					lcd_send_custom_char(player_idle, 0, 1);
					lcd_send_custom_char(player_sword, 1, 7);
				} else if(position_length == 1) {
					lcd_send_custom_char(player_staggared, 0, 0);
					lcd_change_char_line2(32, 7);
				} else if(position_length == 0) {
					lcd_send_custom_char(player_staggared, 0, 1);
				}
			} else if(position == 10) { // death
				// write symbol
				lcd_change_char_line1(32, 4);
				if(position_length == 7) {
					lcd_send_custom_char(player_idle, 0, 0);
					lcd_send_custom_char(player_sword, 1, 0);
				} else if(position_length == 6) {
					lcd_send_custom_char(player_idle, 0, 0);
					lcd_send_custom_char(player_sword, 1, 2);
				} else if(position_length == 5) {
					lcd_send_custom_char(player_idle, 0, 0);
					lcd_send_custom_char(player_sword, 1, 8);
				} else if(position_length == 4) {
					lcd_send_custom_char(player_dead, 0, 0);
					lcd_send_custom_char(player_dead, 1, 5);
				} else if(position_length == 3) {
					lcd_send_custom_char(player_dead, 0, 1);
					lcd_send_custom_char(player_dead, 1, 6);
				} else if(position_length == 2) {
					lcd_send_custom_char(player_dead, 0, 2);
					lcd_send_custom_char(player_dead, 1, 6);
				} else if(position_length == 1) {
					lcd_send_custom_char(player_dead, 0, 3);
					lcd_send_custom_char(player_dead, 1, 7);
				} else if(position_length == 0) {
					lcd_send_custom_char(player_dead, 0, 4);
					lcd_send_custom_char(player_dead, 1, 7);
				}
			}

			// see if player finished the current move
			if(position_length > 0) {
				position_length--;
			} else if(position_length == 0 && position < 6) {
				position = 0;
				position_length = -1;
				// delete symbol
				lcd_change_char_line1(32, 4);
				// move player to idle position
				lcd_change_char_line1(32, 6);
				lcd_change_char_line1(32, 7);
				lcd_change_char_line2(32, 5);
				lcd_change_char_line2(0, 6);
				lcd_change_char_line2(1, 7);
				// move sword  and player to idle position
				lcd_send_custom_char(player_sword, 1, 0);
				lcd_send_custom_char(player_idle, 0, 0);
				// unlock the button
				button_unlock();
			} else if(position_length == 0 && position == 10) {
				position_length = -2;
				button_unlock();
			} else if(position_length == 0 && position == 6) {
				position_length = -2;
			} else if(position_length == -1) button_unlock();

			// =============================================================================
			// =============================================================================
			// THE ENEMY ==================================================================
			// get random
			random = rnd_gen(100);
			
			// ODACHI ASHIGARU ==================================================================
			if(enemy_number == 0) { // if Odachi ashigaru
				// get next move
				if(enemy_position_length == -1 && position == 4) {
					enemy_position = 3;
					enemy_position_length = 25;
				} else if(enemy_position_length == -1 && random < 2) {
					enemy_position = 4;
					enemy_position_length = 25;
				}
				// animate enemy
				if(enemy_position == 0) { // idle pose
					// animate enemy
					if(loop == 0) {
						lcd_send_custom_char(sword_enemy_idle, 2, 0);
					} else if (loop == 10) {
						lcd_send_custom_char(sword_enemy_idle, 2, 1);
					}
				} else if(enemy_position == 3) { // parry
					// write symbol
					lcd_change_char_line1(79, 11);
					// move sword up
					if (enemy_position_length == 25) {
						lcd_send_custom_char(sword_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_sword, 3, 0);
					} else if (enemy_position_length == 24) {
						lcd_send_custom_char(sword_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_sword, 3, 0);
					} else if (enemy_position_length == 23) {
						lcd_send_custom_char(sword_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_sword, 3, 3);
					} else if (enemy_position_length == 22) {
						lcd_send_custom_char(sword_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_sword, 3, 1);
					} else if (enemy_position_length == 21) {
						lcd_send_custom_char(sword_enemy_parry, 2, 0);
						lcd_change_char_line2(32, 8);
					} else if (enemy_position_length == 4) {
						lcd_send_custom_char(sword_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_sword, 3, 1);
						lcd_change_char_line2(3, 8);
					} else if (enemy_position_length == 3) {
						lcd_send_custom_char(sword_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_sword, 3, 3);
					} else if (enemy_position_length == 2) {
						lcd_send_custom_char(sword_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_sword, 3, 0);
					} else if (enemy_position_length == 1) {
						lcd_send_custom_char(sword_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_sword, 3, 0);
					}
				} else if(enemy_position == 4) { // attack
					// write symbol
					lcd_change_char_line1(60, 11);
					// move sword to attack position
					if (enemy_position_length == 25) {
						lcd_send_custom_char(sword_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_sword, 3, 0);
					} else if (enemy_position_length == 24) {
						lcd_send_custom_char(sword_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_sword, 3, 0);
					} else if (enemy_position_length == 23) {
						lcd_send_custom_char(enemy_sword, 3, 3);
					} else if (enemy_position_length == 22) {
						lcd_send_custom_char(enemy_sword, 3, 1);
					} else if (enemy_position_length == 11) {
						lcd_send_custom_char(enemy_sword, 3, 3);
					} else if (enemy_position_length == 10) {
						lcd_send_custom_char(enemy_sword, 3, 0);
					} else if (enemy_position_length == 9) {
						lcd_send_custom_char(enemy_sword, 3, 4);
					} else if (enemy_position_length == 8) {
						lcd_send_custom_char(enemy_sword, 3, 2);
						if(position == 0 || position == 1 || position == 4 || position == 5) {
							health = health - 1;
							lcd_change_char_line2(health+48, 1);
						} else if(position == 6) {
							health = 0;
							lcd_change_char_line2(health+48, 1);
						} else if(position == 3) {
							enemy_posture = enemy_posture - 1;
							lcd_change_char_line2(enemy_posture+48, 13);
							if(enemy_posture == 0) {
								enemy_position = 6;
								enemy_position_length = 9;
							}
						}
					} else if (enemy_position_length == 3) {
						lcd_send_custom_char(enemy_sword, 3, 4);
					} else if (enemy_position_length == 2) {
						lcd_send_custom_char(enemy_sword, 3, 0);
					} else if (enemy_position_length == 1) {
						lcd_send_custom_char(sword_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_sword, 3, 0);
					}
				} else if(enemy_position == 6) { // staggared
					// write symbol
					lcd_change_char_line1(32, 11);
					// show player in staggared pose
					if(enemy_position_length == 9) {
						lcd_send_custom_char(sword_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_sword, 3, 0);
					} else if(enemy_position_length == 8) {
						lcd_send_custom_char(sword_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_sword, 3, 0);
					} else if(enemy_position_length == 7) {
						lcd_send_custom_char(sword_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_sword, 3, 4);
					} else if(enemy_position_length == 6) {
						lcd_send_custom_char(sword_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_sword, 3, 2);
					} else if(enemy_position_length == 5) {
						lcd_send_custom_char(sword_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_sword, 3, 5);
					} else if(enemy_position_length == 4) {
						lcd_send_custom_char(sword_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_sword, 3, 6);
					} else if(enemy_position_length == 3) {
						lcd_send_custom_char(sword_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_sword, 3, 7);
					} else if(enemy_position_length == 2) {
						lcd_send_custom_char(sword_enemy_staggared, 2, 0);
						lcd_change_char_line2(32, 8);
					} else if(enemy_position_length == 1) {
						lcd_send_custom_char(sword_enemy_staggared, 2, 1);
					}
				} else if(enemy_position == 10) { // death
					// write symbol
					lcd_change_char_line1(32, 11);
					if(enemy_position_length == 8) {
						lcd_send_custom_char(sword_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_sword, 3, 0);
					} else if(enemy_position_length == 7) {
						lcd_send_custom_char(sword_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_sword, 3, 2);
					} else if(enemy_position_length == 6) {
						lcd_send_custom_char(sword_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_sword, 3, 8);
					} else if(enemy_position_length == 5) {
						lcd_send_custom_char(sword_enemy_dead, 2, 0);
						lcd_send_custom_char(sword_enemy_dead, 3, 5);
					} else if(enemy_position_length == 4) {
						lcd_send_custom_char(sword_enemy_dead, 2, 1);
						lcd_send_custom_char(sword_enemy_dead, 3, 6);
					} else if(enemy_position_length == 3) {
						lcd_send_custom_char(sword_enemy_dead, 2, 2);
						lcd_send_custom_char(sword_enemy_dead, 3, 6);
					} else if(enemy_position_length == 2) {
						lcd_send_custom_char(sword_enemy_dead, 2, 3);
						lcd_send_custom_char(sword_enemy_dead, 3, 7);
					} else if(enemy_position_length == 1) {
						lcd_send_custom_char(sword_enemy_dead, 2, 4);
						lcd_send_custom_char(sword_enemy_dead, 3, 7);
					}
				}
			// YARI ASHIGARU =============================================================
			} else if(enemy_number == 1) { // if Yari Ashigaru
				if(enemy_position_length == -1 && position == 4 && position_length >8) {
					enemy_position = 3;
					enemy_position_length = 25;
				} else if(enemy_position_length == -1 && random < 10) {
					enemy_position = 4;
					enemy_position_length = 25;
				} else if(enemy_position_length == -1 && random >= 10 && random < 20) {
					enemy_position = 8;
					enemy_position_length = 25;
				}
				if(enemy_position == 0) { // idle pose
					// animate enemy
					if(loop == 0) {
						lcd_send_custom_char(enemy_spear, 3, 1);
						lcd_send_custom_char(spear_enemy_idle, 2, 1);
					} else if (loop == 10) {
						lcd_send_custom_char(enemy_spear, 3, 0);
						lcd_send_custom_char(spear_enemy_idle, 2, 0);
					}
				} else if(enemy_position == 3) { // parry
					// write symbol
					lcd_change_char_line1(79, 11);
					// move sword up
					if (enemy_position_length == 25) {
						lcd_send_custom_char(spear_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 0);
					} else if (enemy_position_length == 24) {
						lcd_send_custom_char(spear_enemy_parry, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 2);
					} else if (enemy_position_length == 23) {
						lcd_send_custom_char(spear_enemy_parry, 2, 1);
						lcd_send_custom_char(enemy_spear, 3, 3);
					} else if (enemy_position_length == 22) {
						lcd_send_custom_char(spear_enemy_parry, 2, 2);
						lcd_send_custom_char(enemy_spear, 3, 4);
					} else if (enemy_position_length == 21) {
						lcd_send_custom_char(spear_enemy_parry, 2, 3);
						lcd_change_char_line2(32, 8);
					} else if (enemy_position_length == 4) {
						lcd_send_custom_char(spear_enemy_parry, 2, 2);
						lcd_send_custom_char(enemy_spear, 3, 4);
						lcd_change_char_line2(3, 8);
					} else if (enemy_position_length == 3) {
						lcd_send_custom_char(spear_enemy_parry, 2, 1);
						lcd_send_custom_char(enemy_spear, 3, 3);
					} else if (enemy_position_length == 2) {
						lcd_send_custom_char(spear_enemy_parry, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 2);
					} else if (enemy_position_length == 1) {
						lcd_send_custom_char(spear_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 0);
					}
				} else if(enemy_position == 4) { // attack
					// write symbol
					lcd_change_char_line1(60, 11);
					// move sword to attack position
					if (enemy_position_length == 25) {
						lcd_send_custom_char(spear_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 0);
					} else if (enemy_position_length == 24) {
						lcd_send_custom_char(sword_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 2);
					} else if (enemy_position_length == 23) {
						lcd_send_custom_char(sword_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 10);
					} else if (enemy_position_length == 10) {
						lcd_send_custom_char(sword_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 2);
					} else if (enemy_position_length == 9) {
						lcd_send_custom_char(sword_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 5);
					} else if (enemy_position_length == 8) {
						lcd_send_custom_char(sword_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 6);
					} else if (enemy_position_length == 7) {
						lcd_send_custom_char(sword_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 11);
						if(position == 0 || position == 1 || position == 4 || position == 5) {
							health = health - 1;
							lcd_change_char_line2(health+48, 1);
						} else if(position == 6) {
							health = 0;
							lcd_change_char_line2(health+48, 1);
						} else if(position == 3) {
							enemy_posture = enemy_posture - 1;
							lcd_change_char_line2(enemy_posture+48, 13);
							if(enemy_posture == 0) {
								enemy_position = 6;
								enemy_position_length = 9;
							}
						}
					} else if (enemy_position_length == 2) {
						lcd_send_custom_char(sword_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 6);
					} else if (enemy_position_length == 1) {
						lcd_send_custom_char(spear_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 0);
					}
				} else if(enemy_position == 6) { // staggared
					// write symbol
					lcd_change_char_line1(32, 11);
					// show player in staggared pose
					if(enemy_position_length == 5) {
						lcd_send_custom_char(spear_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 0);
					} else if(enemy_position_length == 5) {
						lcd_send_custom_char(spear_enemy_staggared, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 1);
					} else if(enemy_position_length == 4) {
						lcd_send_custom_char(spear_enemy_staggared, 2, 1);
						lcd_send_custom_char(enemy_spear, 3, 5);
					} else if(enemy_position_length == 3) {
						lcd_send_custom_char(spear_enemy_staggared, 2, 2);
						lcd_send_custom_char(enemy_spear, 3, 8);
					} else if(enemy_position_length == 2) {
						lcd_send_custom_char(spear_enemy_staggared, 2, 3);
						lcd_send_custom_char(enemy_spear, 3, 9);
					} else if(enemy_position_length == 1) {
						lcd_send_custom_char(spear_enemy_staggared, 2, 4);
					}
				} else if(enemy_position == 8) { // thrust attack
					// write symbol
					lcd_change_char_line1(60, 11);
					// move sword to attack position
					if (enemy_position_length == 25) {
						lcd_send_custom_char(spear_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 0);
					} else if (enemy_position_length == 24) {
						lcd_send_custom_char(spear_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_spear, 3, 1);
					} else if (enemy_position_length == 23) {
						lcd_send_custom_char(spear_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 0);
					} else if (enemy_position_length == 9) {
						lcd_send_custom_char(spear_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_spear, 3, 1);
					} else if (enemy_position_length <= 8 && enemy_position_length >= 3) {
						lcd_send_custom_char(sword_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 5);
						if(enemy_position_length == 3 && (position == 0 || position == 2 || position == 3 || position == 4 || (position == 5 && (position_length > 15 || position_length <= 5)))) {
							health = health - 1;
							lcd_change_char_line2(health+48, 1);
						} else if(position == 6) {
							health = 0;
							lcd_change_char_line2(health+48, 1);
						} else if(position == 5 && position_length <= 15 && position_length > 5) {
							enemy_posture = enemy_posture - 1;
							lcd_change_char_line2(enemy_posture+48, 13);
							if(enemy_posture == 0) {
								enemy_position = 6;
								enemy_position_length = 5;
							} else {
								enemy_position = 12;
								enemy_position_length = 40;
							}
						}
					} else if (enemy_position_length == 2 ) {
						lcd_send_custom_char(spear_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_spear, 3, 1);
					} else if (enemy_position_length == 1) {
						lcd_send_custom_char(spear_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 0);
					}
				} else if(enemy_position == 10) { // death
					// write symbol
					lcd_change_char_line1(32, 11);
					if(enemy_position_length == 7) {
						lcd_send_custom_char(spear_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 0);
					} else if(enemy_position_length == 7) {
						lcd_send_custom_char(spear_enemy_staggared, 2, 2);
						lcd_send_custom_char(enemy_spear, 3, 8);
					} else if(enemy_position_length == 6) {
						lcd_send_custom_char(spear_enemy_staggared, 2, 3);
						lcd_send_custom_char(enemy_spear, 3, 9);
					} else if(enemy_position_length == 5) {
						lcd_send_custom_char(sword_enemy_dead, 2, 0);
						lcd_send_custom_char(sword_enemy_dead, 3, 5);
					} else if(enemy_position_length == 4) {
						lcd_send_custom_char(sword_enemy_dead, 2, 1);
						lcd_send_custom_char(sword_enemy_dead, 3, 6);
					} else if(enemy_position_length == 3) {
						lcd_send_custom_char(sword_enemy_dead, 2, 2);
						lcd_send_custom_char(sword_enemy_dead, 3, 6);
					} else if(enemy_position_length == 2) {
						lcd_send_custom_char(sword_enemy_dead, 2, 3);
						lcd_send_custom_char(sword_enemy_dead, 3, 7);
					} else if(enemy_position_length == 1) {
						lcd_send_custom_char(sword_enemy_dead, 2, 4);
						lcd_send_custom_char(sword_enemy_dead, 3, 7);
					}
				} else if(enemy_position == 12) { // mikiri countered
					// write symbol
					lcd_change_char_line1(32, 11);
					// move sword up
					if (enemy_position_length == 40) {
						lcd_send_custom_char(sword_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 5);
					} else if (enemy_position_length == 39) {
						lcd_send_custom_char(sword_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 6);
					} else if (enemy_position_length == 38) {
						lcd_send_custom_char(sword_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 7);
					} else if (enemy_position_length == 4) {
						lcd_send_custom_char(sword_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 6);
						lcd_change_char_line2(3, 8);
					} else if (enemy_position_length == 3) {
						lcd_send_custom_char(sword_enemy_attack, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 5);
					} else if (enemy_position_length == 2) {
						lcd_send_custom_char(spear_enemy_idle, 2, 1);
						lcd_send_custom_char(enemy_spear, 3, 1);
					} else if (enemy_position_length == 1) {
						lcd_send_custom_char(spear_enemy_idle, 2, 0);
						lcd_send_custom_char(enemy_spear, 3, 0);
					}
				} 
			// ONO SAMURAI ===========================================================
			} else if(enemy_number == 2) { // if Ono Samurai
				if(enemy_position == 0) { // idle pose
					// animate enemy
					if(loop == 0) {
						lcd_send_custom_char(fat_enemy_idle, 2, 0);
					} else if (loop == 10) {
						lcd_send_custom_char(fat_enemy_idle, 2, 1);
					}
				}
			// KATANA SAMURAI ==============================================================
			} else if(enemy_number == 3) { // if Katana Samurai
				if(enemy_position == 0) { // idle pose
					// animate enemy
					if(loop == 0) {
						lcd_send_custom_char(samurai_enemy_idle, 2, 0);
					} else if (loop == 10) {
						lcd_send_custom_char(samurai_enemy_idle, 2, 1);
					}
				}
			// NAGINATA RONIN ===========================================================
			} else if(enemy_number == 4) { // if Naginata Ronin
				if(enemy_position == 0) { // idle pose
					// animate enemy
					if(loop == 0) {
						lcd_send_custom_char(ronin_enemy_idle, 2, 0);
					} else if (loop == 10) {
						lcd_send_custom_char(ronin_enemy_idle, 2, 1);
					}
				}
			}

			// see if the enemy finished the current move
			if(enemy_position_length > 0) {
				enemy_position_length--;
			} else if(enemy_position_length == 0 && enemy_position == 0) {
				enemy_position_length = -1;
			} else if(enemy_position_length == 0 && (enemy_position < 6 || enemy_position == 8 || enemy_position == 12)) {
				enemy_position = 0;
				enemy_position_length = 10;
				// delete symbol
				lcd_change_char_line1(32, 11);
				// move enemy to idle position
				lcd_change_char_line1(32, 10);
				lcd_change_char_line1(32, 9);
				lcd_change_char_line2(32, 10);
				lcd_change_char_line2(2, 9);
				lcd_change_char_line2(3, 8);
			} else if(enemy_position_length == 0 && enemy_position == 10) {
				enemy_position_length = -2;
				button_unlock();
			} else if(enemy_position_length == 0 && enemy_position == 6) {
				enemy_position_length = -2;
			}

			// end the game if one died =============================================================
			if(health == 0 && end == 0) {
				// initialize custom chars for the player
				position = 10;
				position_length = 7;
				enemy_position = 0;
				enemy_position_length = -2;
				end = 1;
				// delete symbol
				lcd_change_char_line1(32, 4);
				lcd_change_char_line1(32, 11);
				// move player to idle position
				lcd_change_char_line1(32, 5);
				lcd_change_char_line1(32, 6);
				lcd_change_char_line2(32, 7);
				lcd_change_char_line2(0, 6);
				lcd_change_char_line2(1, 7);
				lcd_change_char_line1(32, 8);
				lcd_change_char_line1(32, 9);
				lcd_change_char_line2(32, 10);
				lcd_change_char_line2(2, 9);
				lcd_change_char_line2(3, 8);
				// move sword and player to idle position
				lcd_send_custom_char(player_sword, 1, 0);
				lcd_send_custom_char(player_idle, 0, 0);
				if (enemy_number == 0){
					lcd_send_custom_char(enemy_sword, 3, 0);
					lcd_send_custom_char(sword_enemy_idle, 2, 0);
				} else if(enemy_number == 1) {
					lcd_send_custom_char(enemy_spear, 3, 0);
					lcd_send_custom_char(spear_enemy_idle, 2, 0);
				} else if(enemy_number == 2) {
					lcd_send_custom_char(enemy_axe, 3, 0);
					lcd_send_custom_char(fat_enemy_idle, 2, 0);
				} else if(enemy_number == 3) {
					lcd_send_custom_char(enemy_sword, 3, 0);
					lcd_send_custom_char(samurai_enemy_idle, 2, 0);
				} else if(enemy_number == 4) {
					lcd_send_custom_char(enemy_naginata, 3, 0);
					lcd_send_custom_char(ronin_enemy_idle, 2, 0);
				}
			} else if(enemy_health == 0 && end == 0) {
				// initialize custom chars for the enemy
				position = 0;
				position_length = -2;
				enemy_position = 10;
				enemy_position_length = 8;
				end = 1;
				// delete symbol
				lcd_change_char_line1(32, 4);
				lcd_change_char_line1(32, 11);
				// move player to idle position
				lcd_change_char_line1(32, 5);
				lcd_change_char_line1(32, 6);
				lcd_change_char_line2(32, 7);
				lcd_change_char_line2(0, 6);
				lcd_change_char_line2(1, 7);
				lcd_change_char_line1(32, 8);
				lcd_change_char_line1(32, 9);
				lcd_change_char_line2(32, 10);
				lcd_change_char_line2(3, 8);
				lcd_change_char_line2(2, 9);
				// move sword and player to idle position
				lcd_send_custom_char(player_sword, 1, 0);
				lcd_send_custom_char(player_idle, 0, 0);
				if (enemy_number == 0){
					lcd_send_custom_char(enemy_sword, 3, 0);
					lcd_send_custom_char(sword_enemy_idle, 2, 0);
				} else if(enemy_number == 1) {
					lcd_send_custom_char(enemy_spear, 3, 0);
					lcd_send_custom_char(spear_enemy_idle, 2, 0);
				} else if(enemy_number == 2) {
					lcd_send_custom_char(enemy_axe, 3, 0);
					lcd_send_custom_char(fat_enemy_idle, 2, 0);
				} else if(enemy_number == 3) {
					lcd_send_custom_char(enemy_sword, 3, 0);
					lcd_send_custom_char(samurai_enemy_idle, 2, 0);
				} else if(enemy_number == 4) {
					lcd_send_custom_char(enemy_naginata, 3, 0);
					lcd_send_custom_char(ronin_enemy_idle, 2, 0);
				}
				
			}
			// delay
			for(int k = 0; k<100; k++) for(int l=0; l<10000; l++);
		}
	}
	return 0;
}