all: clean raytrace
	

raytrace: raytrace.cpp Makefile vec.h camera.h geom.h geomfunc.h
	clang++  -g -std=c++11 -Wall -lglut -lGLU  -lGL -lOpenCL -Wl,--no-as-needed  -fno-threadsafe-statics -DSMALLPT_GPU -o raytrace raytrace.cpp


clean:
	rm -f *.o raytrace
