
all: test test32


SRCS=src/test.c src/debug.c


test: $(SRCS)
		gcc -o test $(SRCS)

test32: $(SRCS)
		gcc -m32 -o test32 $(SRCS)

