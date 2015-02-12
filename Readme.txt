
This is a contribution by Mike Kelvin <mk101005@gmail.com>
 

The KST MBXXXMA (KST MB114MA, KST MB015MA, etc.) is an AM/FM tuner module
used on mid-end 
home theaters and sound systems (NAD, Marantz etc). It is made
by a company named Kwang Sung Electronics
(http://www.kse.com.hk/).



The module is based on a Sanyo LC72131 PLL and a tuner IC Sanyo LA1837. 
The RDS
is delivered from Rohm BU1924F. RDS demodulator can communicate with a 
microcontroller via 
serial bit stream clocked at 1187.5Hz.

In this repository you will find a BU1924 Arduino Library, along with a complete
radio tuner sketch to interface with the MB114MA module.

This sketch uses the Arduino SanyoCCD library: http://github.com/RodLophus/SanyoCCB

This is a contribution by Mike Kelvin <mk101005@gmail.com>