
#include "raytrace.hpp"

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
static int currentSphere = 0;
std::vector<Sphere> spheres;
unsigned int sphere_count = 0;
unsigned int *pixels;

boost::barrier *threadStartBarrier = new boost::barrier(2);
boost::barrier *threadEndBarrier = new boost::barrier(2);
boost::thread renderThread;
cl::Event kernelExecutionTime;

double WallClockTime() {
#if defined(__linux__) || defined(__APPLE__)
	struct timeval t;
	gettimeofday(&t, NULL);

	return t.tv_sec + t.tv_usec / 1000000.0;
#elif defined(WIN32)
	return GetTickCount64() / 1000.0;
#else
	#error "Unsupported Platform !!!"
#endif
}

void ReadScene(std::string fileName) {
	std::string unused_strings;
	std::cerr << "Reading scene: " << fileName << std::endl;
	std::ifstream file(fileName);

	if (!file) {
		std::cerr << "Failed to open file: " << fileName << std::endl;
		exit(-1);
	}

	/* Read the camera position */
	file >> unused_strings >> camera.orig.x >> camera.orig.y >> camera.orig.z
		>> camera.target.x >> camera.target.y >> camera.target.z;

	/* Read the sphere count */
	file >> unused_strings >> sphere_count;

	/* Read all spheres */
	spheres.assign(sphere_count, Sphere());
	for (size_t i = 0; i < sphere_count; i++) {
		Sphere& s = spheres[i];
		int mat;

		file >> unused_strings;
		file >> s.rad;						//Radius
		file >> s.p.x >> s.p.y >> s.p.z;	//Position
		file >> s.e.x >> s.e.y >> s.e.z;	//Emision
		file >> s.c.x >> s.c.y >> s.c.z;	//Color
		file >> mat;						//Material

		// Convert the number into the enum
		s.refl = returnEnum(mat);
	}
	file.close();
}

Refl returnEnum(int n){
	switch (n) {
		case 0:
			return DIFF;
		case 1:
			return SPEC;
		case 2:
			return REFR;
		default:
			std::cerr << "Failed to read material type for number"<<std::to_string(n) << std::endl;
			exit(-1);
	}

}

std::string returnEnumString(Refl e){
	switch (e) {
		case 0:
			return "Diffuse";
		case 1:
			return "Specular";
		case 2:
			return "Refractive";
		default:
			std::cerr << "Failed to read material type for number" << std::to_string(e) << std::endl;
			exit(-1);
	}

}

void UpdateCamera() {
	vsub(camera.dir, camera.target, camera.orig);
	vnorm(camera.dir);

	const Vec up = {0.f, 1.f, 0.f};
	const float fov = static_cast<float>((M_PI / 180.f) * 45.f);
	vxcross(camera.x, camera.dir, up);
	vnorm(camera.x);
	vsmul(camera.x, image_width * fov / image_height, camera.x);

	vxcross(camera.y, camera.x, camera.dir);
	vnorm(camera.y);
	vsmul(camera.y, fov, camera.y);

	cl_int status = queue.enqueueWriteBuffer(cameraBuffer, CL_TRUE, 0, sizeof(Camera), &camera);
	if (status != CL_SUCCESS) {
		std::cerr << "Failed to write the OpenCL camera buffer: " << std::to_string(status) << std::endl;
		exit(-1);
	}
}

void waitExecute() {

	// Start the rendering threads
	threadStartBarrier->wait();

	// Wait for job done signal (any reference to Bush is not itended...)
	threadEndBarrier->wait();
}

void ExecuteKernel() {
	/* Enqueue a kernel run call */
	if (global_work_size % local_work_size != 0)
		global_work_size = (global_work_size / local_work_size + 1) * local_work_size;
	kernelExecutionTime = cl::Event();
	queue.enqueueNDRangeKernel(kernel, cl::NullRange, global_work_size, local_work_size, NULL, &kernelExecutionTime);
}

