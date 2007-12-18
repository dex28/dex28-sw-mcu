@echo off

"C:\Program Files\Atmel\AVR Tools\STK500\STK500" -dATMEGA16 -s -ccom1 -ms -ifusbpio.hex -e -pf

REM "C:\Program Files\Atmel\AVR Tools\STK500\STK500" -dATMEGA8 -fC93F -FC93F
REM avrdude -p m8 -c STK500 -P com1 -e -m flash -fi -i usbpio.hex