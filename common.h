#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// #define DEBUG_TRACE_EXECUTION // 执行时打印字节码
// #define DEBUG_PRINT_CODE      // 打印字节码

// #define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC

#define NAN_BOXING  // NAN 装箱

#define UINT8_COUNT (UINT8_MAX + 1)

#endif  // clox_common_h