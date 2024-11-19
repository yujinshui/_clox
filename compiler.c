#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "object.h"
#include "memory.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif



typedef void (*ParseFn)(bool canAssign);


typedef enum {
  TYPE_FUNCTION,
  TYPE_INITIALIZER,
  TYPE_METHOD,
  TYPE_SCRIPT
} FunctionType;

typedef struct {
  Token name;
  int depth;
  bool isCaptured;
} Local;


typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;


typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! -
  PREC_CALL,        // . ()
  PREC_PRIMARY
} Precedence;


typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  uint8_t index;
  bool isLocal;
} Upvalue;

typedef struct Circulation{
  struct Circulation* enclosing;
  int loopStart; //记录循环开始位置
  int _break[UINT8_COUNT];  //记录break指令位置
  int _break_count;         //break指令数量
} Circulation;

typedef struct Compiler{
  struct Compiler* enclosing;
  ObjFunction* function; //当前处理的函数
  FunctionType type;

  Local locals[UINT8_COUNT];
  int localCount;
  int scopeDepth;

  Upvalue upvalues[UINT8_COUNT];
} Compiler;

typedef struct ClassCompiler {
  struct ClassCompiler* enclosing;
  bool hasSuperclass;
} ClassCompiler;




Parser parser;
Compiler* current = NULL;
ClassCompiler* currentClass = NULL; //用于记录类，防止在顶层定义this
Circulation * currentCirculation = NULL; //用于记录循环

/****************************************/
/****    private function declaration  **/
/****************************************/
static void initCompiler(Compiler* compiler,  FunctionType type);
static void advance();
static void consume(TokenType type, const char* message);
static bool match(TokenType type);
static ObjFunction* endCompiler();

// compile expression
static void expression();
static void number(bool canAssign);
static void grouping(bool canAssign);
static void unary(bool canAssign);
static void binary(bool canAssign);
static void literal(bool canAssign);
static void string(bool canAssign);
static void variable(bool canAssign);
static void and_(bool canAssign);
static void or_(bool canAssign);
static void call(bool canAssign);
static void dot(bool canAssign);
static void this_(bool canAssign);
static void super_(bool canAssign);


// compile statement
static void declaration();
static void statement();

/* declaration  */
static void varDeclaration();
static void funDeclaration();
static void classDeclaration();

/* statement */
static void printStatement();
static void breakStatement();
static void continueStatement();
static void expressionStatement();
static void ifStatement();
static void whileStatement();
static void forStatement();
static void returnStatement();
static void beginScope();
static void endScope();
static void block();
static void function(FunctionType type);



ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     dot,   PREC_CALL},
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]          = {unary,     NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     binary,   PREC_EQUALITY},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary,   PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,     binary,   PREC_EQUALITY},
  [TOKEN_GREATER_EQUAL] = {NULL,     binary,   PREC_EQUALITY},
  [TOKEN_LESS]          = {NULL,     binary,   PREC_EQUALITY},
  [TOKEN_LESS_EQUAL]    = {NULL,     binary,   PREC_EQUALITY},
  [TOKEN_IDENTIFIER]    = {variable,     NULL,   PREC_NONE},
  [TOKEN_STRING]        = {string,     NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,     NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,     NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     or_,   PREC_OR},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {super_,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {this_,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,     NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};


/****************************************/
/****    public function definition  ****/
/****************************************/
ObjFunction*  compile(const char* source) {
  initScanner(source);
  parser.hadError = false;
  parser.panicMode = false;
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);

  advance();
  
  while (!match(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_EOF, "Expect end of expression.");
  ObjFunction* function = endCompiler();
  return parser.hadError ? NULL : function;
}


void markCompilerRoots() {
  Compiler* compiler = current;
  while (compiler != NULL) {
    markObject((Obj*)compiler->function);
    compiler = compiler->enclosing;
  }
}



/****************************************/
/****    private function definition  ***/
/****************************************/
static void initCompiler(Compiler* compiler,  FunctionType type) {
  compiler->enclosing = current;
 
  compiler->function = NULL;
  compiler->type = type;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->function = newFunction();
  current = compiler;
  if (type != TYPE_SCRIPT) {
    current->function->name = copyString(parser.previous.start,
                                         parser.previous.length);
  }
  Local* local = &current->locals[current->localCount++];
  local->depth = 0;
  local->isCaptured = false;
  if (type != TYPE_FUNCTION) {
    local->name.start = "this";
    local->name.length = 4;
  } else {
    local->name.start = "";
    local->name.length = 0;
  }

}


static void errorAt(Token* token, const char* message) {
  if (parser.panicMode) return;
    parser.panicMode = true;

  fprintf(stderr, "[line %d column %d] Error", token->line, token->column);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}


static void errorAtCurrent(const char* message) {
  errorAt(&parser.current, message);
}


static void errorAtPrevious(const char* message) {
  errorAt(&parser.previous, message);
}


/**
 * 获取当前正在编译的代码块。
 * @return 当前编译的代码块指针。
 */
static Chunk *currentChunk() {
  return &current->function->chunk;
}


/**
 * @brief 前进到下一个词法单元
 *
 * 该函数用于将解析器的当前词法单元移动到下一个。如果遇到错误类型的词法单元，
 * 则调用 errorAtCurrent 函数报告错误。
 *
 * @note 此函数为内部实现细节，不应在模块外部调用。
 */
static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR) break;

    errorAtCurrent(parser.current.start);
  }
}


