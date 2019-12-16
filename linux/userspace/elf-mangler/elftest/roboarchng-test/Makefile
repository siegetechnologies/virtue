CC = gcc
LDFLAGS =
CFLAGS = -Wall -Ofast
#CFLAGS = -Wall -Ofast -ffunction-section -fdata-sections
SHUFFLER = ../../elf-mangler.selfhosting -sbrCMS
OBJECTS = roboarchngasm.o stringlib.o
MODOBJECTS = $(OBJECTS:.o=.o.mod)
MODMODOBJECTS = $(MODOBJECTS:.o.mod=.o.mod.mod)

all: testsuite1 testsuite2 testsuite3
	@bash -c "diff <(./testsuite1 ./fibbo.rasm ./fibbo1.bin) <(./testsuite2 ./fibbo.rasm ./fibbo2.bin)"
#	@bash -c "diff <(valgrind -q ./testsuite1 ./fibbo.rasm ./fibbo3.bin 2>&1) <(valgrind -q ./testsuite2 ./fibbo.rasm ./fibbo4.bin 2>&1)"
	@bash -c "diff <(./testsuite1 ./fibbo.rasm ./fibbo5.bin) <(./testsuite3 ./fibbo.rasm ./fibbo6.bin)"
#	@bash -c "diff <(valgrind -q ./testsuite1 ./fibbo.rasm ./fibbo7.bin 2>&1) <(valgrind -q ./testsuite3 ./fibbo.rasm ./fibbo8.bin 2>&1)"
#	@bash -c "radiff2 ./*.bin"
	@bash -c "diff -q --from-file ./*.bin"
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
	@rm -f fibbo*.bin
