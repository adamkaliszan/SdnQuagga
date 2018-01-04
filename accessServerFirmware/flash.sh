#!/bin/bash
avrdude -p m88p -P /dev/ttyUSB1 -c stk500v2 -U flash:w:./bin/Release/accessServer.hex

#sudo avrdude -p m88p -P /dev/ttyUSB0 -c stk500v2 -U lfuse:w:0xff:m -U hfuse:w:0xdf:m -U efuse:w:0xf9:m -u
