include ./Makefile.env

.PHONY: clean


all:
	cd vm; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd vm-debug; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd ext-core; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd ext-sub; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)
	cd ext-tk; make DEBUG_MODE=$(DEBUG_MODE) TARGET=$(TARGET)


clean:
	cd vm; make clean TARGET=$(TARGET)
	cd vm-debug; make clean TARGET=$(TARGET)
	cd ext-core; make clean TARGET=$(TARGET)
	cd ext-sub; make clean TARGET=$(TARGET)
	cd ext-tk; make clean TARGET=$(TARGET)

