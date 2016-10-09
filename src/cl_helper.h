/*********************************************************************************************************************/
/*                                                  /===-_---~~~~~~~~~------____                                     */
/*                                                 |===-~___                _,-'                                     */
/*                  -==\\                         `//~\\   ~~~~`---.___.-~~                                          */
/*              ______-==|                         | |  \\           _-~`                                            */
/*        __--~~~  ,-/-==\\                        | |   `\        ,'                                                */
/*     _-~       /'    |  \\                      / /      \      /                                                  */
/*   .'        /       |   \\                   /' /        \   /'                                                   */
/*  /  ____  /         |    \`\.__/-~~ ~ \ _ _/'  /          \/'                                                     */
/* /-'~    ~~~~~---__  |     ~-/~         ( )   /'        _--~`                                                      */
/*                   \_|      /        _)   ;  ),   __--~~                                                           */
/*                     '~~--_/      _-~/-  / \   '-~ \                                                               */
/*                    {\__--_/}    / \\_>- )<__\      \                                                              */
/*                    /'   (_/  _-~  | |__>--<__|      |                                                             */
/*                   |0  0 _/) )-~     | |__>--<__|     |                                                            */
/*                   / /~ ,_/       / /__>---<__/      |                                                             */
/*                  o o _//        /-~_>---<__-~      /                                                              */
/*                  (^(~          /~_>---<__-      _-~                                                               */
/*                 ,/|           /__>--<__/     _-~                                                                  */
/*              ,//('(          |__>--<__|     /                  .----_                                             */
/*             ( ( '))          |__>--<__|    |                 /' _---_~\                                           */
/*          `-)) )) (           |__>--<__|    |               /'  /     ~\`\                                         */
/*         ,/,'//( (             \__>--<__\    \            /'  //        ||                                         */
/*       ,( ( ((, ))              ~-__>--<_~-_  ~--____---~' _/'/        /'                                          */
/*     `~/  )` ) ,/|                 ~-_~>--<_/-__       __-~ _/                                                     */
/*   ._-~//( )/ )) `                    ~~-'_/_/ /~~~~~~~__--~                                                       */
/*    ;'( ')/ ,)(                              ~~~~~~~~~~                                                            */
/*   ' ') '( (/                                                                                                      */
/*     '   '  `                                                                                                      */
/*********************************************************************************************************************/
#ifndef _CL_HELPER_H_
#define _CL_HELPER_H_

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#ifdef __APPLE__
    #include <OpenCL/opencl.h>
#else
    #include <CL/cl.h>
#endif

/* Convinience macro for embedding OpenCL code in source */
#define CLSRC(src) "" #src
/* Check macro */
#define cl_check_error(E, S) _cl_check_error(E, S, __FILE__, __LINE__)

/* Common vendor enumeration */
enum cl_vendor {
    CL_VENDOR_INTEL   = 1 << 1,
    CL_VENDOR_AMD     = 1 << 2,
    CL_VENDOR_NVIDIA  = 1 << 3,
    CL_VENDOR_OTHER   = 1 << 4,
    CL_VENDOR_ANY_GPU = CL_VENDOR_AMD|CL_VENDOR_NVIDIA,
    CL_VENDOR_ANY_CPU = CL_VENDOR_AMD|CL_VENDOR_INTEL
};

/* Translates given OpenCL error code to its string equivalent */
const char* cl_err_code(cl_int err_in);
/* Prints given build info log for given program and device pair */
void cl_print_prog_build_info_log(cl_program prog, cl_device_id did);

/* Function used by cl_check_error macro, not meant to be used directly */
void _cl_check_error(cl_int err, const char* operation, char* filename, int line);

#endif /* ! _CL_HELPER_H_ */
