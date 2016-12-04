#include "envmap.c"
#include "sh.c"

#pragma OPENCL EXTENSION cl_khr_fp64: enable
#pragma OPENCL EXTENSION cl_khr_int64_base_atomics: enable

void atomic_add_dbl(__global double* val, double delta)
{
    union {
        double f;
        ulong i;
    } old;
    union {
        double f;
        ulong i;
    } new;
    do {
        old.f = *val;
        new.f = old.f + delta;
    } while (atom_cmpxchg ((volatile __global ulong *)val, old.i, new.i) != old.i);
}

__kernel void booo(__global double* sh_coeffs,
                   __global double* weight_accum,
                   __global unsigned char* img_in,
                   __global float* nsa_idx,
                   const unsigned int face_sz,
                   const unsigned int face)
{
    /* Current processing pixel */
    unsigned int xdst = get_global_id(1);
    unsigned int ydst = get_global_id(0);
    const int channels = 3;

    /* Fill in input envmap struct */
    struct envmap em;
    em.channels = channels;
    em.data = img_in;
    em.width = face_sz * 4;
    em.height = face_sz * 3;
    em.type = EM_TYPE_HCROSS;

    /* Ptr to the normal/solid angle index */
    __global float* nsa_ptr = nsa_idx + ((face * face_sz * face_sz) + ydst * face_sz + xdst) * 4;

    /* Current pixel values */
    __global uint8_t* src_ptr = envmap_pixel_ptr(&em, xdst, ydst, face);
    const double rr = (double)src_ptr[0];
    const double gg = (double)src_ptr[1];
    const double bb = (double)src_ptr[2];

    /* Calculate SH Basis */
    double sh_basis[SH_COEFF_NUM];
    sh_eval_basis5(sh_basis, nsa_ptr);
    const double weight = (double)nsa_ptr[3];
    for (uint8_t ii = 0; ii < SH_COEFF_NUM; ++ii) {
        atomic_add_dbl(sh_coeffs + ii * 3 + 0, rr * sh_basis[ii] * weight);
        atomic_add_dbl(sh_coeffs + ii * 3 + 1, gg * sh_basis[ii] * weight);
        atomic_add_dbl(sh_coeffs + ii * 3 + 2, bb * sh_basis[ii] * weight);
    }
    atomic_add_dbl(weight_accum, weight);
}
