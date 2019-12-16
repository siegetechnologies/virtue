CC := gcc
LD := ld
#CCFLAGS := -Wall -Werror -g  -Ofast -I /usr/include/libr/ -ffunction-sections -fdata-sections
CCFLAGS := -Wall -Werror -g  -Ofast -fno-optimize-sibling-calls -I /usr/include/libr/ -ffunction-sections -fdata-sections
#CCFLAGS := -Wall -Werror -g  -Ofast -fno-optimize-sibling-calls -I /usr/include/libr/ -ffunction-sections -fdata-sections -fno-omit-frame-pointer
#CCFLAGS := -Wall -g  -Ofast -fno-optimize-sibling-calls -I /usr/include/libr/		##cruftables can break eh_frame FDEs if no -ffunction-sections
C_SRC := $(wildcard *.c)
OBJS := $(C_SRC:.c=.o)
MODOBJECTS = $(OBJS:.o=.o.mod)
LD_FLAGS := -lr_anal -lr_util -lr_asm
HEXEXTRACTOR := sh ./extract_hex.sh

EXE := elf-mangler

# from https://stackoverflow.com/questions/39892692/how-to-implement-make-install-in-a-makefile
#PREFIX is environment variable, but if it is not set, then set default value
ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

.PHONY: all
#all: $(EXE)
all: $(EXE) $(EXE).selfhosting

$(EXE): $(OBJS)
	$(CC) $(CCFLAGS) $^ $(LD_FLAGS) -o $@

$(EXE).selfhosting: $(EXE) $(MODOBJECTS)
	$(CC) $(CCFLAGS) $(MODOBJECTS) $(LD_FLAGS) -o $@

%.o : %.c
	$(CC) $(CCFLAGS) -c $< -o $@

%.o : %.asm
	yasm -felf64 $< -o $@

%.o.mod: %.o $(EXE)
	./$(EXE) -sbCM $< $@
# stack reodering breaks one odd part with the confused cruftables code... due to -foptimize-sibling-calls
# currently anything with the stack is considered "unstable"... that includes -r, -S, and -D


%.hex.h: %.o
	$(HEXEXTRACTOR) $< $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(EXE) $(MODOBJECTS) $(EXE).selfhosting

install: $(EXE)
	install $(EXE) $(PREFIX)/bin/
