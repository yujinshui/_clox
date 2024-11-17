#ifndef clox_value_h
#define clox_value_h

#include "common.h"


typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING
#include <string.h>
#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN     ((uint64_t)0x7ffc000000000000)
#define TAG_NIL   1 // 01.
#define TAG_FALSE 2 // 10.
#define TAG_TRUE  3 // 11.



typedef uint64_t Value;

#define AS_NUMBER(value)    valueToNum(value)
#define AS_BOOL(value)      ((value) == TRUE_VAL)
#define AS_OBJ(value) \
    ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))


#define NUMBER_VAL(num) numToValue(num)
#define FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL         ((Value)(uint64_t)(QNAN | TAG_NIL))
#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)
#define OBJ_VAL(obj) \
    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))


#define IS_NIL(value)       ((value) == NIL_VAL)
#define IS_NUMBER(value)    (((value) & QNAN) != QNAN)
#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL)
#define IS_OBJ(value) \
    (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

static inline Value numToValue(double num) {
  Value value;
  //期待编译器将double类型强制转换为uint64_t类型
  //而不是通过memcpy函数
  memcpy(&value, &num, sizeof(double));
  return value;
}

static inline double valueToNum(Value value) {
  double num;
  memcpy(&num, &value, sizeof(Value));
  return num;
}

#else
typedef enum {
  VAL_BOOL,
  VAL_NIL, 
  VAL_NUMBER,
  VAL_OBJ
} ValueType;


typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Obj* obj;
  } as; 
} Value;






#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})


#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)
#define AS_OBJ(value)     ((value).as.obj)


#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)    
#define IS_OBJ(value)     ((value).type == VAL_OBJ)



#endif // NAN_BOXING

/****************************************/
/********    macro definition  **********/
/****************************************/
/* 获取当前数组中元素的数量 */
#define VALUE_COUNT(ValueArray) (ValueArray.count)
/* 获取当前数组中指定索引处的元素 */
#define VALUE_AT(ValueArray, index) (ValueArray.values[index])

typedef struct {
  int capacity;     //数组的容量
  int count;        //当前数组中元素的数量
  Value* values;  
} ValueArray;



void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);
bool valuesEqual(Value a, Value b);

#endif