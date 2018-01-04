#include "hardware.h"

struct buffer
{
    uint8_t writeIdx;
    uint8_t readIdx;
    uint8_t data[16];
};

volatile struct buffer txBuffer = {.writeIdx=0, .readIdx=0};

void portsInit()
{
  //PORTC0 - PWR6    PORTC4 - PWR2
  //PORTC1 - PWR5    PORTC5 - PWR1
  //PORTC2 - PWR4
  //PORTC3 - PWR3
  DDRC  = 0x3F;
  PORTC = 0;
  //PORTD0 - RxD     PORTD4 - kl3
  //PORTD1 - TxD     PORTD5 - Rs6
  //PORTD2 - Kl1     PORTD6 - Rs5
  //PORTD3 - Kl2     PORTD7 - Rs4
  DDRD  = 0xE2;

  PORTD = 0xC8;
  //PORTB0 - Rs2
  //PORTB1 - Rs3
  //PORTB2 - Rs1
  //PORTB3 - PWR7
  DDRB  = 0x0F;
  PORTB = 0x00;
}

void serialInit()
{
    UCSR0B = ((1<<RXCIE0)|(1<<TXEN0)|(1<<RXEN0));

    UBRR0L = 7;
    UBRR0H = 0;

}

int kl1pressed(void)
{
  return PIND & 0x04;
}

int kl2pressed(void)
{
  return PIND & 0x08;
}

int kl3pressed(void)
{
  return PIND & 0x10;
}

void forwardSerial(uint8_t serNo)
{
    switch(serNo)
    {
        case  1: forwardSerial1();  break;
        case  2: forwardSerial1();  break;
        case  3: forwardSerial1();  break;
        case  4: forwardSerial1();  break;
        case  5: forwardSerial1();  break;
        case  6: forwardSerial1();  break;
        default: disableSerialFw(); break;
    }
}

void powerOn(uint8_t devNo)
{
    switch(devNo)
    {
        case  0: powerOn0();  break;
        case  1: powerOn1();  break;
        case  2: powerOn2();  break;
        case  3: powerOn3();  break;
        case  4: powerOn4();  break;
        case  5: powerOn5();  break;
        case  6: powerOn6();  break;
        default: break;
    }
}

void powerOff(uint8_t devNo)
{
    switch(devNo)
    {
        case  0: powerOff0();  break;
        case  1: powerOff1();  break;
        case  2: powerOff2();  break;
        case  3: powerOff3();  break;
        case  4: powerOff4();  break;
        case  5: powerOff5();  break;
        case  6: powerOff6();  break;
        default: break;
    }
}

void sendData(uint8_t data)
{
  while(((txBuffer.writeIdx+1) & 0x0F) == txBuffer.readIdx)
        ; //Uwaga ten kod może źle działać. Zawiesi program, jeśli wartości zmiennych przechowane są w rejestrze
  txBuffer.data[txBuffer.writeIdx] = data;
  txBuffer.writeIdx++;
  txBuffer.writeIdx &= 0x0F;
  UCSR0B = ((1<<RXCIE0)|(1<<TXEN0)|(1<<RXEN0)|(1<<UDRIE0));
}

void allPowerOn()
{
    uint8_t devNo;
    for (devNo=0; devNo<=6; devNo++)
    {
        _delay_ms(100);
        powerOn(devNo);
    }
}

void allPowerOff()
{
    uint8_t devNo;
    for (devNo=0; devNo<=6; devNo++)
    {
        _delay_ms(100);
        powerOff(devNo);
    }
}

ISR(USART_UDRE_vect)
{
  if (txBuffer.readIdx == txBuffer.writeIdx)            // Sprawdzić czy bufor jest pusty
  {
    UCSR0B = ((1<<RXCIE0)|(1<<TXEN0)|(1<<RXEN0));
    return;
  }

  uint8_t tmp = txBuffer.data[txBuffer.readIdx];
  txBuffer.readIdx++;
  txBuffer.readIdx &= 0x0F;
  UDR0 = tmp;
}

ISR(USART_RX_vect)
{
  uint8_t data = UDR0;
  (void) data;
  //TODO dokończyć implementację
}
