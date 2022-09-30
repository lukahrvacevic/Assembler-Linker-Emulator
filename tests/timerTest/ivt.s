.extern timer, my_start

.section ivt
.word my_start
.skip 2
.word timer
.skip 2

.end
