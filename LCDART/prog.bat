@echo off

copy/y lcdart.hex \\BugsBunny\root\lcdart.hex

D:\cygwin\bin\ssh -l root BugsBunny avrdude -q -p m8 -c avrisp -e -U flash:w:lcdart.hex:i

D:\cygwin\bin\ssh -l root BugsBunny ./init_lcdart

REM "D:\Program Files\Atmel\AVR Tools\STK500\STK500" -dATMEGA8 -s -ccom4 -z -ms -iflcdart.hex -e -pf -vf
REM "D:\Program Files\Atmel\AVR Tools\STK500\STK500" -dATMEGA8 -s -ccom1 -ut5.1 -ua5.1 -z -ms -iflcdart.hex -e -pf -vf
REM "D:\Program Files\Atmel\AVR Tools\STK500\STK500" -dATMEGA8 -fC93F -FC93F
REM avrdude -p m8 -c STK500 -P com1 -e -m flash -fi -i lcdart.hex