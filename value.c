#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "value.h"
#include "object.h"

/****************************************/
/****    public function definition  ****/
/****************************************/
/**
 * @brief 初始化 ValueArray 结构体
 *
 * 该函数用于初始化一个 ValueArray 结构体，将其 values 指针设置为 NULL，
 * 容量 capacity 和元素计数 count 都设置为 0。
 *
 * @param array 需要初始化的 ValueArray 结构体指针
 */
void initValueArray(ValueArray *array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

/**
 * 将给定的值写入值数组。如果数组容量不足，会自动扩容。
 * @param array 指向 ValueArray 结构体的指针，表示要写入的数组。
 * @param value 要写入数组的值。
 */
void writeValueArray(ValueArray *array, Value value) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values = GROW_ARRAY(Value, array->values,
                               oldCapacity, array->capacity);
  }
  array->values[array->count] = value;
  array->count++;
}


/**
 * 释放 ValueArray 结构体中的值数组，并重新初始化 ValueArray。
 * 该函数用于释放分配给值数组的内存，并将 ValueArray 结构体重置为初始状态。
 *
 * @param array 要释放和重置的 ValueArray 结构体指针。
 */
void freeValueArray(ValueArray *array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  initValueArray(array);
}


void printValue(Value value) {
#ifdef NAN_BOXING
  if (IS_BOOL(value)) {
    printf(AS_BOOL(value) ? "true" : "false");
  } else if (IS_NIL(value)) {
    printf("nil");
  } else if (IS_NUMBER(value)) {
    printf("%g", AS_NUMBER(value));
  } else if (IS_OBJ(value)) {
    printObject(value);
  }
#else
  switch (value.type) {
    case VAL_BOOL:
      printf(AS_BOOL(value) ? "true" : "false");
      break;
    case VAL_NIL: printf("nil"); break;
    case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
    case VAL_OBJ: printObject(value); break;
  }
#endif // NAN_BOXING
}  


bool valuesEqual(Value a, Value b) {
#ifdef NAN_BOXING
  //NAN != NAN
  if (IS_NUMBER(a) && IS_NUMBER(b)) {
        return AS_NUMBER(a) == AS_NUMBER(b);
  }
  return a == b;
#else
  if (a.type != b.type) return false;
  switch (a.type) {
    case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL:    return true;
    case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_OBJ:    return AS_OBJ(a) == AS_OBJ(b);
    default:         return false; // Unreachable.
  }
#endif // NAN_BOXING
}