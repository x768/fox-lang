include ../Makefile.env

.PHONY: clean


all:
	cd crypt; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd image; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd math; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd media; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd net; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd process; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd reflect; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd sqlite; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd text_unicode; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd text_util; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd xml; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd zip; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)


clean:
	cd crypt; make clean TARGET=$(TARGET)
	cd image; make clean TARGET=$(TARGET)
	cd math; make clean TARGET=$(TARGET)
	cd media; make clean TARGET=$(TARGET)
	cd net; make clean TARGET=$(TARGET)
	cd process; make clean TARGET=$(TARGET)
	cd reflect; make clean TARGET=$(TARGET)
	cd sqlite; make clean TARGET=$(TARGET)
	cd text_unicode; make clean TARGET=$(TARGET)
	cd text_util; make clean TARGET=$(TARGET)
	cd xml; make clean TARGET=$(TARGET)
	cd zip; make clean TARGET=$(TARGET)

