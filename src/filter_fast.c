#include <emproc/filter.h>
#include <stdlib.h>
#include <stdio.h>
#include "cl_helper.h"
#include "gpufilter.h"

static int cl_choose_platform_and_device(cl_platform_id* plat_id, cl_device_id* dev_id)
{
    int found_id_pair = 0;
    /* Query available platform id count */
    cl_uint platform_id_cnt = 0;
    clGetPlatformIDs(0, 0, &platform_id_cnt);
    printf("Found %d platforms\n", platform_id_cnt);

    /* Allocate buffer to hold platform ids and fill it */
    cl_platform_id* platform_ids = calloc(platform_id_cnt, sizeof(cl_platform_id));
    clGetPlatformIDs(platform_id_cnt, platform_ids, 0);

    /* For each platform */
    for (size_t i = 0; i < platform_id_cnt; ++i) {
        cl_platform_id pid = platform_ids[i];
        /* Query available devices id count */
        cl_uint device_id_cnt = 0;
        clGetDeviceIDs(pid, CL_DEVICE_TYPE_ALL, 0, 0, &device_id_cnt);
        /* Show platform info */
        char plat_buf[256];
        clGetPlatformInfo(pid, CL_PLATFORM_VENDOR, sizeof(plat_buf), plat_buf, 0);
        printf("Platform %s (%d devices)\n", plat_buf, device_id_cnt);
        /* Allocate buffer to hold device ids and fill it */
        cl_device_id* device_ids = calloc(device_id_cnt, sizeof(cl_device_id));
        clGetDeviceIDs(pid, CL_DEVICE_TYPE_ALL, device_id_cnt, device_ids, 0);
        /* For each device */
        for (size_t j = 0; j < device_id_cnt; ++j) {
            cl_device_id did = device_ids[j];
            /* Get first pair */
            if (!found_id_pair) {
                *plat_id = pid;
                *dev_id = did;
                found_id_pair = 1;
            }
            /* Show device info */
            char dev_name_buf[256];
            clGetDeviceInfo(did, CL_DEVICE_NAME, sizeof(dev_name_buf), dev_name_buf, 0);
            char dev_type_buf[256];
            clGetDeviceInfo(did, CL_DEVICE_TYPE, sizeof(dev_type_buf), dev_type_buf, 0);
            printf("  Device %s (Type: %s)\n", dev_name_buf, dev_type_buf);
        }
        free(device_ids);
    }
    free(platform_ids);
    return found_id_pair;
}

void irradiance_filter_fast(int width, int height, int channels, unsigned char* in, unsigned char* out, filter_progress_fn progress_fn, void* userdata)
{
    (void)width;
    (void)height;
    (void)channels;
    (void)in;
    (void)out;

    /* Platform and device ids used to create the context */
    cl_int err;
    cl_platform_id ctx_pid;
    cl_device_id ctx_did;
    err = cl_choose_platform_and_device(&ctx_pid, &ctx_did);
    if (!err) {
        printf("No OpenCL valid platform/device pair found!\n");
        return;
    }

    /* Create context */
    const cl_context_properties ctx_props[] = {
        CL_CONTEXT_PLATFORM, (cl_context_properties) ctx_pid,
        0, 0
    };
    cl_context ctx = clCreateContext(ctx_props, 1, &ctx_did, 0, 0, &err);
    if (err != CL_SUCCESS) {
        printf("Could not create OpenCL context!\n");
        return;
    }

    /* Create program */
    const char* cl_src = (const char*) gpufilter_pp;
    const size_t cl_src_len = gpufilter_pp_len;
    cl_program prog = clCreateProgramWithSource(ctx, 1, &cl_src, &cl_src_len, &err);
    cl_check_error(err, "Creating program");
    err = clBuildProgram(prog, 0, 0, 0, 0, 0);
    /* Check for build errors */
    if (err != CL_SUCCESS) {
        cl_print_prog_build_info_log(prog, ctx_did);
        goto cleanup_prog;
    }

    /* Create kernel from source */
    cl_kernel kernel = clCreateKernel(prog, "fooo", &err);
    cl_check_error(err, "Creating Kernel");

    /* Create command queue */
    cl_command_queue_properties cmd_queue_props = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    cl_command_queue cmd_queue = clCreateCommandQueue(ctx, ctx_did, cmd_queue_props, &err);
    cl_check_error(err, "Creating Command Queue");

    /* Fill 2 random vectors with data */
    const int vsz = 100;
    int* hv1 = malloc(sizeof(int) * vsz);
    int* hv2 = malloc(sizeof(int) * vsz);
    for (int i = 0; i < vsz; ++i) {
        hv1[i] = rand() / RAND_MAX;
        hv2[i] = rand() / RAND_MAX;
    }
    /* Allocate result vector */
    int* hres = malloc(sizeof(int) * vsz);

    /* Create input arrays in device memory */
    cl_mem dv1 = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(int) * vsz, hv1, &err);
    cl_mem dv2 = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(int) * vsz, hv2, &err);

    /* Create output array in device memory */
    cl_mem dres = clCreateBuffer(ctx, CL_MEM_WRITE_ONLY, sizeof(int) * vsz, 0, &err);

    /* Enqueue kernel */
    err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &dv1);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &dv2);
    err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &dres);
    cl_check_error(err, "Setting kernel arguments");

    /* Execute the kernel over the entire range of our 1d input data set
       letting the OpenCL runtime choose the work-group size */
    size_t global = vsz;
    err = clEnqueueNDRangeKernel(cmd_queue, kernel, 1, 0, &global, 0, 0, 0, 0);
    cl_check_error(err, "Enqueueing kernel 1st time");

    /* Read back the result from the compute device */
    err = clEnqueueReadBuffer(cmd_queue, dres, CL_TRUE, 0, sizeof(int) * vsz, hres, 0, 0, 0);
    cl_check_error(err, "Reading back result");

    /* Test results */
    int correct = 0;
    for (int i = 0; i < vsz; ++i) {
        int tmp = hv1[i] + hv2[i];
        if (tmp == hres[i])
            ++correct;
    }

    /* Results */
    printf("C = A + B: %d out of %d results were correct.\n", correct, vsz);

    /* Free device resources */
    clReleaseMemObject(dres);
    clReleaseMemObject(dv2);
    clReleaseMemObject(dv1);

    /* Free host resources */
    free(hres);
    free(hv2);
    free(hv1);

    /* Release OpenCL objects */
    clReleaseCommandQueue(cmd_queue);
    clReleaseKernel(kernel);
cleanup_prog:
    clReleaseProgram(prog);
    clReleaseContext(ctx);
}
