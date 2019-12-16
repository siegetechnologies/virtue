CC = gcc
LDFLAGS =
CFLAGS = -Wall -Ofast -ffunction-sections -fdata-sections -fno-stack-protector
#CFLAGS = -Wall -Ofast
SHUFFLER = ../../elf-mangler -s
OBJECTS = ptrleakrop1.o
MODOBJECTS = $(OBJECTS:.o=.o.mod)

all: ptrleakrop1 ptrleakrop1mod

ptrleakrop1: $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
ptrleakrop1mod: $(MODOBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o.mod: %.o
	$(SHUFFLER) $< $@

clean:
	@rm -f $(OBJECTS)
	@rm -f $(MODOBJECTS)
