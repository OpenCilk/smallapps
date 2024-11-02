CXX ?= clang++
SRCDIR := src
BUILDDIR := release
CILKFLAG ?= -fopencilk
CFLAGS := $(CILKFLAG) -std=c++11 -O3 -march=native
LIBFLAGS := $(CILKFLAG) $(EXTRA_LDFLAGS)
option := res/nahelam.bmp res/nahelam1.bmp
# vecreport := 1
perf_num := 1
ifdef vecreport
	CFLAGS += -vec-report1
endif

ifdef perf_num
	CFLAGS += -D PERF_NUM
endif

TARGET := $(BUILDDIR)/AveragingFilter

SOURCES := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.cpp=.o))

$(TARGET): $(OBJECTS)
	@echo " Linking..."
	$(CXX) $^ $(LIBFLAGS) -o $(TARGET)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) -c $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $< 

run: $(TARGET)
	./$(TARGET) $(option)

clean:
	@echo " Cleaning..."
	@rm -fr $(BUILDDIR) $(TARGET) 2>/dev/null || true
	@rm -f *.valsig

.PHONY: clean
