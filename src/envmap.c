#include <emproc/envmap.h>
#ifndef OPENCL_MODE
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#define GLOBAL_CONSTANT const
#else
#define fabsf fabs
#define fmaxf fmax
#define sqrtf sqrt
#define cosf cos
#define sinf sin
#define acosf acos
#define atan2f atan2
#define assert(x)
#define GLOBAL_CONSTANT __constant
#endif

/*======================================================================
 * Environment map type detection
 *======================================================================*/
/* Floating equals with epsilon */
static int fequalse(float a, float b, float epsilon)
{
    return fabs(a - b) < epsilon;
}

static int image_is_hcross(int width, int height)
{
    float aspect = (float)width / (float)height;
    return fequalse(aspect, 4.0f / 3.0f, 10e-3);
}

static int image_is_vcross(int width, int height)
{
    float aspect = (float)width / (float)height;
    return fequalse(aspect, 3.0f / 4.0f, 10e-3);
}

static int image_is_latlong(int width, int height)
{
    float aspect = (float)width / (float)height;
    return fequalse(aspect, 2.0f, 10e-4);
}

static int image_is_vstrip(int width, int height)
{
    float aspect = (float)width / (float)height;
    return fequalse(aspect, 1.0f / 6.0f, 10e-4);
}

enum envmap_type envmap_detect_type(int width, int height)
{
    if (image_is_hcross(width, height))
        return EM_TYPE_HCROSS;
    else if (image_is_vcross(width, height))
        return EM_TYPE_VCROSS;
    else if (image_is_latlong(width, height))
        return EM_TYPE_LATLONG;
    else if (image_is_vstrip(width, height))
        return EM_TYPE_VSTRIP;
    return EM_TYPE_UNKNOWN;
}

/*======================================================================
 * Sampling helpers
 *======================================================================*/
/* 3D dot product */
#ifndef OPENCL_MODE
static float vec3_dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
#else
#define vec3_dot(x, y) dot(vload3(0, x), vload3(0, y))
#endif

/*======================================================================
 * Cubemap helpers
 *======================================================================*/
/*
 *     --> U    _____
 *    |        |     |
 *    v        | +Y  |
 *    V   _____|_____|_____ _____
 *       |     |     |     |     |
 *       | -X  | +Z  | +X  | -Z  |
 *       |_____|_____|_____|_____|
 *             |     |
 *             | -Y  |
 *             |_____|
 *
 *  Neighbour faces in order: left, right, top, bottom.
 *  FaceEdge is the edge that belongs to the neighbour face.
 */
GLOBAL_CONSTANT uint8_t cm_face_neighbours[6][4][2] =
{
    { /* POS_X */
        { CM_FACE_POS_Z, CM_EDGE_RIGHT },
        { CM_FACE_NEG_Z, CM_EDGE_LEFT  },
        { CM_FACE_POS_Y, CM_EDGE_RIGHT },
        { CM_FACE_NEG_Y, CM_EDGE_RIGHT },
    },
    { /* NEG_X */
        { CM_FACE_NEG_Z, CM_EDGE_RIGHT },
        { CM_FACE_POS_Z, CM_EDGE_LEFT  },
        { CM_FACE_POS_Y, CM_EDGE_LEFT  },
        { CM_FACE_NEG_Y, CM_EDGE_LEFT  },
    },
    { /* POS_Y */
        { CM_FACE_NEG_X, CM_EDGE_TOP },
        { CM_FACE_POS_X, CM_EDGE_TOP },
        { CM_FACE_NEG_Z, CM_EDGE_TOP },
        { CM_FACE_POS_Z, CM_EDGE_TOP },
    },
    { /* NEG_Y */
        { CM_FACE_NEG_X, CM_EDGE_BOTTOM },
        { CM_FACE_POS_X, CM_EDGE_BOTTOM },
        { CM_FACE_POS_Z, CM_EDGE_BOTTOM },
        { CM_FACE_NEG_Z, CM_EDGE_BOTTOM },
    },
    { /* POS_Z */
        { CM_FACE_NEG_X, CM_EDGE_RIGHT  },
        { CM_FACE_POS_X, CM_EDGE_LEFT   },
        { CM_FACE_POS_Y, CM_EDGE_BOTTOM },
        { CM_FACE_NEG_Y, CM_EDGE_TOP    },
    },
    { /* NEG_Z */
        { CM_FACE_POS_X, CM_EDGE_RIGHT  },
        { CM_FACE_NEG_X, CM_EDGE_LEFT   },
        { CM_FACE_POS_Y, CM_EDGE_TOP    },
        { CM_FACE_NEG_Y, CM_EDGE_BOTTOM },
    }
};

