.data

.code

daddiu $t0, $zero, 0
daddiu $t1, $zero, 2
dsubu $t4, $t0, $t1
sd $t4, 0($zero)
daddiu $t5, $zero, 4
daddiu $t6, $zero, 2
ddiv $t5, $t6
mflo $t9
sd $t9, 4($zero)
ld $t0, 0($zero)
ld $t1, 4($zero)
daddu $t2, $t0, $t1
sd $t2, 8($zero)
daddiu $t3, $zero, 0
daddiu $t4, $zero, 12
dsubu $t7, $t3, $t4
sd $t7, 12($zero)

