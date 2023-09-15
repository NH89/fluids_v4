#define __CL_ENABLE_EXCEPTIONS
#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_TARGET_OPENCL_VERSION 300

//float3 type definition
typedef struct {
    float x;
    float y;
    float z;
} float3;

//int3 type definition
typedef struct {
	int x;
	int y;
	int z;
} int3;

float3 make_float3(float x, float y, float z)
{
    float3 t; t.x = x; t.y = y; t.z = z; return t;
}

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <math.h>
#include <regex>
#include <jsoncpp/json/json.h>
//#include <CL/cl.h>
#define CHECK_ERROR(err) if (err != CL_SUCCESS) { printf("Error: %d\n", err); exit(1); }
//#include "RunCL.h"
#include "fluid.h"
#include "fluid_system.h"
//#include "fluid_system.cpp"
#include "host_CL.cpp"


#define SDK_SUCCESS 0
#define SDK_FAILURE 1

using namespace std;

int main()
{
    Json::Reader reader;
    Json::Value obj_;
    Json::Value obj;
    obj["verbosity"] = 1;
    obj["opencl_platform"] = 0;
    obj["opencl_device"] = 0;
    obj["kernel_filepath"] = "/home/goldi/Documents/KDevelop Projects/Morphogenesis/Morphogenesis/src/kernelTest.cl";
    FluidSystem fluid(obj);
    fluid.FluidSystem;
	fluid.Initialize();
    //fluid.launchParams.debug = 1;
    fluid.InitializeOpenCL();
    fluid.InsertParticlesCL ( 0x0, 0x0, 0x0 );


    //fluid.Run2PhysicalSort();

    //fluid.AllocateParticles(mMaxPoints, gpu_mode, cpu_mode);
    //fluid.InsertParticlesCL( 0x0, 0x0, 0x0 );
    //fluid.PrefixSumCellsCL(1); //Dependent on allocatemem!!
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// int main() {
//     cl_uint numPlatforms;
//     clGetPlatformIDs(0, NULL, &numPlatforms);
//     std::cout << "Number of platforms: " << numPlatforms << std::endl;
//
//     cl_platform_id* platforms = new cl_platform_id[numPlatforms];
//     clGetPlatformIDs(numPlatforms, platforms, NULL);
//
//     for (cl_uint i = 0; i < numPlatforms; i++) {
//         char platformName[128];
//         clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, sizeof(platformName), platformName, NULL);
//         std::cout << "Platform " << i << ": " << platformName << std::endl;
//
//         cl_uint numDevices;
//         clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &numDevices);
//         std::cout << "  Number of devices: " << numDevices << std::endl;
//
//         cl_device_id* devices = new cl_device_id[numDevices];
//         clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, numDevices, devices, NULL);
//
//         for (cl_uint j = 0; j < numDevices; j++) {
//             char deviceName[128];
//             clGetDeviceInfo(devices[j], CL_DEVICE_NAME, sizeof(deviceName), deviceName, NULL);
//             std::cout << "  Device " << j << ": " << deviceName << std::endl;
//         }
//
//         delete[] devices;
//     }
//
//     delete[] platforms;
//     return 0;
// }
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//     cl_int err;
//     cl_uint num_platforms;
//     cl_platform_id platform;
//     cl_device_id gpu_device;
//     cl_context context;
//     cl_command_queue gpu_queue;
//     cl_program program;
//     cl_kernel kernel;
//     cl_mem input_buffer;
//     int input_data[16] = {0};
//
//
//     // Read the kernel source from the file
//     std::ifstream kernelFile("/home/goldi/Documents/KDevelop Projects/Morphogenesis/Morphogenesis/src/fluid_system_opencl.cl");
//     std::string kernelSource((std::istreambuf_iterator<char>(kernelFile)),
//                              std::istreambuf_iterator<char>());
//
//     // Output the size of the kernel source file
//     std::cout << "Size of kernel source file: " << kernelSource.size() << " bytes" << std::endl;
//
//     // Count the number of kernels defined in the source
//     std::regex kernelRegex("\\b__kernel\\b");
//     auto kernelBegin = std::sregex_iterator(kernelSource.begin(), kernelSource.end(), kernelRegex);
//     auto kernelEnd = std::sregex_iterator();
//     int kernelCount = std::distance(kernelBegin, kernelEnd);
//
//     // Output the number of kernels
//     std::cout << "Number of kernels: " << kernelCount << std::endl;
//
//     const char* kernelSourceStr = kernelSource.c_str();
//
//    // Get platform
//     err = clGetPlatformIDs(1, &platform, &num_platforms);
//     CHECK_ERROR(err);
//
//     // Get GPU device
//     err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &gpu_device, NULL);
//     CHECK_ERROR(err);
//
//     // Create context
//     context = clCreateContext(NULL, 1, &gpu_device, NULL, NULL, &err);
//     CHECK_ERROR(err);
//
//     // Create command queue
//     cl_queue_properties properties[] = {0};
//     gpu_queue = clCreateCommandQueueWithProperties(context, gpu_device, properties, &err);
//     CHECK_ERROR(err);
//
//
//
//     // Create program
//     program = clCreateProgramWithSource(context, 1, &kernelSourceStr, NULL, &err);
//     CHECK_ERROR(err);
//
//         // Output the number of kernels
//     //std::cout << "Kernels: \n" << kernelSourceStr << std::endl;
//
//     // Build program
//     err = clBuildProgram(program, 1, &gpu_device, "", NULL, NULL);
//     if (err != CL_SUCCESS) {
//         size_t log_size;
//         char *log;
//         clGetProgramBuildInfo(program, gpu_device, CL_PROGRAM_BUILD_LOG,
//                               0, NULL,&log_size);
//         log = (char*)malloc(log_size+1);
//         clGetProgramBuildInfo(program, gpu_device,
//                               CL_PROGRAM_BUILD_LOG,
//                               log_size+1,
//                               log,
//                               NULL);
//         printf("%s\n", log);
//         free(log);
//         exit(1);
//     }
//
//     // Get maximum work-group size for device
//     size_t max_work_group_size;
//     clGetDeviceInfo(gpu_device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &max_work_group_size, NULL);
//     double work_size = max_work_group_size;
//     printf("Max size is %f",work_size);
//
//     // Create kernel
//     kernel = clCreateKernel(program,"increment",&err);
//     CHECK_ERROR(err);
//
//    // Debug Output
//     std::cout << "After Kernel\n " << std::endl;
//
//     // Create buffer
//     input_buffer = clCreateBuffer(context,
//                                   CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
//                                   sizeof(input_data),
//                                   input_data,
//                                   &err);
//     CHECK_ERROR(err);
//
//    // Set kernel arguments
//    err = clSetKernelArg(kernel,
//                         0,
//                         sizeof(cl_mem),
//                         &input_buffer);
//    CHECK_ERROR(err);
//
//    // Enqueue kernel to queue
//    size_t global_size = 16;
//    err = clEnqueueNDRangeKernel(gpu_queue,
//                                 kernel,
//                                 1,
//                                 NULL,
//                                 &global_size,
//                                 NULL,
//                                 0,
//                                 NULL,
//                                 NULL);
//    CHECK_ERROR(err);
//
//    // Wait for queue to finish
//    err = clFinish(gpu_queue);
//    CHECK_ERROR(err);
//
//    // Read results
//    err = clEnqueueReadBuffer(gpu_queue,
//                              input_buffer,
//                              CL_TRUE,
//                              0,
//                              sizeof(input_data),
//                              input_data,
//                              0,
//                              NULL,
//                              NULL);
//    CHECK_ERROR(err);
//
//    for (int i=0; i<16; i++) {
//        printf("%d ", input_data[i]);
//    }
//    printf("\n");

   // Debug Output
//     std::cout << "End of File\n " << std::endl;
//
//    return 0;
