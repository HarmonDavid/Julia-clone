// This file is a part of Julia. License is MIT: https://julialang.org/license

#ifndef JL_TIMING_H
#define JL_TIMING_H

#ifdef __cplusplus
extern "C" {
#endif
void jl_init_timing(void);
void jl_destroy_timing(void) JL_NOTSAFEPOINT;
#ifdef __cplusplus
}
#endif

#ifndef ENABLE_TIMINGS
#define JL_TIMING(owner)
#else

#include "julia_assert.h"

#ifdef __cplusplus
extern "C" {
#endif
void jl_print_timings(void);
jl_timing_block_t *jl_pop_timing_block(jl_timing_block_t *cur_block);
void jl_timing_block_start(jl_timing_block_t *cur_block);
void jl_timing_block_stop(jl_timing_block_t *cur_block);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#define HAVE_TIMING_SUPPORT
#elif defined(_COMPILER_CLANG_)
#define HAVE_TIMING_SUPPORT
#elif defined(_COMPILER_GCC_)
#define HAVE_TIMING_SUPPORT
#endif

#ifndef HAVE_TIMING_SUPPORT
#define JL_TIMING(owner)
#else

#define JL_TIMING_OWNERS          \
        X(ROOT),                  \
        X(GC),                    \
        X(LOWERING),              \
        X(PARSING),               \
        X(INFERENCE),             \
        X(CODEGEN),               \
        X(METHOD_LOOKUP_SLOW),    \
        X(METHOD_LOOKUP_FAST),    \
        X(LLVM_OPT),              \
        X(LLVM_MODULE_FINISH),    \
        X(METHOD_MATCH),          \
        X(TYPE_CACHE_LOOKUP),     \
        X(TYPE_CACHE_INSERT),     \
        X(STAGED_FUNCTION),       \
        X(MACRO_INVOCATION),      \
        X(AST_COMPRESS),          \
        X(AST_UNCOMPRESS),        \
        X(SYSIMG_LOAD),           \
        X(SYSIMG_DUMP),           \
        X(NATIVE_DUMP),           \
        X(ADD_METHOD),            \
        X(LOAD_MODULE),           \
        X(SAVE_MODULE),           \
        X(INIT_MODULE),

enum jl_timing_owners {
#define X(name) JL_TIMING_ ## name
    JL_TIMING_OWNERS
#undef X
    JL_TIMING_LAST
};

extern uint64_t jl_timing_data[(int)JL_TIMING_LAST];
extern const char *jl_timing_names[(int)JL_TIMING_LAST];

struct _jl_timing_block_t { // typedef in julia.h
    jl_timing_block_t *prev;
    uint64_t total;
    uint64_t t0;
    int owner;
#ifdef JL_DEBUG_BUILD
    uint8_t running;
#endif
};

STATIC_INLINE void _jl_timing_block_stop(jl_timing_block_t *block, uint64_t t) JL_NOTSAFEPOINT {
#ifdef JL_DEBUG_BUILD
    assert(block->running);
    block->running = 0;
#endif
    block->total += t - block->t0;
}

STATIC_INLINE void _jl_timing_block_start(jl_timing_block_t *block, uint64_t t) JL_NOTSAFEPOINT {
#ifdef JL_DEBUG_BUILD
    assert(!block->running);
    block->running = 1;
#endif
    block->t0 = t;
}

STATIC_INLINE uint64_t _jl_timing_block_init(jl_timing_block_t *block, int owner) JL_NOTSAFEPOINT {
    uint64_t t = cycleclock();
    block->owner = owner;
    block->total = 0;
#ifdef JL_DEBUG_BUILD
    block->running = 0;
#endif
    _jl_timing_block_start(block, t);
    return t;
}

STATIC_INLINE void _jl_timing_block_ctor(jl_timing_block_t *block, int owner) JL_NOTSAFEPOINT {
    uint64_t t = _jl_timing_block_init(block, owner);
    jl_task_t *ct = jl_current_task;
    jl_timing_block_t **prevp = &ct->ptls->timing_stack;
    block->prev = *prevp;
    if (block->prev)
        _jl_timing_block_stop(block->prev, t);
    *prevp = block;
}

STATIC_INLINE void _jl_timing_block_destroy(jl_timing_block_t *block) JL_NOTSAFEPOINT {
    uint64_t t = cycleclock();
    jl_task_t *ct = jl_current_task;
    _jl_timing_block_stop(block, t);
    jl_timing_data[block->owner] += block->total;
    jl_timing_block_t **pcur = &ct->ptls->timing_stack;
    assert(*pcur == block);
    *pcur = block->prev;
    if (block->prev)
        _jl_timing_block_start(block->prev, t);
}

#ifdef __cplusplus
struct jl_timing_block_cpp_t {
    jl_timing_block_t block;
    jl_timing_block_cpp_t(int owner) JL_NOTSAFEPOINT {
        _jl_timing_block_ctor(&block, owner);
    }
    ~jl_timing_block_cpp_t() JL_NOTSAFEPOINT {
        _jl_timing_block_destroy(&block);
    }
    jl_timing_block_cpp_t(const jl_timing_block_cpp_t&) = delete;
    jl_timing_block_cpp_t(const jl_timing_block_cpp_t&&) = delete;
    jl_timing_block_cpp_t& operator=(const jl_timing_block_cpp_t &) = delete;
    jl_timing_block_cpp_t& operator=(const jl_timing_block_cpp_t &&) = delete;
};
#define JL_TIMING(owner) jl_timing_block_cpp_t __timing_block(JL_TIMING_ ## owner)
#else
#define JL_TIMING(owner) \
    __attribute__((cleanup(_jl_timing_block_destroy))) \
    jl_timing_block_t __timing_block; \
    _jl_timing_block_ctor(&__timing_block, JL_TIMING_ ## owner)
#endif

#endif
#endif

#endif
