/*

The KST MBXXXMA (KST MB114MA, KST MB015MA, etc.) is an AM/FM tuner module used on mid-end
home theaters and sound systems (NAD, Marantz etc). It is made by a company named Kwang Sung Electronics
(http://www.kse.com.hk/).  

The module is based on a Sanyo LC72131 PLL and a tuner IC Sanyo LA1837.
The RDS is delivered from Rohm BU1924F. RDS demodulator can communicate with a microcontroller via 
serial bit stream clocked at 1187.5Hz.
The PLL can communicate with a microcontroller via the Sanyo CCB bus, and have
6 I/O pins which controls the tuner:

PLL pin    Direction       Function
BO1        PLL -> Tuner    Band selector (0 = FM; 1 = AM)
BO2        PLL -> Tuner    Band selector (0 = LW; 1 = MW)
BO3        PLL -> Tuner    Audio mode (0 = Stereo; 1 = Mono)
BO4        PLL -> Tuner    Band selector (0 = FM; 1 = AM)
IO1        Tuner -> PLL    Mute 
IO2        Tuner -> PLL    IF output (0 =  IF counter mode 1 = Normal tuner mode)

Note: IO1 must be set to "0" for the tuner to output the IF signal to the PLL.
This means the tuner will have to be muted every time one want the PLL to count
the IF frequency.



KST MBXXXMA module:

FM
Antenna +-----------------------+
	+---|                   |- RDS clock
	+---|			|- RDS data
	    |     KST MBXXXMA   |- Stereo
	    |                   |- Tuned
	    |                   |- Out-L
	    |                   |- Out-R
AM	    |                   |- VDD (12V)
Antenna |                       |- DO
	+---|                   |- CL
	|   |                   |- DI
	|   |                   |- CE
	+---|                   |- GND
	+-----------------------+

KST MBXXXMA to Arduino connections:
Arduino                KST MBXXXMA
A5 (DO)                  DI
A4 (CL)                  CL
A3 (DI)                  DO
A2 (CE)                  CE
A1 (ST)					 Stereo
D3 (RDS DI)				 RDS data
D2 (RDS CL)				 RDS Clock
GND                      GND

Note:   This example uses an analog keypad with the following schematics:

A0
|
2k2    |   330R        620R         1k          3k3
VCC -----\/\/\---+---\/\/\---+---\/\/\---+---\/\/\---+---\/\/\-----+----- GND
|           |           |           |             |
X SCAN_UP   X UP        X DOWN      X SCAN_DOWN   X BAND
|           |           |           |             |
GND         GND         GND         GND           GND

X = keys (N.O.)

*/

#include <inttypes.h>
#include <LiquidCrystal.h>
#include <avr/eeprom.h>
#include <SanyoCCB.h>
#include <bu1924.h>

// This example uses an analog 5-key keypad tied to A0
	#define KEYPAD_PIN A0
	#define STEREO_PIN A1 
// Keypad keys
	#define KEY_BAND      5
	#define KEY_SCAN_DOWN 4
	#define KEY_DOWN      3
	#define KEY_UP        2
	#define KEY_SCAN_UP   1
	#define KEY_NONE      0

// LC72131 Adresses
	#define IN1_ADDRESS 0x82
	#define IN2_ADDRESS 0x92
	#define OUT_ADDRESS 0xA2
// LC72131 IN1, byte 0
//--------------------------------------------------------------
//--------------------------------------------------------------
	/*
	R3		R2		R1		R0		Reference freq(kHz)
	0		0		0		0		100
	0		0		0		1		50
	0		0		1		0		25
	0		0		1		1		25
	0		1		0		0		12.5
	0		1		0		1		6.25
	0		1		1		0		3.125
	0		1		1		1		3.125
	1		0		0		0		10
	1		0		0		1		9
	1		0		1		0		5
	1		0		1		1		1
	1		1		0		0		3
	1		1		0		1		15
	1		1		1		0		PLL INHIBIT + XTAL OSC STOP
	1		1		1		1		PLL INHIBIT
	*/
	#define IN10_R3     7
	#define IN10_R2     6
	#define IN10_R1     5
	#define IN10_R0     4
	/*
		XS		Resonator
		0		4.5 Mhz
		1		7.2 MHz
	*/
	#define IN10_XS     3
	/*
		CTE  IFCounterEnable
		0	Counter reset
		1	Counter start
	*/
	#define IN10_CTE    2
	/*
	DVS		SNS		Input Pin	Freq.Range
	1		*		FMIN		10-160 Mhz
	0		1		AMIN		2-40 Mhz
	0		0		AMIN		0.5-10	MHz
	*/
	#define IN10_DVS    1
	#define IN10_SNS    0

