CC = gcc
LDFLAGS =
CFLAGS = -Wall -Os -ffunction-sections -fdata-sections -fno-stack-protector
#CFLAGS = -Wall -Ofast
SHUFFLER = ../../elf-mangler -S
OBJECTS = stackbreak.o
MODOBJECTS = $(OBJECTS:.o=.o.mod)

all: stackbreak stackbreakmod

stackbreak: $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
stackbreakmod: $(MODOBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o.mod: %.o
	$(SHUFFLER) $< $@

clean:
	@rm -f $(OBJECTS)
	@rm -f $(MODOBJECTS)
	@rm -f $(BETTERMODOBJECTS)
