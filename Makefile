PACKAGES = sdl2 SDL2_mixer SDL2_image
LDLIBS = -lm -lWs2_32
CFLAGS = -Wall -Wno-unused-function

ifeq ($(OS), Windows_NT)
   LDLIBS += -lopengl32 -mwindows -lWinmm
else
   PACKAGES += gl
endif

CFLAGS += $(shell pkg-config --cflags $(PACKAGES))
LDLIBS += $(shell pkg-config --libs $(PACKAGES))

Typtiza: src/Typtiza.cpp
	g++ $(CFLAGS) -o $@ $< $(LDLIBS)
