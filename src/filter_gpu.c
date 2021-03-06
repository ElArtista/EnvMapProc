#include <emproc/filter.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cl_helper.h"
#include "gpufilter.h"

void irradiance_filter_gpu(struct envmap* em_out, struct envmap* em_in, filter_progress_fn progress_fn, void* userdata)
{
    /* Sizes */
    uint8_t bytes_per_channel = sizeof(unsigned char);
    size_t data_sz = bytes_per_channel * em_in->channels * em_in->width * em_in->height;

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

    /* Create input and output array in device memory */
    cl_mem in_dev_mem = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, data_sz, em_in->data, &err);
    cl_mem out_dev_mem = clCreateBuffer(ctx, CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR, data_sz, em_out->data, &err);

    /* Enqueue kernel */
    int face_size = em_in->width / 4;
    err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &out_dev_mem);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &in_dev_mem);
    err |= clSetKernelArg(kernel, 2, sizeof(unsigned int), &face_size);
    cl_check_error(err, "Setting kernel arguments");

    for (unsigned int i = 0; i < 6; ++i) {
        /* Set face id argument */
        err |= clSetKernelArg(kernel, 3, sizeof(unsigned int), &i);
        /* Execute the kernel over the entire range of our 2D input data set
           letting the OpenCL runtime choose the work-group size */
        size_t work_size[2] = {face_size, face_size};
        err = clEnqueueNDRangeKernel(cmd_queue, kernel, 2, 0, work_size, 0, 0, 0, 0);
        cl_check_error(err, "Enqueueing kernel");
        /* Read back the result from the compute device */
        err = clEnqueueReadBuffer(cmd_queue, out_dev_mem, CL_TRUE, 0, data_sz, em_out->data, 0, 0, 0);
        cl_check_error(err, "Reading back result");
        /* Wait for current face to finish */
        clFinish(cmd_queue);
        /* If progress fn given call it */
        if (progress_fn)
            progress_fn(userdata);
    }

    /* Free device resources */
    clReleaseMemObject(out_dev_mem);
    clReleaseMemObject(in_dev_mem);

    /* Release OpenCL objects */
    clReleaseCommandQueue(cmd_queue);
    clReleaseKernel(kernel);
cleanup_prog:
    clReleaseProgram(prog);
    clReleaseContext(ctx);
}