// LC72131 IN2, byte 0
//--------------------------------------------------------------
//--------------------------------------------------------------
	#define IN20_TEST2  7 //LSI Test data, must be set to 0
	#define IN20_TEST1  6
	#define IN20_TEST0  5
	#define IN20_IFS    4 // IF sensitivity IFS=0 => input sensitivity degradation
	#define IN20_DLC    3 // Dead lock control, DLC=1 =>	force oscillator control voltage low
	#define IN20_TBC    2 // Time base signal control, TBC=1 => 8Hz at BO1	
	/*
	GT1		GT0		Measure time[ms]		Wait Time[ms]
	0		0		4						3-4
	0		1		8						3-4
	1		0		32						7-8
	1		1		64						7-8
	*/
	#define IN20_GT1    1
	#define IN20_GT0    0

// LC72131 IN2, byte 1
//--------------------------------------------------------------
//--------------------------------------------------------------
	//Phase comparatror dead zone
	#define IN21_DZ1    7
	#define IN21_DZ0    6
	//PLL unlock detection
	#define IN21_UL1    5
	#define IN21_UL0    4
	//DO pin control
	#define IN21_DOC2   3
	#define IN21_DOC1   2
	#define IN21_DOC0   1
	//Dont care, must be set to 0 :)
	#define IN21_DNC    0

// LC72131 IN2, byte 2
//--------------------------------------------------------------
//--------------------------------------------------------------
	#define IN22_BO4    7
	#define IN22_BO3    6
	#define IN22_BO2    5
	#define IN22_BO1    4
	#define IN22_IO2    3
	#define IN22_IO1    2
	/*
	IO control of IO1,IO2: 0 = input mode, 1 = output mode
	*/
	#define IN22_IOC2   1
	#define IN22_IOC1   0

// LC72131 DO0, byte 0
//--------------------------------------------------------------
//--------------------------------------------------------------
	#define DO0_IN2     7
	#define DO0_IN1     6
	#define DO0_UL      4

// For function SetMode
	#define MONO    1
	#define STEREO  2
	#define MUTE    3
	#define UNMUTE  4
	#define BAND_FM 5
	#define BAND_AM 6

// Acceptable IF frequency deviation window (for the PLL) when searching for radio stations
// You may need to tweek these values to have a reliable "scan" mode
	#define FM_TUNED_WINDOW 180  //1600
	#define AM_TUNED_WINDOW 20  //80
	
	#define AM_min 53    // KHZ / 10
	#define AM_max 170   // KHZ / 10
	#define FM_min 880   // MHz * 10
	#define FM_max 1080  // MHz * 10

// Initial frequencies
uint16_t FMFrequency = FM_min;   // MHz * 10
uint16_t AMFrequency = AM_min;    // KHZ / 10

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
/*
LC72131 to Arduino connections :
Arduino                LC72131
A5(DO)                  DI
A4(CL)                  CL
A3(DI)                  DO
A2(CE)                  CE
GND                     GND
*/
SanyoCCB ccb(A5, A4, A3, A2); // Pins: DO CL DI CE
//Transmit bytes
byte pll_in1[3];
byte pll_in2[3];

uint8_t band = BAND_FM;
uint8_t tuned = 0;
uint8_t scan = 0;
int8_t delta = 0;
uint8_t key = KEY_NONE;

/************************************************\
*               Init()                   *
* Initialize the PLL settings vectors with     *
* parameters common to booth AM and FM modes   *
\************************************************/
void Init() {
	memset(pll_in1, 0, 3);
	memset(pll_in2, 0, 3);
	bitSet(pll_in1[0], IN10_XS);	// select Xtal. frequency 7.2 Mhz
	bitSet(pll_in2[0], IN20_IFS);   // IF counter in normal mode
	bitSet(pll_in2[1], IN21_UL0);   // Phase error detection width = 0us, 0 = open
        bitSet(pll_in2[2], IN22_IOC1);
	bitSet(pll_in2[2], IN22_IOC2);
	bitSet(pll_in2[2], IN22_IO2);   // Mute off / normal tuner mode
        bitSet(pll_in2[2], IN22_IO1);	
        initStoredFrequency();
}

void storeEEPROM(int index, int value)
{
	if(loadEEPROM(index) != value)
	{
		eeprom_busy_wait();
		eeprom_write_word((uint16_t*)(index * 2), (uint16_t)value);  //mult by 2 to store uint16 (2 bytes)
	}
}

int loadEEPROM(int index)
{
	eeprom_busy_wait();
	return (int)eeprom_read_word((uint16_t*)(index * 2)); //mult by 2 to store uint16 (2 bytes)
}

