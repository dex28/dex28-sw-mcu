
PRG            = USB-TAU-D
OBJ            = USB-TAU-D.o Cadence.o ELU28.o ELU28_Master.o ELU28_Slave.o
MCU_TARGET     = atmega128
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

#text: hex bin srec
text: hex

hex:  $(PRG).hex
bin:  $(PRG).bin
srec: $(PRG).srec

%.o: %.cpp
	avr-g++ -c $(CFLAGS) -o $@ $<

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

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

USB-TAU-D.o : Makefile USB-TAU-D.cpp FT245.h Cadence.h ELU28.h ELUFNC.h TAU-D.h

Cadence.o : Cadence.h

ELU28.o : ELU28.cpp ELU28.h ELUFNC.h

ELU28_Master.o : ELU28_Master.cpp ELU28.h ELUFNC.h

ELU28_Slave.o : ELU28_Slave.cpp ELU28.h ELUFNC.h

