#include <stdlib.h>

#include "memory.h"
#include "tlsf/tlsf.h"
#include "compiler.h"

#include <stdio.h>
#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

/****************************************/
/**********  gloal variables   **********/
/****************************************/
tlsf_t *memoryPool = NULL;

#define GC_HEAP_GROW_FACTOR 2

/****************************************/
/****    static function declaration  ***/
/****************************************/
static void freeObject(Obj* object);
static void markRoots();
static void traceReferences();
static void blackenObject(Obj* object);
static void markArray(ValueArray* array);
static void sweep();

/****************************************/
/****    public function definition  ****/
/****************************************/

/**
 * @brief 初始化内存管理模块
 *
 * 该函数负责分配一块内存，并使用这块内存创建一个内存池。
 * 如果内存分配失败或内存池创建失败，程序将退出。
 *
 * @note 该函数应在程序启动时调用，以确保内存管理模块正确初始化。
 */
void initializeMemory() {
  void *memory = malloc(MEMORY_HEAP_SIZE);
  if (memory == NULL)
    exit(1);
  memoryPool = tlsf_create_with_pool(memory, MEMORY_HEAP_SIZE);
  if (memoryPool == NULL) exit(1);
}

void freeMemory() {
  free(memoryPool);
}


/**
 * @brief 重新分配内存块的大小。
 *
 * 如果 newSize 为 0，则释放 pointer 指向的内存并返回 NULL。
 * 否则，使用 realloc 尝试调整 pointer 指向的内存块大小。
 * 如果 realloc 失败，则退出程序。
 *
 * @param pointer 指向需要重新分配内存的原始内存块的指针。
 * @param oldSize 原始内存块的大小。
 * @param newSize 新的内存块大小。
 * @return void* 重新分配后的内存块的指针，如果失败则返回 NULL。
 */
void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  vm.bytesAllocated += newSize - oldSize;
  if (newSize > oldSize) {
    #ifdef DEBUG_STRESS_GC
      collectGarbage();
    #endif
  }

  if (vm.bytesAllocated > vm.nextGC) {
    collectGarbage();
  }

  if (newSize == 0) {
    tlsf_free(memoryPool, pointer);
    return NULL;
  }
  void* result = tlsf_realloc(memoryPool, pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}


void* clox_malloc(size_t size) {
  return tlsf_malloc(memoryPool, size);
}

void* clox_realloc(void* ptr, size_t size) {
  return tlsf_realloc(memoryPool, ptr, size);
}


void clox_free(void* pointer) {
  tlsf_free(memoryPool, pointer);
}

/**
 * 释放所有对象及其相关资源。
 * 该函数遍历对象链表，逐个释放每个对象，并清空灰色栈。
 */
void freeObjects()
{
  Obj *object = vm.objects;
  while (object != NULL)
  {
    Obj* next = object->next;
    freeObject(object);
    object = next;
  }
  clox_free(vm.grayStack);
}

/**
 * 执行垃圾回收的函数。
 * 该函数首先标记所有根对象，然后追踪它们的引用，移除白色对象，并进行清理。
 * 最后，根据当前分配的字节数更新下一次垃圾回收的阈值。
 * 如果定义了 DEBUG_LOG_GC，还会在垃圾回收前后打印日志信息。
 */
void collectGarbage()
{
#ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
  size_t before = vm.bytesAllocated;
#endif

  markRoots();
  traceReferences();
  tableRemoveWhite(&vm.strings);
  sweep();

  vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

  #ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
         before - vm.bytesAllocated, before, vm.bytesAllocated,
         vm.nextGC);
  #endif
}

void markValue(Value value) {
  if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

void markObject(Obj* object) {
  if (object == NULL) return;
  if (object->isMarked) return;

#ifdef DEBUG_LOG_GC
  printf("%p mark ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif
  object->isMarked = true;

  if (vm.grayCapacity < vm.grayCount + 1) {
    vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
    vm.grayStack = (Obj**)clox_realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);
  }
  vm.grayStack[vm.grayCount++] = object;

}

/****************************************/
/****    static function definition  ****/
/****************************************/
static void freeObject(Obj* object) {
  #ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
  #endif

  switch (object->type) {
    case OBJ_BOUND_METHOD:
      FREE(ObjBoundMethod, object);
      break;
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      freeTable(&instance->fields);
      FREE(ObjInstance, object);
      break;
    }
    case OBJ_CLASS: {
      ObjClass* klass = (ObjClass*)object;
      freeTable(&klass->methods);
      FREE(ObjClass, object);
      break;
    } 
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      FREE_ARRAY(ObjUpvalue*, closure->upvalues,
                 closure->upvalueCount);
      FREE(ObjClosure, object);
      break;
    }
    case OBJ_UPVALUE:
      FREE(ObjUpvalue, object);
      break;
    case OBJ_FUNCTION: {
      //不用显式地释放函数名称
      ObjFunction* function = (ObjFunction*)object;
      freeChunk(&function->chunk);
      FREE(ObjFunction, object);
      break;
    }
    case OBJ_NATIVE:
      FREE(ObjNative, object);
      break;
    case OBJ_STRING: {
      ObjString* string = (ObjString*)object;
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ObjString, object);
      break;
    }
  }
}



static void markRoots() {
  for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
    markValue(*slot);
  }

  for (int i = 0; i < vm.frameCount; i++) {
    markObject((Obj*)vm.frames[i].closure);
  }

  for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
    markObject((Obj*)upvalue);
  }

  markTable(&vm.globals);

  markCompilerRoots();

  markObject((Obj*)vm.initString);
}



static void traceReferences() {
  while (vm.grayCount > 0) {
    Obj* object = vm.grayStack[--vm.grayCount];
    blackenObject(object);
  }
}


static void blackenObject(Obj* object) {
  #ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
  #endif
  switch (object->type) {
    case OBJ_BOUND_METHOD: {
      ObjBoundMethod* bound = (ObjBoundMethod*)object;
      markValue(bound->receiver);
      markObject((Obj*)bound->method);
      break;
    }
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      markObject((Obj*)instance->klass);
      markTable(&instance->fields);
      break;
    }
    case OBJ_CLASS: {
      ObjClass* klass = (ObjClass*)object;
      markObject((Obj*)klass->name);
      markTable(&klass->methods);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      markObject((Obj*)closure->function);
      for (int i = 0; i < closure->upvalueCount; i++) {
        markObject((Obj*)closure->upvalues[i]);
      }
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      markObject((Obj*)function->name);
      markArray(&function->chunk.constants);
      break;
    }
    case OBJ_UPVALUE:
      markValue(((ObjUpvalue*)object)->closed);
    break;
    case OBJ_NATIVE:
    case OBJ_STRING:
      break;
    
  }
}


/**
 * 标记数组中的所有值。
 *
 * @param array 要标记的 ValueArray 指针。
 * 此函数遍历数组中的每个元素，并对其调用 markValue 函数进行标记。
 */
static void markArray(ValueArray *array)
{
  for (int i = 0; i < array->count; i++)
  {
    markValue(array->values[i]);
  }
}


static void sweep() {
  Obj* previous = NULL;
  Obj* object = vm.objects;
  while (object != NULL) {
    if (object->isMarked) {
      object->isMarked = false;
      previous = object;
      object = object->next;
    } else {
      Obj* unreached = object;
      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        vm.objects = object;
      }
      freeObject(unreached);
    }
  }
}