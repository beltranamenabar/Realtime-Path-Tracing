#pragma once
#include <string>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;
typedef unsigned char ubyte;

#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 300
#ifdef __APPLE__
#include <OpenCL/cl.hpp>
#else
#include <CL/opencl.hpp>
#endif

#ifdef __APPLE__
#include <GLut/glut.h>
#else
#include <GL/glut.h>
#endif

#include <boost/thread.hpp>

#include "vec.h"

typedef struct
{
	/* User defined values */
	Vec orig, target;
	/* Calculated values */
	Vec dir, x, y;
} Camera;

typedef struct
{
	Vec o, d;
} Ray;

#define rinit(r, a, b)     \
	{                      \
		vassign((r).o, a); \
		vassign((r).d, b); \
	}
#define rassign(a, b)          \
	{                          \
		vassign((a).o, (b).o); \
		vassign((a).d, (b).d); \
	}

enum Refl
{
	DIFF,
	SPEC,
	REFR
}; /* material types, used in radiance() */

typedef struct
{

	float rad; /* radius */

	Vec p, e, c;	/* position, emission, color */
	enum Refl refl; /* reflection type (DIFFuse, SPECular, REFRactive) */
} Sphere;

#ifdef WIN32
#define _USE_MATH_DEFINES
#endif
#include <cmath>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/time.h>
#elif defined(WIN32)
#include <windows.h>
#else
#error "Unsupported Platform."
#endif
#define MOVE_STEP 10.0f
#define ROTATE_STEP (2.f * M_PI / 180.f)


void ReadScene(std::string fileName);
void UpdateCamera();
void waitExecute();
void ExecuteKernel();
void UpdateRendering();
void idleFunc(void);
void saveImage();
void ReInit(const int reallocBuffers);
#define MOVE_STEP 10.0f
#define ROTATE_STEP (2.f * M_PI / 180.f)
void keyFunc(unsigned char key, int x, int y);
void displayFunc(void);
void FreeBuffers();
void AllocateBuffers();
void reshapeFunc(int newWidth, int newHeight);
void InitGlut(int argc, char *argv[], char *windowTittle);
void runGlut();
void render();
int setOpenCL(const std::string &sourceName);
void ReInitScene();
int main(int argc, char *argv[]);
void specialFunc(int key, int x, int y);