/*
 *              +----------+
 *              | +---->+x |
 *              | |        |
 *              | |  +y    |
 *              |+z      2 |
 *   +----------+----------+----------+----------+
 *   | +---->+z | +---->+x | +---->-z | +---->-x |
 *   | |        | |        | |        | |        |
 *   | |  -x    | |  +z    | |  +x    | |  -z    |
 *   |-y      1 |-y      4 |-y      0 |-y      5 |
 *   +----------+----------+----------+----------+
 *              | +---->+x |
 *              | |        |
 *              | |  -y    |
 *              |-z      3 |
 *              +----------+
 */
static GLOBAL_CONSTANT float cm_face_uv_vectors[6][3][3] =
{
    { /* +x face */
        {  0.0f,  0.0f, -1.0f }, /* u -> -z */
        {  0.0f, -1.0f,  0.0f }, /* v -> -y */
        {  1.0f,  0.0f,  0.0f }, /* +x face */
    },
    { /* -x face */
        {  0.0f,  0.0f,  1.0f }, /* u -> +z */
        {  0.0f, -1.0f,  0.0f }, /* v -> -y */
        { -1.0f,  0.0f,  0.0f }, /* -x face */
    },
    { /* +y face */
        {  1.0f,  0.0f,  0.0f }, /* u -> +x */
        {  0.0f,  0.0f,  1.0f }, /* v -> +z */
        {  0.0f,  1.0f,  0.0f }, /* +y face */
    },
    { /* -y face */
        {  1.0f,  0.0f,  0.0f }, /* u -> +x */
        {  0.0f,  0.0f, -1.0f }, /* v -> -z */
        {  0.0f, -1.0f,  0.0f }, /* -y face */
    },
    { /* +z face */
        {  1.0f,  0.0f,  0.0f }, /* u -> +x */
        {  0.0f, -1.0f,  0.0f }, /* v -> -y */
        {  0.0f,  0.0f,  1.0f }, /* +z face */
    },
    { /* -z face */
        { -1.0f,  0.0f,  0.0f }, /* u -> -x */
        {  0.0f, -1.0f,  0.0f }, /* v -> -y */
        {  0.0f,  0.0f, -1.0f }, /* -z face */
    }
};

/* u and v are in [0.0 .. 1.0] range. */
static void cm_vec_to_texel_coord(PRIVATE float* u, PRIVATE float* v, PRIVATE uint8_t* face_idx, PRIVATE const float* vec)
{
    const float abs_vec[3] = {
        fabsf(vec[0]),
        fabsf(vec[1]),
        fabsf(vec[2]),
    };
    const float max = fmaxf(fmaxf(abs_vec[0], abs_vec[1]), abs_vec[2]);

    /* Get face id (max component == face vector). */
    if (max == abs_vec[0]) {
        *face_idx = (vec[0] >= 0.0f) ? (uint8_t)CM_FACE_POS_X : (uint8_t)CM_FACE_NEG_X;
    } else if (max == abs_vec[1]) {
        *face_idx = (vec[1] >= 0.0f) ? (uint8_t)CM_FACE_POS_Y : (uint8_t)CM_FACE_NEG_Y;
    } else { /*if (max == abs_vec[2])*/
        *face_idx = (vec[2] >= 0.0f) ? (uint8_t)CM_FACE_POS_Z : (uint8_t)CM_FACE_NEG_Z;
    }

    /* Divide by max component. */
    float face_vec[3];
    face_vec[0] = vec[0] * (1.0f / max);
    face_vec[1] = vec[1] * (1.0f / max);
    face_vec[2] = vec[2] * (1.0f / max);

    /* Project other two components to face uv basis. */
    *u = (vec3_dot(cm_face_uv_vectors[*face_idx][0], face_vec) + 1.0f) * 0.5f;
    *v = (vec3_dot(cm_face_uv_vectors[*face_idx][1], face_vec) + 1.0f) * 0.5f;
}

/* u and v should be center adressing and in [-1.0+invSize..1.0-invSize] range */
static void cm_texel_coord_to_vec(PRIVATE float* out3f, float u, float v, uint8_t face_id)
{
    /* out = u * face_uv[0] + v * face_uv[1] + face_uv[2]. */
    out3f[0] = cm_face_uv_vectors[face_id][0][0] * u + cm_face_uv_vectors[face_id][1][0] * v + cm_face_uv_vectors[face_id][2][0];
    out3f[1] = cm_face_uv_vectors[face_id][0][1] * u + cm_face_uv_vectors[face_id][1][1] * v + cm_face_uv_vectors[face_id][2][1];
    out3f[2] = cm_face_uv_vectors[face_id][0][2] * u + cm_face_uv_vectors[face_id][1][2] * v + cm_face_uv_vectors[face_id][2][2];

    /* Normalize. */
    const float inv_len = 1.0f / sqrtf(out3f[0] * out3f[0] + out3f[1] * out3f[1] + out3f[2] * out3f[2]);
    out3f[0] *= inv_len;
    out3f[1] *= inv_len;
    out3f[2] *= inv_len;
}

