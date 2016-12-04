#include <emproc/sh.h>
#ifndef OPENCL_MODE
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#endif

#define PI      3.1415926535897932384626433832795028841971693993751058
#define PI4     12.566370614359172953850573533118011536788677597500423
#define PI16    50.265482457436691815402294132472046147154710390001693
#define PI64    201.06192982974676726160917652988818458861884156000677
#define SQRT_PI 1.7724538509055160272981674833411451827975494561223871

void sh_eval_basis5(double* sh_basis, GLOBAL const float* dir)
{
    const double x = (double)dir[0];
    const double y = (double)dir[1];
    const double z = (double)dir[2];

    const double x2 = x*x;
    const double y2 = y*y;
    const double z2 = z*z;

    const double z3 = pow(z, 3.0);

    const double x4 = pow(x, 4.0);
    const double y4 = pow(y, 4.0);
    const double z4 = pow(z, 4.0);

    /* Equations based on data from: http://ppsloan.org/publications/stupid_sH36.pdf */
    sh_basis[0]  =  1.0 / (2.0 * SQRT_PI);

    sh_basis[1]  = -sqrt(3.0 / PI4) * y;
    sh_basis[2]  =  sqrt(3.0 / PI4) * z;
    sh_basis[3]  = -sqrt(3.0 / PI4) * x;

    sh_basis[4]  =  sqrt(15.0 /  PI4) * y * x;
    sh_basis[5]  = -sqrt(15.0 /  PI4) * y * z;
    sh_basis[6]  =  sqrt(5.0  / PI16) * (3.0 * z2 - 1.0);
    sh_basis[7]  = -sqrt(15.0 /  PI4) * x * z;
    sh_basis[8]  =  sqrt(15.0 / PI16) * (x2 - y2);

    sh_basis[9]  = -sqrt( 70.0 / PI64) * y * (3 * x2 - y2);
    sh_basis[10] =  sqrt(105.0 /  PI4) * y * x * z;
    sh_basis[11] = -sqrt( 21.0 / PI16) * y * (-1.0 + 5.0 * z2);
    sh_basis[12] =  sqrt(  7.0 / PI16) * (5.0 * z3 - 3.0 * z);
    sh_basis[13] = -sqrt( 42.0 / PI64) * x * (-1.0 + 5.0 * z2);
    sh_basis[14] =  sqrt(105.0 / PI16) * (x2 - y2) * z;
    sh_basis[15] = -sqrt( 70.0 / PI64) * x * (x2 - 3.0 * y2);

    sh_basis[16] =  3.0 * sqrt(35.0 / PI16) * x * y * (x2 - y2);
    sh_basis[17] = -3.0 * sqrt(70.0 / PI64) * y * z * (3.0 * x2 - y2);
    sh_basis[18] =  3.0 * sqrt( 5.0 / PI16) * y * x * (-1.0 + 7.0 * z2);
    sh_basis[19] = -3.0 * sqrt(10.0 / PI64) * y * z * (-3.0 + 7.0 * z2);
    sh_basis[20] =  (105.0 * z4 -90.0 * z2 + 9.0) / (16.0 * SQRT_PI);
    sh_basis[21] = -3.0 * sqrt(10.0 / PI64) * x * z * (-3.0 + 7.0 * z2);
    sh_basis[22] =  3.0 * sqrt( 5.0 / PI64) * (x2 - y2) * (-1.0 + 7.0 * z2);
    sh_basis[23] = -3.0 * sqrt(70.0 / PI64) * x * z * (x2 - 3.0 * y2);
    sh_basis[24] =  3.0 * sqrt(35.0 / (4.0 * PI64)) * (x4 - 6.0 * y2 * x2 + y4);
}

