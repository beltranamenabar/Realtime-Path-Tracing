#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <stdlib.h>
#include <chrono>
#include <algorithm>

typedef unsigned char ubyte;

#define __CL_ENABLE_EXCEPTIONS
#ifdef __APPLE__
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif

cl::Program program;
std::vector<cl::Device> devices;
cl::Kernel kernel;
cl::CommandQueue queue;
cl_float4* cpu_output;
cl::Buffer cl_output;
cl::Context context;
const int image_width = 1280;
const int image_height = 720;


int setOpenCL(const std::string &sourceName)
{
  int platform_id = 0, device_id = 0;

  try
  {

    // Query for platforms
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);

    // Get a list of devices on this platform

    // Select the platform.
    platforms[platform_id].getDevices(CL_DEVICE_TYPE_GPU, &devices);
    std::cout << "Platform:" << std::endl;
    std::cout << platforms[platform_id].getInfo<CL_PLATFORM_NAME>() << std::endl
              << std::endl;

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

inline float clamp(float x){ return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }

// convert RGB float in range [0,1] to int in range [0, 255]
inline int toInt(float x){ return int(clamp(x) * 255 + .5); }

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
if (setOpenCL("Ray tracer.cl") == 0){
cpu_output = new cl_float3[image_width * image_height];
cl_output = cl::Buffer(context, CL_MEM_WRITE_ONLY, image_width * image_height * sizeof(cl_float3));
kernel.setArg(0, cl_output);
kernel.setArg(1, image_width);
kernel.setArg(2, image_height);
std::size_t global_work_size = image_width * image_height;
std::size_t local_work_size = 64; 
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