void UpdateRendering() {

	double startTime = WallClockTime();
	int startSampleCount = currentSample;

	//--------------------------------------------------------------------------

	if (currentSample < 20) {
		waitExecute();

		currentSample++;
	}
	else {

		/* After first 20 samples, continue to execute kernels for more and more time */
		const double k = static_cast<double>(std::min(currentSample - 20u, 100u)) / 100.0;
		const double tresholdTime = 0.35 * k;
		while(true) {
			waitExecute();

			currentSample++;

			const double elapsedTime = WallClockTime() - startTime;
			if (elapsedTime > tresholdTime)
				break;
		}
	}

	/* Enqueue readBuffer */
	cl_int status = queue.enqueueReadBuffer(
		pixelBuffer,
		CL_TRUE,
		0,
		static_cast<size_t>(image_width) * static_cast<size_t>(image_height) * sizeof(unsigned int),
		pixels
	);

	if (status != CL_SUCCESS) {

		std::cerr << "Failed to read the OpenCL pixel buffer:" << std::to_string(status) << std::endl;
		exit(-1);
	}

	const double elapsedTime = WallClockTime() - startTime;
}

void specialFunc(int key, int x, int y) {
	switch (key) {
		case GLUT_KEY_UP: {
			Vec t = camera.target;
			vsub(t, t, camera.orig);
			t.y = t.y * cos(-ROTATE_STEP) + t.z * sin(-ROTATE_STEP);
			t.z = -t.y * sin(-ROTATE_STEP) + t.z * cos(-ROTATE_STEP);
			vadd(t, t, camera.orig);
			camera.target = t;
			std::cout << "Camera origin (" << camera.orig.x << "," << camera.orig.y << "," << camera.orig.z <<
				"). Camera target: (" << camera.target.x << "," << camera.target.y << "," << camera.target.z << ")."
				<< std::endl;
			ReInit(0);
			break;
		}
		case GLUT_KEY_DOWN: {
			Vec t = camera.target;
			vsub(t, t, camera.orig);
			t.y = t.y * cos(ROTATE_STEP) + t.z * sin(ROTATE_STEP);
			t.z = -t.y * sin(ROTATE_STEP) + t.z * cos(ROTATE_STEP);
			vadd(t, t, camera.orig);
			camera.target = t;
			std::cout << "Camera origin (" << camera.orig.x << "," << camera.orig.y << "," << camera.orig.z <<
				"). Camera target: (" << camera.target.x << "," << camera.target.y << "," << camera.target.z << ")."
				<< std::endl;
			ReInit(0);
			break;
		}
		case GLUT_KEY_LEFT: {
			Vec t = camera.target;
			vsub(t, t, camera.orig);
			t.x = t.x * cos(-ROTATE_STEP) - t.z * sin(-ROTATE_STEP);
			t.z = t.x * sin(-ROTATE_STEP) + t.z * cos(-ROTATE_STEP);
			vadd(t, t, camera.orig);
			camera.target = t;
			std::cout << "Camera origin (" << camera.orig.x << "," << camera.orig.y << "," << camera.orig.z <<
				"). Camera target: (" << camera.target.x << "," << camera.target.y << "," << camera.target.z << ")."
				<< std::endl;
			ReInit(0);
			break;
		}
		case GLUT_KEY_RIGHT: {
			Vec t = camera.target;
			vsub(t, t, camera.orig);
			t.x = t.x * cos(ROTATE_STEP) - t.z * sin(ROTATE_STEP);
			t.z = t.x * sin(ROTATE_STEP) + t.z * cos(ROTATE_STEP);
			vadd(t, t, camera.orig);
			camera.target = t;
			std::cout << "Camera origin (" << camera.orig.x << "," << camera.orig.y << "," << camera.orig.z <<
				"). Camera target: (" << camera.target.x << "," << camera.target.y << "," << camera.target.z << ")."
				<< std::endl;
			ReInit(0);
			break;
		}
		default:
			break;
	}
}

void idleFunc(void) {
	UpdateRendering();
	glutPostRedisplay();
}

