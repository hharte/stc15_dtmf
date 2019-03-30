SDCCOPTS ?= --iram-size 256
PORT ?= /dev/ttyS13
STCGAL ?= stcgal
FLASHFILE ?= dtmf_dec.hex
SYSCLK ?= 11059

SRC = 

OBJ=$(patsubst src%.c,build%.rel, $(SRC))

all: dtmf_dec

build/%.rel: src/%.c
	mkdir -p $(dir $@)
	sdcc $(SDCCOPTS) -o $@ -c $<

dtmf_dec: $(OBJ)
	sdcc -o build/ src/$@.c $(SDCCOPTS) $^
	cp build/$@.ihx $@.hex
	
flash:
	$(STCGAL) -p $(PORT) -t $(SYSCLK) $(FLASHFILE)

clean:
	rm -f *.ihx *.hex *.bin
	rm -rf build/*

