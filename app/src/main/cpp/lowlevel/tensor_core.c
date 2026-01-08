/**
 * RAFCODE-Φ Tensor Core
 * 
 * Low-level tensor processing using ARM NEON intrinsics for zero-latency
 * vector operations without Java overhead.
 * 
 * This implementation provides the "Black Box" (Caixa Preta) processing
 * for input vector tokenization and transformation.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <string.h>
#include <arm_neon.h>

#include "../raf_constants.h"

/**
 * 4x4 Matrix structure for NEON operations
 * Aligned to 16 bytes for optimal SIMD performance
 */
typedef struct {
    float32x4_t rows[4];
} raf_matrix4x4_t __attribute__((aligned(RAF_NEON_ALIGNMENT)));

/**
 * Vector4 structure for NEON operations
 */
typedef struct {
    float32x4_t data;
} raf_vector4_t __attribute__((aligned(RAF_NEON_ALIGNMENT)));

/**
 * Initialize identity matrix using NEON intrinsics
 */
void raf_tensor_identity(raf_matrix4x4_t* mat) {
    const float identity_data[4][4] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}
    };
    
    for (int i = 0; i < 4; i++) {
        mat->rows[i] = vld1q_f32(identity_data[i]);
    }
}

/**
 * Matrix-vector multiplication using NEON intrinsics
 * Result = Matrix * Vector
 * 
 * This is the core "tokenization" operation that transforms input vectors
 * through the matrix without Java latency.
 */
void raf_tensor_matvec_mul(const raf_matrix4x4_t* mat, const raf_vector4_t* vec, raf_vector4_t* result) {
    // Broadcast vector components
    float32x4_t v0 = vdupq_lane_f32(vget_low_f32(vec->data), 0);
    float32x4_t v1 = vdupq_lane_f32(vget_low_f32(vec->data), 1);
    float32x4_t v2 = vdupq_lane_f32(vget_high_f32(vec->data), 0);
    float32x4_t v3 = vdupq_lane_f32(vget_high_f32(vec->data), 1);
    
    // Multiply and accumulate
    float32x4_t r0 = vmulq_f32(mat->rows[0], v0);
    float32x4_t r1 = vmulq_f32(mat->rows[1], v1);
    float32x4_t r2 = vmulq_f32(mat->rows[2], v2);
    float32x4_t r3 = vmulq_f32(mat->rows[3], v3);
    
    // Sum all contributions
    float32x4_t sum01 = vaddq_f32(r0, r1);
    float32x4_t sum23 = vaddq_f32(r2, r3);
    result->data = vaddq_f32(sum01, sum23);
}

/**
 * 4x4 Matrix multiplication using NEON intrinsics
 * Result = A * B
 * 
 * Optimized using NEON vector operations for maximum throughput.
 */
void raf_tensor_matmul(const raf_matrix4x4_t* a, const raf_matrix4x4_t* b, raf_matrix4x4_t* result) {
    for (int i = 0; i < 4; i++) {
        // Broadcast each element of row i from matrix A
        float32x4_t a0 = vdupq_lane_f32(vget_low_f32(a->rows[i]), 0);
        float32x4_t a1 = vdupq_lane_f32(vget_low_f32(a->rows[i]), 1);
        float32x4_t a2 = vdupq_lane_f32(vget_high_f32(a->rows[i]), 0);
        float32x4_t a3 = vdupq_lane_f32(vget_high_f32(a->rows[i]), 1);
        
        // Multiply and accumulate
        float32x4_t r0 = vmulq_f32(a0, b->rows[0]);
        float32x4_t r1 = vmulq_f32(a1, b->rows[1]);
        float32x4_t r2 = vmulq_f32(a2, b->rows[2]);
        float32x4_t r3 = vmulq_f32(a3, b->rows[3]);
        
        // Sum contributions
        float32x4_t sum01 = vaddq_f32(r0, r1);
        float32x4_t sum23 = vaddq_f32(r2, r3);
        result->rows[i] = vaddq_f32(sum01, sum23);
    }
}

/**
 * Apply resonance constant to vector
 * Scales vector by the sacred constant (42)
 */
void raf_tensor_apply_resonance(raf_vector4_t* vec) {
    float32x4_t resonance = vdupq_n_f32((float)RAF_CONSTANT_ANSWER);
    vec->data = vmulq_f32(vec->data, resonance);
}

/**
 * Apply Maya cycle transformation
 * Modulates vector components by cycle constant
 */
void raf_tensor_apply_maya_cycle(raf_vector4_t* vec) {
    float32x4_t cycle = vdupq_n_f32((float)RAF_CONSTANT_MAYA_CYCLE);
    vec->data = vdivq_f32(vec->data, cycle);
}

/**
 * Tokenize input data buffer into vector representation
 * This is the "Black Box" entry point
 * 
 * @param input Input byte buffer
 * @param input_len Length of input buffer
 * @param output Output vector (4 floats)
 */
void raf_tensor_tokenize(const uint8_t* input, uint32_t input_len, raf_vector4_t* output) {
    // Initialize output to zero
    output->data = vdupq_n_f32(0.0f);
    
    if (input_len == 0) {
        return;
    }
    
    // Accumulate input bytes into vector components
    float accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    for (uint32_t i = 0; i < input_len; i++) {
        uint32_t component = i % 4;
        accum[component] += (float)input[i];
    }
    
    // Normalize by input length and load into NEON register
    float scale = 1.0f / (float)input_len;
    for (int i = 0; i < 4; i++) {
        accum[i] *= scale;
    }
    
    output->data = vld1q_f32(accum);
    
    // Apply resonance constant for alignment
    raf_tensor_apply_resonance(output);
}

/**
 * Process vector through "Sensorium Global" pipeline
 * Applies full transformation sequence
 */
void raf_tensor_sensorium_process(raf_vector4_t* vec) {
    // Step 1: Apply resonance
    raf_tensor_apply_resonance(vec);
    
    // Step 2: Apply Maya cycle modulation
    raf_tensor_apply_maya_cycle(vec);
    
    // Step 3: Normalize to unit range [0, 1]
    float32x4_t ones = vdupq_n_f32(1.0f);
    float32x4_t abs_vec = vabsq_f32(vec->data);
    
    // Find max component for normalization
    float32x2_t max1 = vpmax_f32(vget_low_f32(abs_vec), vget_high_f32(abs_vec));
    float32x2_t max2 = vpmax_f32(max1, max1);
    float max_val = vget_lane_f32(max2, 0);
    
    if (max_val > 0.0f) {
        float32x4_t max_vec = vdupq_n_f32(max_val);
        vec->data = vdivq_f32(vec->data, max_vec);
    }
}

/**
 * Extract float array from vector for output
 */
void raf_tensor_vector_to_array(const raf_vector4_t* vec, float* output) {
    vst1q_f32(output, vec->data);
}

/**
 * Load float array into vector
 */
void raf_tensor_array_to_vector(const float* input, raf_vector4_t* vec) {
    vec->data = vld1q_f32(input);
}
