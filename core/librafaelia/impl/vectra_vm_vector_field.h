#ifndef VECTRA_VM_VECTOR_FIELD_H
#define VECTRA_VM_VECTOR_FIELD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VECTRA_Q15_ONE        32768u
#define VECTRA_SQRT3_OVER_2   28378u
#define VECTRA_MOD_BASE       42u
#define VECTRA_ARC_BASE       360u
#define VECTRA_PHI_Q8_INIT    200u
#define VECTRA_PHI_Q8_MAX     255u
#define VECTRA_NUMBER_COUNT   10u

#define VOP_VEC_LOAD_NUM      0x60u
#define VOP_VEC_MOD42         0x61u
#define VOP_VEC_ARC360        0x62u
#define VOP_VEC_CHORD_Q15     0x63u
#define VOP_VEC_H_EQ_Q15      0x64u
#define VOP_VEC_TOROID_NODE   0x65u
#define VOP_VEC_CORRECT       0x66u
#define VOP_VEC_AUDIT         0x67u
#define VOP_VEC_SEAL          0x6Fu

typedef struct {
    uint32_t n_raw;
    uint32_t n_mod42;
    uint32_t arc_deg;
    int32_t chord_q15;
    int32_t h_q15;
    uint32_t toroid_node;
    uint32_t spiral_q15;
    uint32_t gap_q15;
    uint32_t audit_crc;
} VectraVectorState;

typedef struct {
    uint8_t stack[64];
    uint8_t sp;
    uint32_t pc;
    uint32_t phi_q8;
    uint8_t running;
    VectraVectorState vf;
} VectraVM;

typedef enum {
    VECTRA_STATUS_OK = 0,
    VECTRA_STATUS_NULL = -1,
    VECTRA_STATUS_TRUNCATED = -2,
    VECTRA_STATUS_HALTED = 1
} VectraStatus;

void vectra_init(VectraVM *vm);
VectraStatus vectra_step(VectraVM *vm, const uint8_t *bc, uint32_t len);
VectraStatus vectra_run(VectraVM *vm, const uint8_t *bc, uint32_t len);
uint32_t vectra_number_at(uint8_t index);
uint32_t vectra_chord_proxy_q15(uint32_t deg);
uint32_t vectra_q15_mul(uint32_t a, uint32_t b);

#ifdef __cplusplus
}
#endif

#endif /* VECTRA_VM_VECTOR_FIELD_H */
