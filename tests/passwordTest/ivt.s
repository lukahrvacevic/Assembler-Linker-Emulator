.extern my_start, isr_timer, isr_terminal

.section ivt
.word my_start
.skip 2
.word isr_timer
.word isr_terminal

.end
