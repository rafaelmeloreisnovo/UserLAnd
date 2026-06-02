#include "vectra_vm_vector_field.h"

#include <stddef.h>

/*
 * VECTRA VM — deterministic vector-field module.
 * No heap, no libm, no statistical inference: number -> mod42 -> arc -> chord
 * -> h=(sqrt(3)/2)*chord -> toroidal node -> contractive correction.
 */

static const uint32_t VECTRA_NUMBERS[VECTRA_NUMBER_COUNT] = {
    56u, 70u, 14u, 7u, 50u, 35u, 777u, 555u, 936u, 999u
};

typedef void (*VectraHandler)(VectraVM *, uint8_t);

uint32_t vectra_number_at(uint8_t index) {
    return (index < VECTRA_NUMBER_COUNT) ? VECTRA_NUMBERS[index] : (uint32_t)index;
}

uint32_t vectra_chord_proxy_q15(uint32_t deg) {
    uint32_t d = deg % VECTRA_ARC_BASE;
    if (d > 180u) {
        d = VECTRA_ARC_BASE - d;
    }
    return (d * VECTRA_Q15_ONE) / 180u;
}

static uint32_t vectra_crc_mix(uint32_t h, uint32_t x) {
    h ^= x + 0x9E3779B9u + (h << 6) + (h >> 2);
    return h;
}

uint32_t vectra_q15_mul(uint32_t a, uint32_t b) {
    return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 15);
}

static void vectra_contract_once(VectraVectorState *vf) {
    vf->gap_q15 = vectra_q15_mul(vf->gap_q15, VECTRA_SQRT3_OVER_2);
    vf->spiral_q15 = vectra_q15_mul(vf->spiral_q15, VECTRA_SQRT3_OVER_2);
    vf->audit_crc = vectra_crc_mix(vf->audit_crc, vf->gap_q15);
}

static void vop_vec_load_num(VectraVM *vm, uint8_t arg) {
    const uint32_t n = vectra_number_at(arg);
    vm->vf.n_raw = n;
    vm->vf.audit_crc = vectra_crc_mix(vm->vf.audit_crc, n);
    vm->pc += 2u;
}

static void vop_vec_mod42(VectraVM *vm, uint8_t arg) {
    (void)arg;
    vm->vf.n_mod42 = vm->vf.n_raw % VECTRA_MOD_BASE;
    vm->vf.audit_crc = vectra_crc_mix(vm->vf.audit_crc, vm->vf.n_mod42);
    vm->pc += 2u;
}

static void vop_vec_arc360(VectraVM *vm, uint8_t arg) {
    (void)arg;
    vm->vf.arc_deg = vm->vf.n_raw % VECTRA_ARC_BASE;
    vm->vf.audit_crc = vectra_crc_mix(vm->vf.audit_crc, vm->vf.arc_deg);
    vm->pc += 2u;
}

static void vop_vec_chord_q15(VectraVM *vm, uint8_t arg) {
    (void)arg;
    vm->vf.chord_q15 = (int32_t)vectra_chord_proxy_q15(vm->vf.arc_deg);
    vm->vf.audit_crc = vectra_crc_mix(vm->vf.audit_crc, (uint32_t)vm->vf.chord_q15);
    vm->pc += 2u;
}

static void vop_vec_h_eq_q15(VectraVM *vm, uint8_t arg) {
    (void)arg;
    vm->vf.h_q15 = (int32_t)vectra_q15_mul((uint32_t)vm->vf.chord_q15, VECTRA_SQRT3_OVER_2);
    vm->vf.audit_crc = vectra_crc_mix(vm->vf.audit_crc, (uint32_t)vm->vf.h_q15);
    vm->pc += 2u;
}

static void vop_vec_toroid_node(VectraVM *vm, uint8_t arg) {
    (void)arg;
    const uint64_t node =
        ((uint64_t)vm->vf.n_raw * 17u) +
        ((uint64_t)vm->vf.n_mod42 * 42u) +
        ((uint64_t)vm->vf.arc_deg * 3u) +
        ((uint64_t)((uint32_t)vm->vf.h_q15 >> 5));
    vm->vf.toroid_node = (uint32_t)(node % 1000u);
    vm->vf.audit_crc = vectra_crc_mix(vm->vf.audit_crc, vm->vf.toroid_node);
    vm->pc += 2u;
}

