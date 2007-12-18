@echo off

"C:\Program Files\Atmel\AVR Tools\STK500\STK500" -dATMEGA16 -s -ccom1 -z -ms -iflcdart.hex -e -pf -vf

REM "C:\Program Files\Atmel\AVR Tools\STK500\STK500" -dATMEGA8 -fC93F -FC93F

REM avrdude -p m8 -c STK500 -P com1 -e -m flash -fi -i lcdart.hex