void saveImage() {

	std::ofstream file("Path trace image.ppm");

	if (!file) {
		std::cerr << "Failed to open image file: Path trace image.ppm" << std::endl;
		return;
	}

	file << "P3" << std::endl;
	file << image_width << " " << image_height << std::endl;
	file << 255 << std::endl;

	for (int y = image_height - 1; y >= 0; --y) {
		unsigned char* p = (unsigned char*)(&pixels[y * image_width]);
		for (int x = 0; x < image_width; ++x, p += 4)
			file << (int)p[0] << " " << (int)p[1] << " " << (int)p[2] << " ";
	}
	file.close();
}

void ReInit(const int reallocBuffers) {
	queue.finish();

	// Check if I have to reallocate buffers
	if (reallocBuffers) {

		FreeBuffers();
		AllocateBuffers();
	}

	UpdateCamera();

	currentSample = 0;
}

void keyFunc(unsigned char key, int x, int y) {
	switch (key) {
		case 'p':
			saveImage();
			std::cout << "Imaged saved as Path trace image.ppm" <<std::endl;
			break;
		case 27: /* Escape key */
			fprintf(stderr, "Done.\n");
			exit(0);
			break;
		case ' ': /* Refresh display */
			ReInit(1);
			break;
		case 'a': {
			Vec dir = camera.x;
			vnorm(dir);
			vsmul(dir, -MOVE_STEP, dir);
			vadd(camera.orig, camera.orig, dir);
			vadd(camera.target, camera.target, dir);
			fprintf(stderr, "Camera origin (%f,%f,%f). Camera target: (%f,%f,%f).\n",
					camera.orig.x, camera.orig.y, camera.orig.z,camera.target.x,camera.target.y,camera.target.z);
			ReInit(0);
			break;
		}
		case 'd': {
			Vec dir = camera.x;
			vnorm(dir);
			vsmul(dir, MOVE_STEP, dir);
			vadd(camera.orig, camera.orig, dir);
			vadd(camera.target, camera.target, dir);
			fprintf(stderr, "Camera origin (%f,%f,%f). Camera target: (%f,%f,%f).\n",
					camera.orig.x, camera.orig.y, camera.orig.z,camera.target.x,camera.target.y,camera.target.z);
			ReInit(0);
			break;
		}
		case 'w': {
			Vec dir = camera.dir;
			vsmul(dir, MOVE_STEP, dir);
			vadd(camera.orig, camera.orig, dir);
			vadd(camera.target, camera.target, dir);
			fprintf(stderr, "Camera origin (%f,%f,%f). Camera target: (%f,%f,%f).\n",
					camera.orig.x, camera.orig.y, camera.orig.z,camera.target.x,camera.target.y,camera.target.z);
			ReInit(0);
			break;
		}
		case 's': {
			Vec dir = camera.dir;
			vsmul(dir, -MOVE_STEP, dir);
			vadd(camera.orig, camera.orig, dir);
			vadd(camera.target, camera.target, dir);
			fprintf(stderr, "Camera origin (%f,%f,%f). Camera target: (%f,%f,%f).\n",
					camera.orig.x, camera.orig.y, camera.orig.z,camera.target.x,camera.target.y,camera.target.z);
			ReInit(0);
			break;
		}
		case 'r':
			camera.orig.y += MOVE_STEP;
			camera.target.y += MOVE_STEP;
			fprintf(stderr, "Camera origin (%f,%f,%f). Camera target: (%f,%f,%f).\n",
					camera.orig.x, camera.orig.y, camera.orig.z,camera.target.x,camera.target.y,camera.target.z);
			ReInit(0);
			break;
		case 'f':
			camera.orig.y -= MOVE_STEP;
			camera.target.y -= MOVE_STEP;
			fprintf(stderr, "Camera origin (%f,%f,%f). Camera target: (%f,%f,%f).\n",
					camera.orig.x, camera.orig.y, camera.orig.z,camera.target.x,camera.target.y,camera.target.z);
			ReInit(0);
			break;
		case '+':
			currentSphere = (currentSphere + 1) % sphere_count;
			fprintf(stderr, "Selected sphere %d (%f %f %f)\n", currentSphere,
					spheres[currentSphere].p.x, spheres[currentSphere].p.y, spheres[currentSphere].p.z);
			ReInitScene();
			break;
		case '-':
			currentSphere = (currentSphere + (sphere_count - 1)) % sphere_count;
			fprintf(stderr, "Selected sphere %d (%f %f %f)\n", currentSphere,
					spheres[currentSphere].p.x, spheres[currentSphere].p.y, spheres[currentSphere].p.z);
			ReInitScene();
			break;
		case '4':
			spheres[currentSphere].p.x -= 0.5f * MOVE_STEP;
			fprintf(stderr, "Selected sphere %d (%f %f %f)\n", currentSphere,
					spheres[currentSphere].p.x, spheres[currentSphere].p.y, spheres[currentSphere].p.z);
			ReInitScene();
			break;
		case '6':
			spheres[currentSphere].p.x += 0.5f * MOVE_STEP;
			fprintf(stderr, "Selected sphere %d (%f %f %f)\n", currentSphere,
					spheres[currentSphere].p.x, spheres[currentSphere].p.y, spheres[currentSphere].p.z);
			ReInitScene();
			break;
		case '7':
			spheres[currentSphere].refl = returnEnum((spheres[currentSphere].refl +1) % 3);
			std::cout << "Selected sphere " <<std::to_string(currentSphere)<<". Type of reflection: "
				<< returnEnumString(returnEnum((spheres[currentSphere].refl +1) % 3))<<"."<<std::endl;
			ReInitScene();
			break;
		case '8':
			spheres[currentSphere].p.z -= 0.5f * MOVE_STEP;
			fprintf(stderr, "Selected sphere %d (%f %f %f)\n", currentSphere,
					spheres[currentSphere].p.x, spheres[currentSphere].p.y, spheres[currentSphere].p.z);
			ReInitScene();
			break;
		case '2':
			spheres[currentSphere].p.z += 0.5f * MOVE_STEP;
			fprintf(stderr, "Selected sphere %d (%f %f %f)\n", currentSphere,
					spheres[currentSphere].p.x, spheres[currentSphere].p.y, spheres[currentSphere].p.z);
			ReInitScene();
			break;
		case '9':
			spheres[currentSphere].p.y += 0.5f * MOVE_STEP;
			fprintf(stderr, "Selected sphere %d (%f %f %f)\n", currentSphere,
					spheres[currentSphere].p.x, spheres[currentSphere].p.y, spheres[currentSphere].p.z);
			ReInitScene();
			break;
		case '3':
			spheres[currentSphere].p.y -= 0.5f * MOVE_STEP;
			fprintf(stderr, "Selected sphere %d (%f %f %f)\n", currentSphere,
					spheres[currentSphere].p.x, spheres[currentSphere].p.y, spheres[currentSphere].p.z);
			ReInitScene();
			break;
		case '1':
			spheres[currentSphere].refl = returnEnum( (spheres[currentSphere].refl + (2)) % 3);
			std::cout << "Selected sphere " <<std::to_string(currentSphere)<<". Type of reflection: "
				<< returnEnumString(returnEnum((spheres[currentSphere].refl +1) % 3))<<"."<<std::endl;
			ReInitScene();
			break;
		default:
			break;
	}
}

