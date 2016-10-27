#
# Compile opencl sources to C headers
#
CLSCRS   := $(call rwildcard, $(RESDIR), *.cl)
GENHDRS  := $(foreach h, $(CLSCRS:.cl=.h), $(BUILDDIR)/$(VARIANT)/$(h))
CLPPOPTS := $(INCDIR_$(D)) -E -P $(DEFINEFLAG)OPENCL_MODE

$(BUILDDIR)/$(VARIANT)/%.h: %.cl
	@$(info $(LGREEN_COLOR)[>] Encoding$(NO_COLOR) $(LYELLOW_COLOR)$<$(NO_COLOR))
	@$(call mkdir, $(@D))
	$(showcmd)$(call dep-gen-wrapper, cpp $(CLPPOPTS) $(COUTFLAG)$(@:.h=.pp) $<)
	$(showcmd)cd $(@D) && xxd -i $(@F:.h=.pp) $(@F)

# Generate dependency rules
$(eval $(foreach o, $(OBJ_$(D)), $(o): $(GENHDRS)${\n}))

# Include header dependencies of OpenCL sources
HDEPS_$(D) += $(GENHDRS:.h=$(HDEPEXT))
