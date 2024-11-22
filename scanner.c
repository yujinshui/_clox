#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

#define TABLE_GAP (4)
#define TABLE_GAP_MASK (~(TABLE_GAP - 1))
#define UP_ALIGN_TABLE(x) ((x + TABLE_GAP - 1) & TABLE_GAP_MASK)

typedef struct {
  const char* start;
  const char* current;
  int line;   //行号
  int column;
} Scanner;


Scanner scanner;

/****************************************/
/****    static function declaration  ***/
/****************************************/
static bool isAtEnd();
static Token makeToken(TokenType type);
static Token errorToken(const char *message);
static char advance();
static bool match(char expected);
static char peek();
static void skipWhitespace();
static char peekNext();
static Token string();
static bool isDigit(char c);
static Token number();
static bool isAlpha(char c);
static Token identifier();


/****************************************/
/****    public function definition  ****/
/****************************************/
void initScanner(const char* source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
  scanner.column = 1;
}

/**
 * 扫描并生成下一个 token。
 * 该函数首先跳过空白字符，然后根据当前字符判断 token 的类型，
 * 并调用相应的函数生成 token。如果遇到无法识别的字符，则返回错误 token。
 */
Token scanToken() {
  skipWhitespace();
  scanner.start = scanner.current;

  if (isAtEnd()) return makeToken(TOKEN_EOF);
  char c = advance();
  if (isAlpha(c)) return identifier();
  if (isDigit(c)) return number();

  switch (c) {
    case '(': return makeToken(TOKEN_LEFT_PAREN);
    case ')': return makeToken(TOKEN_RIGHT_PAREN);
    case '{': return makeToken(TOKEN_LEFT_BRACE);
    case '}': return makeToken(TOKEN_RIGHT_BRACE);
    case ';': return makeToken(TOKEN_SEMICOLON);
    case ',': return makeToken(TOKEN_COMMA);
    case '.': return makeToken(TOKEN_DOT);
    case '-': return makeToken(match('-') ? TOKEN_DECREASE : TOKEN_MINUS);
    case '+': return makeToken(match('+') ? TOKEN_INCREASE : TOKEN_PLUS);
    case '/': return makeToken(TOKEN_SLASH);
    case '*': return makeToken(TOKEN_STAR);
    case ':': return makeToken(TOKEN_COLON);
    case '!':
      return makeToken(
          match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=':
      return makeToken(
          match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<':
      return makeToken(
          match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>':
      return makeToken(
          match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '"': return string();
  }


  return errorToken("Unexpected character.");
}

/****************************************/
/****    static function definition  ****/
/****************************************/

/**
 * 判断扫描器当前位置是否已经到达输入字符串的末尾。
 * @return 如果当前位置是字符串的末尾（即遇到了空字符 '\0'），则返回 true；否则返回 false。
 */
static bool isAtEnd() {
  return *scanner.current == '\0';
}

/**
 * 创建一个新的Token实例。
 * @param type Token的类型。
 * @return 返回一个填充了当前扫描器位置信息的Token实例。
 */
static Token makeToken(TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;
  token.column = scanner.column;
  scanner.column += token.length;
  return token;
}

/**
 * 创建并返回一个表示错误的 Token。
 *
 * @param message 错误信息字符串。
 * @return 返回一个 Token 对象，其类型为 TOKEN_ERROR，起始位置为错误信息，
 *         长度为错误信息的长度，行号为扫描器当前行号。
 */
static Token errorToken(const char *message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;
  token.column = scanner.column;
  return token;
}

/**
 * 将扫描器的当前位置向前移动一个字符，并返回前一个字符。
 * 这是一个实现细节的函数，用于内部处理字符串扫描。
 */
static char advance() {
  scanner.current++;
  return scanner.current[-1];
}

/**
 * 检查当前扫描器指针指向的字符是否与预期字符匹配。
 * 如果匹配，则将扫描器指针向前移动一位并返回true；否则返回false。
 *
 * @param expected 预期的字符
 * @return 如果匹配则返回true，否则返回false
 */
static bool match(char expected) {
  if (isAtEnd())
    return false;
  if (*scanner.current != expected)
    return false;
  scanner.current++;
  return true;
}

/**
 * 返回扫描器当前指向的字符，但不移动扫描器的位置。
 * 这是一个实现细节，仅供内部使用。
 */
static char peek() {
  return *scanner.current;
}

/**
 * 返回扫描器当前位置的字符的下一个字符。
 * 如果已经到达输入的末尾，则返回空字符'\0'。
 */
static char peekNext() {
  if (isAtEnd())
    return '\0';
  return scanner.current[1];
}

/**
 * 跳过输入中的空白字符，包括空格、制表符、回车符和换行符。
 * 如果遇到单行注释（//），则跳过注释直到行尾。
 * 该函数在扫描器中用于预处理输入，以便后续处理非空白和非注释部分。
 */
static void skipWhitespace() {
  for (;;) {
    char c = peek();
    int column = 0;
    switch (c) {
      case ' ':
        column = scanner.column + 1;
      case '\r':
      case '\t':
        scanner.column = column ? column : UP_ALIGN_TABLE(scanner.column);
        advance();
        break;
      case '\n':
        scanner.line++;
        scanner.column = 1;
        advance();
        break;
      case '/':
        if (peekNext() == '/') {
          // A comment goes until the end of the line.
          while (peek() != '\n' && !isAtEnd()) advance();
        } else {
          return;
        }
        break;
      default:
        return;
    }
  }
}

/**
 * 解析字符串字面量并生成相应的Token。
 * 该函数会连续读取字符直到遇到结束的双引号，期间如果遇到换行符则更新行号。
 * 如果在字符串未结束前到达文件末尾，则返回一个错误Token。
 * 成功读取完整字符串后，会跳过结束的双引号并生成TOKEN_STRING类型的Token。
 */
static Token string() {
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n')
      scanner.line++;
    advance();
  }

  if (isAtEnd()) return errorToken("Unterminated string.");

  // The closing quote.
  advance();
  return makeToken(TOKEN_STRING);
}

/**
 * 判断给定字符是否为数字字符。
 * @param c 要检查的字符。
 * @return 如果字符是 '0' 到 '9' 之间的数字，则返回 true，否则返回 false。
 */
static bool isDigit(char c) {
  return c >= '0' && c <= '9';
}

/**
 * 解析并返回一个数字 Token。
 * 该函数首先跳过所有的数字字符，然后检查是否存在小数部分。
 * 如果存在小数部分，它会先消费掉 '.' 字符，然后继续跳过所有的数字字符。
 * 最后，它创建并返回一个 TOKEN_NUMBER 类型的 Token。
 */
static Token number() {
  while (isDigit(peek()))
    advance();

  // Look for a fractional part.
  if (peek() == '.' && isDigit(peekNext())) {
    // Consume the ".".
    advance();

    while (isDigit(peek())) advance();
  }

  return makeToken(TOKEN_NUMBER);
}

/**
 * 判断给定字符是否为字母或下划线。
 * @param c 需要检查的字符。
 * @return 如果字符是字母（大小写）或下划线，则返回true，否则返回false。
 */
static bool isAlpha(char c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         c == '_';
}


static TokenType checkKeyword(int start, int length,
    const char* rest, TokenType type) {
  if (scanner.current - scanner.start == start + length &&
      memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

/**
 * 根据扫描器的起始字符确定标识符的类型。
 * 该函数检查标识符的首字母，并根据首字母及其后续字符判断是否为关键字。
 * 如果是关键字，则返回相应的TokenType；否则，返回TOKEN_IDENTIFIER。
 *
 * @return TokenType 标识符的类型
 */
static TokenType identifierType()
{
  switch (scanner.start[0])
  {
  case 'a':
    return checkKeyword(1, 2, "nd", TOKEN_AND);
  case 'b':
    return checkKeyword(1, 4, "reak", TOKEN_BREAK);
  case 'c':
    if (scanner.current - scanner.start > 1)
    {
      switch (scanner.start[1])
      {
      case 'l':
        return checkKeyword(2, 3, "ass", TOKEN_CLASS);
      case 'a':
        return checkKeyword(2, 2, "se", TOKEN_CASE);
      case 'o':
        return checkKeyword(2, 6, "ntinue", TOKEN_CONTINUE);
      }
    }
    break;
  case 'd':
    return checkKeyword(1, 4, "efault", TOKEN_DEFAULT);
  case 'e':
    return checkKeyword(1, 3, "lse", TOKEN_ELSE);
  case 'f':
    if (scanner.current - scanner.start > 1)
    {
      switch (scanner.start[1])
      {
      case 'a':
        return checkKeyword(2, 3, "lse", TOKEN_FALSE);
      case 'o':
        return checkKeyword(2, 1, "r", TOKEN_FOR);
      case 'u':
        return checkKeyword(2, 1, "n", TOKEN_FUN);
      }
    }
    break;
  case 'i':
    return checkKeyword(1, 1, "f", TOKEN_IF);
  case 'n':
    return checkKeyword(1, 2, "il", TOKEN_NIL);
  case 'o':
    return checkKeyword(1, 1, "r", TOKEN_OR);
  case 'p':
    return checkKeyword(1, 4, "rint", TOKEN_PRINT);
  case 'r':
    return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
  case 's':
    if (scanner.current - scanner.start > 1)
    {
      switch (scanner.start[1])
      {
      case 'u':
        return checkKeyword(2, 4, "uper", TOKEN_SUPER);
      case 'w':
        return checkKeyword(2, 4, "itch", TOKEN_SWITCH);
      }
    }
    break;
  case 't':
    if (scanner.current - scanner.start > 1)
    {
      switch (scanner.start[1])
      {
      case 'h':
        return checkKeyword(2, 2, "is", TOKEN_THIS);
      case 'r':
        return checkKeyword(2, 2, "ue", TOKEN_TRUE);
      }
    }
    break;
  case 'v':
    return checkKeyword(1, 2, "ar", TOKEN_VAR);
  case 'w':
    return checkKeyword(1, 4, "hile", TOKEN_WHILE);
  }

  return TOKEN_IDENTIFIER;
}

/**
 * 识别标识符并生成相应的 Token。
 * 该函数会连续读取字符，直到遇到非字母或数字的字符为止。
 * 然后根据读取的字符序列确定标识符的类型，并生成对应的 Token。
 */
static Token identifier() {
  while (isAlpha(peek()) || isDigit(peek())) advance();
  return makeToken(identifierType());
} 
