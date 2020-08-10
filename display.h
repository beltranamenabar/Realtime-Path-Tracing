#pragma once
#include <math.h>

#ifdef __APPLE__
#include <GLut/glut.h>
#else
#include <GL/glut.h>
#endif

#include "vec.h"

void InitGlut(int argc, char *argv[], char *windowTittle,int width,int height);
void ReadScene(char *fileName);