void displayFunc(void) {
	UpdateRendering();

	glClear(GL_COLOR_BUFFER_BIT);
	glRasterPos2i(0, 0);
	glDrawPixels(image_width, image_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glutSwapBuffers();
}
void FreeBuffers()
{
	delete[] seeds;
	delete[] colors;
	delete[] pixels;
}

void AllocateBuffers() {

	try {
		const size_t pixelCount = static_cast<size_t>(image_width) * static_cast<size_t>(image_height);
		colors = new Vec[pixelCount];

		seeds = new unsigned int[pixelCount * 2];
		for (unsigned int i = 0; i < pixelCount * 2; i++) {
			seeds[i] = static_cast<unsigned int>(rand());
			if (seeds[i] < 2)
				seeds[i] = 2;
		}

		pixels = new unsigned int[pixelCount];
		// Test colors
		for (unsigned int i = 0; i < pixelCount; ++i)
			pixels[i] = i;

		size_t sizeBytes = sizeof(Vec) * image_width * image_height;
		colorBuffer = cl::Buffer(context, CL_MEM_READ_WRITE, sizeBytes);

		sizeBytes = sizeof(unsigned int) * image_width * image_height;
		pixelBuffer = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeBytes);

		sizeBytes = sizeof(unsigned int) * image_width * image_height * 2;
		seedBuffer = cl::Buffer(context, CL_MEM_READ_WRITE, sizeBytes);
		queue.enqueueWriteBuffer(seedBuffer, CL_TRUE, 0, sizeBytes, seeds);
	}
	catch (cl::Error &err) {
		std::cout << "Error: " << err.what() << "(" << err.err() << ")" << std::endl;
		for (cl::Device dev : devices) {
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
	catch (std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
}

void reshapeFunc(int newWidth, int newHeight) {
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
	glutInitWindowPosition(0, 0);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutInit(&argc, argv);

	glutCreateWindow(windowTittle);
}

void runGlut() {
	glutSetKeyRepeat(GLUT_KEY_REPEAT_ON);
	glutReshapeFunc(reshapeFunc);
	glutKeyboardFunc(keyFunc);
	glutSpecialFunc(specialFunc);
	glutDisplayFunc(displayFunc);
	glutIdleFunc(idleFunc);

	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, image_width, image_height);
	glLoadIdentity();
	glOrtho(0.f, image_width - 1.f, 0.f, image_height - 1.f, -1.f, 1.f);
	glutMainLoop();
}

void render() {
	try {

		int kernel_count = 0;
		double time_accum = 0;

		printf("Showing average time taken every 60 kernels");

		while (true) {
			threadStartBarrier->wait();

			/* Set kernel arguments */
			kernel.setArg(0, colorBuffer);
			kernel.setArg(1, seedBuffer);
			kernel.setArg(2, sphereBuffer);
			kernel.setArg(3, cameraBuffer);
			kernel.setArg(4, sphere_count);
			kernel.setArg(5, image_width);
			kernel.setArg(6, image_height);
			kernel.setArg(7, currentSample);
			kernel.setArg(8, pixelBuffer);

			ExecuteKernel();
			cl_int status = queue.enqueueReadBuffer(
				pixelBuffer,
				CL_TRUE,
				0,
				static_cast<size_t>(image_width) * static_cast<size_t>(image_height) * sizeof(unsigned int),
				pixels
			);

			kernelExecutionTime.wait();

			cl_ulong time_start = 0;
			cl_ulong time_end = 0;

			time_start = kernelExecutionTime.getProfilingInfo<CL_PROFILING_COMMAND_START>();
			time_end = kernelExecutionTime.getProfilingInfo<CL_PROFILING_COMMAND_END>();

			time_accum += time_end - time_start;


			kernel_count++;

			if (kernel_count > 60) {
				printf("Kernel Execution time is: %0.5f milliseconds \n", (time_accum / 1000000.0) / 30);
				kernel_count = 0;
				time_accum = 0;
			}

			queue.finish();

			threadEndBarrier->wait();
		}
	}
	catch (boost::thread_interrupted) {
		std::cerr << " Rendering thread halted" << std::endl;
	}
	catch (cl::Error err) {
		std::cerr << " ERROR: " << err.what() << "(" << err.err() << ")" << std::endl;
	}
}
int setOpenCL(const std::string &sourceName) {

	try {

		// Query for platforms
		std::vector<cl::Platform> platforms;
		cl::Platform::get(&platforms);

		// Get a list of devices on this platform

		// Select the platform.
		int platform_id;
		if (platforms.size() > 1) {

			std::cout << "Platforms:" << std::endl;
			for (unsigned int i = 0; i < platforms.size(); i++) {
				std::cout << std::to_string(i) << ") " << platforms[i].getInfo<CL_PLATFORM_NAME>() << std::endl;
			}
			std::cout << std::endl;

			int correct_platform = 0;
			std::cout << "Enter the number of the platform to use." << std::endl;
			while (correct_platform == 0) {

				std::cin >> platform_id;
				if (platform_id < platforms.size())
					correct_platform = 1;
				else
					std::cout << "Number of platform doesn't exist. Please try again with a number from the list." << std::endl;
			}
			std::cout << std::endl;
		}
		else {
			platform_id = 0;
		}

		//Select the device.
		int device_id;
		platforms[platform_id].getDevices(CL_DEVICE_TYPE_ALL, &devices);
		if (devices.size() > 1) {
			std::cout << "Devices:" << std::endl;
			for (unsigned int i = 0; i < devices.size(); i++) {
				std::cout << std::to_string(i) << ") " << devices[i].getInfo<CL_DEVICE_NAME>() << std::endl;
			}
			std::cout << std::endl;

			int correct_device = 0;
			std::cout << "Enter the number of the device to use." << std::endl;
			while (correct_device == 0) {

				std::cin >> device_id;
				if (device_id < devices.size())
					correct_device = 1;
				else
					std::cout << "Number of device doesn't exist. Please try again with a number from the list." << std::endl;
			}
		}
		else {
			device_id = 0;
		}

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

		// Create a command queue
		queue = clCreateCommandQueue(context(), devices[device_id](), CL_QUEUE_PROFILING_ENABLE, NULL);

		//queue = cl::CommandQueue(context, devices[device_id], CL_QUEUE_PROFILING_ENABLE);


		sphereBuffer = cl::Buffer(context,
#ifdef __APPLE__
								  CL_MEM_READ_WRITE, // NOTE: not READ_ONLY because of Apple's OpenCL bug
#else
								  CL_MEM_READ_ONLY,
#endif
								  sizeof(Sphere) * sphere_count);
		queue.enqueueWriteBuffer(sphereBuffer, CL_TRUE, 0, sizeof(Sphere) * sphere_count, spheres.data());

		cameraBuffer = cl::Buffer(context,
#ifdef __APPLE__
								  CL_MEM_READ_WRITE, // NOTE: not READ_ONLY because of Apple's OpenCL bug
#else
								  CL_MEM_READ_ONLY,
#endif
								  sizeof(Camera));
		queue.enqueueWriteBuffer(cameraBuffer, CL_TRUE, 0, sizeof(Camera), &camera);

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
		global_work_size = static_cast<size_t>(image_width) * static_cast<size_t>(image_height);
		local_work_size = kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(devices[device_id]);
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
void ReInitScene() {
	currentSample = 0;

	// Redownload the scene
	cl_int status = queue.enqueueWriteBuffer(sphereBuffer, CL_TRUE, 0, sizeof(Sphere) * sphere_count, spheres.data());

	if (status != CL_SUCCESS) {
		std::cerr << "Failed to write the OpenCL scene buffer: " << std::to_string(status) << std::endl;
		exit(-1);
	}
}
int main(int argc, char *argv[]) {
	try {

		std::cout << std::endl;
		std::cout << "Enter width of image." << std::endl;
		std::cin >> image_width;

		std::cout << "Enter height of image." << std::endl;
		std::cin >> image_height;
		std::cout << std::endl;

		std::cout << "List of scenes availables: " << std::endl;
		std::string path = "res/scenes";
		int counter = 0;
		for (const auto &entry : fs::directory_iterator(path)) {
			counter += 1;
		}
		std::vector<std::string> scenes(counter);
		counter = 0;
		for (const auto &entry : fs::directory_iterator(path)) {
			std::cout << std::to_string(counter) << ") " << entry.path() << std::endl;
			scenes[counter] = entry.path().string();
			counter += 1;
		}
		int correct_scene = 0;
		std::string scene;
		int selected;
		std::cout << std::endl;
		while (correct_scene == 0) {
			std::cout << "Enter the number of the scene to load: " << std::endl;
			std::cin >> selected;

			if (selected < counter) {

				scene = scenes[selected];
				correct_scene = 1;
			}
			else {
				std::cout << "Number of scene doesn't exist. Please try again with a number from the list." << std::endl;
			}
		}

		char const *title = "test";
		InitGlut(argc, argv, (char *)title);
		ReadScene(scene);
		setOpenCL("res/kernels/Ray tracer.cl");
		std::cout << "Glut init" << std::endl;
		currentSample = 0;
		runGlut();
		return 0;
	}

	catch (cl::Error &e) {
		std::cout << e.what() << std::endl;
	}
}
