ALL_TESTS = fft cholesky nqueens qsort rectmul strassen cilksort heat lu matmul fibred

CC ?= clang
CXX ?= clang++

CFLAGS = -Wall -O3
CXXFLAGS = -Wall -O3
LDFLAGS =
LDLIBS = # -ltcmalloc

CILKFLAG ?= -fopencilk

CFLAGS += $(CILKFLAG) $(EXTRA_CFLAGS)
CXXFLAGS += $(CILKFLAG) $(EXTRA_CFLAGS)
LDFLAGS += $(CILKFLAG) $(EXTRA_LDFLAGS)
LDLIBS += $(EXTRA_LDLIBS)

.PHONY : default check clean

default: all

all: $(ALL_TESTS)

%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

%.o : %.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $<

cholesky: getoptions.o cholesky.o
cilksort: getoptions.o cilksort.o
fft: getoptions.o fft.o
heat: getoptions.o heat.o
lu: getoptions.o lu.o
matmul: getoptions.o matmul.o
nqueens: getoptions.o nqueens.o
rectmul: getoptions.o rectmul.o
strassen: getoptions.o strassen.o

qsort: qsort.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

fibred: fibred.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

fft cholesky heat : LDLIBS += -lm

# qsort : CXXFLAGS += -falign-functions=32

choleskyARGS=-n 4000 -z 8000
cilksortARGS=-n 80000000
fftARGS=-n 20000000
heatARGS=-nx 4096 -ny 4096 -nt 200
luARGS=-n 4096
matmulARGS=-n 2048
nqueensARGS=13
qsortARGS=50000000
rectmulARGS=-x 4096 -y 4096 -z 2048
strassenARGS=-n 4096
fibredARGS=40

TRIALS=0 1 2 3 4

check-qsort : qsort
	$(foreach var,$(TRIALS),./$^ ${$^ARGS};)

check-heat : heat
	$(foreach var,$(TRIALS),./$^ ${$^ARGS};)

check-fibred : fibred
	$(foreach var,$(TRIALS),./$^ ${$^ARGS};)

check-nqueens : nqueens
	./$^ ${$^ARGS} -c
	$(foreach var,$(TRIALS),./$^ ${$^ARGS};)

check-% : %
	./$^ -c
	$(foreach var,$(TRIALS),./$^ ${$^ARGS};)

check : $(patsubst %,check-%,$(ALL_TESTS))

clean :
	rm -f $(ALL_TESTS) *.o *.d* *~
