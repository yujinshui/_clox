#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "object.h"
#include "vm.h"

/****************************************/
/********    macro definition  **********/
/****************************************/
/* 堆内存大小 */
#define MEMORY_HEAP_SIZE (1024 * 1024 * 1024)

/* 申请内存 */
#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

/* 动态数组容量扩容 */
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)


/* 动态数组空间扩容 */
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))
           

/* 动态数组空间释放 */
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

/* 释放指向的空间 */
#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)


void initializeMemory();
void freeMemory();
void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void* clox_malloc(size_t size);
void* clox_realloc(void* ptr, size_t size);
void clox_free(void* pointer);
void freeObjects();


void collectGarbage();
void markValue(Value value);
void markObject(Obj* object);

#endif