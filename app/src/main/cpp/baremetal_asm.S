/**
 * RAFCODE-Φ Bare Metal Assembly
 * 
 * Pure AArch64 NEON assembly implementation for 4x4 matrix operations.
 * This provides zero-overhead matrix multiplication for the "Sequência Viva"
 * (Living Sequence) processing pipeline.
 * 
 * Function: raf_baremetal_matmul_4x4
 * Multiplies two 4x4 float matrices using NEON SIMD instructions
 * 
 * Arguments:
 *   x0: pointer to matrix A (16 floats)
 *   x1: pointer to matrix B (16 floats)
 *   x2: pointer to result matrix (16 floats)
 * 
 * Matrix layout in memory (row-major):
 *   [0  1  2  3]
 *   [4  5  6  7]
 *   [8  9  10 11]
 *   [12 13 14 15]
 */

.text
.align 4
.global raf_baremetal_matmul_4x4
.type raf_baremetal_matmul_4x4, %function

raf_baremetal_matmul_4x4:
    // Load all rows of matrix B into NEON registers
    // v16-v19 will hold the 4 rows of matrix B
    ld1     {v16.4s, v17.4s, v18.4s, v19.4s}, [x1]
    
    // Process each row of matrix A
    
    // Row 0 of A
    ld1     {v0.4s}, [x0], #16          // Load row 0 of A
    
    // Broadcast each element of row 0
    dup     v1.4s, v0.s[0]              // Broadcast A[0][0]
    dup     v2.4s, v0.s[1]              // Broadcast A[0][1]
    dup     v3.4s, v0.s[2]              // Broadcast A[0][2]
    dup     v4.4s, v0.s[3]              // Broadcast A[0][3]
    
    // Multiply and accumulate for row 0 result
    fmul    v5.4s, v1.4s, v16.4s        // A[0][0] * B[0][:]
    fmla    v5.4s, v2.4s, v17.4s        // + A[0][1] * B[1][:]
    fmla    v5.4s, v3.4s, v18.4s        // + A[0][2] * B[2][:]
    fmla    v5.4s, v4.4s, v19.4s        // + A[0][3] * B[3][:]
    
    st1     {v5.4s}, [x2], #16          // Store result row 0
    
    // Row 1 of A
    ld1     {v0.4s}, [x0], #16          // Load row 1 of A
    
    dup     v1.4s, v0.s[0]              // Broadcast A[1][0]
    dup     v2.4s, v0.s[1]              // Broadcast A[1][1]
    dup     v3.4s, v0.s[2]              // Broadcast A[1][2]
    dup     v4.4s, v0.s[3]              // Broadcast A[1][3]
    
    fmul    v6.4s, v1.4s, v16.4s        // A[1][0] * B[0][:]
    fmla    v6.4s, v2.4s, v17.4s        // + A[1][1] * B[1][:]
    fmla    v6.4s, v3.4s, v18.4s        // + A[1][2] * B[2][:]
    fmla    v6.4s, v4.4s, v19.4s        // + A[1][3] * B[3][:]
    
    st1     {v6.4s}, [x2], #16          // Store result row 1
    
    // Row 2 of A
    ld1     {v0.4s}, [x0], #16          // Load row 2 of A
    
    dup     v1.4s, v0.s[0]              // Broadcast A[2][0]
    dup     v2.4s, v0.s[1]              // Broadcast A[2][1]
    dup     v3.4s, v0.s[2]              // Broadcast A[2][2]
    dup     v4.4s, v0.s[3]              // Broadcast A[2][3]
    
    fmul    v7.4s, v1.4s, v16.4s        // A[2][0] * B[0][:]
    fmla    v7.4s, v2.4s, v17.4s        // + A[2][1] * B[1][:]
    fmla    v7.4s, v3.4s, v18.4s        // + A[2][2] * B[2][:]
    fmla    v7.4s, v4.4s, v19.4s        // + A[2][3] * B[3][:]
    
    st1     {v7.4s}, [x2], #16          // Store result row 2
    
    // Row 3 of A
    ld1     {v0.4s}, [x0], #16          // Load row 3 of A
    
    dup     v1.4s, v0.s[0]              // Broadcast A[3][0]
    dup     v2.4s, v0.s[1]              // Broadcast A[3][1]
    dup     v3.4s, v0.s[2]              // Broadcast A[3][2]
    dup     v4.4s, v0.s[3]              // Broadcast A[3][3]
    
    fmul    v8.4s, v1.4s, v16.4s        // A[3][0] * B[0][:]
    fmla    v8.4s, v2.4s, v17.4s        // + A[3][1] * B[1][:]
    fmla    v8.4s, v3.4s, v18.4s        // + A[3][2] * B[2][:]
    fmla    v8.4s, v4.4s, v19.4s        // + A[3][3] * B[3][:]
    
    st1     {v8.4s}, [x2]               // Store result row 3
    
    ret

