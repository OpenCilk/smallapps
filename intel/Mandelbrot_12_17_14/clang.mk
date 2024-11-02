CC ?= clang
CXX ?= clang++

CFLAGS := $(CILKFLAG) -O3

CFLAGS += $(OPTFLAGS) # -fcilkplus
CXXFLAGS += $(OPTFLAGS) # -fcilkplus

LINK.o = $(CXX) $(LDFLAGS) $(TARGET_ARCH)
LDFLAGS += # -lcilkrts

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	mkdir -p $(BUILDDIR)
	$(CXX) -c $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $< 