/**
 * 检查当前解析器令牌的类型是否与给定类型匹配。
 * @param type 要检查的令牌类型。
 * @return 如果当前令牌类型与给定类型匹配，则返回 true，否则返回 false。
 */
static bool check(TokenType type) {
  return parser.current.type == type;
}


/**
 * 检查当前解析的标记是否与给定类型匹配，并在匹配时前进到下一个标记。
 *
 * @param type 需要匹配的标记类型。
 * @return 如果匹配成功则返回 true，否则返回 false。
 */
static bool match(TokenType type) {
  if (!check(type))
    return false;
  advance();
  return true;
}


/**
 * 消费一个指定类型的词法单元，并在当前词法单元类型与指定类型匹配时前进到下一个词法单元。
 * 如果不匹配，则在当前位置报告错误。
 *
 * @param type 需要消费的词法单元类型
 * @param message 当前词法单元类型与指定类型不匹配时的错误信息
 */
static void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}


/**
 * 向当前代码块写入一个字节，并记录该字节的行号。
 *
 * @param byte 要写入的字节
 */
static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}


static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}


/**
 * 将给定的值放入当前代码块中的常量池中，并返回该常量的索引。
 *
 * @param value 要添加到常量池的值
 * @return 添加的常量的索引，如果出错则返回 0
 */
static uint8_t makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);
  if (constant > UINT8_MAX)
  {
    errorAtPrevious("Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t)constant;
}


/**
 * @param value 要添加到常量池的值
 * 
 * 添加OP_CONSTANT OP_CONSTANT_INDEX 到字节码中。
 */
static void emitConstant(Value value) {
  emitBytes(OP_CONSTANT, makeConstant(value));
}


/**
 * 将词法单元的词素的字符串形式作为OBJ_STRING放入常量池中，并返回该常量的索引。
 *
 * @param name 词法单元
 * @return 添加的常量的索引，如果出错则返回 0
 */
static uint8_t identifierConstant(Token* name) {
  return makeConstant(OBJ_VAL(copyString(name->start,
                                         name->length)));
}


/**
 * 比较两个标识符Token是否相等。
 *
 * @param a 第一个标识符Token指针。
 * @param b 第二个标识符Token指针。
 * @return 如果两个标识符的长度相同且内容相同，则返回true，否则返回false。
 */
static bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}


/**
 * @brief 发射返回指令到字节码中。
 *
 * 该函数用于在编译过程中向字节码流中添加一个返回指令，表示当前函数的执行结束并返回到调用者。
 *
 * @note 该函数是编译器内部实现细节，不对外暴露。
 */
static void emitReturn() {
  if (current->type == TYPE_INITIALIZER) {
    emitBytes(OP_GET_LOCAL, 0);
  } else {
    emitByte(OP_NIL);
  }  
  emitByte(OP_RETURN);
}


/**
 * @brief 发射跳转指令到当前代码块。
 *
 * 此函数用于发射一个跳转指令到当前正在构建的代码块中。它首先发射指令本身，
 * 然后发射两个字节的占位符（0xff），这两个字节将在后续的代码生成过程中被替换为实际的跳转偏移量。
 *
 * @param instruction 要发射的跳转指令字节。
 * @return 返回当前代码块中的计数器减去2的值，即跳转指令的实际地址。
 */
