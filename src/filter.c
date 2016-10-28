#include <emproc/filter.h>
#include <emproc/envmap.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

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
    struct envmap em_out = em_in;
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
                float tot[3] = {0.0f, 0.0f, 0.0f};
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
                dst[0] = tot[0] / total_weight;
                dst[1] = tot[1] / total_weight;
                dst[2] = tot[2] / total_weight;
                envmap_setpixel(&em_out, xdst, ydst, face, dst);
            }
        }
    }
}
