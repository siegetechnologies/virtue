section .text.insert_shim

insert_shim:
push rax
;//push all the registers that i need to
;//todo change this to rsp+addressing and clear the values to 0 after so less likely to get a leak
;//todo only push ones that I need to push?
push rcx
push rdx
push rsi
push rdi
push r8
push r9
push r10
push r11
;//what about floats? Im going to ignore them for now

;// call rand
;mov rax, 4	;//chosen by fair dice roll, gaurenteed to be random
;mov rax, 16	;//chosen by four fair dice roll, gaurenteed to be four times as random (for alignment tests)

rdrand ax
;rdrand eax
;rdrand rax
;//math to calculate out the shim

;//todo i need to add stuff in dynamically to do the alignment, maxsize, etc
;//for now im going to align it to 16, and maxsize of 4080 or something

and rax, 0x0FF0


;// pop all the saved regs
;//todo zero em?

pop r11
pop r10
pop r9
pop r8
pop rdi
pop rsi
pop rdx
pop rcx

;// our stack pointer should be right at the saved rax

;// modify our stack
sub rsp, rax
add rax, 0x10	;// so we reset the stack to before we pushed the rax on it
push rax
mov rax, [rsp+rax-8]; //CISC ftw
;// could ZERO rsp+rax for less extra stack info
;mov [rsp+rax], 0
;// move rbp as well?
;mov rbp, rsp
;//orig stack frame alloc should go here


section .text.remove_shim

;// orig stack frame dealloc goes here
remove_shim:
add rsp, [rsp]