void initStoredFrequency()
{
	FMFrequency = loadEEPROM(0);
	AMFrequency = loadEEPROM(1);
}

void storeFrequency()
{	
	storeEEPROM(0, FMFrequency);
	storeEEPROM(1, AMFrequency);
}

void initStereoIndicator(uint8_t stereo_pin)
{
	pinMode(stereo_pin, INPUT);
	digitalWrite(stereo_pin, LOW);
}

/************************************************\
*              SetMode()                 *
* Some predefined setups for the LC72131 module *
\************************************************/
void SetMode(uint8_t mode) {
	switch (mode) {
	case STEREO:
		bitClear(pll_in2[2], IN22_BO3);
		break;

	case MONO:
		bitSet(pll_in2[2], IN22_BO3);
		break;

	case MUTE:
                bitClear(pll_in2[2], IN22_IO1);
		bitClear(pll_in2[2], IN22_IO2);                
		break;

	case UNMUTE:
                bitSet(pll_in2[2], IN22_IO1);
		bitSet(pll_in2[2], IN22_IO2);
		break;

	case BAND_FM:
		band = BAND_FM;
		bitWrite(pll_in1[0], IN10_R0, 1); // Reference frequency = 50kHz
		bitWrite(pll_in1[0], IN10_R3, 0); //
		bitWrite(pll_in1[0], IN10_DVS, 1); // Programmable Divider divisor = 2
		bitWrite(pll_in2[0], IN20_GT0, 0); // IF counter mesurement period = 32ms
		bitWrite(pll_in2[0], IN20_GT1, 1); //
		bitWrite(pll_in2[1], IN21_DZ0, 1); // Dead zone = DZB, 0 = DZA
		bitWrite(pll_in2[1], IN21_DZ1, 0); //
		bitWrite(pll_in2[2], IN22_BO1, 1); // FM mode
		//bitWrite(pll_in2[2], IN22_BO4, 0); // FM mode
		break;

	case BAND_AM:
		band = BAND_AM;
		bitWrite(pll_in1[0], IN10_R0, 0); // Reference frequency = 10kHz
		bitWrite(pll_in1[0], IN10_R3, 1); //
		bitWrite(pll_in1[0], IN10_DVS, 0); // Programmable Divider divisor = 1
		bitWrite(pll_in2[0], IN20_GT0, 1); // IF counter mesurement period = 8ms
		bitWrite(pll_in2[0], IN20_GT1, 0); //
		bitWrite(pll_in2[1], IN21_DZ0, 0); // Dead zone = DZC
		bitWrite(pll_in2[1], IN21_DZ1, 1); //
		bitWrite(pll_in2[2], IN22_BO1, 0); // AM mode
		//bitWrite(pll_in2[2], IN22_BO4, 1); // AM mode
		break;
	}
	ccb.write(IN1_ADDRESS, pll_in1, 3);
	ccb.write(IN2_ADDRESS, pll_in2, 3);
}
uint16_t fpd = 0;
void updateFrequencyProgrammableDivider(uint16_t frequency)
{
  	switch (band) {
	case BAND_FM:
		// FM: fpd = (frequency + FI) / (50 * 2)
		fpd = (frequency + 107);
		break;

	case BAND_AM:
		// AM: fpd = ((frequency + FI) / 10) << 4
		fpd = (frequency + 45) << 4;
		break;
	}
}

void resetCounter()
{
	bitClear(pll_in1[0], IN10_CTE);        
	pll_in1[1] = byte(fpd >> 8);
	pll_in1[2] = byte(fpd & 0x00ff);
	ccb.write(IN1_ADDRESS, pll_in1, 3);
}

void startCounter()
{
	// Start the IF counter
	bitSet(pll_in1[0], IN10_CTE);
	ccb.write(IN1_ADDRESS, pll_in1, 3);
}

unsigned long IFCounter = 0;

