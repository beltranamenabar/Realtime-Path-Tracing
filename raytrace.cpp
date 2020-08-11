//#include <stdio.h>
#include <cstdlib>
//#include <time.h>
#include <string>
#include <iostream>
#include <fstream>
typedef unsigned char ubyte;

#define __CL_ENABLE_EXCEPTIONS
#ifdef __APPLE__
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif

#ifdef __APPLE__
#include <GLut/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
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
//#include "display.h"
//#include "geomfunc.h"

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
Unsupported Platform !!!
#endif

static int image_width = 640;
static int image_height = 480;

static std::size_t global_work_size = 0;
static std::size_t local_work_size = 0;

static std::vector<cl::Device> devices;

static cl::Context context;
static cl::Buffer colorBuffer;
static cl::Buffer pixelBuffer;
static cl::Buffer seedBuffer;
static cl::Buffer sphereBuffer;
static cl::Buffer cameraBuffer;
static cl::CommandQueue queue;
static cl::Program program;
static cl::Kernel kernel;

static Vec *colors;
static unsigned int *seeds;
Camera camera;
static int currentSample = 0;
Sphere *spheres;
unsigned int sphere_count = 0;
unsigned int *pixels;
boost::barrier *threadStartBarrier = new boost::barrier(2);
boost::barrier *threadEndBarrier = new boost::barrier(2);
boost::thread renderThread;
cl::Event kernelExecutionTime;
double WallClockTime()
{
#if defined(__linux__) || defined(__APPLE__)
	struct timeval t;
	gettimeofday(&t, NULL);

	return t.tv_sec + t.tv_usec / 1000000.0;
#elif defined(WIN32)
	return GetTickCount() / 1000.0;
#else
	Unsupported Platform !!!
#endif
}

