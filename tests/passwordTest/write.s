.global write

.section my_code
write: push r0
push r1
push r2
push r3
push r4
ldr r0, [r6 + 12]
ldr r1, [r6 + 14]
wait: ldr r2, $0
cmp r0, r2
jeq break
ldr r3, [r1]
str r3, 0xFF00
ldr r3, $1
add r1, r3
sub r0, r3
jmp wait
break:
pop r4
pop r3
pop r2
pop r1
pop r0
ret




.end