static int emitJump(uint8_t instruction)
{
  emitByte(instruction);
  emitByte(0xff);
  emitByte(0xff);
  return currentChunk()->count - 2;
}


/**
 * @brief 修补跳转指令的偏移量
 *
 * 此函数用于计算并修补当前字节码块中的跳转指令的偏移量。
 * 它首先计算从当前字节码计数到指定偏移量的相对距离，
 * 然后将这个距离拆分为两个字节并写入到字节码数组中。
 * 如果计算出的跳转距离超过了16位无符号整数的最大值，
 * 则会报告错误。
 *
 * @param offset 要修补的跳转指令的偏移量
 */
static void patchJump(int offset)
{
  // -2 to adjust for the bytecode for the jump offset itself.
  // 这里计算的是相对偏移量
  int jump = currentChunk()->count - offset - 2;

  if (jump > UINT16_MAX) {
    errorAtPrevious("Too much code to jump over.");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}


static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);

  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX) errorAtPrevious("Loop body too large.");

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}


static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) return;
    switch (parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;

      default:
        ; // Do nothing.
    }

    advance();
  }
}


/**
 * 根据给定的 TokenType 类型获取对应的 ParseRule。
 * @param type TokenType 类型。
 * @return 返回指向对应 ParseRule 的指针。
 */
static ParseRule *getRule(TokenType type) {
  return &rules[type];
}


static void parsePrecedence(Precedence precedence) {
  advance();
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    errorAtPrevious("Expect expression.");
    return;
  }
  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    errorAtPrevious("Invalid assignment target.");
  }
}


/**
 * 添加一个新的局部变量到当前函数的局部变量列表中。
 * 如果当前函数的局部变量数量已达到上限，则报告错误并返回。
 *
 * @param name 局部变量的名称，类型为Token。
 */
static void addLocal(Token name)
{
  if (current->localCount == UINT8_COUNT)
  {
    errorAtPrevious("Too many local variables in function.");
    return;
  }
  Local* local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
  local->isCaptured = false;
}


/**
 * 该函数用于定义局部变量。
 */
static void declareVariable()
{
  if (current->scopeDepth == 0)
    return;
  Token *name = &parser.previous;
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local* local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break; 
    }

    if (identifiersEqual(name, &local->name)) {
      errorAtPrevious("Already a variable with this name in this scope.");
    }
  }

  addLocal(*name);
}


static uint32_t parseVariable(const char* errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);
  declareVariable();
  if (current->scopeDepth > 0) return 0;
  return identifierConstant(&parser.previous);
}


static void markInitialized() {
  if (current->scopeDepth == 0) return;
  current->locals[current->localCount - 1].depth =
      current->scopeDepth;
}


static void defineVariable(uint8_t global) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }
  emitBytes(OP_DEFINE_GLOBAL, global);
}


/**
 * 解析局部变量，返回其在编译器局部变量表中的索引。
 * 如果未找到匹配的局部变量，则返回-1。
 *
 * @param compiler 指向编译器实例的指针
 * @param name 要查找的局部变量的标识符
 * @return 局部变量在局部变量表中的索引，如果未找到则返回-1
 */
static int resolveLocal(Compiler* compiler, Token* name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local* local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      // 变量字节给自己赋值
      if (local->depth == -1) {
        errorAtPrevious("Can't read local variable in its own initializer.");
      }
      return i;
    }
  }

  return -1;
}


static int addUpvalue(Compiler* compiler, uint8_t index,
                      bool isLocal) {
  int upvalueCount = compiler->function->upvalueCount;
  
  for (int i = 0; i < upvalueCount; i++) {
    Upvalue* upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      return i;
    }
  }

  if (upvalueCount == UINT8_COUNT) {
    errorAtPrevious("Too many closure variables in function.");
    return 0;
  }

  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;
  return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
  if (compiler->enclosing == NULL) return -1;

  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
    compiler->enclosing->locals[local].isCaptured = true;
    return addUpvalue(compiler, (uint8_t)local, true);
  }

  int upvalue = resolveUpvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(compiler, (uint8_t)upvalue, false);
  }

  return -1;
}


static void namedVariable(Token name, bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else if ((arg = resolveUpvalue(current, &name)) != -1) {
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  } else {
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }
  //arg 对于全局变量来说是常量池索引，
  //对于局部变量来说是执行栈位置。
  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitBytes(setOp, (uint8_t)arg);
  } else {
    emitBytes(getOp, (uint8_t)arg);
  }  
}