/************************************************************\
*                       Tune()                       *
* Set the tuner frequency and return 1 if it is tuned      *
* or 0 otherwise.                                          *
*                                                          *
* The frequency divisors was chosen in a way the frequency *
* representation can be directly sent to the PLL and is    *
* easy to represent:                                       *
* - FM mode (divisor = 100): frequency (MHz) * 10          *
* - AM mode (divisor = 10):  frequency (kHZ) / 10          *
\************************************************************/
uint8_t tune(uint16_t frequency) {

	uint8_t i = 0;
	uint8_t r[3];
	IFCounter = 0;
	updateFrequencyProgrammableDivider(frequency);
 
	// Reset the IF counter and program the Frequency Programmable Divider (fpd)
        resetCounter();
	// Start the IF counter
	startCounter();
	// Wait for PLL to be locked (DO0_UL == 1)
	while (i < 20) {
		delay(50);
		ccb.read(OUT_ADDRESS, r, 3);  // Discard the 1st result: it is latched from the last count (as said on the datasheet)
		ccb.read(OUT_ADDRESS, r, 3);  // The 20 rightmost bits from r[0..2] are the IF counter result
		i = (bitRead(r[0], DO0_UL)) ? 100 : i + 1;
	}
	//set to stereo
	if(band == BAND_FM)
		bitClear(pll_in2[2], IN22_BO3);	
	IFCounter = (r[0] & 0x0f);
	IFCounter = (IFCounter << 16) | (unsigned long)(r[1] << 8) | (r[2]);
	//Serial.println(IFCounter);

	if (i == 100) return isPLLLocked();
	return 0;
}

/**************************************************\
*                   isPLLLocked()                 *
* Returns 1 if PLL is locked					  *
\**************************************************/
int8_t isPLLLocked()
{
	// PLL is locked.  If the IF deviation is within the defined (window) interval,
	// the radio is likely to be tuned.
	// Note: this "tuned" detection method is not recommended on the  datasheet as 
	// it can give false positive results.  A better approach would be to get the "tuned"
	// flag from a RDS decoder with signal quality detection (e.g.: PT2579 or Philips SAA6588)
	// connected to the  tuner module "RDS Output" pin. SAA6588 is more powerful/popular, 
	// but PT2579 looks a lot easier to use as it is not programmable and has a dedicated
	// signal quality output pin.
	switch (band) {
	case BAND_FM:
		// Expected value: IF (10.7MHz) * Mesurement period (32ms, as set via GT[1..0]) = 342400
		if ((IFCounter > 342400) && ((IFCounter - 342400) < FM_TUNED_WINDOW)) return 1;
		if ((IFCounter < 342400) && ((342400 - IFCounter) < FM_TUNED_WINDOW)) return 1;
		break;
	case BAND_AM:
		// Expected value: IF (450kHz) * Mesurement period (8ms, as set via GT[1..0]) = 3600
		// Note: scan mode in AM is really poor.  I have done my best in tweaking it, but it barely works
		if ((IFCounter > 3600) && ((IFCounter - 3600) < AM_TUNED_WINDOW)) return 1;
		if ((IFCounter < 3600) && ((3600 - IFCounter) < AM_TUNED_WINDOW)) return 1;
		break;
	}
	return 0;  
}

/**************************************************\
*                   IsStereo()             *
* Returns 1 if the tuned radio station is stereo *
\**************************************************/
uint8_t isStereo() {
	//keep ccb alive
	uint8_t r[3];
	//ccb.read(OUT_ADDRESS, r, 3);	
	return digitalRead(STEREO_PIN) ? 0 : 1;
}


/*******************************************************\
*                       getKey()                      *
* Read the (analog) keypad.                           *
* If you are using an digital (one key per input pin) *
* keypad, just this function to return the correct    *
* values                                              *
\*******************************************************/
uint8_t getKey(uint8_t keypadPin) {
	uint16_t keypad;
	uint8_t key = KEY_NONE;

	keypad = analogRead(keypadPin);

	if (keypad < 870) key = KEY_BAND;
	if (keypad < 600) key = KEY_SCAN_DOWN;
	if (keypad < 390) key = KEY_DOWN;
	if (keypad < 220) key = KEY_UP;
	if (keypad < 60)  key = KEY_SCAN_UP;

	return key;
}


/*******************\
* Arduino setup() *
\*******************/
void setup() {
	lcd.begin(16, 2);
	ccb.init();
	delay(1000);
	Init();
	initStereoIndicator(STEREO_PIN);
	SetMode(BAND_FM);
}

/**********************************\
* Updates the Stereo/Band indicator *
\**********************************/
void updateStereoIndicator()
{
	lcd.setCursor(14, 0);
	if (isStereo() && band == BAND_FM)
		lcd.print("ST");
	else
    switch(band){
      case BAND_FM:
        lcd.print("FM ");
        break;
      case BAND_AM:
        lcd.print("AM ");
        break;
    }
}

/********************************\
* Updates the Frequency indicator *
\********************************/
void updateFrequencyIndicator()
{
  lcd.setCursor(8, 0);
  char buffer[5];
  char strbuf[5];
  memset(buffer, 0, sizeof(buffer));
  memset(strbuf, 0, sizeof(buffer));
	switch (band) {
  	case BAND_FM:
                dtostrf((float)FMFrequency / 10, 3, 1, strbuf);
                sprintf(buffer, "%5s", strbuf);                                 
  		break;
  	case BAND_AM:		
  		sprintf(buffer, "%5u", AMFrequency * 10);  		
  		break;
	}
  lcd.print(buffer);
}

