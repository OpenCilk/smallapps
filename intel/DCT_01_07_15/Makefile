COMPILER ?= clang

SRCDIR := src
BUILDDIR := release
CILKFLAG ?= -fopencilk
LIBFLAGS := $(CILKFLAG) $(EXTRA_LDFLAGS)
option := res/nahelam.bmp res/nahelam1.bmp
ifdef vecreport
	CFLAGS += -vec-report1
endif

ifdef perf_num
	EXTRA_CFLAGS += -D PERF_NUM
endif

TARGET := $(BUILDDIR)/DCT

SOURCES := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.cpp=.o))

include $(COMPILER).mk

$(BUILDDIR) :
	if [ ! -d $(BUILDDIR) ]; then mkdir -p $(BUILDDIR); fi

$(TARGET): $(OBJECTS)
	@echo " Linking..."
	$(CXX) $^ $(LIBFLAGS) -o $(TARGET)

# $(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
# 	@mkdir -p $(BUILDDIR)
# 	$(CXX) -c $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $< 

run: $(TARGET)
	@./$(TARGET) $(option)

clean:
	@echo " Cleaning..."
	@rm -fr $(BUILDDIR) $(TARGET) 2>/dev/null || true
	@rm -f *.bmp *.valsig

.PHONY: clean

