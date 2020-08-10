#ifdef WIN32
#define _USE_MATH_DEFINES
#endif

#if defined(__linux__) || defined(__APPLE__)
#include <sys/time.h>
#elif defined (WIN32)
#include <windows.h>
#else
        Unsupported Platform !!!
#endif



#ifdef __APPLE__
#include <GLut/glut.h>
#else
#include <GL/glut.h>
#endif

#include "camera.h"
#include "geomfunc.h"
#include <iostream>

extern int image_width;
extern int image_height;
extern unsigned int *pixels;
extern unsigned int sphere_count;
extern Camera camera;
extern Sphere *spheres;

void InitGlut(int argc, char *argv[], char *windowTittle);
void ReadScene(char *fileName);
double WallClockTime();
void UpdateCamera();
void ReInit(const int reallocBuffers);
void idleFunc(void);
void UpdateRendering();