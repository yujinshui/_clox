#include <stdio.h>
#include "debug.h"
#include "chunk.h"
#include "value.h"
#include "object.h"
/****************************************/
/****    static function declaration  ***/
/****************************************/
static int simpleInstruction(const char* name, int offset);
static int constantInstruction(const char *name, Chunk *chunk, int offset);
static int constantLongInstruction(const char *name, Chunk *chunk, int offset);
static int byteInstruction(const char* name, Chunk* chunk, int offset);
static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset);
static int invokeInstruction(const char* name, Chunk* chunk, int offset);

/****************************************/
/****    public function definition  ****/
/****************************************/
/**
 * 反汇编给定代码块中的指令。
 *
 * @param chunk 指向要反汇编的代码块的指针。
 * @param name 用于标识代码块的名称。
 */
void disassembleChunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);
  for (int offset = 0; offset < chunk->count;)
  {
    offset = disassembleInstruction(chunk, offset);
  }
}


int disassembleInstruction(Chunk* chunk, int offset) {
  printf("%04d ", offset);
  if (offset > 0 &&
    getLine(chunk, offset) == getLine(chunk, offset - 1) ) {
    printf("   | ");
  } else {
    printf("%4d ",  getLine(chunk, offset));
  }

  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
    case OP_CONSTANT:
      return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_NIL:
      return simpleInstruction("OP_NIL", offset);
    case OP_TRUE:
      return simpleInstruction("OP_TRUE", offset);
    case OP_FALSE:
      return simpleInstruction("OP_FALSE", offset); 

    case OP_POP:
      return simpleInstruction("OP_POP", offset);

    case OP_GET_LOCAL:
      return byteInstruction("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
      return byteInstruction("OP_SET_LOCAL", chunk, offset);


    case OP_GET_GLOBAL:
      return constantInstruction("OP_GET_GLOBAL", chunk, offset);
    case OP_DEFINE_GLOBAL:
      return constantInstruction("OP_DEFINE_GLOBAL", chunk,
                                 offset);
    case OP_SET_GLOBAL:
      return constantInstruction("OP_SET_GLOBAL", chunk, offset);
    case OP_GET_UPVALUE:
      return byteInstruction("OP_GET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE:
      return byteInstruction("OP_SET_UPVALUE", chunk, offset);
    case OP_GET_PROPERTY:
      return constantInstruction("OP_GET_PROPERTY", chunk, offset);
    case OP_SET_PROPERTY:
      return constantInstruction("OP_SET_PROPERTY", chunk, offset);
    case OP_GET_SUPER:
      return constantInstruction("OP_GET_SUPER", chunk, offset);

    case OP_EQUAL:
      return simpleInstruction("OP_EQUAL", offset);
    case OP_NOT_EQUAL:
      return simpleInstruction("OP_NOT_EQUAL", offset);
    case OP_GREATER:
          return simpleInstruction("OP_GREATER", offset);
    case OP_GREATER_EQUAL:
          return simpleInstruction("OP_GREATER_EQUAL", offset);
    case OP_LESS:
          return simpleInstruction("OP_LESS", offset);
    case OP_LESS_EQUAL:
          return simpleInstruction("OP_LESS_EQUAL", offset);
    case OP_TERNARY:
      return simpleInstruction("OP_TERNARY", offset);
    case OP_ADD:
      return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT:
          return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
          return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
          return simpleInstruction("OP_DIVIDE", offset);    
    case OP_NOT:
      return simpleInstruction("OP_NOT", offset);  
    case OP_NEGATE:
      return simpleInstruction("OP_NEGATE", offset);
    
    case OP_PRINT:
      return simpleInstruction("OP_PRINT", offset);

    case OP_JUMP:
      return jumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_JUMP_IF_FALSE:
      return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
    case OP_LOOP:
      return jumpInstruction("OP_LOOP", -1, chunk, offset);

    case OP_CALL:
      return byteInstruction("OP_CALL", chunk, offset);
    case OP_INVOKE:
      return invokeInstruction("OP_INVOKE", chunk, offset);
    case OP_SUPER_INVOKE:
      return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
    case OP_CLOSURE: {
      offset++;
      uint8_t constantIndex = chunk->code[offset++];
      printf("%-16s %4d ", "OP_CLOSURE", constantIndex);
      printValue(chunk->constants.values[constantIndex]);
      printf("\n");

      ObjFunction* function = AS_FUNCTION(
          chunk->constants.values[constantIndex]);
      for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf("%04d      |                     %s %d\n",
               offset - 2, isLocal ? "local" : "upvalue", index);
      }


      return offset;
    }
    case OP_CLOSE_UPVALUE:
      return simpleInstruction("OP_CLOSE_UPVALUE", offset);
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);
    case OP_CLASS:
      return constantInstruction("OP_CLASS", chunk, offset);
    case OP_INHERIT:
      return simpleInstruction("OP_INHERIT", offset);
    case OP_METHOD:
      return constantInstruction("OP_METHOD", chunk, offset);
    default:
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}



/****************************************/
/****    static function definition  ****/
/****************************************/
/**
 * @brief 打印指令名称并返回偏移量加一的结果。
 *
 * 该函数用于处理简单的指令，接收指令名称和当前偏移量作为参数，
 * 打印指令名称，并返回更新后的偏移量。
 *
 * @param name 指令的名称。
 * @param offset 当前的偏移量。
 * @return 更新后的偏移量。
 */
static int simpleInstruction(const char *name, int offset) {
  // opcocde
  printf("%s\n", name);
  return offset + 1;
}

/**
 * 打印常量指令的信息。
 *
 * @param name 指令名称。
 * @param chunk 包含指令的代码块。
 * @param offset 指令在代码块中的偏移量。
 * @return 返回下一个指令的偏移量。
 */
static int constantInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t constantIndex = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constantIndex);
  printValue(VALUE_AT(chunk->constants, constantIndex));
  printf("'\n");
  return offset + 2;
}


static int constantLongInstruction(const char *name, Chunk *chunk, int offset) {
  uint32_t constantIndex = (uint32_t)GET_THREE_BYTE(chunk, offset + 1);
  printf("%-16s %4d '", name, constantIndex);
  printValue(VALUE_AT(chunk->constants, constantIndex));
  printf("'\n");
  return offset + 4;
}

/**
 * 打印字节指令的名称和局部变量的槽位值。
 *
 * @param name 指令的名称。
 * @param chunk 包含指令的代码块。
 * @param offset 指令在代码块中的偏移量。
 * @return 返回下一个指令的偏移量。
 */
static int byteInstruction(const char *name, Chunk *chunk,
                           int offset){
  uint8_t slot = chunk->code[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}


static int jumpInstruction(const char* name, int sign,
                           Chunk* chunk, int offset) {
  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
  jump |= chunk->code[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset,
         offset + 3 + sign * jump);
  return offset + 3;
}

static int invokeInstruction(const char* name, Chunk* chunk,
                                int offset) {
  //opcode name_index arg_count                                  
  uint8_t constant = chunk->code[offset + 1];
  uint8_t argCount = chunk->code[offset + 2];
  printf("%-16s (%d args) %4d '", name, argCount, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 3;
}