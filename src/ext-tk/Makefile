include ../Makefile.env

.PHONY: clean


all:
	cd editor; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd foxtk; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)


clean:
	cd editor; make clean TARGET=$(TARGET)
	cd foxtk; make clean TARGET=$(TARGET)

