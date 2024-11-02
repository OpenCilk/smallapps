.PHONY: default check clean

default: all

all:
	make -C cilk5
	make -C intel

check:
	make -C cilk5 check
	make -C intel check

clean:
	make -C cilk5 clean
	make -C intel clean