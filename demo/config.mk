PRJTYPE = Executable
LIBS = envmapproc assetloader macu freetype png jpeg tiff zlib glfw glad
ifeq ($(OS), Windows_NT)
	LIBS += glu32 opengl32 gdi32 winmm ole32 shell32 user32
else
	LIBS += GLU GL X11 Xcursor Xinerama Xrandr Xxf86vm Xi pthread m dl
endif
MOREDEPS = ..