#ifndef OPENCL_MODE
void sh_coeffs(double sh_coeffs[SH_COEFF_NUM][3], struct envmap* em, float* nsa_idx)
{
    const size_t face_sz = em->width / 4;
    memset(sh_coeffs, 0, SH_COEFF_NUM * 3 * sizeof(double));

#ifndef WITH_OPENMP
    float* nsa_ptr = nsa_idx;
#endif
    double weight_accum = 0.0;
    for (int face = 0; face < 6; ++face) {
#ifdef WITH_OPENMP
        #pragma omp parallel for
#endif
        for (size_t ydst = 0; ydst < face_sz; ++ydst) {
            for (size_t xdst = 0; xdst < face_sz; ++xdst) {
#ifdef WITH_OPENMP
                float* nsa_ptr = nsa_idx + ((face * face_sz * face_sz) + ydst * face_sz + xdst) * 4;
#endif
                /* Current pixel values */
                uint8_t* src_ptr = envmap_pixel_ptr(em, xdst, ydst, face);
                const double rr = (double)src_ptr[0];
                const double gg = (double)src_ptr[1];
                const double bb = (double)src_ptr[2];
                /* Calculate SH Basis */
                double sh_basis[SH_COEFF_NUM];
                sh_eval_basis5(sh_basis, nsa_ptr);
                const double weight = (double)nsa_ptr[3];
                for (uint8_t ii = 0; ii < SH_COEFF_NUM; ++ii) {
#ifdef WITH_OPENMP
                    #pragma omp atomic update
#endif
                    sh_coeffs[ii][0] += rr * sh_basis[ii] * weight;
#ifdef WITH_OPENMP
                    #pragma omp atomic update
#endif
                    sh_coeffs[ii][1] += gg * sh_basis[ii] * weight;
#ifdef WITH_OPENMP
                    #pragma omp atomic update
#endif
                    sh_coeffs[ii][2] += bb * sh_basis[ii] * weight;
                }
                weight_accum += weight;
#ifndef WITH_OPENMP
                /* Forward index ptr */
                nsa_ptr += 4;
#endif
            }
        }
    }

    /*
     * Normalization.
     * This is not really necesarry because usually PI*4 - weightAccum ~= 0.000003
     * so it doesn't change almost anything, but it doesn't cost much be more correct.
     */
    const double norm = PI4 / weight_accum;
    for (uint8_t ii = 0; ii < SH_COEFF_NUM; ++ii) {
        sh_coeffs[ii][0] *= norm;
        sh_coeffs[ii][1] *= norm;
        sh_coeffs[ii][2] *= norm;
    }
}

void sh_irradiance(float irr[3], double sh_rgb[SH_COEFF_NUM][3], float dir[3])
{
    /* Eval basis for current direction */
    double sh_basis[SH_COEFF_NUM];
    sh_eval_basis5(sh_basis, dir);

    /* Calculate pixel value using sh */
    double rgb[3] = {0.0f, 0.0f, 0.0f};

    /* Band 0 (factor 1.0) */
    rgb[0] += sh_rgb[0][0] * sh_basis[0] * 1.0f;
    rgb[1] += sh_rgb[0][1] * sh_basis[0] * 1.0f;
    rgb[2] += sh_rgb[0][2] * sh_basis[0] * 1.0f;

    /* Band 1 (factor 2/3). */
    uint8_t ii = 1;
    for (; ii < 4; ++ii) {
        rgb[0] += sh_rgb[ii][0] * sh_basis[ii] * (2.0f/3.0f);
        rgb[1] += sh_rgb[ii][1] * sh_basis[ii] * (2.0f/3.0f);
        rgb[2] += sh_rgb[ii][2] * sh_basis[ii] * (2.0f/3.0f);
    }

    /* Band 2 (factor 1/4). */
    for (; ii < 9; ++ii) {
        rgb[0] += sh_rgb[ii][0] * sh_basis[ii] * (1.0f/4.0f);
        rgb[1] += sh_rgb[ii][1] * sh_basis[ii] * (1.0f/4.0f);
        rgb[2] += sh_rgb[ii][2] * sh_basis[ii] * (1.0f/4.0f);
    }

    /* Band 3 (factor 0). */
    ii = 16;

    /* Band 4 (factor -1/24). */
    for (; ii < 25; ++ii) {
        rgb[0] += sh_rgb[ii][0] * sh_basis[ii] * (-1.0f/24.0f);
        rgb[1] += sh_rgb[ii][1] * sh_basis[ii] * (-1.0f/24.0f);
        rgb[2] += sh_rgb[ii][2] * sh_basis[ii] * (-1.0f/24.0f);
    }

    /* Store output */
    irr[0] = (float)rgb[0] / 255.0f;
    irr[1] = (float)rgb[1] / 255.0f;
    irr[2] = (float)rgb[2] / 255.0f;
}
#endif
