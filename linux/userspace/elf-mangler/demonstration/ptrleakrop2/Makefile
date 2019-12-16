CC = gcc
LDFLAGS =
CFLAGS = -Wall -Os -ffunction-sections -fdata-sections -fno-stack-protector
#CFLAGS = -Wall -Ofast
SHUFFLER = ../../elf-mangler -sb
BETTERSHUFFLER = ../../elf-mangler --cruftsize 16
OBJECTS = ptrleakrop2.o
MODOBJECTS = $(OBJECTS:.o=.o.mod)
BETTERMODOBJECTS = $(OBJECTS:.o=.o.bettermod)

all: ptrleakrop2 ptrleakrop2mod ptrleakrop2bettermod

ptrleakrop2: $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
ptrleakrop2mod: $(MODOBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
ptrleakrop2bettermod: $(BETTERMODOBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o.mod: %.o
	$(SHUFFLER) $< $@
%.o.bettermod: %.o
	$(BETTERSHUFFLER) $< $@

clean:
	@rm -f $(OBJECTS)
	@rm -f $(MODOBJECTS)
	@rm -f $(BETTERMODOBJECTS)