.size raf_baremetal_matmul_4x4, .-raf_baremetal_matmul_4x4

/**
 * Function: raf_baremetal_matvec_4x4
 * Multiplies a 4x4 matrix by a 4-element vector using NEON
 * 
 * Arguments:
 *   x0: pointer to matrix (16 floats, row-major)
 *   x1: pointer to vector (4 floats)
 *   x2: pointer to result vector (4 floats)
 */
.global raf_baremetal_matvec_4x4
.type raf_baremetal_matvec_4x4, %function

raf_baremetal_matvec_4x4:
    // Load vector into v0
    ld1     {v0.4s}, [x1]
    
    // Load matrix rows
    ld1     {v16.4s, v17.4s, v18.4s, v19.4s}, [x0]
    
    // Broadcast each vector element
    dup     v1.4s, v0.s[0]              // Broadcast v[0]
    dup     v2.4s, v0.s[1]              // Broadcast v[1]
    dup     v3.4s, v0.s[2]              // Broadcast v[2]
    dup     v4.4s, v0.s[3]              // Broadcast v[3]
    
    // Multiply and accumulate
    fmul    v5.4s, v1.4s, v16.4s        // v[0] * row0
    fmla    v5.4s, v2.4s, v17.4s        // + v[1] * row1
    fmla    v5.4s, v3.4s, v18.4s        // + v[2] * row2
    fmla    v5.4s, v4.4s, v19.4s        // + v[3] * row3
    
    // Store result
    st1     {v5.4s}, [x2]
    
    ret

.size raf_baremetal_matvec_4x4, .-raf_baremetal_matvec_4x4

/**
 * Function: raf_baremetal_vector_add_4
 * Adds two 4-element vectors using NEON
 * 
 * Arguments:
 *   x0: pointer to vector A (4 floats)
 *   x1: pointer to vector B (4 floats)
 *   x2: pointer to result vector (4 floats)
 */
.global raf_baremetal_vector_add_4
.type raf_baremetal_vector_add_4, %function

raf_baremetal_vector_add_4:
    ld1     {v0.4s}, [x0]               // Load vector A
    ld1     {v1.4s}, [x1]               // Load vector B
    fadd    v2.4s, v0.4s, v1.4s         // Add vectors
    st1     {v2.4s}, [x2]               // Store result
    ret

.size raf_baremetal_vector_add_4, .-raf_baremetal_vector_add_4

/**
 * Function: raf_baremetal_vector_scale_4
 * Scales a 4-element vector by a scalar using NEON
 * 
 * Arguments:
 *   x0: pointer to vector (4 floats)
 *   s0: scalar value (float in NEON register)
 *   x1: pointer to result vector (4 floats)
 */
.global raf_baremetal_vector_scale_4
.type raf_baremetal_vector_scale_4, %function

raf_baremetal_vector_scale_4:
    ld1     {v0.4s}, [x0]               // Load vector
    dup     v1.4s, v0.s[0]              // Duplicate scalar
    fmul    v2.4s, v0.4s, v1.4s         // Scale vector
    st1     {v2.4s}, [x1]               // Store result
    ret

.size raf_baremetal_vector_scale_4, .-raf_baremetal_vector_scale_4
