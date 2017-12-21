PRJTYPE = Executable
LIBS = envmapproc gfxwnd assetloader physfs macu freetype png jpeg tiff zlib glfw glad opencl
ifeq ($(TARGET_OS), Windows_NT)
	LIBS += glu32 opengl32 gdi32 winmm ole32 shell32 user32 kernel32
else
	LIBS += GLU GL X11 Xcursor Xinerama Xrandr Xxf86vm Xi pthread m dl
endif
ifeq ($(TOOLCHAIN), GCC)
	MLDFLAGS := -fopenmp
endif
MOREDEPS = ..
ADDLIBDIR = ../deps/OpenCL/lib
EXTDEPS = assetloader::dev gfxwnd::0.0.1dev
