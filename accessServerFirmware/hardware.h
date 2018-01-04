#ifndef HARDWARE_H_INCLUDED
#define HARDWARE_H_INCLUDED

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

void portsInit(void);
void serialInit(void);

int kl1pressed(void);
int kl2pressed(void);
int kl3pressed(void);

void sendData(uint8_t data);


void forwardSerial(uint8_t serNo);
void powerOn(uint8_t devNo);
void powerOff(uint8_t devNo);

void allPowerOn();
void allPowerOff();


#define disableSerialFw() { PORTB |= 0x07; PORTD |= 0xE0; }

#define forwardSerial1() { disableSerialFw(); PORTB &= 0xFB; }
#define forwardSerial2() { disableSerialFw(); PORTB &= 0xFD; }
#define forwardSerial3() { disableSerialFw(); PORTB &= 0xFE; }
#define forwardSerial4() { disableSerialFw(); PORTD &= 0xEF; }
#define forwardSerial5() { disableSerialFw(); PORTD &= 0xDF; }
#define forwardSerial6() { disableSerialFw(); PORTD &= 0xBF; }

#define powerOn0()  { PORTC |= 0x20; }
#define powerOff0() { PORTC &= 0xDF; }
#define powerOn1()  { PORTC |= 0x10; }
#define powerOff1() { PORTC &= 0xEF; }
#define powerOn2()  { PORTC |= 0x08; }
#define powerOff2() { PORTC &= 0xF7; }
#define powerOn3()  { PORTC |= 0x04; }
#define powerOff3() { PORTC &= 0xFB; }
#define powerOn4()  { PORTC |= 0x02; }
#define powerOff4() { PORTC &= 0xFD; }
#define powerOn5()  { PORTC |= 0x01; }
#define powerOff5() { PORTC &= 0xFE; }
#define powerOn6()  { PORTB |= 0x08; }
#define powerOff6() { PORTB &= 0xF7; }


#define forwardSerial2() { disableSerialFw(); PORTB &= 0xFD; }
#define forwardSerial3() { disableSerialFw(); PORTB &= 0xFE; }
#define forwardSerial4() { disableSerialFw(); PORTD &= 0xEF; }
#define forwardSerial5() { disableSerialFw(); PORTD &= 0xDF; }
#define forwardSerial6() { disableSerialFw(); PORTD &= 0xBF; }


#endif // HARDWARE_H_INCLUDED
