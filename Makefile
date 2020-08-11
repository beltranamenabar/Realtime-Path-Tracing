all: clean raytrace
	

raytrace: raytrace.cpp Makefile vec.h geom.h geomfunc.h raytrace.hpp
	g++  -g -std=c++17 -O3 -pthread -lboost_thread -lglut  -lGL -lOpenCL  -DSMALLPT_GPU -o raytrace raytrace.cpp


clean:
	rm -f *.o raytrace
