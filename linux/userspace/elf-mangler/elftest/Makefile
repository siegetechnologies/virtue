CC = gcc
LDFLAGS =
CFLAGS = -Wall -Ofast -ffunction-sections -fdata-sections
#CFLAGS = -Wall -Ofast
SHUFFLER = ../elf-mangler.selfhosting -sbCMr --shimsize 16
OBJECTS = testsuite.o
MODOBJECTS = $(OBJECTS:.o=.o.mod)
MODMODOBJECTS = $(MODOBJECTS:.o.mod=.o.mod.mod)

all: testsuite1 testsuite2 testsuite3
	@bash -c "diff <(./testsuite1) <(./testsuite2)"
#	@bash -c "diff <(valgrind -q ./testsuite1 2>&1) <(valgrind -q ./testsuite2 2>&1)"
	@bash -c "diff <(./testsuite1) <(./testsuite3)"
#	@bash -c "diff <(valgrind -q ./testsuite1 2>&1) <(valgrind -q ./testsuite3 2>&1)"
	@echo "tests passed with no differences"
testsuite1: $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
testsuite2: $(MODOBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
testsuite3: $(MODMODOBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o.mod: %.o
	$(SHUFFLER) $< $@
%.o.mod.mod: %.o.mod
	$(SHUFFLER) $< $@

clean:
	@rm -f $(OBJECTS)
	@rm -f $(MODOBJECTS)
	@rm -f $(MODMODOBJECTS)
