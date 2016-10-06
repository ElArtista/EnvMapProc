#include <emproc/filter.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* Piz */
#define pi          3.14159265358979323846f
#define two_pi      6.28318530717958647692f
#define pi_half     1.57079632679489661923f
#define inv_pi      0.31830988618379067153f
#define inv_pi_half 0.15915494309189533576f

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
static const float face_uv_vectors[6][3][3] =
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

/* u and v should be center adressing and in [-1.0+invSize..1.0-invSize] range */
static void texel_coord_to_vec(float* out3f, float u, float v, uint8_t face_id)
{
    /* out = u * face_uv[0] + v * face_uv[1] + face_uv[2]. */
    out3f[0] = face_uv_vectors[face_id][0][0] * u + face_uv_vectors[face_id][1][0] * v + face_uv_vectors[face_id][2][0];
    out3f[1] = face_uv_vectors[face_id][0][1] * u + face_uv_vectors[face_id][1][1] * v + face_uv_vectors[face_id][2][1];
    out3f[2] = face_uv_vectors[face_id][0][2] * u + face_uv_vectors[face_id][1][2] * v + face_uv_vectors[face_id][2][2];

    /* Normalize. */
    const float inv_len = 1.0f / sqrtf(out3f[0] * out3f[0] + out3f[1] * out3f[1] + out3f[2] * out3f[2]);
    out3f[0] *= inv_len;
    out3f[1] *= inv_len;
    out3f[2] *= inv_len;
}

/* Notice: faceSize should not be equal to one! */
static float warp_fixup_factor(float face_size)
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
static void texel_coord_to_vec_warp(float* out3f, float u, float v, uint8_t face_id, float warp_fixup)
{
    u = (warp_fixup * u*u*u) + u;
    v = (warp_fixup * v*v*v) + v;
    texel_coord_to_vec(out3f, u, v, face_id);
}

/* u and v are in [0.0 .. 1.0] range. */
static void vec_to_texel_coord(float* u, float* v, uint8_t* face_idx, const float* vec)
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
    *u = (vec3_dot(face_uv_vectors[*face_idx][0], face_vec) + 1.0f) * 0.5f;
    *v = (vec3_dot(face_uv_vectors[*face_idx][1], face_vec) + 1.0f) * 0.5f;
}

/* Theta is horizontal, and phi is vertical angles */
static void sc_to_vec(float vec[3], float theta, float phi)
{
    vec[0] = sinf(theta) * sinf(phi);
    vec[1] = cosf(phi);
    vec[2] = cosf(theta) * sinf(phi);
}

static void vec_to_sc(float* theta, float* phi, const float vec[3])
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
static float texel_solid_angle(float u, float v, float inv_face_size)
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
static size_t cross_face_offset(int face, size_t face_size)
{
    size_t stride = 4 * face_size;
    return hcross_face_map[face][1] * face_size * stride + hcross_face_map[face][0] * face_size;
}

static void sample_hcross_map(float col[3], uint8_t* base, int face_size, int channels, float vec[3])
{
    float u, v;
    uint8_t face_idx;
    vec_to_texel_coord(&u, &v, &face_idx, vec);

    int x = u * (face_size - 1);
    int y = v * (face_size - 1);
    size_t stride = 4 * face_size;
    size_t offset = (cross_face_offset(face_idx, face_size) + y * stride + x) * channels;
    uint8_t* data = base + offset;
    col[0] = data[0] / 255.0f;
    col[1] = data[1] / 255.0f;
    col[2] = data[2] / 255.0f;
}

void irradiance_filter(int width, int height, int channels, unsigned char* src_base, unsigned char* dst_base)
{
    (void) height;
    int src_face_size = width / 4;
    int dst_face_size = src_face_size;
    float texel_size = 1.0f / (float)src_face_size;
    for (int face = 0; face < 6; ++face) {
        uint8_t* dst_face_data = dst_base + cross_face_offset(face, dst_face_size) * channels;
        /* Iterate through dest pixels */
        for (int ydst = 0; ydst < dst_face_size; ++ydst) {
            uint8_t* dst_face_row = dst_face_data + ydst * width * channels;
            /* Map value to [-1, 1], offset by 0.5 to point to texel center */
            float v = 2.0f * ((ydst + 0.5f) * texel_size) - 1.0f;
            for (int xdst = 0; xdst < dst_face_size; ++xdst) {
                /* Current destination pixel location */
                uint8_t* dst = dst_face_row + xdst * channels;
                /* Map value to [-1, 1], offset by 0.5 to point to texel center */
                float u = 2.0f * ((xdst + 0.5f) * texel_size) - 1.0f;
                //float solid_angle = texel_solid_angle(u, v, 1.0f / dst_face_size);

                /* Get sampling vector for the above u, v set */
                float dir[3];
                texel_coord_to_vec_warp(dir, u, v, face, warp_fixup_factor(dst_face_size));
                /* */
                float theta, phi;
                vec_to_sc(&theta, &phi, dir);

                /* Full convolution */
                float total_weight = 0;
                float tot[3];
                memset(tot, 0, sizeof(float) * 3);
                float step = pi_half / 8.0f;
                float bound = pi_half / 2.0f;
                for (float k = -bound; k < bound; k += step) {
                    /* Current angle values */
                    float ctheta = theta + k;
                    float cphi = phi + 0;
                    /* */
                    float cdir[3];
                    sc_to_vec(cdir, ctheta, cphi);
                    /* Sample for color in the given direction and add it to the sum */
                    float col[3];
                    sample_hcross_map(col, src_base, src_face_size, channels, cdir);
                    tot[0] += col[0];
                    tot[1] += col[1];
                    tot[2] += col[2];
                    ++total_weight;
                }
                /* Divide by the total of samples */
                dst[0] = 255.0f * tot[0] / total_weight;
                dst[1] = 255.0f * tot[1] / total_weight;
                dst[2] = 255.0f * tot[2] / total_weight;
            }
        }
    }
}
