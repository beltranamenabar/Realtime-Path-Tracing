#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#ifdef WIN32
#define _USE_MATH_DEFINES
#endif
#include <cmath>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/time.h>
#elif defined (WIN32)
#include <windows.h>
#else
        Unsupported Platform !!!
#endif

#include "camera.h"
#include "geom.h"
#include "display.h"

extern void ReInit(const int);
extern void ReInitScene();
extern void UpdateRendering();
extern void UpdateCamera();

extern Camera camera;
extern Sphere *spheres;
extern unsigned int sphere_count;

int amiSmallptCPU;

int image_width = 640;
int image_height = 480;
unsigned int *pixels;



//static int currentSphere;

double WallClockTime() {
#if defined(__linux__) || defined(__APPLE__)
	struct timeval t;
	gettimeofday(&t, NULL);

	return t.tv_sec + t.tv_usec / 1000000.0;
#elif defined (WIN32)
	return GetTickCount() / 1000.0;
#else
	Unsupported Platform !!!
#endif
}





void ReadScene(char const *fileName) {
	//fprintf(stderr, "Reading scene: %s\n", fileName);
	std::cerr << "Reading scene: "  << fileName << std::endl;
	FILE *f = fopen(fileName, "r");
	if (!f) {
		//fprintf(stderr, "Failed to open file: %s\n", fileName);
		std::cerr << "Failed to open file: "  << fileName << std::endl;
		exit(-1);
	}

	/* Read the camera position */
	int c = fscanf(f,"camera %f %f %f  %f %f %f\n",
			&camera.orig.x, &camera.orig.y, &camera.orig.z,
			&camera.target.x, &camera.target.y, &camera.target.z);
	if (c != 6) {
		//fprintf(stderr, "Failed to read 6 camera parameters: %d\n", c);
		std::cerr << "Failed to read 6 camera parameters: "  << std::to_string(c) << std::endl;
		exit(-1);
	}

	/* Read the sphere count */
	c = fscanf(f,"size %u\n", &sphere_count);
	//std::cout << "Number of spheres: " << std::to_string(sphere_count)<<std::endl;
	if (c != 1) {
		//fprintf(stderr, "Failed to read sphere count: %d\n", c);
		std::cerr << "Failed to read sphere count: "  << std::to_string(c) << std::endl;
		exit(-1);
	}
	//fprintf(stderr, "Scene size: %d\n", sphereCount);
	//std::cerr << "Scene size: "  << std::to_string(sphere_count) << std::endl;
	/* Read all spheres */
	spheres = (Sphere *)malloc(sizeof(Sphere) * sphere_count);
	unsigned int i;
	for (i = 0; i < sphere_count; i++) {
		Sphere *s = &spheres[i];
		int mat;
		int c = fscanf(f,"sphere %f  %f %f %f  %f %f %f  %f %f %f  %d\n",
				&s->rad,
				&s->p.x, &s->p.y, &s->p.z,
				&s->e.x, &s->e.y, &s->e.z,
				&s->c.x, &s->c.y, &s->c.z,
				&mat);
		switch (mat) {
			case 0:
				s->refl = DIFF;
				break;
			case 1:
				s->refl = SPEC;
				break;
			case 2:
				s->refl = REFR;
				break;
			default:
				//fprintf(stderr, "Failed to read material type for sphere #%d: %d\n", i, mat);
				std::cerr << "Failed to read material type for sphere #"  << std::to_string(i) << ": " <<std::to_string(mat) <<std::endl;
				exit(-1);
				break;
		}
		if (c != 11) {
			//fprintf(stderr, "Failed to read sphere #%d: %d\n", i, c);
			std::cerr << "Failed to read sphere #"  << std::to_string(i) << ": " <<std::to_string(c) <<std::endl;
			exit(-1);
		}
	}

	fclose(f);
}
void UpdateCamera() {
	std::cout << "UpdateCamera"<<std::endl;
	vsub(camera.dir, camera.target, camera.orig);
	vnorm(camera.dir);

	const Vec up = {0.f, 1.f, 0.f};
	const float fov = (M_PI / 180.f) * 45.f;
	vxcross(camera.x, camera.dir, up);
	vnorm(camera.x);
	vsmul(camera.x, image_width * fov / image_height, camera.x);

	vxcross(camera.y, camera.x, camera.dir);
	vnorm(camera.y);
	vsmul(camera.y, fov, camera.y);
}
void idleFunc(void) {
	std::cout << "IdleFUnch"<<std::endl;
	UpdateRendering();

	glutPostRedisplay();
}

void displayFunc(void) {
	try{
	std::cout << "DisplayFUnc"<<std::endl;
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f); 
	glClear(GL_COLOR_BUFFER_BIT);
	//glRasterPos2i(0, 0);
	glDrawPixels(image_width, image_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	glutSwapBuffers();
	}
	catch(std::exception& e){
	std::cout << "test "<<std::endl;
	std::cerr << e.what() <<std::endl;
}
}

void reshapeFunc(int newWidth, int newHeight) {
	std::cout << "ReshapeFunc"<<std::endl;
	image_width = newWidth;
	image_height = newHeight;

	glViewport(0, 0, image_width, image_height);
	glLoadIdentity();
	glOrtho(0.f, image_width - 1.f, 0.f, image_height - 1.f, -1.f, 1.f);

	ReInit(1);

	glutPostRedisplay();
}
void InitGlut(int argc, char *argv[], char *windowTittle) {
    glutInitWindowSize(image_width, image_height);
    glutInitWindowPosition(0,0);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInit(&argc, argv);

	glutCreateWindow(windowTittle);

  
}
void runGlut(){
  glutReshapeFunc(reshapeFunc);
    //glutKeyboardFunc(keyFunc);
    //glutSpecialFunc(specialFunc);
    glutDisplayFunc(displayFunc);
	glutIdleFunc(idleFunc);

	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, image_width, image_height);
	glLoadIdentity();
	glOrtho(0.f, image_width - 1.f, 0.f, image_height - 1.f, -1.f, 1.f);
	glutMainLoop();
}