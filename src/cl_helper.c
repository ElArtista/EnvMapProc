#include "cl_helper.h"
#include <stdio.h>
#include <stdlib.h>

const char* cl_err_code(cl_int err_in)
{
    switch (err_in) {
        case CL_SUCCESS:
            return "CL_SUCCESS";
        case CL_DEVICE_NOT_FOUND:
            return "CL_DEVICE_NOT_FOUND";
        case CL_DEVICE_NOT_AVAILABLE:
            return "CL_DEVICE_NOT_AVAILABLE";
        case CL_COMPILER_NOT_AVAILABLE:
            return "CL_COMPILER_NOT_AVAILABLE";
        case CL_MEM_OBJECT_ALLOCATION_FAILURE:
            return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
        case CL_OUT_OF_RESOURCES:
            return "CL_OUT_OF_RESOURCES";
        case CL_OUT_OF_HOST_MEMORY:
            return "CL_OUT_OF_HOST_MEMORY";
        case CL_PROFILING_INFO_NOT_AVAILABLE:
            return "CL_PROFILING_INFO_NOT_AVAILABLE";
        case CL_MEM_COPY_OVERLAP:
            return "CL_MEM_COPY_OVERLAP";
        case CL_IMAGE_FORMAT_MISMATCH:
            return "CL_IMAGE_FORMAT_MISMATCH";
        case CL_IMAGE_FORMAT_NOT_SUPPORTED:
            return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
        case CL_BUILD_PROGRAM_FAILURE:
            return "CL_BUILD_PROGRAM_FAILURE";
        case CL_MAP_FAILURE:
            return "CL_MAP_FAILURE";
        case CL_MISALIGNED_SUB_BUFFER_OFFSET:
            return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
        case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:
            return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
        case CL_INVALID_VALUE:
            return "CL_INVALID_VALUE";
        case CL_INVALID_DEVICE_TYPE:
            return "CL_INVALID_DEVICE_TYPE";
        case CL_INVALID_PLATFORM:
            return "CL_INVALID_PLATFORM";
        case CL_INVALID_DEVICE:
            return "CL_INVALID_DEVICE";
        case CL_INVALID_CONTEXT:
            return "CL_INVALID_CONTEXT";
        case CL_INVALID_QUEUE_PROPERTIES:
            return "CL_INVALID_QUEUE_PROPERTIES";
        case CL_INVALID_COMMAND_QUEUE:
            return "CL_INVALID_COMMAND_QUEUE";
        case CL_INVALID_HOST_PTR:
            return "CL_INVALID_HOST_PTR";
        case CL_INVALID_MEM_OBJECT:
            return "CL_INVALID_MEM_OBJECT";
        case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
            return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
        case CL_INVALID_IMAGE_SIZE:
            return "CL_INVALID_IMAGE_SIZE";
        case CL_INVALID_SAMPLER:
            return "CL_INVALID_SAMPLER";
        case CL_INVALID_BINARY:
            return "CL_INVALID_BINARY";
        case CL_INVALID_BUILD_OPTIONS:
            return "CL_INVALID_BUILD_OPTIONS";
        case CL_INVALID_PROGRAM:
            return "CL_INVALID_PROGRAM";
        case CL_INVALID_PROGRAM_EXECUTABLE:
            return "CL_INVALID_PROGRAM_EXECUTABLE";
        case CL_INVALID_KERNEL_NAME:
            return "CL_INVALID_KERNEL_NAME";
        case CL_INVALID_KERNEL_DEFINITION:
            return "CL_INVALID_KERNEL_DEFINITION";
        case CL_INVALID_KERNEL:
            return "CL_INVALID_KERNEL";
        case CL_INVALID_ARG_INDEX:
            return "CL_INVALID_ARG_INDEX";
        case CL_INVALID_ARG_VALUE:
            return "CL_INVALID_ARG_VALUE";
        case CL_INVALID_ARG_SIZE:
            return "CL_INVALID_ARG_SIZE";
        case CL_INVALID_KERNEL_ARGS:
            return "CL_INVALID_KERNEL_ARGS";
        case CL_INVALID_WORK_DIMENSION:
            return "CL_INVALID_WORK_DIMENSION";
        case CL_INVALID_WORK_GROUP_SIZE:
            return "CL_INVALID_WORK_GROUP_SIZE";
        case CL_INVALID_WORK_ITEM_SIZE:
            return "CL_INVALID_WORK_ITEM_SIZE";
        case CL_INVALID_GLOBAL_OFFSET:
            return "CL_INVALID_GLOBAL_OFFSET";
        case CL_INVALID_EVENT_WAIT_LIST:
            return "CL_INVALID_EVENT_WAIT_LIST";
        case CL_INVALID_EVENT:
            return "CL_INVALID_EVENT";
        case CL_INVALID_OPERATION:
            return "CL_INVALID_OPERATION";
        case CL_INVALID_GL_OBJECT:
            return "CL_INVALID_GL_OBJECT";
        case CL_INVALID_BUFFER_SIZE:
            return "CL_INVALID_BUFFER_SIZE";
        case CL_INVALID_MIP_LEVEL:
            return "CL_INVALID_MIP_LEVEL";
        case CL_INVALID_GLOBAL_WORK_SIZE:
            return "CL_INVALID_GLOBAL_WORK_SIZE";
        case CL_INVALID_PROPERTY:
            return "CL_INVALID_PROPERTY";
        default:
            return "UNKNOWN ERROR";
    }
}

void cl_print_prog_build_info_log(cl_program prog, cl_device_id did)
{
    /* Query info buffer length */
    size_t blen = 0;
    clGetProgramBuildInfo(prog, did, CL_PROGRAM_BUILD_LOG, 0, 0, &blen);
    /* Allocate needed buffer and query contents */
    char* buf = calloc(blen, 1);
    clGetProgramBuildInfo(prog, did, CL_PROGRAM_BUILD_LOG, blen, buf, 0);
    /* Print buffer and free it */
    printf("Build Log:\n%s\n", buf);
    free(buf);
}

int cl_choose_platform_and_device(cl_platform_id* plat_id, cl_device_id* dev_id)
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

void _cl_check_error(cl_int err, const char* operation, char* filename, int line)
{
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error during operation '%s', ", operation);
        fprintf(stderr, "in '%s' on line %d\n", filename, line);
        fprintf(stderr, "Error code was \"%s\" (%d)\n", cl_err_code(err), err);
        exit(EXIT_FAILURE);
    }
}
