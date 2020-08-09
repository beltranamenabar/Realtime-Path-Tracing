all: clean raytrace
	


raytrace:
	g++ raytrace.cpp -o raytrace -lOpenCL -std=c++11

clean:
	rm -f *.o raytrace
