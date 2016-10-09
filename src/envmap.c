#include <emproc/envmap.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

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

enum envmap_type envmap_detect_type(int width, int height)
{
    if (image_is_hcross(width, height))
        return EM_TYPE_HCROSS;
    else if (image_is_vcross(width, height))
        return EM_TYPE_VCROSS;
    else if (image_is_latlong(width, height))
        return EM_TYPE_LATLONG;
    return EM_TYPE_UNKNOWN;
}

/*======================================================================
 * Sampling helpers
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
uint8_t cube_face_neighbours[6][4][2] =
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

static float vec3_dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

/*======================================================================
 * Cross sampling
 *======================================================================*/
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
static const float hcross_face_uv_vectors[6][3][3] =
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

/* Offset in faces from top left of a cross image */
static int hcross_face_map[6][2] = {
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

/* u and v are in [0.0 .. 1.0] range. */
static void hcross_vec_to_texel_coord(float* u, float* v, uint8_t* face_idx, const float* vec)
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
    *u = (vec3_dot(hcross_face_uv_vectors[*face_idx][0], face_vec) + 1.0f) * 0.5f;
    *v = (vec3_dot(hcross_face_uv_vectors[*face_idx][1], face_vec) + 1.0f) * 0.5f;
}

/* u and v should be center adressing and in [-1.0+invSize..1.0-invSize] range */
static void hcross_texel_coord_to_vec(float* out3f, float u, float v, uint8_t face_id)
{
    /* out = u * face_uv[0] + v * face_uv[1] + face_uv[2]. */
    out3f[0] = hcross_face_uv_vectors[face_id][0][0] * u + hcross_face_uv_vectors[face_id][1][0] * v + hcross_face_uv_vectors[face_id][2][0];
    out3f[1] = hcross_face_uv_vectors[face_id][0][1] * u + hcross_face_uv_vectors[face_id][1][1] * v + hcross_face_uv_vectors[face_id][2][1];
    out3f[2] = hcross_face_uv_vectors[face_id][0][2] * u + hcross_face_uv_vectors[face_id][1][2] * v + hcross_face_uv_vectors[face_id][2][2];

    /* Normalize. */
    const float inv_len = 1.0f / sqrtf(out3f[0] * out3f[0] + out3f[1] * out3f[1] + out3f[2] * out3f[2]);
    out3f[0] *= inv_len;
    out3f[1] *= inv_len;
    out3f[2] *= inv_len;
}

static void sample_hcross_map(float col[3], uint8_t* base, int face_size, int channels, float vec[3])
{
    float u, v;
    uint8_t face_idx;
    hcross_vec_to_texel_coord(&u, &v, &face_idx, vec);

    int x = u * (face_size - 1);
    int y = v * (face_size - 1);
    size_t stride = 4 * face_size;
    size_t offset = (hcross_face_offset(face_idx, face_size) + y * stride + x) * channels;
    uint8_t* data = base + offset;
    col[0] = data[0] / 255.0f;
    col[1] = data[1] / 255.0f;
    col[2] = data[2] / 255.0f;
}

static void hcross_setpixel(uint8_t* base, uint32_t face_size, uint8_t channels, uint32_t x, uint32_t y, enum cubemap_face face, float val[3])
{
    size_t stride = 4 * face_size;
    uint8_t* dst = base + (hcross_face_offset(face, face_size) + y * stride + x) * channels;
    dst[0] = val[0];
    dst[1] = val[1];
    dst[2] = val[2];
}

/*======================================================================
 * Public interface
 *======================================================================*/
/* u and v are in [0.0 .. 1.0] range. */
void envmap_vec_to_texel_coord(float* u, float* v, uint8_t* face_idx, enum envmap_type em_type, const float* vec)
{
    switch(em_type) {
        case EM_TYPE_HCROSS:
            hcross_vec_to_texel_coord(u, v, face_idx, vec);
            break;
        default:
            assert(0 && "Not implemented");
            break;
    }
}

/* u and v should be center adressing and in [-1.0+invSize..1.0-invSize] range */
void envmap_texel_coord_to_vec(float* out3f, enum envmap_type em_type, float u, float v, uint8_t face_id)
{
    switch(em_type) {
        case EM_TYPE_HCROSS:
            hcross_texel_coord_to_vec(out3f, u, v, face_id);
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
void envmap_texel_coord_to_vec_warp(float* out3f, enum envmap_type em_type, float u, float v, uint8_t face_id, float warp_fixup)
{
    u = (warp_fixup * u*u*u) + u;
    v = (warp_fixup * v*v*v) + v;
    envmap_texel_coord_to_vec(out3f, em_type, u, v, face_id);
}

void envmap_sample(float col[3], struct envmap* em, float vec[3])
{
    switch(em->type) {
        case EM_TYPE_HCROSS:
            sample_hcross_map(col, em->data, em->width / 4.0f, em->channels, vec);
            break;
        default:
            assert(0 && "Not implemented");
            break;
    }
}

void envmap_setpixel(struct envmap* em, uint32_t x, uint32_t y, enum cubemap_face face, float val[3])
{
    switch(em->type) {
        case EM_TYPE_HCROSS:
            hcross_setpixel(em->data, em->width / 4.0f, em->channels, x, y, face, val);
            break;
        default:
            assert(0 && "Not implemented");
            break;
    }
}