/**
 * RAFCODE-Φ Engine Constants
 * 
 * These constants define the fundamental parameters of the RAFCODE-Φ processing engine.
 * They are hardcoded as compile-time constants to ensure optimal performance and
 * alignment with the symbolic logic system.
 * 
 * Based on: Maya-Egyptian cycle mathematics and sacred geometry principles
 */

#ifndef RAF_CONSTANTS_H
#define RAF_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The Answer: Base resonance constant
 * Used for: Primary synchronization frequency
 */
#define RAF_CONSTANT_ANSWER 42

/**
 * Maya Long Count: Cycle completion constant
 * Used for: Temporal alignment and cycle counting
 */
#define RAF_CONSTANT_MAYA_CYCLE 288

/**
 * Harmonic Frequency: Processing base frequency in Hz
 * Used for: Rhythm synchronization and sample rate alignment
 * This ensures the processor rhythm aligns with symbolic logic
 */
#define RAF_CONSTANT_FREQUENCY_HZ 144000

/**
 * Derived constants for computation
 */
#define RAF_MATRIX_SIZE_4X4 16
#define RAF_MATRIX_DIM 4

/**
 * Memory alignment for NEON operations (16-byte alignment for optimal SIMD)
 */
#define RAF_NEON_ALIGNMENT 16

/**
 * Android 16 page size (16KB) for memory allocation
 */
#define RAF_PAGE_SIZE_16KB 16384

#ifdef __cplusplus
}
#endif

#endif /* RAF_CONSTANTS_H */