static void vop_vec_correct(VectraVM *vm, uint8_t arg) {
    if (vm->vf.gap_q15 == 0u) {
        vm->vf.gap_q15 = VECTRA_Q15_ONE;
    }
    if (vm->vf.spiral_q15 == 0u) {
        vm->vf.spiral_q15 = VECTRA_Q15_ONE;
    }

    for (uint8_t i = 0u; i < arg; ++i) {
        vectra_contract_once(&vm->vf);
    }

    if (vm->phi_q8 < VECTRA_PHI_Q8_MAX) {
        vm->phi_q8 += (uint32_t)arg;
        if (vm->phi_q8 > VECTRA_PHI_Q8_MAX) {
            vm->phi_q8 = VECTRA_PHI_Q8_MAX;
        }
    }

    vm->pc += 2u;
}

static void vop_vec_audit(VectraVM *vm, uint8_t arg) {
    (void)arg;
    uint8_t ok = 1u;
    if (vm->vf.n_raw == 0u) {
        ok = 0u;
    }
    if (vm->vf.chord_q15 == 0 && vm->vf.arc_deg != 0u) {
        ok = 0u;
    }
    if (vm->vf.h_q15 == 0 && vm->vf.chord_q15 != 0) {
        ok = 0u;
    }

    if (ok != 0u) {
        if (vm->phi_q8 < 250u) {
            vm->phi_q8 += 5u;
        }
    } else if (vm->phi_q8 > 10u) {
        vm->phi_q8 -= 10u;
    }

    vm->vf.audit_crc = vectra_crc_mix(vm->vf.audit_crc, vm->phi_q8);
    vm->pc += 2u;
}

static void vop_vec_seal(VectraVM *vm, uint8_t arg) {
    (void)arg;
    vm->running = 0u;
    vm->pc += 2u;
}

static void vop_unknown(VectraVM *vm, uint8_t arg) {
    (void)arg;
    if (vm->phi_q8 > 0u) {
        --vm->phi_q8;
    }
    vm->pc += 2u;
}

static const VectraHandler VECTRA_DISPATCH[256] = {
    [VOP_VEC_LOAD_NUM] = vop_vec_load_num,
    [VOP_VEC_MOD42] = vop_vec_mod42,
    [VOP_VEC_ARC360] = vop_vec_arc360,
    [VOP_VEC_CHORD_Q15] = vop_vec_chord_q15,
    [VOP_VEC_H_EQ_Q15] = vop_vec_h_eq_q15,
    [VOP_VEC_TOROID_NODE] = vop_vec_toroid_node,
    [VOP_VEC_CORRECT] = vop_vec_correct,
    [VOP_VEC_AUDIT] = vop_vec_audit,
    [VOP_VEC_SEAL] = vop_vec_seal
};

void vectra_init(VectraVM *vm) {
    if (vm == NULL) {
        return;
    }

    for (uint32_t i = 0u; i < 64u; ++i) {
        vm->stack[i] = 0u;
    }
    vm->sp = 0u;
    vm->pc = 0u;
    vm->phi_q8 = VECTRA_PHI_Q8_INIT;
    vm->running = 1u;

    vm->vf.n_raw = 0u;
    vm->vf.n_mod42 = 0u;
    vm->vf.arc_deg = 0u;
    vm->vf.chord_q15 = 0;
    vm->vf.h_q15 = 0;
    vm->vf.toroid_node = 0u;
    vm->vf.spiral_q15 = VECTRA_Q15_ONE;
    vm->vf.gap_q15 = VECTRA_Q15_ONE;
    vm->vf.audit_crc = 0x52414656u;
}

VectraStatus vectra_step(VectraVM *vm, const uint8_t *bc, uint32_t len) {
    if (vm == NULL || bc == NULL) {
        return VECTRA_STATUS_NULL;
    }
    if (vm->running == 0u) {
        return VECTRA_STATUS_HALTED;
    }
    if (vm->pc + 1u >= len) {
        vm->running = 0u;
        return VECTRA_STATUS_TRUNCATED;
    }

    const uint8_t op = bc[vm->pc];
    const uint8_t arg = bc[vm->pc + 1u];
    VectraHandler handler = VECTRA_DISPATCH[op];
    if (handler == NULL) {
        handler = vop_unknown;
    }
    handler(vm, arg);
    return (vm->running == 0u) ? VECTRA_STATUS_HALTED : VECTRA_STATUS_OK;
}

VectraStatus vectra_run(VectraVM *vm, const uint8_t *bc, uint32_t len) {
    if (vm == NULL || bc == NULL) {
        return VECTRA_STATUS_NULL;
    }

    VectraStatus status = VECTRA_STATUS_OK;
    while (vm->running != 0u) {
        status = vectra_step(vm, bc, len);
        if (status != VECTRA_STATUS_OK) {
            break;
        }
    }
    return status;
}
