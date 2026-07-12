PROJECT_NAME = radioastro
MAKEFLAGS += " -j2 "
# CFLAGS=-O3 -Wall
CFLAGS = -O3 -march=armv7-a -mfpu=neon -mfloat-abi=hard -ffast-math -DNDEBUG -Wall -fopenmp -std=c++11  # Added -std=c++11 for C++11 compatibility
# CFLAGS = -O0 -Wall -g
.PHONY: all

all: obj $(PROJECT_NAME)

$(PROJECT_NAME): obj/radioastro.o obj/signal_processing.o obj/utils.o obj/CAccelProxy.o obj/CXADCProxy.o obj/CFFTDriver.o Makefile
	g++ $(CFLAGS) obj/radioastro.o obj/signal_processing.o obj/utils.o obj/CAccelProxy.o obj/CXADCProxy.o obj/CFFTDriver.o -o $(PROJECT_NAME) -lcma -lpthread

obj/radioastro.o: src/radioastro.cpp src/signal_processing.h src/utils.h src/fxp_utils.h Makefile
	g++ -c $(CFLAGS) src/radioastro.cpp -o obj/radioastro.o
obj/signal_processing.o: src/signal_processing.cpp src/signal_processing.h src/utils.h src/fxp_utils.h Makefile
	g++ -c $(CFLAGS) src/signal_processing.cpp -o obj/signal_processing.o
obj/utils.o: src/utils.cpp src/utils.h Makefile
	g++ -c $(CFLAGS) src/utils.cpp -o obj/utils.o
obj/CAccelProxy.o: src/CAccelProxy.cpp src/CAccelProxy.hpp src/utils.h Makefile
	g++ -c $(CFLAGS) src/CAccelProxy.cpp -o obj/CAccelProxy.o
obj/CXADCProxy.o: src/CXADCProxy.cpp src/CXADCProxy.hpp src/CAccelProxy.hpp src/utils.h Makefile
	g++ -c $(CFLAGS) src/CXADCProxy.cpp -o obj/CXADCProxy.o
obj/CFFTDriver.o: src/CFFTDriver.cpp src/CFFTDriver.hpp src/CAccelProxy.hpp src/utils.h Makefile
	g++ -c $(CFLAGS) src/CFFTDriver.cpp -o obj/CFFTDriver.o

obj:
	mkdir obj/

clean:
	rm -f $(PROJECT_NAME)
	rm -rf obj/
	rm -f sds_trace_data.dat
