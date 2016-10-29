#include "envmap.c"

__kernel void fooo(__global unsigned char* out,
                   __global unsigned char* in,
                   const unsigned int face_size,
                   const unsigned int face_idx)
{
    /* Current processing pixel */
    unsigned int xdst = get_global_id(1);
    unsigned int ydst = get_global_id(0);

    int channels = 3;
    /* Fill in input envmap struct */
    struct envmap em_in;
    em_in.channels = channels;
    em_in.data = in;
    em_in.width = face_size * 4;
    em_in.height = face_size * 3;
    em_in.type = EM_TYPE_HCROSS;
    /* Fill in output envmap struct */
    struct envmap em_out = em_in;
    em_out.data = out;

    float texel_size = 1.0f / (float)face_size;
    /* Map value to [-1, 1], offset by 0.5 to point to texel center */
    float v = 2.0f * ((ydst + 0.5f) * texel_size) - 1.0f;
    float u = 2.0f * ((xdst + 0.5f) * texel_size) - 1.0f;
    /* Get sampling vector for the above u, v set */
    float dir[3];
    envmap_texel_coord_to_vec_warp(dir, em_in.type, u, v, face_idx, envmap_warp_fixup_factor(face_size));
    /* */
    float theta, phi;
    vec_to_sc(&theta, &phi, dir);

    /* Full convolution */
    float total_weight = 0;
    float tot[3] = {0.0f, 0.0f, 0.0f};
    float istep = pi_half / 16.0f;
    float bound = pi_half;
    for (float k = -bound; k <= bound; k += istep) {
        for (float l = -bound; l <= bound; l += istep) {
            /* Current angle values */
            float ctheta = theta + k;
            float cphi = phi + l;
            /* Get 3D vector from angles */
            float cdir[3];
            sc_to_vec(cdir, ctheta, cphi);
            /* Get dot product between normal and current direction */
            float c = fabsf(vec3_dot(dir, cdir));
            /* Sample for color in the given direction and add it to the sum */
            float col[3];
            envmap_sample(col, &em_in, cdir);
            tot[0] += c * col[0];
            tot[1] += c * col[1];
            tot[2] += c * col[2];
            total_weight += c;
        }
    }
    /* Divide by the total of samples */
    float dst[3];
    dst[0] = tot[0] / total_weight;
    dst[1] = tot[1] / total_weight;
    dst[2] = tot[2] / total_weight;
    envmap_setpixel(&em_out, xdst, ydst, face_idx, dst);
}