static uint8_t argumentList() {
  uint8_t argCount = 0;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
      if (argCount == 255) {
        errorAtPrevious("Can't have more than 255 arguments.");
      }
      argCount++;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return argCount;
}


static void method() {
  consume(TOKEN_IDENTIFIER, "Expect method name.");
  uint8_t constant = identifierConstant(&parser.previous);
  FunctionType type =  TYPE_METHOD;

  if (parser.previous.length == 4 &&
      memcmp(parser.previous.start, "init", 4) == 0) {
      type = TYPE_INITIALIZER;
  }


  function(type);
  emitBytes(OP_METHOD, constant);
}

/**
 * 创建一个合成 Token。
 *
 * @param text Token 的文本内容。
 * @return 返回一个新的 Token 结构体，其 start 指向传入的文本，length 为文本的长度。
 */
static Token syntheticToken(const char *text)
{
  Token token;
  token.start = text;
  token.length = (int)strlen(text);
  return token;
}

static void expression() {
  parsePrecedence(PREC_ASSIGNMENT);
}


/**
 * 将解析器的前一个标记转换为双精度浮点数，并将其作为常量发出。
 * 此函数用于处理 Lox 语言中的数字字面量。
 */
static void number(bool canAssign) {
  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

static void grouping(bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}


static void unary(bool canAssign) {
  static const void *const unaryDisptab[] = { 
    [TOKEN_MINUS] = &&TOKEN_MINUS,
    [TOKEN_BANG] = &&TOKEN_BANG,
  };  

  TokenType operatorType = parser.previous.type;
  // Compile the operand.
  parsePrecedence(PREC_ASSIGNMENT);
  goto *unaryDisptab[operatorType];
  // Emit the operator instruction.
TOKEN_BANG:
  emitByte(OP_NOT);
  goto END;
TOKEN_MINUS:
  emitByte(OP_NEGATE);
  goto END;
END:
  return;
}


static void binary(bool canAssign) {
  static const void *const binaryDisptab[] = { 
   [TOKEN_PLUS] = &&TOKEN_PLUS,
   [TOKEN_MINUS] = &&TOKEN_MINUS,
   [TOKEN_STAR] = &&TOKEN_STAR,
   [TOKEN_SLASH] = &&TOKEN_SLASH,
   [TOKEN_BANG_EQUAL] = &&TOKEN_BANG_EQUAL,
   [TOKEN_EQUAL_EQUAL] = &&TOKEN_EQUAL_EQUAL,
   [TOKEN_GREATER] = &&TOKEN_GREATER,
   [TOKEN_GREATER_EQUAL] = &&TOKEN_GREATER_EQUAL,
   [TOKEN_LESS] = &&TOKEN_LESS,
   [TOKEN_LESS_EQUAL] = &&TOKEN_LESS_EQUAL,
  };  

  TokenType operatorType = parser.previous.type;
  ParseRule* rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1));
  goto *binaryDisptab[operatorType];
  
TOKEN_PLUS:
  emitByte(OP_ADD);
  goto END;
TOKEN_MINUS:
  emitByte(OP_SUBTRACT);
  goto END;
TOKEN_STAR:
  emitByte(OP_MULTIPLY);
  goto END;
TOKEN_SLASH:
  emitByte(OP_DIVIDE);
  goto END;
TOKEN_BANG_EQUAL:
  emitByte(OP_NOT_EQUAL);
  goto END;
TOKEN_EQUAL_EQUAL:
  emitByte(OP_EQUAL);
  goto END;
TOKEN_GREATER:
  emitByte(OP_GREATER);
  goto END;
TOKEN_GREATER_EQUAL:
  emitByte(OP_GREATER_EQUAL);
  goto END;
TOKEN_LESS:
  emitByte(OP_LESS);
  goto END;
TOKEN_LESS_EQUAL:
  emitByte(OP_LESS_EQUAL);
END:
  return;
}


