PRJTYPE = StaticLib
SRC = \
	src/icd.c \
	src/icd_dispatch.c
ifeq ($(OS), Windows_NT)
	SRC += src/icd_windows.c
else
	SRC += src/icd_linux.c
endif
