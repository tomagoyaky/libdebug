

all: test test32

test:
		gcc -o test test.c debug.c 


test32:
		gcc -m32 -o test32 test.c debug.c 

