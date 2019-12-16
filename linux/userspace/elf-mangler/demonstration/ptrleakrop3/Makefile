CC = gcc
LDFLAGS =
CFLAGS = -Wall -Os -ffunction-sections -fdata-sections -fno-stack-protector
#CFLAGS = -Wall -Ofast
SHUFFLER = ../../elf-mangler -sCM
OBJECTS = ptrleakrop3.o
MODOBJECTS = $(OBJECTS:.o=.o.mod)

all: ptrleakrop3 ptrleakrop3mod

ptrleakrop3: $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
ptrleakrop3mod: $(MODOBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o.mod: %.o
	$(SHUFFLER) $< $@

clean:
	@rm -f $(OBJECTS)
	@rm -f $(MODOBJECTS)
	@rm -f $(BETTERMODOBJECTS)
