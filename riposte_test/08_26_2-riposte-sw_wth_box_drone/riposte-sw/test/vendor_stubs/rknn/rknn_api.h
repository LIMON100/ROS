// Stub of the RKNN C API surface this repo uses — SYNTAX CHECK ONLY.
//
// The vendor SDKs are not redistributable and are absent from CI and from
// most dev machines, so the RIPOSTE_WITH_* translation units are normally
// never compiled at all. That let syntax and type errors sit in them until
// hardware bring-up. ci/vendor_syntax.sh compiles those TUs against these
// stubs with -fsyntax-only, which catches that class of error immediately.
//
// This declares just enough to parse. It has NO behaviour: nothing here may
// ever be linked or executed, and a passing check says the code compiles,
// not that it works against the real device.
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t rknn_context;
#define RKNN_SUCC 0
#define RKNN_FLAG_PRIOR_HIGH 0
typedef enum { RKNN_TENSOR_UINT8 = 0, RKNN_TENSOR_FLOAT32 = 1 } rknn_tensor_type;
typedef enum { RKNN_TENSOR_NHWC = 0, RKNN_TENSOR_NCHW = 1 } rknn_tensor_format;
typedef enum {
    RKNN_NPU_CORE_AUTO = 0,
    RKNN_NPU_CORE_0 = 1,
    RKNN_NPU_CORE_1 = 2,
    RKNN_NPU_CORE_2 = 4
} rknn_core_mask;
typedef enum {
    RKNN_QUERY_IN_OUT_NUM = 0,
    RKNN_QUERY_INPUT_ATTR = 1,
    RKNN_QUERY_OUTPUT_ATTR = 2
} rknn_query_cmd;
typedef struct {
    uint32_t n_input;
    uint32_t n_output;
} rknn_input_output_num;
typedef struct {
    uint32_t index;
    uint32_t n_dims;
    uint32_t dims[16];
    char name[256];
    uint32_t n_elems;
    uint32_t size;
    rknn_tensor_format fmt;
    rknn_tensor_type type;
} rknn_tensor_attr;
typedef struct {
    uint32_t index;
    void* buf;
    uint32_t size;
    uint8_t pass_through;
    rknn_tensor_type type;
    rknn_tensor_format fmt;
} rknn_input;
typedef struct {
    uint8_t want_float;
    uint8_t is_prealloc;
    uint32_t index;
    void* buf;
    uint32_t size;
} rknn_output;
static inline int rknn_init(rknn_context* c, void* m, uint32_t s, uint32_t f, void* e) {
    (void)c;
    (void)m;
    (void)s;
    (void)f;
    (void)e;
    return 0;
}
static inline int rknn_destroy(rknn_context c) {
    (void)c;
    return 0;
}
static inline int rknn_query(rknn_context c, rknn_query_cmd q, void* info, uint32_t sz) {
    (void)c;
    (void)q;
    (void)info;
    (void)sz;
    return 0;
}
static inline int rknn_set_core_mask(rknn_context c, rknn_core_mask m) {
    (void)c;
    (void)m;
    return 0;
}
static inline int rknn_inputs_set(rknn_context c, uint32_t n, rknn_input* i) {
    (void)c;
    (void)n;
    (void)i;
    return 0;
}
static inline int rknn_run(rknn_context c, void* e) {
    (void)c;
    (void)e;
    return 0;
}
static inline int rknn_outputs_get(rknn_context c, uint32_t n, rknn_output* o, void* e) {
    (void)c;
    (void)n;
    (void)o;
    (void)e;
    return 0;
}
static inline int rknn_outputs_release(rknn_context c, uint32_t n, rknn_output* o) {
    (void)c;
    (void)n;
    (void)o;
    return 0;
}
#ifdef __cplusplus
}
#endif
