
.global my_start, counter

.section my_code
my_start:
ldr r0, $3
str r0, 0xFF10
wait: ldr r0, counter
ldr r1, $10
cmp r0, r1
jne wait
halt

.section my_data
counter: .word 0

.end
