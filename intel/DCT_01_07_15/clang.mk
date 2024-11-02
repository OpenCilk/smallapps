CC ?= gcc
CXX ?= g++

CFLAGS := $(CILKFLAG) -O3 # -march=native

CFLAGS += $(OPTFLAGS)
CXXFLAGS += $(OPTFLAGS)

LINK.o = $(CXX) $(LDFLAGS) $(TARGET_ARCH)
LDFLAGS +=

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) -c $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $< 
