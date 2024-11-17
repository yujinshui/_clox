#include <stdlib.h>
#include "chunk.h"
#include "vm.h"
#include "memory.h"
/****************************************/
/****    static function declaration  ***/
/****************************************/
static void addLine(Chunk *chunk, int line);


/****************************************/
/****    public function definition  ****/
/****************************************/

void initChunk(Chunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->rle = NULL;
  chunk->rleIndex = 0;
  chunk->rleCapacity = 0;

  initValueArray(&chunk->constants);
}


/**
 * 向当前Chunk写入一个字节，并更新行号信息。
 * 如果当前Chunk的容量不足以容纳新的字节，则会自动扩容。
 *
 * @param chunk 要写入的Chunk指针
 * @param byte 要写入的字节
 * @param line 字节对应的源代码行号
 */
void writeChunk(Chunk *chunk, uint8_t byte, int line) {
  
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
  }
  chunk->code[chunk->count] = byte;
  chunk->count++;
  addLine(chunk, line);
}

/**
 * @brief 释放 Chunk 结构体占用的内存，并重新初始化该结构体。
 *
 * 该函数首先释放 Chunk 结构体中的代码数组所占用的内存，然后调用 initChunk 函数重新初始化该 Chunk 结构体。
 * 这使得同一个 Chunk 结构体可以被重新使用，而不需要每次都分配新的内存。
 *
 * @param chunk 需要被释放和重新初始化的 Chunk 结构体指针。
 */
void freeChunk(Chunk *chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(uint8_t, chunk->rle, chunk->rleCapacity);
  freeValueArray(&chunk->constants);
  initChunk(chunk);
}


/**
 * 向给定的Chunk中添加一个常量值，并返回该常量在常量表中的索引。
 *
 * @param chunk 要添加常量的Chunk指针
 * @param value 要添加的常量值
 * @return 返回新添加的常量在常量表中的索引
 */
int addConstant(Chunk *chunk, Value value) {
  push(value);
  writeValueArray(&chunk->constants, value);
  pop();
  return VALUE_COUNT(chunk->constants) - 1;
}

/**
 * 从给定的Chunk中获取指定偏移量的字节码所在行号。
 *
 * @param chunk 指向要查询的Chunk的指针。
 * @param offset 要获取的字节码指令的偏移量。
 * @return 如果找到，则返回指定偏移量的字节码行号；否则返回-1。
 */
int getLine(Chunk *chunk, int offset) {
  int proOpCodeCount = 0;
  int i = 0;
  while (i <= chunk->rleIndex) {
    proOpCodeCount += chunk->rle[i];
    if (proOpCodeCount > offset) {
      return chunk->rle[i + 1];
    }
    i += 2;
  }
  return -1;
}

/****************************************/
/****    static function definition  ****/
/****************************************/

/**
 * 向Chunk中添加一行代码，并记录行号。如果连续的行号相同，则增加计数器，否则添加新的行号记录。
 * @param chunk 要添加行号的Chunk结构体指针
 * @param line 要添加的行号
 */
static void addLine(Chunk *chunk, int line) {
  if (chunk->rleIndex + 2 > chunk->rleCapacity) {
    int oldCapacity = chunk->rleCapacity;
    chunk->rleCapacity = GROW_CAPACITY(oldCapacity);
    chunk->rle = GROW_ARRAY(uint8_t, chunk->rle, oldCapacity, chunk->rleCapacity);
  }

  if(chunk->rle[chunk->rleIndex] == 0) {
    chunk->rle[chunk->rleIndex] = 1;
    chunk->rle[chunk->rleIndex + 1] = line;
  } else if (chunk->rle[chunk->rleIndex + 1] == line) {
    chunk->rle[chunk->rleIndex] += 1;
  } else {
    chunk->rleIndex += 2;
    chunk->rle[chunk->rleIndex] = 1;
    chunk->rle[chunk->rleIndex + 1] = line;
  }
}