static void literal(bool canAssign) {
  static const void *const literalDisptab[] = { 
    [TOKEN_FALSE] = &&TOKEN_FALSE,
    [TOKEN_NIL] = &&TOKEN_NIL,
    [TOKEN_TRUE] = &&TOKEN_TRUE,
  };  

  goto *literalDisptab[parser.previous.type];
TOKEN_FALSE:
  emitByte(OP_FALSE);
  goto END;
TOKEN_NIL:
  emitByte(OP_NIL);
  goto END;
TOKEN_TRUE:
  emitByte(OP_TRUE);
  goto END;
END:
  return;
}


static void string(bool canAssign) {
  emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,
                                  parser.previous.length - 2)));
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}


static void and_(bool canAssign) {
  int endJump = emitJump(OP_JUMP_IF_FALSE);

  emitByte(OP_POP);
  parsePrecedence(PREC_AND);

  patchJump(endJump);
}


static void or_(bool canAssign) {
  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  int endJump = emitJump(OP_JUMP);

  patchJump(elseJump);
  emitByte(OP_POP);

  parsePrecedence(PREC_OR);
  patchJump(endJump);
}


static void dot(bool canAssign) {
  consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
  uint8_t name = identifierConstant(&parser.previous);

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitBytes(OP_SET_PROPERTY, name);
  } else if (match(TOKEN_LEFT_PAREN)) {
    uint8_t argCount = argumentList();
    emitBytes(OP_INVOKE, name);
    emitByte(argCount);
  } else {
    emitBytes(OP_GET_PROPERTY, name);
  }
}

static void this_(bool canAssign) {
  if (currentClass == NULL) {
    errorAtPrevious("Can't use 'this' outside of a class.");
    return;
  }
  variable(false);
} 

static void super_(bool canAssign) {
  if (currentClass == NULL) {
    errorAtPrevious("Can't use 'super' outside of a class.");
  } else if (!currentClass->hasSuperclass) {
    errorAtPrevious("Can't use 'super' in a class with no superclass.");
  }

  consume(TOKEN_DOT, "Expect '.' after 'super'.");
  consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
  uint8_t name = identifierConstant(&parser.previous); //方法名称
  
  namedVariable(syntheticToken("this"), false); //绑定方法的this
  
   if (match(TOKEN_LEFT_PAREN)) {
    uint8_t argCount = argumentList();
    namedVariable(syntheticToken("super"), false);
    emitBytes(OP_SUPER_INVOKE, name);
    emitByte(argCount);
  } else {
    namedVariable(syntheticToken("super"), false); //super类 
    emitBytes(OP_GET_SUPER, name);
  }
  
}

static void call(bool canAssign) {
  uint8_t argCount = argumentList();
  emitBytes(OP_CALL, argCount);
}



static void declaration() {
  if (match(TOKEN_CLASS)) {
    classDeclaration();
  }else if (match(TOKEN_FUN)) {
    funDeclaration();
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    statement();
  }
  
  if (parser.panicMode) synchronize();
}


static void varDeclaration() {
  int32_t global = parseVariable("Expect variable name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON,
          "Expect ';' after variable declaration.");

  defineVariable(global);
}

static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope(); 

  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
  
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        errorAtCurrent("Can't have more than 255 parameters.");
      }
      uint8_t constant = parseVariable("Expect parameter name.");
      defineVariable(constant);
    } while (match(TOKEN_COMMA));
  }


  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
  block();

  ObjFunction* function = endCompiler();
  emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));
  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}


static void funDeclaration() {
  uint8_t global = parseVariable("Expect function name.");
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global);
}



static void classDeclaration() {
  consume(TOKEN_IDENTIFIER, "Expect class name.");
  //获取类名
  Token className = parser.previous;
  uint8_t nameConstant = identifierConstant(&parser.previous);
  declareVariable();

  emitBytes(OP_CLASS, nameConstant);
  defineVariable(nameConstant);


  ClassCompiler classCompiler;
  classCompiler.hasSuperclass = false;
  classCompiler.enclosing = currentClass;
  currentClass = &classCompiler;

  if (match(TOKEN_LESS)) {
    consume(TOKEN_IDENTIFIER, "Expect superclass name.");
    variable(false);
    if (identifiersEqual(&className, &parser.previous)) {
      errorAtPrevious("A class can't inherit from itself.");
    }
    beginScope();
    addLocal(syntheticToken("super"));
    defineVariable(0);
    namedVariable(className, false);
    emitByte(OP_INHERIT);
    classCompiler.hasSuperclass = true;
  }

  //将类对象压入栈中
  namedVariable(className, false);
  consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    method();
  } 
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
  //弹出类对象
  emitByte(OP_POP);


  if (classCompiler.hasSuperclass) {
    endScope();
  }
  currentClass = currentClass->enclosing;

}

