#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include "common.h"
#include "vm.h"
#include "debug.h"
#include "memory.h"
#include "value.h"
#include "compiler.h"
#include "object.h"
#include "string.h"

VM vm; 

/****************************************/
/****    static function declaration  ***/
/****************************************/
static InterpretResult run();
static void resetStack();
static Value peek(int distance);
static void runtimeError(const char* format, ...);
static bool isFalsey(Value value);
static void concatenate();
static bool call(ObjClosure* closure, int argCount);
static bool callValue(Value callee, int argCount);
static void defineNative(const char* name, NativeFn function);
static ObjUpvalue* captureUpvalue(Value* local);
static void closeUpvalues(Value* last);
static void defineMethod(ObjString* name);
static bool bindMethod(ObjClass* klass, ObjString* name);
static bool invoke(ObjString* name, int argCount);
static bool invokeFromClass(ObjClass* klass, ObjString* name,
                            int argCount);

static Value clockNative(int argCount, Value* args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

/****************************************/
/****    public function definition  ****/
/****************************************/
void initVM() {
  initializeMemory();
  //GC
  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;
  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;


  initTable(&vm.strings);
  initTable(&vm.globals);
  vm.objects = NULL;
  vm.stackCapacity = 256;
  vm.stack = GROW_ARRAY(Value, NULL, 0, vm.stackCapacity);
  resetStack();
  //防止运行GC 标记initString时，指针错误指向
  vm.initString = NULL;
  vm.initString = copyString("init", 4);
  defineNative("clock", clockNative);
}

void freeVM() {
  FREE_ARRAY(Value, vm.stack, vm.stackCapacity);
  freeTable(&vm.strings);
  freeTable(&vm.globals);
  vm.initString = NULL;
  freeObjects();
  freeMemory();
}

InterpretResult interpret(const char* source) {
  ObjFunction* function = compile(source);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;
  push(OBJ_VAL(function));
  ObjClosure* closure = newClosure(function);
  pop();
  push(OBJ_VAL(closure));
  call(closure, 0);
  return run();
}

void push(Value value) {
  /* 进行栈扩容操作 */
  //TODO: 在进行栈扩容操作时，需要考虑函数调用返回位置的保存
  if (vm.stackTop == vm.stack + vm.stackCapacity) {
    int oldCapacity = vm.stackCapacity;
    vm.stackCapacity = GROW_CAPACITY(vm.stackCapacity);
    vm.stack = GROW_ARRAY(Value, vm.stack, oldCapacity, vm.stackCapacity);
    vm.stackTop = vm.stack + oldCapacity;
  }
  *vm.stackTop = value;
  vm.stackTop++;
}


Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}




