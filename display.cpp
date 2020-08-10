
#include "display.h"


void reshapeFunc(int newWidth, int newHeight) {
	image_width = newWidth;
	image_height = newHeight;

	//glViewport(0, 0, image_width, image_height);
	glLoadIdentity();
	glOrtho(0.f, image_width - 1.f, 0.f, image_height - 1.f, -1.f, 1.f);

	ReInit(1);

	glutPostRedisplay();
}
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

void UpdateCamera(Camera camera,int width, int height) {
	vsub(camera.dir, camera.target, camera.orig);
	vnorm(camera.dir);

	const Vec up = {0.f, 1.f, 0.f};
	const float fov = (M_PI / 180.f) * 45.f;
	vxcross(camera.x, camera.dir, up);
	vnorm(camera.x);
	vsmul(camera.x, width * fov / height, camera.x);

	vxcross(camera.y, camera.x, camera.dir);
	vnorm(camera.y);
	vsmul(camera.y, fov, camera.y);
}
void displayFunc(void) {
	glClear(GL_COLOR_BUFFER_BIT);
	glRasterPos2i(0, 0);
	glDrawPixels(image_width, image_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	glutSwapBuffers();
}

void InitGlut(int argc, char *argv[], char *windowTittle) {
    glutInitWindowSize(image_width, image_height);
    glutInitWindowPosition(0,0);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInit(&argc, argv);

	glutCreateWindow(windowTittle);

    glutReshapeFunc(reshapeFunc);
    //glutKeyboardFunc(keyFunc);
    //glutSpecialFunc(specialFunc);
    glutDisplayFunc(displayFunc);
	glutIdleFunc(idleFunc);

	glViewport(0, 0, image_width, image_height);
	glLoadIdentity();
	glOrtho(0.f, image_width - 1.f, 0.f, image_height - 1.f, -1.f, 1.f);
}
void idleFunc(void) {
	UpdateRendering();

	glutPostRedisplay();
}

void ReadScene(char *fileName,Sphere* spheres,unsigned int sphereCount,Camera camera) {
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
	c = fscanf(f,"size %u\n", &sphereCount);
	if (c != 1) {
		//fprintf(stderr, "Failed to read sphere count: %d\n", c);
		std::cerr << "Failed to read sphere count: "  << std::to_string(c) << std::endl;
		exit(-1);
	}
	//fprintf(stderr, "Scene size: %d\n", sphereCount);
	std::cerr << "Scene size: "  << std::to_string(sphereCount) << std::endl;
	/* Read all spheres */
	spheres = (Sphere *)malloc(sizeof(Sphere) * sphereCount);
	unsigned int i;
	for (i = 0; i < sphereCount; i++) {
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