void ReadScene(char const *fileName)
{
	//fprintf(stderr, "Reading scene: %s\n", fileName);
	std::cerr << "Reading scene: " << fileName << std::endl;
	FILE *f = fopen(fileName, "r");
	if (!f)
	{
		//fprintf(stderr, "Failed to open file: %s\n", fileName);
		std::cerr << "Failed to open file: " << fileName << std::endl;
		exit(-1);
	}

	/* Read the camera position */
	int c = fscanf(f, "camera %f %f %f  %f %f %f\n",
				   &camera.orig.x, &camera.orig.y, &camera.orig.z,
				   &camera.target.x, &camera.target.y, &camera.target.z);
	if (c != 6)
	{
		//fprintf(stderr, "Failed to read 6 camera parameters: %d\n", c);
		std::cerr << "Failed to read 6 camera parameters: " << std::to_string(c) << std::endl;
		exit(-1);
	}

	/* Read the sphere count */
	c = fscanf(f, "size %u\n", &sphere_count);
	//std::cout << "Number of spheres: " << std::to_string(sphere_count)<<std::endl;
	if (c != 1)
	{
		//fprintf(stderr, "Failed to read sphere count: %d\n", c);
		std::cerr << "Failed to read sphere count: " << std::to_string(c) << std::endl;
		exit(-1);
	}
	//fprintf(stderr, "Scene size: %d\n", sphereCount);
	//std::cerr << "Scene size: "  << std::to_string(sphere_count) << std::endl;
	/* Read all spheres */
	spheres = (Sphere *)malloc(sizeof(Sphere) * sphere_count);
	unsigned int i;
	for (i = 0; i < sphere_count; i++)
	{
		Sphere *s = &spheres[i];
		int mat;
		int c = fscanf(f, "sphere %f  %f %f %f  %f %f %f  %f %f %f  %d\n",
					   &s->rad,
					   &s->p.x, &s->p.y, &s->p.z,
					   &s->e.x, &s->e.y, &s->e.z,
					   &s->c.x, &s->c.y, &s->c.z,
					   &mat);
		switch (mat)
		{
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
			std::cerr << "Failed to read material type for sphere #" << std::to_string(i) << ": " << std::to_string(mat) << std::endl;
			exit(-1);
			break;
		}
		if (c != 11)
		{
			//fprintf(stderr, "Failed to read sphere #%d: %d\n", i, c);
			std::cerr << "Failed to read sphere #" << std::to_string(i) << ": " << std::to_string(c) << std::endl;
			exit(-1);
		}
	}

	fclose(f);
}
void UpdateCamera()
{
	std::cout << "UpdateCamera" << std::endl;
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
void waitExecute()
{

	// Start the rendering threads
	threadStartBarrier->wait();

	// Wait for job done signal (any reference to Bush is not itended...)
	threadEndBarrier->wait();
}
void ExecuteKernel()
{
	//std::cout << "ExecuteKernel" << std::endl;
	/* Enqueue a kernel run call */
	//std::cout << std::to_string(global_work_size) << std::endl;
	//std::cout << std::to_string(local_work_size) << std::endl;
	kernelExecutionTime = cl::Event();
	queue.enqueueNDRangeKernel(kernel, cl::NullRange, global_work_size, local_work_size, NULL, &kernelExecutionTime);
}
void UpdateRendering()
{

	double startTime = WallClockTime();
	int startSampleCount = currentSample;

	//--------------------------------------------------------------------------

	if (currentSample < 20)
	{
		waitExecute();

		currentSample++;
	}
	else
	{

		/* After first 20 samples, continue to execute kernels for more and more time */
		const float k = std::min(currentSample - 20u, 100u) / 100.f;
		const float tresholdTime = 0.5f * k;
		for (;;)
		{
			waitExecute();

			currentSample++;

			const float elapsedTime = WallClockTime() - startTime;
			if (elapsedTime > tresholdTime)
				break;
		}
	}

	//--------------------------------------------------------------------------
	//queue.finish();
	/* Enqueue readBuffer */
	cl_int status = queue.enqueueReadBuffer(pixelBuffer, CL_TRUE, 0, image_width * image_height * sizeof(unsigned int), pixels);
	//std::cout << "UpdatedRendering" << std::endl;
	if (status != CL_SUCCESS)
	{

		//fprintf(stderr, "Failed to read the OpenCL pixel buffer: %d\n", status);
		std::cerr << "Failed to read the OpenCL pixel buffer:" << std::to_string(status) << std::endl;
		exit(-1);
	}

	/*------------------------------------------------------------------------*/

	const double elapsedTime = WallClockTime() - startTime;
	//const int samples = currentSample - startSampleCount;
	//const double sampleSec = samples * image_height * image_width / elapsedTime;
	//sprintf(captionBuffer, "Rendering time %.3f sec (pass %d)  Sample/sec  %.1fK\n",
	//		elapsedTime, currentSample, sampleSec / 1000.f);
}
void idleFunc(void)
{
	//std::cout << "IdleFUnch" << std::endl;
	UpdateRendering();

	glutPostRedisplay();
}

void displayFunc(void)
{
	UpdateRendering();

	glClear(GL_COLOR_BUFFER_BIT);
	glRasterPos2i(0, 0);
	//std::cout << "Raster" << std::endl;
	glDrawPixels(640, 480, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	//std::cout << "Draw pixels" << std::endl;
	glutSwapBuffers();
}
void FreeBuffers()
{
	//std::cout << "FreeBuffers" << std::endl;
	free(seeds);
	free(colors);
	free(pixels);
}
void AllocateBuffers()
{
	//std::cout << "AllocateBuffers" << std::endl;
	try
	{
		const size_t pixelCount = image_width * image_height;
		size_t i;
		colors = (Vec *)malloc(sizeof(Vec) * pixelCount);

		seeds = (unsigned int *)malloc(sizeof(unsigned int) * pixelCount * 2);
		for (i = 0; i < pixelCount * 2; i++)
		{
			seeds[i] = rand();
			if (seeds[i] < 2)
				seeds[i] = 2;
		}

		pixels = (unsigned int *)malloc(sizeof(unsigned int) * pixelCount);
		// Test colors
		for (i = 0; i < pixelCount; ++i)
			pixels[i] = i;

		size_t sizeBytes = sizeof(Vec) * image_width * image_height;
		colorBuffer = cl::Buffer(context, CL_MEM_READ_WRITE, sizeBytes);

		sizeBytes = sizeof(unsigned int) * image_width * image_height;
		pixelBuffer = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeBytes);

		sizeBytes = sizeof(unsigned int) * image_width * image_height * 2;
		seedBuffer = cl::Buffer(context, CL_MEM_READ_WRITE, sizeBytes);
		queue.enqueueWriteBuffer(seedBuffer, CL_TRUE, 0, sizeBytes, seeds);
	}
	catch (cl::Error &err)
	{
		std::cout << "Error: " << err.what() << "(" << err.err() << ")" << std::endl;
		for (cl::Device dev : devices)
		{
			// Check the build status
			cl_build_status status = program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(dev);
			if (status != CL_BUILD_ERROR)
				continue;

			// Get the build log
			std::string name = dev.getInfo<CL_DEVICE_NAME>();
			std::string buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(dev);
			std::cerr << "Build log for " << name << ":" << std::endl
					  << buildlog << std::endl;
		}

		return;
	}
	catch (std::exception &e)
	{
		std::cout << "Error: " << std::endl;
	}
}
void ReInit(const int reallocBuffers)
{
	//std::cout << "ReInit" << std::endl;
	// Check if I have to reallocate buffers
	if (reallocBuffers)
	{
		FreeBuffers();
		UpdateCamera();
		AllocateBuffers();
	}
	else
		UpdateCamera();
	cl_int status = queue.enqueueWriteBuffer(cameraBuffer, CL_TRUE, 0, sizeof(Camera), &camera);
	if (status != CL_SUCCESS)
	{
		//fprintf(stderr, "Failed to write the OpenCL camera buffer: %d\n", status);
		std::cerr << "Failed to write the OpenCL camera buffer: " << std::to_string(status) << std::endl;
		exit(-1);
	}

	currentSample = 0;
}
void reshapeFunc(int newWidth, int newHeight)
{
	//std::cout << "ReshapeFunc" << std::endl;
	image_width = newWidth;
	image_height = newHeight;

	glViewport(0, 0, image_width, image_height);
	glLoadIdentity();
	glOrtho(0.f, image_width - 1.f, 0.f, image_height - 1.f, -1.f, 1.f);

	ReInit(1);

	glutPostRedisplay();
}
void InitGlut(int argc, char *argv[], char *windowTittle)
{
	glutInitWindowSize(image_width, image_height);
	glutInitWindowPosition(0, 0);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutInit(&argc, argv);

	glutCreateWindow(windowTittle);
}
void runGlut()
{
	glutReshapeFunc(reshapeFunc);
	//glutKeyboardFunc(keyFunc);
	//glutSpecialFunc(specialFunc);
	glutDisplayFunc(displayFunc);
	glutIdleFunc(idleFunc);

	//glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, image_width, image_height);
	glLoadIdentity();
	glOrtho(0.f, image_width - 1.f, 0.f, image_height - 1.f, -1.f, 1.f);
	glutMainLoop();
}
void render()
{
	try
	{
		for (;;)
		{
			threadStartBarrier->wait();

			/* Set kernel arguments */

			kernel.setArg(0, colorBuffer);
			//std::cout << "arg 0 set" << std::endl;
			kernel.setArg(1, seedBuffer);
			//std::cout << "arg 1 set" << std::endl;
			kernel.setArg(2, sphereBuffer);
			//std::cout << "arg 2 set" << std::endl;
			kernel.setArg(3, cameraBuffer);
			//std::cout << "arg 3 set" << std::endl;
			kernel.setArg(4, sphere_count);
			//std::cout << "arg 4 set" << std::endl;
			kernel.setArg(5, image_width);
			//std::cout << "arg 5 set" << std::endl;
			kernel.setArg(6, image_height);
			//std::cout << "arg 6 set" << std::endl;
			kernel.setArg(7, currentSample);
			//std::cout << "arg 7 set" << std::endl;
			kernel.setArg(8, pixelBuffer);
			//std::cout << "arg 8 set" << std::endl;
			//kernel.setArg(9, workOffset);
			//kernel.setArg(10, workAmount);
			ExecuteKernel();
			cl_int status = queue.enqueueReadBuffer(pixelBuffer, CL_TRUE, 0, image_width * image_height * sizeof(unsigned int), pixels);

			kernelExecutionTime.wait();
			queue.finish();

			threadEndBarrier->wait();
		}
	}
	catch (boost::thread_interrupted)
	{
		std::cerr << " Rendering thread halted" << std::endl;
	}
	catch (cl::Error err)
	{
		std::cerr << " ERROR: " << err.what() << "(" << err.err() << ")" << std::endl;
	}
}

int setOpenCL(const std::string &sourceName)
{

	try
	{

		// Query for platforms
		std::vector<cl::Platform> platforms;
		cl::Platform::get(&platforms);

		// Get a list of devices on this platform

		// Select the platform.
		int platform_id;
		if (platforms.size() > 1)
		{

			std::cout << "Platforms:" << std::endl;
			for (unsigned int i = 0; i < platforms.size(); i++)
			{
				std::cout << std::to_string(i) << ") " << platforms[i].getInfo<CL_PLATFORM_NAME>() << std::endl;
			}
			std::cout << std::endl;

			int correct_platform = 0;
			std::cout << "Enter the number of the platform to use." << std::endl;
			while (correct_platform == 0)
			{

				std::cin >> platform_id;
				if (platform_id < platforms.size())
				{
					correct_platform = 1;
				}
				else
				{
					std::cout << "Number of platform doesn't exist. Please try again with a number from the list." << std::endl;
				}
			}
			std::cout << std::endl;
		}
		else
		{
			platform_id = 0;
		}

		//Select the device.
		int device_id;
		platforms[platform_id].getDevices(CL_DEVICE_TYPE_ALL, &devices);
		if (devices.size() > 1)
		{
			std::cout << "Devices:" << std::endl;
			for (unsigned int i = 0; i < devices.size(); i++)
			{
				std::cout << std::to_string(i) << ") " << devices[i].getInfo<CL_DEVICE_NAME>() << std::endl;
			}
			std::cout << std::endl;

			int correct_device = 0;
			std::cout << "Enter the number of the device to use." << std::endl;
			while (correct_device == 0)
			{

				std::cin >> device_id;
				if (device_id < devices.size())
				{
					correct_device = 1;
				}
				else
				{
					std::cout << "Number of device doesn't exist. Please try again with a number from the list." << std::endl;
				}
			}
		}
		else
		{
			device_id = 0;
		}
		/*
		std::cout << std::endl;
		std::cout << "Enter width of image." << std::endl;
		std::cin >> image_width;

		//std::cout << std::endl;
		std::cout << "Enter height of image." << std::endl;
		std::cin >> image_height;
*/
		image_width = 640;
		image_height = 480;
		/*
	std::cout << std::endl;
	std::cout << "Enter number of samples for Path tracing." << std::endl;
	std::cin >> samples;

*/
		std::cout << std::endl;
		std::cout << "Platform:" << std::endl;
		std::cout << platforms[platform_id].getInfo<CL_PLATFORM_NAME>() << std::endl
				  << std::endl;

		std::cout << "Device:" << std::endl;
		std::cout << devices[device_id].getInfo<CL_DEVICE_NAME>() << std::endl
				  << std::endl;

		// Create a context
		cl::Context contexts(devices);
		context = contexts;
		//std::cout << "Context created" << std::endl;
		// Create a command queue
		queue = cl::CommandQueue(context, devices[device_id]);
		//std::cout << "Command queue created:" << std::endl;
		//std::cout << "Number of spheres:" <<std::to_string(sphere_count) << std::endl;
		//std::cout << "Size of Sphere:" <<std::to_string(sizeof(Sphere)) << std::endl;

		//renderThead.join();
		sphereBuffer = cl::Buffer(context,
#ifdef __APPLE__
								  CL_MEM_READ_WRITE, // NOTE: not READ_ONLY because of Apple's OpenCL bug
#else
								  CL_MEM_READ_ONLY,
#endif
								  sizeof(Sphere) * sphere_count);
		//std::cout << "Sphere buffer created:" << std::endl;
		queue.enqueueWriteBuffer(sphereBuffer, CL_TRUE, 0, sizeof(Sphere) * sphere_count, spheres);
		//std::cout << "Enqueued write buffer spheres" << std::endl;

		cameraBuffer = cl::Buffer(context,
#ifdef __APPLE__
								  CL_MEM_READ_WRITE, // NOTE: not READ_ONLY because of Apple's OpenCL bug
#else
								  CL_MEM_READ_ONLY,
#endif
								  sizeof(Camera));
		//std::cout << "Camera buffer created:" << std::endl;
		queue.enqueueWriteBuffer(cameraBuffer, CL_TRUE, 0, sizeof(Camera), &camera);
		//std::cout << "Enqueued write buffer camera" << std::endl;
		//AllocateBuffers();
		//std::cout << "AllocateBuffers() done" << std::endl;

		// Read the program source
		std::ifstream sourceFile(sourceName);
		std::string sourceCode(std::istreambuf_iterator<char>(sourceFile), (std::istreambuf_iterator<char>()));
		cl::Program::Sources source(1, std::make_pair(sourceCode.c_str(), sourceCode.length()));

		// Make program from the source code
		program = cl::Program(context, source);
		boost::thread renderThread(&render);

// Build the program for the devices
#ifdef __APPLE__
		program.build(devices, "-I. -D__APPLE__");
#else
		program.build(devices, "-I.");
#endif

		// Make kernel

		cl::Kernel ray_kernel(program, "RadianceGPU");
		kernel = ray_kernel;
		global_work_size = image_width * image_height;
		local_work_size = kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(devices[device_id]);
		//lifegame.maxThreads = devices[device_id].getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
		//std::cout << lifegame.maxThreads << std::endl;
	}
	catch (cl::Error &err)
	{
		std::cout << "Error: " << err.what() << "(" << err.err() << ")" << std::endl;
		for (cl::Device dev : devices)
		{
			// Check the build status
			cl_build_status status = program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(dev);
			if (status != CL_BUILD_ERROR)
				continue;

			// Get the build log
			std::string name = dev.getInfo<CL_DEVICE_NAME>();
			std::string buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(dev);
			std::cerr << "Build log for " << name << ":" << std::endl
					  << buildlog << std::endl;
		}

		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

void saveImage()
{

	FILE *f = fopen("image.ppm", "w"); // Write image to PPM file.
	if (!f)
	{
		fprintf(stderr, "Failed to open image file: image.ppm\n");
	}
	else
	{
		fprintf(f, "P3\n%d %d\n%d\n", image_width, image_height, 255);

		int x, y;
		for (y = image_height - 1; y >= 0; --y)
		{
			unsigned char *p = (unsigned char *)(&pixels[y * image_width]);
			for (x = 0; x < image_width; ++x, p += 4)
				fprintf(f, "%d %d %d ", p[0], p[1], p[2]);
		}

		fclose(f);
	}
}

void ReInitScene()
{
	std::cout << "Reinitcene" << std::endl;
	currentSample = 0;

	// Redownload the scene
	cl_int status = queue.enqueueWriteBuffer(sphereBuffer, CL_TRUE, 0, sizeof(Sphere) * sphere_count, spheres);
	if (status != CL_SUCCESS)
	{
		//fprintf(stderr, "Failed to write the OpenCL scene buffer: %d\n", status);
		std::cerr << "Failed to write the OpenCL scene buffer: " << std::to_string(status) << std::endl;
		exit(-1);
	}
}

int main(int argc, char *argv[]) noexcept
{
	try
	{
		char const *title = "test";
		InitGlut(argc, argv, (char *)title);
		char const *scene = "scenes/cornell.scn";
		ReadScene(scene);
		//std::cout << "Scene read" << std::endl;
		//UpdateCamera();
		//std::cout << "Updated Camera" << std::endl;
		setOpenCL("Ray tracer.cl");
		/*
		std::cout<< "Kernel work group size: " << local_work_size << std::endl;
	if (global_work_size % local_work_size != 0)
		global_work_size = (global_work_size / local_work_size + 1) * local_work_size;
	*/

		std::cout << "Glut init" << std::endl;
		currentSample = 0;
		/*for(size_t i = 0 ; i< 80; i++){
UpdateRendering();
	}
	*/
		runGlut();
		return 0;
	}
	catch (cl::Error &e)
	{
		std::cout << e.what() << std::endl;
	}
	//saveImage();
	//std::cout << "Rendering done!\nSaved image to 'opencl_raytracer.ppm'" << std::endl;
}
