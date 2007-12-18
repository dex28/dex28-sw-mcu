
PRG            = lcdart
OBJ            = lcdart.o LCD.o A2D.o USART.o ATX.o Cadence.o Keyboard.o
MCU_TARGET     = atmega8
OPTIMIZE       = -Os

DEFS           =
LIBS           =

###############################################################################
# You should not have to change anything below here.
###############################################################################

CC             = avr-gcc
CCPPLUS        = avr-g++

###############################################################################
# Override is only needed by avr-lib build system.
###############################################################################

override CFLAGS   = -g -Wall $(OPTIMIZE) -mmcu=$(MCU_TARGET) $(DEFS)
override LDFLAGS  = -Wl,-Map,$(PRG).map

OBJCOPY        = avr-objcopy
OBJDUMP        = avr-objdump

all: $(PRG).elf lst text eeprom

$(PRG).elf: $(OBJ)
	$(CCPPLUS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf *.o $(PRG).elf *.eps *.png *.pdf *.bak 
	rm -rf *.lst *.map $(EXTRA_CLEAN_FILES)
	rm -rf $(PRG).bin $(PRG).hex $(PRG).srec
	rm -rf $(PRG)_eeprom.bin $(PRG)_eeprom.hex $(PRG)_eeprom.srec
	rm -rf $(PRG).ncb

lst:  $(PRG).lst

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

###############################################################################
# Rules for building the .text rom images
###############################################################################

text: hex bin srec

hex:  $(PRG).hex
bin:  $(PRG).bin
srec: $(PRG).srec

%.o: %.cpp
	avr-g++ -c $(CFLAGS) -o $@ $<

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@
	prog.bat

%.srec: %.elf
	$(OBJCOPY) -j .text -j .data -O srec $< $@

%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@

###############################################################################
# Rules for building the .eeprom rom images
###############################################################################

eeprom: ehex ebin esrec

ehex:  $(PRG)_eeprom.hex
ebin:  $(PRG)_eeprom.bin
esrec: $(PRG)_eeprom.srec

%_eeprom.hex: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@

%_eeprom.srec: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O srec $< $@

%_eeprom.bin: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O binary $< $@

###############################################################################

lcdart.o : Makefile lcdart.cpp LCD.h A2D.h USART.h Cadence.h Keyboard.h ATX.h

LCD.o : Makefile LCD.cpp LCD.h Cadence.h

A2D.o : Makefile A2D.cpp A2D.h

USART.o : Makefile USART.cpp USART.h

ATX.o : Makefile ATX.cpp ATX.h Cadence.h Keyboard.h LCD.h USART.h

Cadence.o : Makefile Cadence.cpp Cadence.h

Keyboard.o : Makefile Keyboard.cpp Keyboard.h

