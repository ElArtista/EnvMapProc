#include <emproc/filter_util.h>

void normal_solid_angle_index_build(void* mem, struct envmap* em)
{
    const size_t face_sz = em->width / 4;
    const float warp = envmap_warp_fixup_factor(face_sz);
    const float texel_size = 1.0f / (float)face_sz;
    float* dst_ptr = mem;
    for (int face = 0; face < 6; ++face) {
        for (size_t ydst = 0; ydst < face_sz; ++ydst) {
            for (size_t xdst = 0; xdst < face_sz; ++xdst) {
                /* Current destination pixel location */
                /* Map value to [-1, 1], offset by 0.5 to point to texel center */
                const float v = 2.0f * ((ydst + 0.5f) * texel_size) - 1.0f;
                const float u = 2.0f * ((xdst + 0.5f) * texel_size) - 1.0f;
                /* Get sampling vector for the above u, v set */
                envmap_texel_coord_to_vec_warp(dst_ptr, em->type, u, v, face, warp);
                /* Get solid angle for u, v set */
                dst_ptr[3] = texel_solid_angle(u, v, texel_size);
                /* Advance */
                dst_ptr += 4;
            }
        }
    }
}

float normal_solid_angle_index_sz(size_t face_sz)
{
    return face_sz /* width    */
         * face_sz /* height   */
         * 6       /* faces    */
         * 4       /* channels */
         * 4;      /* bytes per channel */
}