/****************************************/
/****    static function definition  ****/
/****************************************/
static InterpretResult run() {
   CallFrame* frame = &vm.frames[vm.frameCount - 1];


#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
        (frame->ip+= 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() \
        (frame->closure->function->chunk.constants.values[READ_BYTE()])

#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
    do { \
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
        runtimeError("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = AS_NUMBER(pop()); \
      double a = AS_NUMBER(pop()); \
      push(valueType(a op b)); \
    } while (false)
#ifndef NAN_BOXING
#define NEGATE(offset)  \
    do {                \
        if (!IS_NUMBER(peek(0))) {    \
          runtimeError("Operand must be a number.");  \
          return INTERPRET_RUNTIME_ERROR; \
        } \
        AS_NUMBER(vm.stackTop[offset]) = -AS_NUMBER(vm.stackTop[offset]);\
    } while(0)
#else 
#define NEGATE(offset)  \
    do {                \
        if (!IS_NUMBER(peek(0))) {    \
          runtimeError("Operand must be a number.");  \
          return INTERPRET_RUNTIME_ERROR; \
        } \
        vm.stackTop[offset] = -AS_NUMBER(vm.stackTop[offset]);\
    } while(0)
#endif // NAN_BOXING



  for (;;) {
  #ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(&frame->closure->function->chunk,
        (int)(frame->ip - frame->closure->function->chunk.code));
  #endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }
      case OP_NIL: push(NIL_VAL); break;
      case OP_TRUE: push(BOOL_VAL(true)); break;
      case OP_FALSE: push(BOOL_VAL(false)); break;
    
      case OP_POP: pop(); break;

      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }

      case OP_GET_GLOBAL: {
        ObjString* name = READ_STRING();
        Value value;
        if (!tableGet(&vm.globals, name, &value)) {
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        ObjString* name = READ_STRING();
        tableSet(&vm.globals, name, peek(0)); //先将变量放在栈上，防止后面GC回收
        pop();
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString* name = READ_STRING();
        if (tableSet(&vm.globals, name, peek(0))) {
          //如果全局变量不存在，先删除全局变量，再报错
          tableDelete(&vm.globals, name); 
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }
      case OP_GET_PROPERTY: {
        if (!IS_INSTANCE(peek(0))) {
          runtimeError("Only instances have properties.");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjInstance* instance = AS_INSTANCE(peek(0));
        ObjString* name = READ_STRING();

        Value value;
        if (tableGet(&instance->fields, name, &value)) {
          pop(); // Instance.
          push(value);
          break;
        }

        if (!bindMethod(instance->klass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }

        break;
      }
      case OP_SET_PROPERTY: {
        if (!IS_INSTANCE(peek(1))) {
          runtimeError("Only instances have fields.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjInstance* instance = AS_INSTANCE(peek(1));
        tableSet(&instance->fields, READ_STRING(), peek(0));
        Value value = pop();
        pop();
        push(value);
        break;
      }
      case OP_GET_SUPER: {
        ObjString* name = READ_STRING();
        ObjClass* superclass = AS_CLASS(pop());

        if (!bindMethod(superclass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }

      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OP_NOT_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(!valuesEqual(a, b)));
        break;
      }
      case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
      case OP_GREATER_EQUAL: BINARY_OP(BOOL_VAL, >=); break;
      case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
      case OP_LESS_EQUAL: BINARY_OP(BOOL_VAL, <=); break;

      case OP_ADD:      {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          double b = AS_NUMBER(pop());
          double a = AS_NUMBER(pop());
          push(NUMBER_VAL(a + b));
        } else {
          runtimeError(
              "Operands must be two numbers or two strings.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break; 
      }
      case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
      case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;

      
      case OP_NOT:
        push(BOOL_VAL(isFalsey(pop())));
        break;
      case OP_NEGATE:   NEGATE(-1); break;
      case OP_PRINT: {
        printValue(pop());
        printf("\n");
        break;
      } 

      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }    
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(0))) frame->ip += offset;
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        int argCount = READ_BYTE();
        if (!callValue(peek(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];  
        break;
      }
      case OP_INVOKE: {
        ObjString* method = READ_STRING();
        int argCount = READ_BYTE();
        if (!invoke(method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_SUPER_INVOKE: {
        ObjString* method = READ_STRING();
        int argCount = READ_BYTE();
        ObjClass* superclass = AS_CLASS(pop());
        if (!invokeFromClass(superclass, method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }

      case OP_CLOSURE: {
        ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
        ObjClosure* closure = newClosure(function);
        push(OBJ_VAL(closure));
        for (int i = 0; i < closure->upvalueCount; i++) {
          uint8_t isLocal = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (isLocal) {
            closure->upvalues[i] =
              captureUpvalue(frame->slots + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        break;
      }
      case OP_CLOSE_UPVALUE:
        closeUpvalues(vm.stackTop - 1);
        pop();
        break;
      case OP_RETURN: {
        Value result = pop();
        vm.frameCount--;
        closeUpvalues(frame->slots);
        if (vm.frameCount == 0) {
          pop();
          return INTERPRET_OK;
        }

        vm.stackTop = frame->slots;
        push(result);
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_CLASS:
        push(OBJ_VAL(newClass(READ_STRING())));
        break;
      case OP_INHERIT: {
        Value superclass = peek(1);
        if (!IS_CLASS(superclass)) {
          runtimeError("Superclass must be a class.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjClass* subclass = AS_CLASS(peek(0));
        tableAddAll(&AS_CLASS(superclass)->methods,
                    &subclass->methods);
        pop(); // Subclass.
        break;
      }
      case OP_METHOD:
        defineMethod(READ_STRING());
        break;
    }
  }

#undef NEGATE
#undef BINARY_OP
#undef READ_STRING
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_BYTE
}

/**
 * 重置虚拟机栈顶指针到栈底，清空栈中的所有元素。
 * 这个函数用于在需要清空栈时调用，例如在执行新的函数调用前。
 */
static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}

/**
 * @brief 获取虚拟机栈顶指定距离的值
 *
 * peek 函数用于获取虚拟机栈顶指定距离的值，而不改变栈指针。
 *
 * @param distance 距离栈顶的距离，距离为正数表示向下查找，负数表示向上查找
 * @return Value 返回指定距离处的值
 */
static Value peek(int distance) {
  return vm.stackTop[-1 - distance];
}

static void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);



   for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
    ObjFunction* function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    int line = getLine(&function->chunk, instruction);
    fprintf(stderr, "[line %d] in ", line);
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  resetStack();
}

/**
 * 判断给定的值是否为假值。
 * 假值包括 nil 和 false。
 * 想当与逻辑取反。
 * 
 * @param value 需要判断的值
 * @return 如果值为假值则返回 true，否则返回 false
 */
static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}


static void concatenate() {
  ObjString* b = AS_STRING(peek(0));
  ObjString* a = AS_STRING(peek(1));

  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString* result = takeString(chars, length);
  pop();
  pop();
  push(OBJ_VAL(result));
}


static bool call(ObjClosure* closure, int argCount) {
  
  
  if (argCount !=  closure->function->arity) {
    runtimeError("Expected %d arguments but got %d.",
         closure->function->arity, argCount);
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame* frame = &vm.frames[vm.frameCount++];

  frame->closure = closure;
  frame->ip = closure->function->chunk.code;

  frame->slots = vm.stackTop - argCount - 1;
  return true;
}


static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_BOUND_METHOD: {  //绑定方法
        ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
        vm.stackTop[-argCount - 1] = bound->receiver;
        return call(bound->method, argCount);
      }
      case OBJ_CLASS: {
        ObjClass* klass = AS_CLASS(callee);
        vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
        Value initializer;
        if (tableGet(&klass->methods, vm.initString, &initializer)) {
            return call(AS_CLOSURE(initializer), argCount);
        } else if (argCount != 0) {
          runtimeError("Expected 0 arguments but got %d.",
                    argCount);
          return false;
        }
      }
      case OBJ_CLOSURE:
        return call(AS_CLOSURE(callee), argCount);
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(argCount, vm.stackTop - argCount);
        vm.stackTop -= argCount + 1;
        push(result);
        return true;
      }
      default:
        break; // Non-callable object type.
    }
  }
  runtimeError("Can only call functions and classes.");
  return false;
}



static void defineNative(const char* name, NativeFn function) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

static ObjUpvalue* captureUpvalue(Value* local) {
  ObjUpvalue* prevUpvalue = NULL;
  ObjUpvalue* upvalue = vm.openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  ObjUpvalue* createdUpvalue = newUpvalue(local);
  createdUpvalue->next = upvalue;

  if (prevUpvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }
  return createdUpvalue;
}


static void closeUpvalues(Value* last) {
  while (vm.openUpvalues != NULL &&
         vm.openUpvalues->location >= last) {
    ObjUpvalue* upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}

/**
 * 定义一个方法到类中。
 * @param name 方法名，类型为 ObjString*。
 * @brief 该方法将栈顶的值作为方法定义到指定类的方法表中。
 * @note 栈顶第二个元素应为类对象，第一个元素为方法值。
 */
static void defineMethod(ObjString *name)
{
  Value method = peek(0);
  ObjClass *klass = AS_CLASS(peek(1));
  tableSet(&klass->methods, name, method);
  pop();
}

static bool bindMethod(ObjClass* klass, ObjString* name) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimeError("Undefined property '%s'.", name->chars);
    return false;
  }

  ObjBoundMethod* bound = newBoundMethod(peek(0),
                                         AS_CLOSURE(method));
  pop();
  push(OBJ_VAL(bound));
  return true;
}

static bool invokeFromClass(ObjClass* klass, ObjString* name,
                            int argCount) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimeError("Undefined property '%s'.", name->chars);
    return false;
  }
  return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString* name, int argCount) {
  Value receiver = peek(argCount);
  if (!IS_INSTANCE(receiver)) {
    runtimeError("Only instances have methods.");
    return false;
  }
  ObjInstance* instance = AS_INSTANCE(receiver);

  Value value;
  if (tableGet(&instance->fields, name, &value)) {
      vm.stackTop[-argCount - 1] = value;
      return callValue(value, argCount);
  }

  return invokeFromClass(instance->klass, name, argCount);
}