/********************************\
* Updates the Scanning indicator *
\********************************/
void updateScanningIndicator()
{
	if (scan) {
		//clear program service name
		lcd.setCursor(0, 0);
		lcd.print("        ");
		//show scanning info
		lcd.setCursor(0, 1);
		lcd.print("  Scanning...   ");               
	}
}

/********************************\
* Updates the Tuned indicator *
\********************************/
void updateTunedIndicator()
{
	// The "Tuned" indicator is updated only when the station changes
	lcd.setCursor(0, 1);
	if (tuned){
		lcd.print("     Tuned      ");
	}
	else
	{
		lcd.print("                ");
	}
}

/********************************\
* Updates the RDS indicator		*
\********************************/
void updateRDSIndicator()
{	
	lcd.setCursor(0, 0);
	char buffer[9];
	memset(buffer, 0, sizeof(buffer));
	sprintf(buffer, "%8s", Bu1924::getInstance()->get_ProgramService());
	lcd.print(buffer);
	lcd.setCursor(0, 1);
	//lcd.print(Bu1924::getInstance()->get_Trace());  
	lcd.print(rotateRadioText());  
}

char lcdBuffer[16];
uint8_t offset = 0;
unsigned long time = 0;
char* rotateRadioText()
{
	// 64 chars to rotate
	memset(lcdBuffer, 0, sizeof(lcdBuffer));	
	uint8_t lenght = strlen(Bu1924::getInstance()->get_RadioText());
	if(millis() - time > 500)
	{
		time = millis();
		if (offset + 1 < lenght - 15) 
			offset += 1;
		else offset = 0;
	}	
	memcpy(lcdBuffer,Bu1924::getInstance()->get_RadioText()+offset,lenght > 16 ? 16 : lenght);
	
}
unsigned long debounce = 0;
void handleKeypad()
{
  	if(millis() - debounce > 1000)
	{
		debounce = millis();		
  	        switch (key) {
          	case KEY_UP:        scan = 0; delta = +1; break;
  	        case KEY_DOWN:      scan = 0; delta = -1; break;
  	        case KEY_SCAN_UP:   scan = 1; delta = +1; break;
  	        case KEY_SCAN_DOWN: scan = 1; delta = -1; break;
  	        case KEY_BAND:		scan = 0; delta = 0;
  		switch (band) {
  		  case BAND_FM:
  			SetMode(BAND_AM);
  			break;
  
  		  case BAND_AM:
  			SetMode(BAND_FM);
  			break;
  		}
  	  }
      }
}

void changeFrequency()
{
	uint16_t Frequency = AM_min;
	//  only injects FI signal into the PLL when in MUTE mode
	SetMode(MUTE); 
	do{
		switch (band) {
		case BAND_FM:
			FMFrequency += delta;
			if (FMFrequency > FM_max) FMFrequency = FM_min;
			if (FMFrequency < FM_min)  FMFrequency = FM_max;
			Frequency = FMFrequency;
			break;
		case BAND_AM:
			AMFrequency += delta;
			if (AMFrequency > AM_max) AMFrequency = AM_min;
			if (AMFrequency < AM_min)  AMFrequency = AM_max;
			Frequency = AMFrequency;
			break;
		}
		tuned = tune(Frequency);
		if (tuned)
		{
			storeFrequency();
		}
		updateFrequencyIndicator();
        	updateScanningIndicator();
        	key = getKey(KEYPAD_PIN);
		handleKeypad();
	} while (scan && (!tuned));
	// Mute off / normal tuner mode
	SetMode(UNMUTE); 
}

/******************\
* Arduino loop() *
\******************/
void loop() {
	updateStereoIndicator();
	key = getKey(KEYPAD_PIN);
	if (key != KEY_NONE) {
		handleKeypad();
		updateScanningIndicator();
		changeFrequency();
		updateTunedIndicator();
		Bu1924::getInstance()->reset();
	} 
	else
	{
		updateFrequencyIndicator();
		if(Bu1924::getInstance()->isInitialized() == 0){
			//initial tuning
			updateScanningIndicator();
			changeFrequency();
			updateTunedIndicator();
			Bu1924::getInstance()->reset();
		}	
		if (tuned){
			// start rds reception
			if(Bu1924::getInstance()->isInitialized() == 0)	{
				Bu1924::getInstance()->init(); 
			}				
			updateRDSIndicator();	        	
		}
	}
}


