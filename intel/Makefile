ALL_TESTS=AveragingFilter_01_07_15 Mandelbrot_12_17_14 BlackScholes_12_17_14 DCT_01_07_15

CC ?= clang
CXX ?= clang++

CILKFLAG ?= -fopencilk

.PHONY: default all check clean

default: all

TRIALS=0 1 2 3 4

all:
	$(foreach test,$(ALL_TESTS),make -C ${test};)

check:
	$(foreach test,$(ALL_TESTS),$(foreach var,$(TRIALS),make -C ${test} run;))

clean:
	$(foreach test,$(ALL_TESTS),make -C ${test} clean;)
