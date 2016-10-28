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
#ifndef _ENVMAP_H_
#define _ENVMAP_H_

#ifndef OPENCL_MODE
#include <stdint.h>
#define PRIVATE
#define GLOBAL
#else
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
#define PRIVATE __private
#define GLOBAL __global
#endif

enum envmap_type {
    EM_TYPE_HCROSS,
    EM_TYPE_VCROSS,
    EM_TYPE_LATLONG,
    EM_TYPE_UNKNOWN
};

struct envmap {
    /* Type */
    enum envmap_type type;
    /* Image dimensions */
    uint32_t width, height;
    /* Number of color channels */
    uint8_t channels;
    /* Raw image data */
    GLOBAL uint8_t* data;
};

enum cubemap_face
{
    CM_FACE_POS_X = 0,
    CM_FACE_NEG_X = 1,
    CM_FACE_POS_Y = 2,
    CM_FACE_NEG_Y = 3,
    CM_FACE_POS_Z = 4,
    CM_FACE_NEG_Z = 5,
};

enum cubemap_edge
{
    CM_EDGE_LEFT   = 0,
    CM_EDGE_RIGHT  = 1,
    CM_EDGE_TOP    = 2,
    CM_EDGE_BOTTOM = 3,
};

/*
 * Environment map utils
 */
/* Detect an envmap type from its dimensions */
enum envmap_type envmap_detect_type(int width, int height);
/* U and V are in [0.0 .. 1.0] range. */
void envmap_vec_to_texel_coord(PRIVATE float* u, PRIVATE float* v, PRIVATE uint8_t* face_idx, enum envmap_type em_type, PRIVATE const float* vec);
/* Notice: faceSize should not be equal to one! */
float envmap_warp_fixup_factor(float face_size);
/* U and V should be center adressing and in [-1.0+invSize..1.0-invSize] range */
void envmap_texel_coord_to_vec(PRIVATE float* out3f, enum envmap_type em_type, float u, float v, uint8_t face_id);
void envmap_texel_coord_to_vec_warp(PRIVATE float* out3f, enum envmap_type em_type, float u, float v, uint8_t face_id, float warp_fixup);
/* Sampling function */
void envmap_sample(PRIVATE float col[3], PRIVATE struct envmap* em, PRIVATE float vec[3]);
/* Set pixel in envmap */
void envmap_setpixel(PRIVATE struct envmap* em, uint32_t x, uint32_t y, enum cubemap_face face, PRIVATE float val[3]);

/*
 * Math utils
 */
/* Piz */
#define pi          3.14159265358979323846f
#define two_pi      6.28318530717958647692f
#define pi_half     1.57079632679489661923f
#define inv_pi      0.31830988618379067153f
#define inv_pi_half 0.15915494309189533576f
/* Spherical coordinates <-> 3D vector conversion */
void sc_to_vec(PRIVATE float vec[3], float theta, float phi);
void vec_to_sc(PRIVATE float* theta, PRIVATE float* phi, PRIVATE const float vec[3]);
/* U and V should be center adressing and in [-1.0+invSize..1.0-invSize] range. */
float texel_solid_angle(float u, float v, float inv_face_size);

#endif /* ! _ENVMAP_H_ */
