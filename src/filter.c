#include <emproc/filter.h>
#include <emproc/filter_util.h>
#include <emproc/envmap.h>
#include <emproc/sh.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

static float vec3_dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void irradiance_filter(int width, int height, int channels, unsigned char* src_base, unsigned char* dst_base, filter_progress_fn progress_fn, void* userdata)
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

    const size_t face_sz = width / 4;
    const float texel_size = 1.0f / (float)face_sz;
    const float warp = envmap_warp_fixup_factor(face_sz);
    for (int face = 0; face < 6; ++face) {
        /* Iterate through dest pixels */
        for (size_t ydst = 0; ydst < face_sz; ++ydst) {
            /* Map value to [-1, 1], offset by 0.5 to point to texel center */
            float v = 2.0f * ((ydst + 0.5f) * texel_size) - 1.0f;
            for (size_t xdst = 0; xdst < face_sz; ++xdst) {
                /* Current destination pixel location */
                /* Map value to [-1, 1], offset by 0.5 to point to texel center */
                float u = 2.0f * ((xdst + 0.5f) * texel_size) - 1.0f;
                //float solid_angle = texel_solid_angle(u, v, texel_size);

                /* Get sampling vector for the above u, v set */
                float dir[3];
                envmap_texel_coord_to_vec_warp(dir, em_in.type, u, v, face, warp);
                /* */
                float theta, phi;
                vec_to_sc(&theta, &phi, dir);

                /* Full convolution */
                float total_weight = 0;
                float tot[3] = {0.0f, 0.0f, 0.0f};
                float step = pi_half / 16.0f;
                float bound = pi_half;
                for (float k = -bound; k <= bound; k += step) {
                    for (float l = -bound; l <= bound; l += step) {
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
                envmap_setpixel(&em_out, xdst, ydst, face, dst);

                /* If progress function given call it */
                if (progress_fn)
                    progress_fn(userdata);
            }
        }
    }
}

void irradiance_filter_sh(int width, int height, int channels, unsigned char* src_base, unsigned char* dst_base, filter_progress_fn progress_fn, void* userdata)
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
    const size_t face_sz = width / 4;

    /* Allocate and build normal/solid angle index */
    float* nsa_idx = malloc(normal_solid_angle_index_sz(face_sz));
    normal_solid_angle_index_build(nsa_idx, &em_in);

    /* Compute spherical harmonic coefficients. */
    time_t start, end;
    time(&start);
    double sh_rgb[SH_COEFF_NUM][3];
    sh_coeffs(sh_rgb, &em_in, nsa_idx);
    time(&end);
    unsigned long long msecs = 1000 * difftime(end, start);
    printf("SH coef calculation time: %llu:%llu:%llu\n", (msecs / 1000) / 60, (msecs / 1000) % 60, msecs % 1000);

    /* Compute irradiance using sh data */
    float* nsa_ptr = nsa_idx;
    for (int face = 0; face < 6; ++face) {
        for (size_t ydst = 0; ydst < face_sz; ++ydst) {
            for (size_t xdst = 0; xdst < face_sz; ++xdst) {
                float dst[3];
                sh_irradiance(dst, sh_rgb, nsa_ptr);
                envmap_setpixel(&em_out, xdst, ydst, face, dst);
                /* Advance index pointer */
                nsa_ptr += 4;
                /* If progress function given call it */
                if (progress_fn)
                    progress_fn(userdata);
            }
        }
    }
    free(nsa_idx);
}
