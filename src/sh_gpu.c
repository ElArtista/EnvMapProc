#include <emproc/sh.h>
#include <stdlib.h>
#include <stdio.h>
#include "cl_helper.h"
#include "gpush.h"

#define PI4     12.566370614359172953850573533118011536788677597500423

void sh_coeffs_gpu(double sh_coeffs[SH_COEFF_NUM][3], struct envmap* em, float* nsa_idx)
{
    /* Sizes */
    const uint8_t bytes_per_channel = sizeof(unsigned char);
    const size_t face_size = em->width / 4;
    const size_t data_sz = bytes_per_channel * em->channels * em->width * em->height;
    const size_t nsa_idx_sz = face_size * face_size * 6 * 4 * sizeof(float);
    const size_t sh_coeffs_sz = SH_COEFF_NUM * 3 * sizeof(double);

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
    const char* cl_src = (const char*) gpush_pp;
    const size_t cl_src_len = gpush_pp_len;
    cl_program prog = clCreateProgramWithSource(ctx, 1, &cl_src, &cl_src_len, &err);
    cl_check_error(err, "Creating program");
    err = clBuildProgram(prog, 0, 0, 0, 0, 0);
    /* Check for build errors */
    if (err != CL_SUCCESS) {
        cl_print_prog_build_info_log(prog, ctx_did);
        goto cleanup_prog;
    }

    /* Create kernel from source */
    cl_kernel kernel = clCreateKernel(prog, "booo", &err);
    cl_check_error(err, "Creating Kernel");

    /* Create command queue */
    cl_command_queue_properties cmd_queue_props = 0;
    cl_command_queue cmd_queue = clCreateCommandQueue(ctx, ctx_did, cmd_queue_props, &err);
    cl_check_error(err, "Creating Command Queue");

    /* Create input and output array in device memory */
    double weight_accum = 0.0f;
    cl_mem sh_out_dev_mem  = clCreateBuffer(ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sh_coeffs_sz, sh_coeffs, &err);
    cl_mem wacum_dev_mem   = clCreateBuffer(ctx, CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(weight_accum), &weight_accum, &err);
    cl_mem img_in_dev_mem  = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, data_sz, em->data, &err);
    cl_mem nsa_idx_dev_mem = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, nsa_idx_sz, nsa_idx, &err);
    cl_check_error(err, "Creating buffers");

    /* Enqueue kernel */
    err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &sh_out_dev_mem);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &wacum_dev_mem);
    err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &img_in_dev_mem);
    err |= clSetKernelArg(kernel, 3, sizeof(cl_mem), &nsa_idx_dev_mem);
    err |= clSetKernelArg(kernel, 4, sizeof(unsigned int), &face_size);
    cl_check_error(err, "Setting kernel arguments");

    for (unsigned int i = 0; i < 6; ++i) {
        /* Set face id argument */
        err |= clSetKernelArg(kernel, 5, sizeof(unsigned int), &i);
        /* Execute the kernel over the entire range of our 2D input data set
           letting the OpenCL runtime choose the work-group size */
        size_t work_size[2] = {face_size, face_size};
        err = clEnqueueNDRangeKernel(cmd_queue, kernel, 2, 0, work_size, 0, 0, 0, 0);
        cl_check_error(err, "Enqueueing kernel");
        /* Wait finish */
        clFinish(cmd_queue);
    }
    /* Read back the result from the compute device */
    err = clEnqueueReadBuffer(cmd_queue, sh_out_dev_mem, CL_TRUE, 0, sh_coeffs_sz, sh_coeffs, 0, 0, 0);
    err = clEnqueueReadBuffer(cmd_queue, wacum_dev_mem, CL_TRUE, 0, sizeof(weight_accum), &weight_accum, 0, 0, 0);
    cl_check_error(err, "Reading back result");

    /* Normalize */
    const double norm = PI4 / weight_accum;
    for (uint8_t ii = 0; ii < SH_COEFF_NUM; ++ii) {
        sh_coeffs[ii][0] *= norm;
        sh_coeffs[ii][1] *= norm;
        sh_coeffs[ii][2] *= norm;
    }

    /* Free device resources */
    clReleaseMemObject(nsa_idx_dev_mem);
    clReleaseMemObject(img_in_dev_mem);
    clReleaseMemObject(wacum_dev_mem);
    clReleaseMemObject(sh_out_dev_mem);

    /* Release OpenCL objects */
    clReleaseCommandQueue(cmd_queue);
    clReleaseKernel(kernel);
cleanup_prog:
    clReleaseProgram(prog);
    clReleaseContext(ctx);
}
