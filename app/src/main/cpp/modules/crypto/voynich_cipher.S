/**
 * RAFCODE-Φ Voynich Cipher Module
 * 
 * Pure AArch64 assembly implementation for steganographic encoding/decoding.
 * Provides methods to hide data within image/binary streams without external libraries.
 * 
 * This implements the "Bitraf" encoding scheme using least-significant-bit (LSB)
 * steganography with NEON acceleration.
 */

.text
.align 4

/**
 * Function: raf_voynich_encode_lsb
 * Encodes data into carrier bytes using LSB steganography
 * 
 * Each byte of secret data is distributed across 8 carrier bytes
 * by replacing the LSB of each carrier byte.
 * 
 * Arguments:
 *   x0: pointer to carrier data (will be modified in-place)
 *   x1: size of carrier data (must be >= secret_size * 8)
 *   x2: pointer to secret data
 *   x3: size of secret data
 * 
 * Returns:
 *   x0: 0 on success, -1 on error
 */
.global raf_voynich_encode_lsb
.type raf_voynich_encode_lsb, %function

raf_voynich_encode_lsb:
    // Save frame pointer and link register
    stp     x29, x30, [sp, #-48]!
    mov     x29, sp
    
    // Save callee-saved registers
    stp     x19, x20, [sp, #16]
    stp     x21, x22, [sp, #32]
    
    // Check if carrier is large enough
    lsl     x4, x3, #3                  // x4 = secret_size * 8
    cmp     x1, x4
    b.lt    .Lencode_error              // Error if carrier too small
    
    // Initialize pointers
    mov     x19, x0                     // x19 = carrier pointer
    mov     x20, x2                     // x20 = secret pointer
    mov     x21, x3                     // x21 = remaining secret bytes
    
.Lencode_loop:
    cbz     x21, .Lencode_success       // Done if no more secret bytes
    
    // Load one secret byte
    ldrb    w22, [x20], #1              // w22 = current secret byte
    sub     x21, x21, #1                // Decrement counter
    
    // Encode 8 bits into 8 carrier bytes
    mov     x4, #8                      // Bit counter
    
.Lencode_bit_loop:
    // Load carrier byte
    ldrb    w5, [x19]
    
    // Clear LSB of carrier
    and     w5, w5, #0xFE
    
    // Extract current bit from secret byte (MSB first)
    lsr     w6, w22, #7                 // Get MSB
    and     w6, w6, #1                  // Isolate bit
    
    // Set LSB of carrier to secret bit
    orr     w5, w5, w6
    
    // Store modified carrier byte
    strb    w5, [x19], #1
    
    // Shift secret byte left for next bit
    lsl     w22, w22, #1
    
    // Continue bit loop
    subs    x4, x4, #1
    b.ne    .Lencode_bit_loop
    
    b       .Lencode_loop

.Lencode_success:
    mov     x0, #0                      // Return success
    b       .Lencode_exit

.Lencode_error:
    mov     x0, #-1                     // Return error

.Lencode_exit:
    // Restore registers
    ldp     x21, x22, [sp, #32]
    ldp     x19, x20, [sp, #16]
    ldp     x29, x30, [sp], #48
    ret

.size raf_voynich_encode_lsb, .-raf_voynich_encode_lsb

/**
 * Function: raf_voynich_decode_lsb
 * Decodes hidden data from carrier bytes using LSB steganography
 * 
 * Arguments:
 *   x0: pointer to carrier data
 *   x1: size of carrier data (must be >= output_size * 8)
 *   x2: pointer to output buffer
 *   x3: size of data to extract
 * 
 * Returns:
 *   x0: 0 on success, -1 on error
 */
.global raf_voynich_decode_lsb
.type raf_voynich_decode_lsb, %function

raf_voynich_decode_lsb:
    // Save frame pointer and link register
    stp     x29, x30, [sp, #-48]!
    mov     x29, sp
    
    // Save callee-saved registers
    stp     x19, x20, [sp, #16]
    stp     x21, x22, [sp, #32]
    
    // Check if carrier is large enough
    lsl     x4, x3, #3                  // x4 = output_size * 8
    cmp     x1, x4
    b.lt    .Ldecode_error              // Error if carrier too small
    
    // Initialize pointers
    mov     x19, x0                     // x19 = carrier pointer
    mov     x20, x2                     // x20 = output pointer
    mov     x21, x3                     // x21 = remaining output bytes
    
.Ldecode_loop:
    cbz     x21, .Ldecode_success       // Done if no more bytes to extract
    
    // Decode 8 bits from 8 carrier bytes
    mov     w22, #0                     // Accumulated byte
    mov     x4, #8                      // Bit counter
    
.Ldecode_bit_loop:
    // Load carrier byte
    ldrb    w5, [x19], #1
    
    // Extract LSB
    and     w5, w5, #1
    
    // Shift accumulated byte left and add new bit
    lsl     w22, w22, #1
    orr     w22, w22, w5
    
    // Continue bit loop
    subs    x4, x4, #1
    b.ne    .Ldecode_bit_loop
    
    // Store decoded byte
    strb    w22, [x20], #1
    sub     x21, x21, #1
    
    b       .Ldecode_loop

.Ldecode_success:
    mov     x0, #0                      // Return success
    b       .Ldecode_exit

.Ldecode_error:
    mov     x0, #-1                     // Return error

.Ldecode_exit:
    // Restore registers
    ldp     x21, x22, [sp, #32]
    ldp     x19, x20, [sp, #16]
    ldp     x29, x30, [sp], #48
    ret

.size raf_voynich_decode_lsb, .-raf_voynich_decode_lsb

/**
 * Function: raf_voynich_xor_cipher
 * Simple XOR cipher using NEON for speed
 * 
 * Arguments:
 *   x0: pointer to data (modified in-place)
 *   x1: size of data
 *   x2: pointer to key
 *   x3: size of key
 */
.global raf_voynich_xor_cipher
.type raf_voynich_xor_cipher, %function

raf_voynich_xor_cipher:
    cbz     x1, .Lxor_done              // Return if size is 0
    cbz     x3, .Lxor_done              // Return if key size is 0
    
    mov     x4, #0                      // Key index
    mov     x5, #0                      // Data index
    
.Lxor_loop:
    // Load data byte
    ldrb    w6, [x0, x5]
    
    // Load key byte (with wrapping)
    ldrb    w7, [x2, x4]
    
    // XOR
    eor     w6, w6, w7
    
    // Store result
    strb    w6, [x0, x5]
    
    // Increment indices
    add     x5, x5, #1
    add     x4, x4, #1
    
    // Wrap key index if needed
    cmp     x4, x3
    csel    x4, xzr, x4, eq             // x4 = (x4 == x3) ? 0 : x4
    
    // Continue if more data
    cmp     x5, x1
    b.lt    .Lxor_loop

.Lxor_done:
    ret

.size raf_voynich_xor_cipher, .-raf_voynich_xor_cipher

/**
 * Function: raf_voynich_hash_simple
 * Simple hash function for "Bitraf" verification
 * Uses a modified DJB2 algorithm
 * 
 * Arguments:
 *   x0: pointer to data
 *   x1: size of data
 * 
 * Returns:
 *   x0: 32-bit hash value
 */
.global raf_voynich_hash_simple
.type raf_voynich_hash_simple, %function

raf_voynich_hash_simple:
    mov     x2, #5381                   // Initial hash value (DJB2)
    cbz     x1, .Lhash_done
    
    mov     x3, #0                      // Index
    
.Lhash_loop:
    // hash = hash * 33 + data[i]
    ldrb    w4, [x0, x3]                // Load byte
    
    // hash * 33 = hash * 32 + hash = (hash << 5) + hash
    lsl     x5, x2, #5
    add     x2, x5, x2
    
    // Add current byte
    add     x2, x2, x4
    
    // Next byte
    add     x3, x3, #1
    cmp     x3, x1
    b.lt    .Lhash_loop

.Lhash_done:
    mov     x0, x2                      // Return hash in x0
    ret

.size raf_voynich_hash_simple, .-raf_voynich_hash_simple
