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

void InitGlut(int argc, char *argv[], char *windowTittle);
void ReadScene(char *fileName);
double WallClockTime();
void ReadScene(char *fileName,Sphere* spheres,unsigned int sphereCount,Camera camera);
void UpdateCamera(Camera camera,int width, int height);
void ReInit(const int reallocBuffers);
void idleFunc(void);
void UpdateRendering();