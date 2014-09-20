include ../Makefile.env

.PHONY: clean


all:
	cd image_formula; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd image_text; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd lzma; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd markdown; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
#	cd mongo; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd rawimage; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd readline; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd sdl2; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd webimage; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
#	cd webm; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd webp; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
ifeq ($(TARGET),win)
	cd windows; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
endif

clean:
	cd image_formula; make clean TARGET=$(TARGET)
	cd image_text; make clean TARGET=$(TARGET)
	cd lzma; make clean TARGET=$(TARGET)
	cd markdown; make clean TARGET=$(TARGET)
	cd mongo; make clean TARGET=$(TARGET)
	cd rawimage; make clean TARGET=$(TARGET)
	cd readline; make clean TARGET=$(TARGET)
	cd sdl2; make clean TARGET=$(TARGET)
	cd webimage; make clean TARGET=$(TARGET)
	cd webm; make clean TARGET=$(TARGET)
	cd webp; make clean TARGET=$(TARGET)
ifeq ($(TARGET),win)
	cd windows; make clean TARGET=$(TARGET)
endif