/*======================================================================
 * Cross sampling
 *======================================================================*/
/* Offset in faces from top left of a cross image */
static GLOBAL_CONSTANT int hcross_face_map[6][2] = {
    {2, 1}, /* Pos X */
    {0, 1}, /* Neg X */
    {1, 0}, /* Pos Y */
    {1, 2}, /* Neg Y */
    {1, 1}, /* Pos Z */
    {3, 1}  /* Neg Z */
};

/* In pixels */
static size_t hcross_face_offset(int face, size_t face_size)
{
    size_t stride = 4 * face_size;
    return hcross_face_map[face][1] * face_size * stride + hcross_face_map[face][0] * face_size;
}

static GLOBAL uint8_t* hcross_pixel_ptr(GLOBAL uint8_t* base, uint32_t x, uint32_t y, int face_size, enum cubemap_face face, int channels)
{
    size_t stride = 4 * face_size;
    size_t offset = (hcross_face_offset(face, face_size) + y * stride + x) * channels;
    GLOBAL uint8_t* data = base + offset;
    return data;
}

static void sample_hcross_map(float col[3], GLOBAL uint8_t* base, int face_size, int channels, PRIVATE const float vec[3])
{
    float u, v;
    uint8_t face_idx;
    cm_vec_to_texel_coord(&u, &v, &face_idx, vec);

    int x = u * (face_size - 1);
    int y = v * (face_size - 1);
    size_t stride = 4 * face_size;
    size_t offset = (hcross_face_offset(face_idx, face_size) + y * stride + x) * channels;
    GLOBAL uint8_t* data = base + offset;
    col[0] = data[0] / 255.0f;
    col[1] = data[1] / 255.0f;
    col[2] = data[2] / 255.0f;
}

static void hcross_setpixel(GLOBAL uint8_t* base, uint32_t face_size, uint8_t channels, uint32_t x, uint32_t y, enum cubemap_face face, float val[3])
{
    size_t stride = 4 * face_size;
    GLOBAL uint8_t* dst = base + (hcross_face_offset(face, face_size) + y * stride + x) * channels;
    dst[0] = val[0] * 255.0f;
    dst[1] = val[1] * 255.0f;
    dst[2] = val[2] * 255.0f;
}

/*======================================================================
 * Strip sampling
 *======================================================================*/
/* In pixels */
static size_t vstrip_face_offset(int face, size_t face_size)
{
    return face_size * face_size * face;
}

static GLOBAL uint8_t* vstrip_pixel_ptr(GLOBAL uint8_t* base, uint32_t x, uint32_t y, int face_size, enum cubemap_face face, int channels)
{
    size_t offset = (vstrip_face_offset(face, face_size) + y * face_size + x) * channels;
    GLOBAL uint8_t* data = base + offset;
    return data;
}

static void sample_vstrip_map(float col[3], GLOBAL uint8_t* base, int face_size, int channels, PRIVATE const float vec[3])
{
    float u, v;
    uint8_t face_idx;
    cm_vec_to_texel_coord(&u, &v, &face_idx, vec);

    int x = u * (face_size - 1);
    int y = v * (face_size - 1);
    size_t offset = (vstrip_face_offset(face_idx, face_size) + y * face_size + x) * channels;
    GLOBAL uint8_t* data = base + offset;
    col[0] = data[0] / 255.0f;
    col[1] = data[1] / 255.0f;
    col[2] = data[2] / 255.0f;
}

static void vstrip_setpixel(GLOBAL uint8_t* base, uint32_t face_size, uint8_t channels, uint32_t x, uint32_t y, enum cubemap_face face, float val[3])
{
    GLOBAL uint8_t* dst = base + (vstrip_face_offset(face, face_size) + y * face_size + x) * channels;
    dst[0] = val[0] * 255.0f;
    dst[1] = val[1] * 255.0f;
    dst[2] = val[2] * 255.0f;
}

/*======================================================================
 * Public interface
 *======================================================================*/
/* u and v are in [0.0 .. 1.0] range. */
void envmap_vec_to_texel_coord(PRIVATE float* u, PRIVATE float* v, PRIVATE uint8_t* face_idx, enum envmap_type em_type, PRIVATE const float* vec)
{
    switch(em_type) {
        case EM_TYPE_HCROSS:
        case EM_TYPE_VCROSS:
        case EM_TYPE_VSTRIP:
            cm_vec_to_texel_coord(u, v, face_idx, vec);
            break;
        default:
            assert(0 && "Not implemented");
            break;
    }
}

