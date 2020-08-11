all: clean raytrace
	

raytrace: raytrace.cpp Makefile vec.h geom.h geomfunc.h
	g++  -g -std=c++17 -Wall -pthread -lboost_thread -lglut -lGLU  -lGL -lOpenCL -Wl,--no-as-needed  -fno-threadsafe-statics -DSMALLPT_GPU -o raytrace raytrace.cpp


clean:
	rm -f *.o raytrace
