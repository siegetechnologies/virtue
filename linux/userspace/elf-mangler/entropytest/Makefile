CC := gcc
LD := ld
CCFLAGS := -Wall -g -Ofast
C_SRC := $(wildcard *.c)
OBJS := $(C_SRC:.c=.o)
LD_FLAGS := -lr_anal -lr_util

EXE := entropy

.PHONY: all
all: $(EXE)

$(EXE): $(OBJS)
	$(CC) $(CCFLAGS) $(LD_FLAGS) $^ -o $@

%.o : %.c
	$(CC) $(CCFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(EXE)
