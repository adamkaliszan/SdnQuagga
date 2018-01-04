/*
 */

#include <avr/io.h>
#include <avr/fuse.h>

#include <avr/fuse.h>
#include <util/delay.h>


#include "hardware.h"

FUSES = { .low = FUSE_SUT0 & FUSE_SUT1 & FUSE_CKSEL3, .high = HFUSE_DEFAULT, .extended = EFUSE_DEFAULT };

int main(void)
{

  // Insert code

  portsInit();
  serialInit();


  allPowerOn();

  forwardSerial1();

  sei();
  UDR0 = '\r';
  UDR0 = '\n';

  sendData('A');
  sendData('d');
  sendData('a');
  sendData('m');
  sendData('\r');
  sendData('\n');


  while(1)
  {
    if (kl1pressed() == 0)
      powerOn(0);
    else
      powerOff(0);

    if (kl2pressed() == 0)
        powerOff(1);
    else
      powerOn(1);

    if (kl3pressed() == 0)
        powerOn(2);
    else
      powerOff(2);

    }



  return 0;
}
