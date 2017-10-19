PRJTYPE = StaticLib
SRC = \
	src/icd.c \
	src/icd_dispatch.c
ifeq ($(TARGET_OS), Windows_NT)
	SRC += src/icd_windows.c
else
	SRC += src/icd_linux.c
endif
