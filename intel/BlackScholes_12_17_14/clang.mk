CC ?= clang
CXX ?= clang++

CFLAGS := -O3 # -march=native

CFLAGS += $(CILKFLAG) $(OPTFLAGS)
CXXFLAGS += $(CILKFLAG) $(OPTFLAGS)

LINK.o = $(CXX) $(LDFLAGS) $(TARGET_ARCH)
LDFLAGS +=

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) -c $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $< 
