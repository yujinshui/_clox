/*
 * @Author: yujinshui 1833703858@qq.com
 * @Date: 2024-11-03 16:38:47
 * @LastEditors: yujinshui 1833703858@qq.com
 * @LastEditTime: 2024-11-03 21:36:16
 * @FilePath: /_clox/vm.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef clox_vm_h
#define clox_vm_h
#include "chunk.h"
#include "hash_table.h"
#include "object.h"

/****************************************/
/********    macro definition  **********/
/****************************************/

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)


typedef struct {
  ObjClosure* closure;
  uint8_t* ip;
  Value* slots;
} CallFrame;


typedef struct {
  CallFrame frames[FRAMES_MAX]; //调用帧
  int frameCount;

  Chunk* chunk;
  uint8_t* ip;               //指令指针

  //动态栈
  Value *stack;       //栈
  Value* stackTop;    //栈顶
  int stackCapacity;  //栈的容量

  Obj* objects; //所有对象的链表
  Table strings; //字符串池
  
  Table globals;  //全局变量表

  ObjString* initString; // 类初始化调用对象

  ObjUpvalue* openUpvalues; //所有的上值
  //处理GC
  int grayCount;
  int grayCapacity;
  Obj** grayStack;
  size_t bytesAllocated;
  size_t nextGC;
} VM;

typedef enum {
  INTERPRET_OK,             //正常
  INTERPRET_COMPILE_ERROR,  //编译错误
  INTERPRET_RUNTIME_ERROR   //运行时错误
} InterpretResult;

extern VM vm;


void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();

#endif