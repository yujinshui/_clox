#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "hash_table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75


/****************************************/
/****    static function declaration  ***/
/****************************************/
static Entry* findEntry(Entry* entries, int capacity,
                        ObjString* key);
static void adjustCapacity(Table* table, int capacity);



/****************************************/
/****    public function definition  ****/
/****************************************/

/**
 * 初始化哈希表。
 * @param table 指向要初始化的哈希表的指针。
 * 该函数将哈希表的计数和容量设置为0，并将条目数组设置为NULL。
 */
void initTable(Table *table)
{
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

/**
 * 释放哈希表所占用的内存，并重新初始化哈希表。
 * 此函数首先释放哈希表中的所有条目，然后调用 initTable 函数将哈希表恢复到初始状态。
 * 注意：此操作会清除哈希表中的所有数据。
 *
 * @param table 需要释放和重新初始化的哈希表指针。
 */
void freeTable(Table *table)
{
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}

/**
 * 向哈希表中设置键值对。如果键已存在，则更新其值；如果键不存在，则插入新的键值对。
 * 如果哈希表的装载因子超过阈值，则会自动扩容。
 *
 * @param table 要操作的哈希表
 * @param key 要设置的键
 * @param value 要设置的值
 * @return 如果键是新添加的，则返回 true；如果键已存在并更新了其值，则返回 false。
 */
bool tableSet(Table *table, ObjString *key, Value value)
{
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD)
  {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }
  Entry* entry = findEntry(table->entries, table->capacity, key);
  bool isNewKey = entry->key == NULL;
  if (isNewKey && IS_NIL(entry->value)) table->count++;

  entry->key = key;
  entry->value = value;
  return isNewKey;
}

/**
 * 将一个表中的所有键值对添加到另一个表中。
 * 遍历源表的所有槽位，如果槽位中有键，则将其键值对设置到目标表中。
 *
 * @param from 源表，其键值对将被复制
 * @param to 目标表，将接收来自源表的键值对
 */
void tableAddAll(Table *from, Table *to)
{
  for (int i = 0; i < from->capacity; i++)
  {
    Entry *entry = &from->entries[i];
    if (entry->key != NULL) {
      tableSet(to, entry->key, entry->value);
    }
  }
}

/**
 * 从哈希表中获取指定键的值。
 * 如果哈希表为空或键不存在，则返回false。
 * 否则，将键对应的值存储在value指针中，并返回true。
 *
 * @param table 要查询的哈希表
 * @param key 要查找的键
 * @param value 存储找到的值的指针
 * @return 如果找到键并成功获取值，则返回true；否则返回false
 */
bool tableGet(Table *table, ObjString *key, Value *value)
{
  if (table->count == 0)
    return false;

  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  *value = entry->value;
  return true;
}

/**
 * 删除哈希表中的指定键值对。
 * 如果哈希表为空，则返回false。
 * 如果找到键，则将其对应的条目标记为墓碑（tombstone），表示该位置已被删除。
 *
 * @param table 要操作的哈希表
 * @param key 要删除的键
 * @return 成功删除返回true，否则返回false
 */
bool tableDelete(Table *table, ObjString *key)
{
  if (table->count == 0)
    return false;

  // Find the entry.
  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  // Place a tombstone in the entry.
  entry->key = NULL;
  entry->value = BOOL_VAL(true);
  return true;
}

/**
 * 在哈希表中查找具有指定字符序列的字符串对象。
 *
 * @param table 要查找的哈希表。
 * @param chars 要查找的字符序列。
 * @param length 字符序列的长度。
 * @param hash 字符序列的哈希值。
 * @return 如果找到匹配的字符串对象，则返回该对象；否则返回NULL。
 *
 * 该函数通过哈希值和线性探测法在哈希表中查找键。如果找到具有相同长度、哈希值和字符序列的键，则返回对应的字符串对象。
 * 如果遇到空槽或墓碑（tombstone），则停止搜索并返回NULL。
 */
ObjString *tableFindString(Table *table, const char *chars,
                           int length, uint32_t hash)
{
  if (table->count == 0)
    return NULL;

  uint32_t index = hash & (table->capacity - 1);
  for (;;) {
    Entry* entry = &table->entries[index];
    if (entry->key == NULL) {
      // Stop if we find an empty non-tombstone entry.
      if (IS_NIL(entry->value)) return NULL;
    } else if (entry->key->length == length &&
        entry->key->hash == hash &&
        memcmp(entry->key->chars, chars, length) == 0) {
      // We found it.
      return entry->key;
    }

    index = (index + 1) & (table->capacity - 1);
  }
}

/**
 * 标记哈希表中的所有条目。
 * 遍历哈希表的每个槽位，标记键和值对象，以便在垃圾回收过程中正确处理它们。
 * @param table 要标记的哈希表。
 */
void markTable(Table *table)
{
  for (int i = 0; i < table->capacity; i++)
  {
    Entry *entry = &table->entries[i];
    markObject((Obj*)entry->key);
    markValue(entry->value);
  }
}


void tableRemoveWhite(Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key != NULL && !entry->key->obj.isMarked) {
      tableDelete(table, entry->key);
    }
  }
}


/****************************************/
/****    static function definition  ****/
/****************************************/

/**
 * 在哈希表中查找具有给定键的条目。
 * 如果找到键，则返回对应的条目指针；如果未找到但找到空位（tombstone），则返回该空位指针；否则返回第一个空条目。
 * 使用线性探测法解决哈希冲突。
 *
 * @param entries 哈希表的条目数组
 * @param capacity 哈希表的容量
 * @param key 要查找的键
 * @return 找到的条目指针，或者空位指针，或者第一个空条目指针
 */
static Entry *findEntry(Entry *entries, int capacity,
                        ObjString *key)
{
  uint32_t index = key->hash & (capacity - 1);
  Entry* tombstone = NULL;
  for (;;) {
    Entry* entry = &entries[index];
    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) {
        // Empty entry.
        return tombstone != NULL ? tombstone : entry;
      } else {
        // We found a tombstone.
        if (tombstone == NULL) tombstone = entry;
      }
    } else if (entry->key == key) {
      // We found the key.
      return entry;
    }
    index = (index + 1) & (capacity - 1);
  }
}

/**
 * 调整哈希表的容量。
 * @param table 需要调整容量的哈希表。
 * @param capacity 新的容量大小。
 *
 * 该函数用于调整哈希表的容量。首先分配一个新的 Entry 数组，
 * 然后将原哈希表中的元素重新插入到新数组中，并释放原数组的内存。
 * 最后更新哈希表的容量和条目数组。
 */
static void adjustCapacity(Table *table, int capacity)
{
  Entry *entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++)
  {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }

  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key == NULL) continue;

    Entry* dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    table->count++;
  }  

  FREE_ARRAY(Entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}