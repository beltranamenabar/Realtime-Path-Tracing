# Real time path tracer with OpenCL.

Students:
* Beltrán Amenábar
* Pablo Sánchez

## Compile the program

Before compiling make sure you have boost (that include thread module and the BOOST_ROOT and
BOOST_LIBRARYDIR env variables) installed and OpenCL drivers. If you are on linux/Mac, open a terminal
on the project root and run the following commands:
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
if you are on Windows open the folder with Visual Studio and configure the project to 

## Controls
* wasd: Move the camera.
* Arrow keys: Rotate the camera.
* r and f: Move the camera up and down as opposed to forward and backwards as w and s does.
* + and -: Change sphere selected.
* Numpad (2,4,6,8,9,3): Move selected sphere.
* 7 and 1: Change sphere material.

The selected sphere is printed on console when hitting + or -.
The coordinates of the camera and spheres are printed in console if they change.

## Acknowledgements
This project was based and modified from Sebastian Alfaro (AlphaSteam)'s project
[on bitbucket](https://bitbucket.org/AlphaSteam/opencl-raytracing/src/master/) which
was based on David Bucciarelli's work
