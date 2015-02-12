#include <inttypes.h>
#include "bu1924.h"
#include "Arduino.h"

//Checkword is last 10 bits of data block  
uint8_t Bu1924::cyclicRedundancyCheck()
{        
	uint8_t i = 0;
	uint8_t j = 0;
	uint16_t checkWord = 0;
	uint32_t k = 1;
	for(i=0;i<block_size;i++)
	{  
		if(bit_stream & (k<<i))
		{  
			checkWord^=(parity_matrix[25-i]);  
		}  
	}  
	for(j=0;j<5;j++)
	{  
		if(checkWord == syndrome[j])  
		{                  
			block_index = j;  
			return 1;  
		}  
		block_index = 0;
	} 
	return 0;
}


//interrupt handler
static void clock()
{
	noInterrupts();
	Bu1924::getInstance()->get_bit();
	interrupts();
}  

Bu1924::Bu1924()
{
	init_state = 0;
	detect_block = 1;
}


void Bu1924::init() {
	_data_pin = 2;
	_cl_pin = 3;
	// set Pin as Input
	pinMode(_data_pin, INPUT);
	pinMode(_cl_pin, INPUT);
	// enable pullup resistor
	digitalWrite(_data_pin, HIGH);
	digitalWrite(_cl_pin, HIGH);

	init_state = 1;
	//attach the interrupt	
	attachInterrupt(INT0, clock, RISING);
}

int8_t Bu1924::isInitialized()
{
	return init_state;
}

void Bu1924::get_bit(void)
{
	bit_stream <<= 1;
	if (PIND & _BV(3)) {
		bit_stream |= 0x00000001;
	}
	else{
		bit_stream &= 0xfffffffe;
	}
	bit_position += 1;
	get_Blocks();
}

void Bu1924::get_A_block()
{
	if(block_index == block_A)
	{  
		detect_block = block_B;  
		bit_position = 0;  
		dataBlock[0] = (uint16_t)(bit_stream>>10)&0xffff;                                   
	} 
	if(bit_position > block_size) bit_position = 0;    
}

void Bu1924::get_B_block()
{
	if(block_index == block_B)
	{  
		bit_position = 0;  
		detect_block = block_C;  
		dataBlock[1] = (uint16_t)(bit_stream >> 10) & 0xffff;
	} 
}

void Bu1924::get_C_block()
{
	if((block_index ==block_C)||(block_index ==block_Ca))
	{  
		bit_position = 0;
		detect_block = block_D;  
		dataBlock[2] = (uint16_t)(bit_stream >> 10) & 0xffff;
		return;
	}  
	detect_block = 2;
}

void Bu1924::get_D_block()
{
	if(block_index == block_D) //block D  
	{    
		detect_block = block_A;            
		bit_position = 0;        
		dataBlock[3] = (uint16_t)(bit_stream >> 10) & 0xffff;
		decode();          
	} 
}

void Bu1924::get_Blocks()
{
	switch(detect_block)  
	{  
		case block_A: 
			if(bit_position >=block_size && cyclicRedundancyCheck())                  
				get_A_block();
			break;  
		case block_B:  
			if(bit_position == block_size && cyclicRedundancyCheck())  
				get_B_block();                
			break;  
		case block_C:  
			if(bit_position == block_size)  
			{  
				bit_position = 0;   
				detect_block = block_B;                   
				if(cyclicRedundancyCheck())  
				{  
					get_C_block();                        
				}   
			}                  
			break;  
		case block_D:  
			if(bit_position == block_size)  
			{  
				bit_position = 0;  
				detect_block = block_B;
				if(cyclicRedundancyCheck())  
				{  
					get_D_block();                                                  
				}  
			}  
			break;  
		default:  
			detect_block = block_A;
			bit_position = 0;     
			break;  
	}  
}

void Bu1924::decode()
{ 		            
	switch( dataBlock[1]>>12 ) //group type   
	{  
	case 0:  
	case 15:   
		updateProgramService();  
		break;  
	case 2:   
		updateRadioText();  
		break;    
	default:                    
		break;  
	}  
}

void Bu1924::reset()
{
	memset(programServiceName,0,sizeof(programServiceName));
	memset(radioText[0],0,sizeof(radioText[0]));    
	memset(radioText[1],0,sizeof(radioText[1]));   
	detect_block = 0;
	bit_position = 0;
	block_index = 0;   
}

void Bu1924::updateProgramService()
{  
	programId = dataBlock[0];
	psIndex = dataBlock[1] & 0x03;
	programServiceName[psIndex*2]=dataBlock[3]>>8;;   
	programServiceName[psIndex*2+1]=dataBlock[3] & 0xFF; 
}

void Bu1924::updateRadioText()
{
	programId = dataBlock[0];
	rtIndex=dataBlock[1] &0x0f;
	rtType= (dataBlock[1]>>4) & 0x01;
	//if((dataBlock[1]>>11) & 0x01 == 0)  //64 chars  
	{   
		radioText[rtType][rtIndex*4]= dataBlock[2] >> 8;   
		radioText[rtType][rtIndex*4+1]= dataBlock[2] & 0xff;   
		radioText[rtType][rtIndex*4+2]= dataBlock[3] >> 8;    
		radioText[rtType][rtIndex*4+3]= dataBlock[3] & 0xff;   
	}  
	//else if ((dataBlock[1]>>11) & 0x01 == 1)
	//{
	//	radioText[rtType][rtIndex*2] = dataBlock[3] >> 8;
	//	radioText[rtType][rtIndex*2+1] = dataBlock[3] & 0xff;
	//}
}

char* Bu1924::get_ProgramService(void)
{
	return programServiceName; 
}

char* Bu1924::get_RadioText()
{ 
	return radioText[rtType];   
}



