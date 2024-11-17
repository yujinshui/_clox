#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

/****************************************/
/********    macro definition  **********/
/****************************************/

// 获取三个字节的值
#define CONSTANT_LONG_MAX (0x00ffffff)

#define GET_THREE_BYTE(chunk, offset) (chunk->code[offset] << 16 | chunk->code[offset + 1] << 8 | chunk->code[offset + 2])

//操作码
typedef enum {
  OP_CONSTANT, //从常量池中获取一个常量， 包含一个操作数
  // OP_CONSTANT_LONG, //从常量池中获取一个常量， 包含三个操作数
 
  OP_NIL,     //nil
  OP_TRUE,    //true
  OP_FALSE,   //false

  OP_POP,


  OP_GET_LOCAL,
  OP_SET_LOCAL,  
  OP_GET_GLOBAL,
  OP_DEFINE_GLOBAL, //定义一个全局变量
  OP_SET_GLOBAL,
  OP_GET_UPVALUE, //获取上值
  OP_SET_UPVALUE, //设置上值
  OP_GET_PROPERTY,
  OP_SET_PROPERTY,
  OP_GET_SUPER,

  OP_EQUAL,     //==
  OP_NOT_EQUAL,     //!=
  OP_GREATER,   //>
  OP_GREATER_EQUAL, //>=
  OP_LESS,      //<
  OP_LESS_EQUAL,    //<=

  OP_ADD,   //  +
  OP_SUBTRACT, // -
  OP_MULTIPLY,  // *
  OP_DIVIDE,    // /
  
  OP_NOT,    //取反
  OP_NEGATE, //取负

  OP_PRINT,


  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_LOOP,

  OP_CALL,
  OP_INVOKE, // 这是一个复杂指令，帮助调用类方法
  OP_SUPER_INVOKE, //这是一个复杂指令，调用父类方法
  OP_CLOSURE,
  OP_CLOSE_UPVALUE,
  
  OP_RETURN, //返回
   
  OP_CLASS, //类
  OP_INHERIT, //继承
  OP_METHOD //类方法
} OpCode;


//代码块
typedef struct {
  //动态数组  
  int count;      //代码块中指令的数量
  int capacity;   //代码块中指令的容量
  uint8_t* code;  //代码块中指令的数组

  ValueArray constants; //代码块中常量的数组

  //利用游程长度编码
  uint8_t* rle;               
  int rleIndex;          
  int rleCapacity;    

} Chunk;


void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
void freeChunk(Chunk* chunk);
int addConstant(Chunk* chunk, Value value);
int getLine(Chunk* chunk, int offset);

#endif // clox_chunk_h