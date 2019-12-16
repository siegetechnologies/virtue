# Elf Mangler

## Installation/Compiling
You need to install libr for this to work...

```bash
git clone https://github.com/radare/radare2.git
cd ./radare2/libr/
make install
```

Outside of that, it does not have any special dependencies.

Run ```make``` to compile

There are two testing programs. Run ```./fasttest.sh``` in the directories to run many rounds of testing.

Note, there is currently a small memory leak in libr. (may be fixed by now?)

## Usage
```bash
Usage: ./elf-parser [options] input_file [output_file]
	-s, --shuffle			Shuffle the sections
	-b, --bumpsize SIZE		Add hlt junk onto the end of most sections, default max size is 1024
	-c, --cruftsize SIZE		Enable inserting crufts into executable sections, default max size is 1024
	-C, --super-cruftables		Enable super-cruftables (implies -c)
	-m, --split-cruftables		Enable the split cruftable method
	-M, --confused-cruftables	Enable the confused cruftable method
	-S, --shimsize SIZE		Insert stack shims, default size is 1024
	-D, --dynshimsize SIZE		Insert dynamic shims (can be used with or without -S), default size is 256
	-p, --print			Print debugging ESIL output
	-r, --reorder-stack		Enable re-ordering the entry and leaves of functions
	-j, --no-jumptable-hack		Disable the system that fixes issues with relative jump tables
	-I, --information		Print information about symbol locations post-modification
	-?, --help			Print usage (this)
	--cruftprobability PROBABILITY	probability at which to insert cruftables per section
	--splitprobability PROBABILITY	probability at which to split a cruftable (if -m or -M enabled)
	--shimalign ALIGNMENT		number of bytes to align a shim to. Default is 16. Use 1 for no alignment. Required for functions that use certain SIMD instructions
	--seed SEED			Use SEED instead of a time based one
```

Options that are considered unstable and should only be used for experimentation:

* -S (stackshims)
* -D (dynamic stack shims)
* -r (stack reordering)


## Techniques used
### Shuffle Sections (-s)
Re-order the sections of the elf.

#### Notes
Use -ffunction-sections and -fdata-sections for best results (one function per section).

Similar to how openbsd re-links libc at every boot, but instead of shuffling the object files, this shuffles the sections inside the object files.

Requires the symbol table to be updated to reflect the new section order. (automatically handled)
Requires the rel and rela sections to be updated to point to the moved symbol table. (automatically handled)

#### Usefulness:
- Without knowledge of the section layout or section offsets, ROP and other attacks are less reliable.

#### Caveats:
- Small number of sections makes the total combinations of section ordering low, possible to guess.
- Might be possible to leak the section order.

### hltjunk (-b)
Add random amount of HLT instructions onto the end of most sections.

#### Notes
Can not be done to sections that use their size to calculate how many items are in them (such as symbol tables, reloc sections, etc). (automatically handled for most)

#### Usefulness
- Increases the randomness of the start of each section (as section sizes are not deterministic/guessable now).
- Knowledge of the order of sections alone will not be enough for ROP anymore.
- If someone attempts to ROP to a section, they must know the size of all the sections before it, or leak an offset to the section (such as leak a return address from the stack).
- Attempts to ROP outside of the original section data will hit a HLT, which is easily detectable.

#### Caveats
- Section data is still the same size and layout. So if an attacker can leak a reliable offset into the section (such as a return address on the stack), then an attacker can ROP successfully.
- Large sections of hlt instructions (0xf1) may be easily searched for with an egghunter.
- Increases the size of the binary.

### Cruftables (-c)
Insert random sized chunks of vestigial code into random locations of executable sections.

#### Notes
Without super-cruftables, can not insert into a "badsection/badzone", which is a relative jump.

Will update symbols, relocations to their new offsets.
Also takes special care with RELA addendums.

The code that is inserted will be composed of either Nops, or a relative jump followed by halt instructions. Nops are only used if the cruftable size is less than 3 bytes.

#### Usefulness
- Much less likely to leak a stable offset into a section. (the more cruftables and the larger the cruftables may be that are inserted between a leaked address and the jump target, the harder it will be to guess the actual address of the jump target).
- Attempts to ROP into a section may hit a HLT instruction, which is easily detectable.
- Obfuscation technique.

