.global timer

.extern counter

.section isr
timer:
push r0
push r1
ldr r1, $0xFF00
ldr r0, $84
str r0, [r1]
ldr r0, $73
str r0, [r1]
ldr r0, $67
str r0, [r1]
ldr r0, $75
str r0, [r1]
ldr r0, $10
str r0, [r1]
ldr r0, counter
ldr r1, $1
add r0,r1
str r0, counter
pop r1
pop r0
iret

.end
