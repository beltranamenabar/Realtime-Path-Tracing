#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <vector>
#include <stdlib.h>
#include <chrono>
#include <algorithm>
#include "camera.h"



typedef unsigned char ubyte;

#define __CL_ENABLE_EXCEPTIONS
#ifdef __APPLE__
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif

#define float3(x, y, z) {{x, y, z}}

std::size_t global_work_size;
std::size_t local_work_size;
cl::Program program;
std::vector<cl::Device> devices;
cl::Kernel kernel;
cl::CommandQueue queue;
cl_float4* cpu_output;
cl::Buffer cl_output;
cl::Buffer cl_spheres;
cl::Context context;
int image_width;
int image_height;
int samples;

static cl::Buffer colorBuffer;
static cl::Buffer pixelBuffer;
static cl::Buffer seedBuffer;

Sphere *spheres;
static Vec *colors;
static unsigned int *seeds;
Camera camera;
static int currentSample = 0;
unsigned int sphereCount;
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


// dummy variables are required for memory alignment
// float3 is considered as float4 by OpenCL
struct Sphere
{
	cl_float radius;
	cl_float dummy1;   
	cl_float dummy2;
	cl_float dummy3;
	cl_float3 position;
	cl_float3 color;
	cl_float3 emission;
};
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
	if(platforms.size()>1){
		
	std::cout << "Platforms:" << std::endl;
	for (int i = 0; i < platforms.size(); i ++){
		std::cout <<std::to_string(i) << ") " <<platforms[i].getInfo<CL_PLATFORM_NAME>() << std::endl;
	}
	std::cout << std::endl;

	
	int correct_platform = 0;
	std::cout << "Enter the number of the platform to use." << std::endl;
	while(correct_platform == 0){

	std::cin >> platform_id;
	if(platform_id < platforms.size()){
		correct_platform = 1;
	}
	else{
	std::cout << "Number of platform doesn't exist. Please try again with a number from the list." << std::endl;
	}
	}
	std::cout << std::endl;

	}
	else{
		platform_id = 0;

	}
	
	//Select the device.
	int device_id;
	platforms[platform_id].getDevices(CL_DEVICE_TYPE_ALL, &devices);
	if(devices.size() > 1){
	std::cout << "Devices:" << std::endl;
	for (int i = 0; i < devices.size(); i ++){
		std::cout <<std::to_string(i) << ") " <<devices[i].getInfo<CL_DEVICE_NAME>() << std::endl;
	}
	std::cout << std::endl;

	
	int correct_device = 0;
	std::cout << "Enter the number of the device to use." << std::endl;
	while(correct_device == 0){

	std::cin >> device_id;
	if(device_id < devices.size()){
		correct_device = 1;
	}
	else{
	std::cout << "Number of device doesn't exist. Please try again with a number from the list." << std::endl;
	}
	}
	}
	else{
		device_id = 0;
	}
    
	std::cout << std::endl;
	std::cout << "Enter width of image." << std::endl;
	std::cin >> image_width;


	//std::cout << std::endl;
	std::cout << "Enter height of image." << std::endl;
	std::cin >> image_height;

	
	std::cout << std::endl;
	std::cout << "Enter number of samples for Path tracing." << std::endl;
	std::cin >> samples;


	std::cout << std::endl;
	std::cout << "Platform:" << std::endl;
	std::cout <<platforms[platform_id].getInfo<CL_PLATFORM_NAME>() << std::endl << std::endl;

    std::cout << "Device:" << std::endl;
    std::cout << devices[device_id].getInfo<CL_DEVICE_NAME>() << std::endl
              << std::endl;

    // Create a context
    cl::Context contextray(devices);
    context = contextray;
    // Create a command queue
    // Select the device.
    queue = cl::CommandQueue(context, devices[device_id]);

    // Read the program source
    std::ifstream sourceFile(sourceName);
    std::string sourceCode(std::istreambuf_iterator<char>(sourceFile), (std::istreambuf_iterator<char>()));
    cl::Program::Sources source(1, std::make_pair(sourceCode.c_str(), sourceCode.length()));

    // Make program from the source code
    program = cl::Program(context, source);

    // Build the program for the devices
    program.build(devices);

    // Make kernel
    
    cl::Kernel ray_kernel(program, "render_kernel");
    kernel = ray_kernel;
    global_work_size = image_width * image_height;
    local_work_size = kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(devices[device_id]);
    //lifegame.maxThreads = devices[device_id].getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
    //std::cout << lifegame.maxThreads << std::endl;
  }
  catch (cl::Error err)
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
void display(void) {
	glClear(GL_COLOR_BUFFER_BIT);
	glRasterPos2i(0, 0);
	glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glutSwapBuffers();
}


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

void cleanUp(){
	delete cpu_output;
}


int main()
{
ReadScene("Scenes/cornell.scn",spheres,sphereCount,camera);
UpdateCamera(camera);
if (setOpenCL("Ray tracer.cl") == 0){
cpu_output = new cl_float3[image_width * image_height];

// initialise scene
const int sphere_count = 10;
Sphere cpu_spheres[sphere_count];
initScene(cpu_spheres);

// Create buffers on the OpenCL device for the image and the scene
cl_output = cl::Buffer(context, CL_MEM_WRITE_ONLY, image_width * image_height * sizeof(cl_float3));
cl_spheres = cl::Buffer(context, CL_MEM_READ_ONLY, sphere_count * sizeof(Sphere));
queue.enqueueWriteBuffer(cl_spheres, CL_TRUE, 0, sphere_count * sizeof(Sphere), cpu_spheres);

// specify OpenCL kernel arguments
kernel.setArg(0, cl_spheres);
kernel.setArg(1, image_width);
kernel.setArg(2, image_height);
kernel.setArg(3, 10);//sphere_count
kernel.setArg(4, samples);
kernel.setArg(5, cl_output);

std::cout << "Kernel work group size: " << local_work_size << std::endl;

// Ensure the global work size is a multiple of local work size
if (global_work_size % local_work_size != 0)
  global_work_size = (global_work_size / local_work_size + 1) * local_work_size;

queue.enqueueNDRangeKernel(kernel, cl::NullRange, global_work_size, local_work_size);
queue.finish();
queue.enqueueReadBuffer(cl_output, CL_TRUE, 0, image_width * image_height * sizeof(cl_float3), cpu_output);
saveImage();
	std::cout << "Rendering done!\nSaved image to 'opencl_raytracer.ppm'" << std::endl;

	// release memory
	cleanUp();
}
else
  {
    return (EXIT_FAILURE);
  }
}
