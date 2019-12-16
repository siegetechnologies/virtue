CC := gcc
LD := ld
CCFLAGS := -Wall -g -Ofast
C_SRC := $(wildcard *.c)
OBJS := $(C_SRC:.c=.o)
LD_FLAGS :=

EXE := maven_us

ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

.PHONY: all
all: $(EXE)

$(EXE): $(OBJS)
	$(CC) $(CCFLAGS) $^ $(LD_FLAGS) -o $@

%.o : %.c
	$(CC) $(CCFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS)

install: $(EXE)
	install $(EXE) $(PREFIX)/bin/
