#include <emproc/filter.h>
#include <emproc/filter_util.h>
#include <emproc/envmap.h>
#include <emproc/sh.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

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
