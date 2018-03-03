src = $(wildcard src/*.c)
obj = $(src:.c=.o)
bin = avrfloppy
hex = $(bin).hex
eep = $(bin).eep

mcu_gcc = atmega88
mcu_dude = m88
mcu_minipro = ATMEGA88A

CC = avr-gcc
OBJCOPY = avr-objcopy

CFLAGS = -Os -pedantic -Wall -mmcu=$(mcu_gcc) -DF_CPU=16000000
LDFLAGS = -Wl,-Map,$(bin).map -mmcu=$(mcu_gcc)

.PHONY: all
all: $(hex) $(eep)

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

$(hex): $(bin)
	$(OBJCOPY) -j .text -j .data -O ihex -R .eeprom $< $@

$(eep): $(bin)
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@

.PHONY: fuses
fuses:
	avrdude -c usbtiny -p $(mcu_dude) -U lfuse:w:0xe6:m -U hfuse:w:0xdf:m -U efuse:w:0xf9:m

.PHONY: program
program: $(hex)
	avrdude -c usbtiny -p $(mcu_dude) -e -U flash:w:$(hex)

.PHONY: burn
burn:
	minipro -p $(mcu_minipro) -s -w $(hex)

.PHONY: burnfuses
burnfuses:
	echo 'fuses_lo = 0x00e6' >/tmp/fuses
	echo 'fuses_hi = 0x00df' >>/tmp/fuses
	echo 'fuses_ext = 0x00f9' >>/tmp/fuses
	echo 'lock_byte = 0x00ff' >>/tmp/fuses
	minipro -p $(mcu_minipro) -e -c config -w /tmp/fuses

.PHONY: clean
clean:
	rm -f $(bin) $(obj) $(hex) $(eep) $(bin).map
