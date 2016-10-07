#include <emproc/filter.h>
#include <emproc/envmap.h>
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

void irradiance_filter(int width, int height, int channels, unsigned char* src_base, unsigned char* dst_base)
{
    /* Fill in input envmap struct */
    struct envmap em_in;
    em_in.channels = channels;
    em_in.data = src_base;
    em_in.width = width;
    em_in.height = height;
    em_in.type = EM_TYPE_HCROSS;
    /* Fill in output envmap struct */
    struct envmap em_out;
    memcpy(&em_out, &em_in, sizeof(struct envmap));
    em_out.data = dst_base;

    int src_face_size = width / 4;
    int dst_face_size = src_face_size;
    float texel_size = 1.0f / (float)src_face_size;
    for (int face = 0; face < 6; ++face) {
        /* Iterate through dest pixels */
        for (int ydst = 0; ydst < dst_face_size; ++ydst) {
            /* Map value to [-1, 1], offset by 0.5 to point to texel center */
            float v = 2.0f * ((ydst + 0.5f) * texel_size) - 1.0f;
            for (int xdst = 0; xdst < dst_face_size; ++xdst) {
                /* Current destination pixel location */
                /* Map value to [-1, 1], offset by 0.5 to point to texel center */
                float u = 2.0f * ((xdst + 0.5f) * texel_size) - 1.0f;
                //float solid_angle = texel_solid_angle(u, v, 1.0f / dst_face_size);

                /* Get sampling vector for the above u, v set */
                float dir[3];
                envmap_texel_coord_to_vec_warp(dir, em_in.type, u, v, face, envmap_warp_fixup_factor(dst_face_size));
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
                    envmap_sample(col, &em_in, cdir);
                    tot[0] += col[0];
                    tot[1] += col[1];
                    tot[2] += col[2];
                    ++total_weight;
                }
                /* Divide by the total of samples */
                float dst[3];
                dst[0] = 255.0f * tot[0] / total_weight;
                dst[1] = 255.0f * tot[1] / total_weight;
                dst[2] = 255.0f * tot[2] / total_weight;
                envmap_setpixel(&em_out, xdst, ydst, face, dst);
            }
        }
    }
}
