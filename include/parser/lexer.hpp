/*
 * Arane - A Perl 6 interpreter.
 * Copyright (C) 2014 Jacob Zhitomirsky
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ARANE__LEXER__H_
#define _ARANE__LEXER__H_

#include <stdexcept>
#include <string>
#include <istream>
#include <vector>
#include <stack>


namespace arane {
  
  /* 
   * Exceptions of this type are thrown by the lexer whenever it encounters
   * invalid tokens.
   */
  class lexer_error: public std::runtime_error
  {
    int ln, col;
    
  public:
    inline int get_line () const { return this->ln; }
    inline int get_column () const { return this->col; }
    
  public:
    lexer_error (const std::string& str, int ln, int col)
      : std::runtime_error (str), ln (ln), col (col)
      { }
  };
  
  
  
  enum token_type
  {
    TOK_INVALID,
    TOK_MORE_TOKENS,
    TOK_EOF,
    
    // data:
    TOK_IDENT_NONE,           //  name
    TOK_IDENT_SCALAR,         // $name
    TOK_IDENT_ARRAY,          // @name
    TOK_IDENT_HASH,           // %name
    TOK_IDENT_HANDLE,         // &name
    TOK_INTEGER,
    TOK_STRING,               // 'something'
    TOK_UNDEF,                // undef
    TOK_TRUE,                 // True
    TOK_FALSE,                // False
    
    // interpolated string stuff:
    TOK_ISTR_BEGIN,           // beginning "
    TOK_ISTR_PART,            // regular text part
    TOK_ISTR_END,             // ending "
    
    // punctuation:
    TOK_SCOL,                 // ;
    TOK_LBRACE,               // {
    TOK_RBRACE,               // }
    TOK_LPAREN,               // (
    TOK_RPAREN,               // )
    TOK_LBRACKET,             // [
    TOK_RBRACKET,             // ]
    TOK_COMMA,                // ,
    TOK_DOT,                  // .
    TOK_TILDE,                // ~
    TOK_BACKSLASH,            // \\ (one)
    TOK_DOLLAR,               // $
    TOK_AT,                   // @
    TOK_LARROW,               // ->
    TOK_CARET,                // ^
    TOK_DQ,                   // ??
    TOK_DEXC,                 // !!
    TOK_DLARROW,              // -->
    
    // operators:
    TOK_ASSIGN,               // =
    TOK_ADD,                  // +
    TOK_SUB,                  // -
    TOK_MUL,                  // *
    TOK_DIV,                  // /
    TOK_MOD,                  // %
    TOK_RANGE,                // ..
    TOK_INC,                  // ++
    TOK_DEC,                  // --
    TOK_OF,                   // of
    TOK_COF,                  // :of
    TOK_ADD_ASSIGN,           // +=
    TOK_SUB_ASSIGN,           // -=
    TOK_MUL_ASSIGN,           // *=
    TOK_DIV_ASSIGN,           // /=
    TOK_MOD_ASSIGN,           // %=
    TOK_TILDE_ASSIGN,         // ~=
    
    // comparison operators:
    TOK_EQ,                   // ==
    TOK_NE,                   // !=
    TOK_LT,                   // <
    TOK_LE,                   // <=
    TOK_GT,                   // >
    TOK_GE,                   // >=
    TOK_NOT,                  // !
    
    // string comparison operators:
    TOK_EQ_S,                 // eq
    
    // named unary operators:
    TOK_MY,
    
    // types
    TOK_TYPE_INT_NATIVE,
    TOK_TYPE_INT,
    TOK_TYPE_BOOL_NATIVE,
    TOK_TYPE_STR,
    TOK_TYPE_ARRAY,
    
    // keywords:
    TOK_SUBROUTINE,
    TOK_RETURN,
    TOK_IF,
    TOK_ELSIF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_LOOP,
    TOK_MODULE,
    TOK_PACKAGE,
    TOK_USE,
    TOK_IS,
    TOK_CLASS,
    TOK_HAS,
    TOK_METHOD,
  };
  
  struct token
  {
    token_type typ;
    int ln, col;
    union
      {
        long long i64;
        char *str;
        token *toks;      // more tokens
      } val;
  };
  
  
  
  class token_seq
  {
    friend class lexer;
    
    std::vector<token>& toks;
    unsigned int pos;
    std::stack<unsigned int> states;
    
  private:
    token_seq (std::vector<token>& toks);
    
  public:
    /* 
     * Returns the next token, and advances the stream.
     */
    token next ();
    
    /* 
     * Returns the next token without advancing the stream.
     */
    token peek_next ();
    
    /* 
     * Returns true if the are more tokens in the stream that could be retreived
     * using next().
     */
    bool has_next ();
    
    
    
    /* 
     * State manipulation.
     */
    void push ();
    void pop ();
    void restore ();
  };
  
  /* 
   * The lexer (or scanner) that performs the lexical analysis -
   * Given a stream of characters as input, a corresponding sequence of tokens
   * is generated as output.
   */
  class lexer
  {
    std::vector<token> toks;
    
  public:
    ~lexer ();
    
  public:
    /* 
     * Performs lexical analysis on the specified stream of characters.
     * Throws exceptions of type `lexer_error' on failure.
     */
    void tokenize (std::istream& strm);
    
    /* 
     * Constructs and returns a new token sequence wrapped on top of the
     * lexer's tokens.
     */
    token_seq get_token_seq ();
  };
}

#endif

