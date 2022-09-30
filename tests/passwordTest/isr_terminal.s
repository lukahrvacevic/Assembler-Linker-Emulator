.global isr_terminal

.extern password, current_pos

.section isr
isr_terminal: push r0
push r1
push r2
push r3
ldr r0, 0xFF02
ldr r1, $password
ldr r2, current_pos
add r1, r2
ldr r2, [r1]
ldr r3, $0x00FF
and r2, r3
cmp r0, r2
jeq same
jmp endif
same: str r0, 0xFF00
ldr r0, current_pos
ldr r1, $1
add r0, r1
ldr r1, $0
cmp r0, r1
str r0, current_pos
endif:
pop r3
pop r2
pop r1
pop r0
ret

.end
