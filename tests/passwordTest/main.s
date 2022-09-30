.global my_start, question, password, password_len, question_len, current_pos

.extern write

.section my_code

my_start:
ldr r6, $0xFEFE
ldr r0, $question
push r0
ldr r0, question_len
push r0
call write
wait: ldr r0, password_len
ldr r1, current_pos
cmp r0, r1
jne wait
ldr r0, $0xA
str r0, 0xFF00
halt



.section my_data
question: .ascii "What's the password:"
.word 0x200A
question_len: .word 22
password: .ascii "sistemski_softver"
password_len: .word 17
current_pos: .word 0

.end
