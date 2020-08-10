all: clean raytrace
	

raytrace: raytrace.o display.o
	g++  -o raytrace  raytrace.o display.o -std=c++11 -lglut -lOpenCL -lGL  

raytrace.o: raytrace.cpp display.h
	g++ -g -c raytrace.cpp

display.o: display.h
	g++ -g -c display.cpp 
clean:
	rm -f *.o raytrace