static void statement() {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else if (match(TOKEN_BREAK)) {
    breakStatement();
  } else if (match(TOKEN_CONTINUE)) {
    continueStatement();
  } else if (match(TOKEN_FOR)) {
    forStatement();
  }  else if (match(TOKEN_RETURN)) {
    returnStatement();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  }  else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else {
    expressionStatement();
  }
}



static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_PRINT);
}

static void breakStatement(){
  if (currentCirculation == NULL) {
    errorAtPrevious("Can't use 'break' outside of a Circulation.");
    return;
  }
  int breakJump = emitJump(OP_JUMP);
  currentCirculation->_break[currentCirculation->_break_count++] = breakJump;
  consume(TOKEN_SEMICOLON, "Expect ';' after break.");
}

static void continueStatement(){
  if (currentCirculation == NULL) {
    errorAtPrevious("Can't use 'continue' outside of a Circulation.");
    return;
  }
  emitLoop(currentCirculation->loopStart);
  consume(TOKEN_SEMICOLON, "Expect ';' after break.");
}

static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(OP_POP);
}

static void beginScope() {
  current->scopeDepth++;
}

static void endScope() {
  current->scopeDepth--;
  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth >
            current->scopeDepth) {
    if (current->locals[current->localCount - 1].isCaptured) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }
    current->localCount--;
  }


}


static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}


static void ifStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition."); 

  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();
  int elseJump = emitJump(OP_JUMP);
  patchJump(thenJump);
  emitByte(OP_POP);
  if (match(TOKEN_ELSE)) statement();
  patchJump(elseJump);
}


static void whileStatement() {
  int loopStart = currentChunk()->count;   
  Circulation loop;
  loop.loopStart = loopStart;
  loop._break_count = 0;
  loop.enclosing = currentCirculation;
  currentCirculation = &loop;

  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();

  emitLoop(loopStart);

 

  //为break填充位置信息
  for (size_t i = 0; i < loop._break_count; i++)
  {
    patchJump(loop._break[i]);
  }
  
  patchJump(exitJump);
  emitByte(OP_POP);

  currentCirculation = currentCirculation->enclosing;
}


static void forStatement() {
  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  if (match(TOKEN_SEMICOLON)) {
    // No initializer.
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    expressionStatement();
  }
  int loopStart = currentChunk()->count;
  int exitJump = -1;

  Circulation loop;
  loop.loopStart = loopStart;
  loop._break_count = 0;
  loop.enclosing = currentCirculation;
  currentCirculation = &loop;

  if (!match(TOKEN_SEMICOLON)) {
      expression();
      consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
      
      // Jump out of the loop if the condition is false.
      exitJump = emitJump(OP_JUMP_IF_FALSE);
      emitByte(OP_POP); // Condition.
  }

  if (!match(TOKEN_RIGHT_PAREN)) {
    int bodyJump = emitJump(OP_JUMP);
    int incrementStart = currentChunk()->count;
    expression();
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

    emitLoop(loopStart);
    loopStart = incrementStart;
    loop.loopStart = loopStart;
    patchJump(bodyJump);
  }

  statement();
  emitLoop(loopStart);

  //为break填充位置信息
  for (size_t i = 0; i < loop._break_count; i++)
  {
    patchJump(loop._break[i]);
  }

  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP); // Condition.
  }

  beginScope();
  currentCirculation = currentCirculation->enclosing;
}


/**
 * 结束编译器的工作流程。
 * 调用此函数将发出返回指令，标志着当前编译单元的结束。
 */
static ObjFunction*  endCompiler() {
  emitReturn();
 
  ObjFunction* function = current->function;
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), function->name != NULL
        ? function->name->chars : "<script>");
  }
#endif
  current = current->enclosing;
  return function;
}


static void returnStatement() {
  if (current->type == TYPE_SCRIPT) {
    errorAtPrevious("Can't return from top-level code.");
  }

  if (match(TOKEN_SEMICOLON)) {
    emitReturn();
  } else {
    if (current->type == TYPE_INITIALIZER) {
      errorAtPrevious("Can't return a value from an initializer.");
    }
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    emitByte(OP_RETURN);
  }
}