/* u and v should be center adressing and in [-1.0+invSize..1.0-invSize] range */
void envmap_texel_coord_to_vec(PRIVATE float* out3f, enum envmap_type em_type, float u, float v, uint8_t face_id)
{
    switch(em_type) {
        case EM_TYPE_HCROSS:
        case EM_TYPE_VCROSS:
        case EM_TYPE_VSTRIP:
            cm_texel_coord_to_vec(out3f, u, v, face_id);
            break;
        default:
            assert(0 && "Not implemented");
            break;
    }
}

/* Notice: faceSize should not be equal to one! */
float envmap_warp_fixup_factor(float face_size)
{
    /* Edge fixup. */
    /* Based on Nvtt : http://code.google.com/p/nvidia-texture-tools/source/browse/trunk/src/nvtt/cube_surface.cpp */
    if (face_size == 1.0f) {
        return 1.0f;
    }

    const float fs = face_size;
    const float fsmo = fs - 1.0f;
    return (fs * fs) / (fsmo * fsmo * fsmo);
}

/* u and v should be center adressing and in [-1.0+invSize..1.0-invSize] range. */
void envmap_texel_coord_to_vec_warp(PRIVATE float* out3f, enum envmap_type em_type, float u, float v, uint8_t face_id, float warp_fixup)
{
    u = (warp_fixup * u*u*u) + u;
    v = (warp_fixup * v*v*v) + v;
    envmap_texel_coord_to_vec(out3f, em_type, u, v, face_id);
}

void envmap_sample(PRIVATE float col[3], struct envmap* em, PRIVATE float vec[3])
{
    switch(em->type) {
        case EM_TYPE_HCROSS:
            sample_hcross_map(col, em->data, em->width / 4.0f, em->channels, vec);
            break;
        case EM_TYPE_VSTRIP:
            sample_vstrip_map(col, em->data, em->width / 4.0f, em->channels, vec);
            break;
        default:
            assert(0 && "Not implemented");
            break;
    }
}

void envmap_setpixel(PRIVATE struct envmap* em, uint32_t x, uint32_t y, enum cubemap_face face, PRIVATE float val[3])
{
    switch(em->type) {
        case EM_TYPE_HCROSS:
            hcross_setpixel(em->data, em->width / 4.0f, em->channels, x, y, face, val);
            break;
        case EM_TYPE_VSTRIP:
            vstrip_setpixel(em->data, em->width / 4.0f, em->channels, x, y, face, val);
            break;
        default:
            assert(0 && "Not implemented");
            break;
    }
}

GLOBAL uint8_t* envmap_pixel_ptr(PRIVATE struct envmap* em, uint32_t x, uint32_t y, enum cubemap_face face)
{
    switch(em->type) {
        case EM_TYPE_HCROSS:
            return hcross_pixel_ptr(em->data, x, y, em->width / 4.0f, face, em->channels);
        case EM_TYPE_VSTRIP:
            return vstrip_pixel_ptr(em->data, x, y, em->width / 4.0f, face, em->channels);
        default:
            assert(0 && "Not implemented");
            break;
    }
}

/* Theta is horizontal, and phi is vertical angles */
void sc_to_vec(PRIVATE float vec[3], float theta, float phi)
{
    vec[0] = sinf(theta) * sinf(phi);
    vec[1] = cosf(phi);
    vec[2] = cosf(theta) * sinf(phi);
}

void vec_to_sc(PRIVATE float* theta, PRIVATE float* phi, PRIVATE const float vec[3])
{
    *theta = atan2f(vec[0], vec[2]);
    *phi = acosf(vec[1]);
}

/* http://www.mpia-hd.mpg.de/~mathar/public/mathar20051002.pdf */
/* http://www.rorydriscoll.com/2012/01/15/cubemap-texel-solid-angle/ */
static float area_element(float x, float y)
{
    return atan2f(x * y, sqrtf(x * x + y * y + 1.0f));
}

/* u and v should be center adressing and in [-1.0+invSize..1.0-invSize] range. */
float texel_solid_angle(float u, float v, float inv_face_size)
{
    /* Specify texel area. */
    const float x0 = u - inv_face_size;
    const float x1 = u + inv_face_size;
    const float y0 = v - inv_face_size;
    const float y1 = v + inv_face_size;

    /* Compute solid angle of texel area. */
    const float solid_angle = area_element(x1, y1)
                            - area_element(x0, y1)
                            - area_element(x1, y0)
                            + area_element(x0, y0);
    return solid_angle;
}