#### Caveats
- Potential to break some code.
- Can reduce performance of the modified binary. (may be an issue with the profiling/sensors?).
- Increases the size of the binary and memory usage.
- Requires the "jumptable hack" enabled (default) if there are any switch statements in the code.

### Super Cruftables (-C)
Gives cruftables the ability to insert anywhere in a section, including "badsections/badzones".

#### Notes
Modifies the relative jump offset to account for the added code.

#### Usefulness
- Increases where a cruftable might be inserted, which will improve the likelihood of a cruftable being between a leaked address and a ROP target.
- Obfuscation technique.

#### Caveats
- Currently, relative short jumps that do not have a near jump counterpart can only have cruftables inserted that don't adjust the opcode size. For example: If a JECXZ (short jump only, offset is only 1 byte) already jumps 100 bytes ahead, then only up to 27 bytes of cruftables can be inserted there. Potential room for improvement would be to replace these sorts of jumps with equivalent code that can do near jumps.

### Split Cruftables (-m)
Cruftable insertion will randomly and recursively split into smaller cruftables.

#### Usefulness
- Further increases obfuscation of binary. 
- Splits up large sections of HLT instructions. May be useful against an egghunter.

#### Caveats
- Further decreases performance of modified binary.
- More likely to insert NOPs or jumps, which will mean that a random jump into the cruftable is less likely to hit a HLT instruction and exit the cruftable successfully.

### Confused Cruftables (-M)
- Builds upon Split cruftables by inserting cruftables that are conditional jumps.
- Both branches of the inserted jumps will eventually exit the cruftable, but will take different paths through.

#### Usefulness
- Obfuscation technique.

#### Caveats
- Performance may decrease in a non-deterministic fashion.

### Stack Reordering (-r)
Reorders the instructions and the order in which registers are pushed/popped on the stack.

#### Notes
Only works on small sections right after an entry into a function and right before the exit(s) of a function (rets).

Will compute register dependencies to avoid hazards.

#### Usefulness
- Changes up commonly used ROP gadgets. Even with a working pointer to the gadget, the gadget might do different things than what was expected.
- Likely will remove the "BROP gadget", making BROP extremely difficult.
- Changes up the order of the stack. May make leaking information from the stack difficult.
- (may break backtraces).
- Obfuscation technique.

#### Caveats
- May break binaries.
- Potential to change performance (reordering of instructions).
- May break code that uses RBP-relative addressing for local variables. (Planned fix)

### Stack Shims (-S)
Changes functions so that they allocate a stack frame a random size larger than they need to.

#### Usefulness
- Changes stack offsets, May make guessing the offsets for leaking information, buffer overflows, and other stack based attacks difficult.
- May add extra space between the end of a stack buffer and the saved return address. Might help prevent buffer overflows and information leaks.
- Obfuscation technique.

#### Caveats
- Will likely break binaries.
- May increase the probability of having a stack overflow on deep recursion due to the larger stack frames.
- Potential to reduce performance. (larger stack frames wont fit as nicely into caches)
- Needs to be aligned if dealing with certain SSE/AVX instructions (default behavior is align to 16 bytes, can set with --shimalign 1 to not align at all)
- Will break code that uses RSP-relative addressing for arguments on the stack (eg: mov rax, [rsp + 0xa8], where the stack frame is sized a0 bytes) (Planned fix)

### Dynamic Stack Shims (-D)
Inserts code to allocate a randomly sized stack frame every time a function is called.

#### Usefulness
- Changes stack offsets, dynamically and constantly, during runtime. Will make buffer overflows, information leaks, and other stack based attacks very difficult and unpredictable.
- Obfuscation technique.

#### Caveats
- Will likely break binaries.
- May increase the probability of having a stack overflow on deep recursion due to the larger stack frames.
- Potential to reduce performance. (larger stack frames wont fit as nicely into caches, generating random sizes takes time)
- Needs to be aligned if dealing with certain SSE/AVX instructions (default behavior is align to 16 bytes)
- Will break code that uses RSP-relative addressing for arguments on the stack (same as normal stack shims) (Planned fix)
- May break code that uses RBP-relative addressing for local variables (variable might override saved rsp offset) (Planned fix)
