PRJTYPE = StaticLib
SRCDIR  := src
ADDINCS := $(BUILDDIR)/$(VARIANT)/$(SRCDIR)

# Can be either CPU_ST | CPU_MT | GPU
SH_CALC_MODE ?= CPU_ST
ifeq ($(SH_CALC_MODE), CPU_MT)
	DEFINES := WITH_OPENMP
	ifeq ($(TOOLCHAIN), GCC)
		MCFLAGS := -fopenmp
	endif
else ifeq ($(SH_CALC_MODE), GPU)
	DEFINES := SH_COEFFS_GPU
endif
