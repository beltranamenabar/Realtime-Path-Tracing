#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <iostream>
#include <fstream>
typedef unsigned char ubyte;

#define __CL_ENABLE_EXCEPTIONS
#ifdef __APPLE__
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif

#include "camera.h"
#include "display.h"
#include "geom.h"
std::size_t global_work_size = 0;
std::size_t local_work_size = 0;

std::vector<cl::Device> devices;
int samples = 0;

 cl::Context context;
 cl::Buffer colorBuffer;
 cl::Buffer pixelBuffer;
 cl::Buffer seedBuffer;
 cl::Buffer sphereBuffer;
 cl::Buffer cameraBuffer;
 cl::CommandQueue queue;
 cl::Program program;
 cl::Kernel kernel;
 cl::Event kernelexecution;
boost::barrier threadStartBarrier = new boost::barrier(2);
boost::barrier threadEndBarrier = new boost::barrier(2);

boost::thread renderThread;
 Vec *colors;
Camera camera;
 unsigned int *seeds;
Sphere *spheres;
 int currentSample = 0;
unsigned int sphere_count = 0;

// dummy variables are required for memory alignment
// float3 is considered as float4 by OpenCL
/*

void initScene(Sphere* cpu_spheres){

	// left wall
	cpu_spheres[0].radius	= 200.005f;
	cpu_spheres[0].position = float3(-200.6f, 0.0f, 0.0f);
	cpu_spheres[0].color    = float3(1.0f, 0.0f, 0.0f);
	cpu_spheres[0].emission = float3(0.0f, 0.0f, 0.0f);

	// right wall
	cpu_spheres[1].radius	= 200.001f;
	cpu_spheres[1].position = float3(200.6f, 0.0f, 0.0f);
	cpu_spheres[1].color    = float3(1.0f, 1.0f, 1.0f);
	cpu_spheres[1].emission = float3(0.0f, 0.0f, 0.0f);

	// floor
	cpu_spheres[2].radius	= 200.001f;
	cpu_spheres[2].position = float3(0.0f, -200.4f, 0.0f);
	cpu_spheres[2].color	= float3(1.0f, 1.0f, 1.0f);
	cpu_spheres[2].emission = float3(0.0f, 0.0f, 0.0f);

	// ceiling
	cpu_spheres[3].radius	= 200.001f;
	cpu_spheres[3].position = float3(0.0f, 200.4f, 0.0f);
	cpu_spheres[3].color	= float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[3].emission = float3(0.0f, 0.0f, 0.0f);

	// back wall
	cpu_spheres[4].radius   = 200.001f;
	cpu_spheres[4].position = float3(0.0f, 0.0f, -200.4f);
	cpu_spheres[4].color    = float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[4].emission = float3(0.0f, 0.0f, 0.0f);

	// front wall 
	cpu_spheres[5].radius   = 200.0f;
	cpu_spheres[5].position = float3(0.0f, 0.0f, 202.0f);
	cpu_spheres[5].color    = float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[5].emission = float3(0.0f, 0.0f, 0.0f);

	// left sphere
	cpu_spheres[6].radius   = 0.16f;
	cpu_spheres[6].position = float3(-0.4f, -0.24f, -0.1f);
	cpu_spheres[6].color    = float3(1.0f, 1.0f, 1.0f);
	cpu_spheres[6].emission = float3(0.0f, 0.0f, 0.0f);

	// right sphere
	cpu_spheres[7].radius   = 0.16f;
	cpu_spheres[7].position = float3(0.25f, -0.24f, 0.1f);
	cpu_spheres[7].color    = float3(0.0f, 0.0f, 0.7f);
	cpu_spheres[7].emission = float3(0.0f, 0.0f, 0.0f);

	// lightsource ceiling
	cpu_spheres[8].radius   = 1.0f;
	cpu_spheres[8].position = float3(0.0f, 1.36f, 0.0f);
	cpu_spheres[8].color    = float3(0.0f, 0.0f, 0.0f);
	cpu_spheres[8].emission = float3(5.0f, 5.0f, 5.0f);

	// lightsource back wall
	cpu_spheres[9].radius   = 0.1f;
	cpu_spheres[9].position = float3(0.25f, 0.1f, 0.0f);
	cpu_spheres[9].color    = float3(0.5f, 2.0f, 0.5f);
	cpu_spheres[9].emission = float3(0.5f, 2.0f, 0.5f);
	
}
*/
void FreeBuffers()
{
	std::cout << "FreeBuffers"<<std::endl;
	delete queue;
	delete kernel;
	delete sphereBuffer;
	delete camaraBuffer;
	if(colorBuffer)
		delete colorBuffer;
	if(pixelBuffer)
		delete pixelBuffer;
	if(seedBuffer)
		delete seedBuffer;
	if(colors)
		delete colors
	if(seeds)
		delete seeds;
	delete context;
}
 void AllocateBuffers()
{	std::cout << "AllocateBuffers"<<std::endl;
	try
	{
		const int pixelCount = image_width * image_height;
		int i;
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

		cl_uint sizeBytes = sizeof(Vec) * image_width * image_height;
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
void renderThread(){
try {
		for (;;) {
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

			ExecuteKernel();
			cl_int status = queue.enqueueReadBuffer(pixelBuffer, CL_TRUE, 0, image_width * image_height * sizeof(unsigned int), pixels);

			renderDevice->FinishExecuteKernel();
			renderDevice->Finish();

			renderDevice->threadEndBarrier->wait();
		}
	} catch (boost::thread_interrupted) {
		cerr << "[Device::" << renderDevice->GetName() << "] Rendering thread halted" << endl;
	} catch (cl::Error err) {
		cerr << "[Device::" << renderDevice->GetName() << "] ERROR: " << err.what() << "(" << err.err() << ")" << endl;
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

		std::cout << std::endl;
		std::cout << "Enter width of image." << std::endl;
		std::cin >> image_width;

		//std::cout << std::endl;
		std::cout << "Enter height of image." << std::endl;
		std::cin >> image_height;

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
		
		context = new cl::Context(devices);
		//std::cout << "Context created" << std::endl;
		// Create a command queue
		queue = cl::CommandQueue(context, devices[device_id]);
		//std::cout << "Command queue created:" << std::endl;
		//std::cout << "Number of spheres:" <<std::to_string(sphere_count) << std::endl;
		//std::cout << "Size of Sphere:" <<std::to_string(sizeof(Sphere)) << std::endl;
		
		renderThread = new boost::thread(boost::bind(RenderDevice::RenderThread, this));
		sphereBuffer = cl::Buffer(*context,
#ifdef __APPLE__
								  CL_MEM_READ_WRITE, // NOTE: not READ_ONLY because of Apple's OpenCL bug
#else
								  CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
#endif
								  sizeof(Sphere) * sphere_count,
								  spheres);
		//std::cout << "Sphere buffer created:" << std::endl;
		queue.enqueueWriteBuffer(sphereBuffer, CL_TRUE, 0, sizeof(Sphere) * sphere_count, spheres);
		//std::cout << "Enqueued write buffer spheres" << std::endl;

		cameraBuffer = cl::Buffer(*context,
#ifdef __APPLE__
								  CL_MEM_READ_WRITE, // NOTE: not READ_ONLY because of Apple's OpenCL bug
#else
								  CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
#endif
								  sizeof(Camera),
								  camera);
		//std::cout << "Camera buffer created:" << std::endl;
		queue.enqueueWriteBuffer(cameraBuffer, CL_TRUE, 0, sizeof(Camera), &camera);
		//std::cout << "Enqueued write buffer camera" << std::endl;
		//AllocateBuffers();
		std::cout << "AllocateBuffers() done" << std::endl;

		// Read the program source
		std::ifstream sourceFile(sourceName);
		std::string sourceCode(std::istreambuf_iterator<char>(sourceFile), (std::istreambuf_iterator<char>()));
		cl::Program::Sources source(1, std::make_pair(sourceCode.c_str(), sourceCode.length()));

		// Make program from the source code
		program = cl::Program(context, source);

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

/*
void saveImage(){

	// write image to PPM file, a very simple image file format
	// PPM files can be opened with IrfanView (download at www.irfanview.com) or GIMP
	FILE *f = fopen("opencl_raytracer.ppm", "w");
	fprintf(f, "P3\n%d %d\n%d\n", image_width, image_height, 255);

	// loop over pixels, write RGB values
	for (int i = 0; i < image_width * image_height; i++)
		fprintf(f, "%d %d %d ",
		toInt(cpu_output[i].s[0]),
		toInt(cpu_output[i].s[1]),
		toInt(cpu_output[i].s[2]));
}

*/
 void ExecuteKernel()
{	std::cout << "ExecuteKernel"<<std::endl;
	/* Enqueue a kernel run call */
	kernelexecution = cl::Event();
	queue.enqueueNDRangeKernel(kernel, cl::NullRange, global_work_size, local_work_size);
}
 void UpdateRendering()
{	std::cout << "UpdateRendering"<<std::endl;
	double startTime = WallClockTime();
	int startSampleCount = currentSample;



	//--------------------------------------------------------------------------

	if (currentSample < 20)
	{
		ExecuteKernel();
		currentSample++;
	}
	else
	{
		/* After first 20 samples, continue to execute kernels for more and more time */
		const float k = min2(currentSample - 20, 100) / 100.f;
		const float tresholdTime = 0.5f * k;
		for (;;)
		{
			ExecuteKernel();
			queue.finish();
			currentSample++;

			const float elapsedTime = WallClockTime() - startTime;
			if (elapsedTime > tresholdTime)
				break;
		}
	}

	//--------------------------------------------------------------------------

	/* Enqueue readBuffer */
	cl_int status = queue.enqueueReadBuffer(pixelBuffer, CL_TRUE, 0, image_width * image_height * sizeof(unsigned int), pixels);
	if (status != CL_SUCCESS)
	{
		//fprintf(stderr, "Failed to read the OpenCL pixel buffer: %d\n", status);
		std::cerr << "Failed to read the OpenCL pixel buffer:" << std::to_string(status) << std::endl;
		exit(-1);
	}

	/*------------------------------------------------------------------------*/

	const double elapsedTime = WallClockTime() - startTime;
	const int samples = currentSample - startSampleCount;
	const double sampleSec = samples * image_height * image_width / elapsedTime;
	//sprintf(captionBuffer, "Rendering time %.3f sec (pass %d)  Sample/sec  %.1fK\n",
	//		elapsedTime, currentSample, sampleSec / 1000.f);
}

void ReInitScene()
{	std::cout << "Reinitcene"<<std::endl;
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
void ReInit(const int reallocBuffers)
{	std::cout << "ReInit"<<std::endl;
	// Check if I have to reallocate buffers
	if (reallocBuffers)
	{
		delete pixels;
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



int main(int argc, char *argv[])
{	try{ 
	char const *scene = "scenes/cornell.scn";
	ReadScene(scene);
	//std::cout << "Scene read" << std::endl;
	UpdateCamera();
	//std::cout << "Updated Camera" << std::endl;
	if (setOpenCL("Ray tracer.cl") == 0)
	{
		std::cout << "Kernel work group size: " << local_work_size << std::endl;
		if (global_work_size % local_work_size != 0)
			global_work_size = (global_work_size / local_work_size + 1) * local_work_size;
		char const *title = "test";
		InitGlut(argc, argv, (char *)title);
		std::cout << "Glut init" << std::endl;
		try
		{

			glutMainLoop();
		}
		catch (std::exception &e)
		{	
			std::cout << "error" << std::endl;
			return 0;
		}

		//saveImage();
		//std::cout << "Rendering done!\nSaved image to 'opencl_raytracer.ppm'" << std::endl;
	}
	else
	{
		return (EXIT_FAILURE);
	}
}
catch(...){
	std::cout << "error" << std::endl;
	//std::cerr << e.what() <<std::endl;
}
}
