PROJECT_NAME = PowerRanger
MAKEFLAGS += " -j2 "
CFLAGS=-O3 -Wall

all: obj $(PROJECT_NAME)

$(PROJECT_NAME): obj/$(PROJECT_NAME).o obj/util.o obj/CAccelProxy.o obj/CXADCProxy.o Makefile
	g++ $(CFLAGS) obj/$(PROJECT_NAME).o obj/util.o obj/CAccelProxy.o obj/CXADCProxy.o -o $(PROJECT_NAME) -lcma -lpthread

obj/$(PROJECT_NAME).o: src/$(PROJECT_NAME).cpp src/util.hpp src/CAccelProxy.hpp src/CXADCProxy.hpp
	g++ -c $(CFLAGS) src/$(PROJECT_NAME).cpp -o obj/$(PROJECT_NAME).o
obj/util.o: src/util.cpp src/util.hpp
	g++ -c $(CFLAGS) src/util.cpp -o obj/util.o
obj/CAccelProxy.o: src/CAccelProxy.cpp src/CAccelProxy.hpp src/util.hpp
	g++ -c $(CFLAGS) src/CAccelProxy.cpp -o obj/CAccelProxy.o
obj/CXADCProxy.o: src/CXADCProxy.cpp src/CXADCProxy.hpp src/CAccelProxy.hpp src/util.hpp
	g++ -c $(CFLAGS) src/CXADCProxy.cpp -o obj/CXADCProxy.o

obj:
	mkdir obj/

clean:
	rm -f $(PROJECT_NAME)
	rm -rf obj/
	rm -f sds_trace_data.dat
