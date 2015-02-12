#ifndef bu1924_h
#define bu1924_h

#include <inttypes.h>
#include "Arduino.h"
// The largest element of message is called group (104 bits)
// Each group consist of 4 blocks (26 bits each)
// Each block consists of data word (16 bits) and checkword (10 bits)
#define block_size	26
#define block_A		0
#define block_B		1
#define block_C		2
#define block_Ca	3
#define block_D		4

/*
Parity matrix
1000000000 0100000000 0010000000 0001000000
0000100000 0000010000 0000001000 0000000100
0000000010 0000000001 1011011100 0101101110
0010110111 1010000111 1110011111 1100010011
1101010101 1101110110 0110111011 1000000001
1111011100 0111101110 0011110111 1010100111
1110001111 1100011011
*/
const uint16_t  parity_matrix[block_size] ={                   
	0x0200,0x0100,0x0080,0x0040,  
	0x0020,0x0010,0x0008,0x0004,  
	0x0002,0x0001,0x02dc,0x016e,  
	0x00b7,0x0287,0x039f,0x0313,   
	0x0355,0x0376,0x01bb,0x0201,  
	0x03dc,0x01ee,0x00f7,0x02a7,  
	0x038f,0x031b,  
};  

/*
block	| offset word	|syndrome	|
offset	|		|		|
	|d9,d8,d7,...d0	| s9,s8,s7,...s0|
---------------------------------------------
A	|0011111100	|	1111011000
B	|0110011000	|	1111010100
C	|0101101000	|	1001011100
C'	|1101010000	|	1111001100
D	|0110110100	|	1001011000
*/
const uint16_t  syndrome[5] = { 0x03d8, 0x03d4, 0x025c, 0x03cc, 0x0258 };

class Bu1924 {
	public: 
		void init();
		int8_t isInitialized();
		char* get_ProgramService();   
		char* get_RadioText();
		void get_bit();
		void reset();
		static Bu1924* getInstance()
		{
			static Bu1924 singleton;
			return &singleton;
		}

	private:		
		volatile uint8_t detect_block;
		volatile uint8_t block_index;     
		volatile uint16_t dataBlock[4];
		volatile int8_t init_state;
		volatile uint32_t  bit_stream;
		volatile uint8_t bit_position;
		volatile int _data_pin;
		volatile int _cl_pin;		

		Bu1924();
		uint8_t cyclicRedundancyCheck(void);    
		void get_Blocks(void);
		void get_A_block(void);
		void get_B_block(void);
		void get_C_block(void);
		void get_D_block(void);

		volatile uint16_t programId; 
		char programServiceName[9]; 
		char radioText[2][65];   		
		volatile uint8_t psIndex;
		volatile uint8_t rtIndex;
		volatile uint8_t rtType;
		
		void decode();
		void updateProgramService();
		void updateRadioText();
};

#endif