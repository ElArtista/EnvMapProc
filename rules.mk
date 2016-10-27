#
# Compile opencl sources to C headers
#
CLSCRS  := $(call rwildcard, $(RESDIR), *.cl)
GENHDRS := $(foreach h, $(CLSCRS:.cl=.h), $(BUILDDIR)/$(h))

$(BUILDDIR)/%.h: %.cl
	@$(info $(LGREEN_COLOR)[>] Encoding$(NO_COLOR) $(LYELLOW_COLOR)$<$(NO_COLOR))
	@$(call mkdir, $(@D))
	$(showcmd)xxd -i $< $@

# Generate dependency rules
$(eval $(foreach o, $(OBJ_$(D)), $(o): $(GENHDRS)${\n}))
