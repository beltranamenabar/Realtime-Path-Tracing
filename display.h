#ifndef _DISPLAYFUNC_H
#define	_DISPLAYFUNC_H
#include <math.h>




#ifdef __APPLE__
#include <GLut/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#endif

#include "vec.h"


extern int image_width;
extern int image_height;
extern unsigned int *pixels;


void InitGlut(int argc, char *argv[], char *windowTittle);
void ReadScene(char const *fileName);
double WallClockTime();
void UpdateCamera();
//void ReInit(const int reallocBuffers);
//void idleFunc(void);
//void UpdateRendering();

#endif	/* _DISPLAYFUNC_H */