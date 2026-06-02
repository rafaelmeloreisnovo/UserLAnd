#include <stdint.h>

#include "vectra_vm_vector_field.h"

#define CHECK(expr) do { if (!(expr)) { return __LINE__; } } while (0)

static int test_reference_program(void) {
    VectraVM vm;
    vectra_init(&vm);

    const uint8_t program[] = {
        VOP_VEC_LOAD_NUM, 0u,
        VOP_VEC_MOD42, 0u,
        VOP_VEC_ARC360, 0u,
        VOP_VEC_CHORD_Q15, 0u,
        VOP_VEC_H_EQ_Q15, 0u,
        VOP_VEC_TOROID_NODE, 0u,
        VOP_VEC_CORRECT, 7u,
        VOP_VEC_AUDIT, 0u,
        VOP_VEC_SEAL, 0u
    };

    const VectraStatus status = vectra_run(&vm, program, (uint32_t)sizeof(program));
    CHECK(status == VECTRA_STATUS_HALTED);
    CHECK(vm.vf.n_raw == 56u);
    CHECK(vm.vf.n_mod42 == 14u);
    CHECK(vm.vf.arc_deg == 56u);
    CHECK(vm.vf.chord_q15 == 10194);
    CHECK(vm.vf.h_q15 == 8828);
    CHECK(vm.vf.toroid_node == 983u);
    CHECK(vm.vf.gap_q15 == 11970u);
    CHECK(vm.vf.spiral_q15 == 11970u);
    CHECK(vm.phi_q8 == 212u);
    CHECK(vm.vf.audit_crc == 0xfe10ffcbu);
    return 0;
}

static int test_edges(void) {
    VectraVM vm;
    vectra_init(&vm);

    CHECK(vectra_run(0, (const uint8_t *)"", 0u) == VECTRA_STATUS_NULL);
    CHECK(vectra_run(&vm, 0, 0u) == VECTRA_STATUS_NULL);

    const uint8_t truncated[] = { VOP_VEC_LOAD_NUM };
    CHECK(vectra_run(&vm, truncated, (uint32_t)sizeof(truncated)) == VECTRA_STATUS_TRUNCATED);
    CHECK(vm.running == 0u);

    vectra_init(&vm);
    const uint8_t unknown[] = { 0x01u, 0u, VOP_VEC_SEAL, 0u };
    CHECK(vectra_run(&vm, unknown, (uint32_t)sizeof(unknown)) == VECTRA_STATUS_HALTED);
    CHECK(vm.phi_q8 == 199u);

    CHECK(vectra_number_at(9u) == 999u);
    CHECK(vectra_number_at(250u) == 250u);
    CHECK(vectra_chord_proxy_q15(0u) == 0u);
    CHECK(vectra_chord_proxy_q15(180u) == VECTRA_Q15_ONE);
    CHECK(vectra_chord_proxy_q15(270u) == (90u * VECTRA_Q15_ONE) / 180u);
    return 0;
}

int main(void) {
    int result = test_reference_program();
    if (result != 0) {
        return result;
    }
    return test_edges();
}
