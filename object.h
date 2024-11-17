#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"
#include "chunk.h"
#include "hash_table.h"


#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)


#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)


typedef enum {
  OBJ_BOUND_METHOD, //绑定方法对象
  OBJ_INSTANCE,   //实例对象
  OBJ_CLASS,     //类对象
  OBJ_CLOSURE,    //闭包对象
  OBJ_UPVALUE,    //上值对象
  OBJ_FUNCTION,   //函数对象
  OBJ_NATIVE,     //原生函数对象
  OBJ_STRING,     //字符串对象
} ObjType;


struct Obj {
  ObjType type;
  bool isMarked;
  struct Obj* next;  //所有对象的链表
};


struct ObjString {
  Obj obj;
  int length;
  char* chars;
  uint32_t hash;
};

typedef struct {
  Obj obj;
  int arity;         //参数个数
  Chunk chunk;      //函数体
  ObjString* name;  //函数名
  int upvalueCount; //上值个数
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
  Obj obj;
  NativeFn function;
} ObjNative;


typedef struct ObjUpvalue {
  Obj obj;
  Value* location;
  Value closed;
  struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
  Obj obj;
  ObjFunction* function;
  ObjUpvalue** upvalues;
  int upvalueCount;
} ObjClosure;


typedef struct {
  Obj obj;
  ObjString* name;
  Table methods;
} ObjClass;

typedef struct {
  Obj obj;
  ObjClass* klass;
  Table fields; 
} ObjInstance;




typedef struct {
  Obj obj;
  Value receiver;
  ObjClosure* method;
} ObjBoundMethod; //类实例方法都是绑定方法


ObjBoundMethod* newBoundMethod(Value receiver,
                               ObjClosure* method);
ObjInstance* newInstance(ObjClass* klass);
ObjClass* newClass(ObjString* name);
ObjFunction* newFunction();
ObjClosure* newClosure(ObjFunction* function);
ObjUpvalue* newUpvalue(Value* slot);
ObjNative* newNative(NativeFn function);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
void printObject(Value value);



static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}




#endif