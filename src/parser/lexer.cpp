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

#include "parser/lexer.hpp"
#include <cctype>
#include <cstring>
#include <sstream>
#include <unordered_map>


namespace arane {
  
  static void
  _destroy_token (token tok)
  {
    switch (tok.typ)
      {
      case TOK_IDENT_NONE:
      case TOK_IDENT_SCALAR:
      case TOK_IDENT_ARRAY:
      case TOK_IDENT_HASH:
      case TOK_IDENT_HANDLE:
      case TOK_STRING:
      case TOK_ISTR_PART:
        delete[] tok.val.str;
        break;
      
      default: ;
      }
  }
  
  namespace {
    
    class token_keeper
    {
      std::vector<token>& toks;
      bool rel;
      
    public:
      token_keeper (std::vector<token>& toks)
        : toks (toks)
      {
        this->rel = false;
      }
      
      ~token_keeper ()
      {
        if (!rel)
          for (token& tok : toks)
            _destroy_token (tok);
      }
      
      void
      release ()
        { this->rel = true; }
    };
  }
  
  
  
  lexer::~lexer ()
  {
    for (token tok : this->toks)
      _destroy_token (tok);
  }
  
  
  
  namespace {
    
#ifndef EOF
#define EOF (-1)
#endif
    
    struct ls_state
    {
      int ln, col, pcol;
      std::istream::pos_type strm_pos;
    };
    
    /* 
     * A wrapper that keeps track of the current line and column numbers.
     */
    class lexer_stream
    {
      std::istream& strm;
      int ln, col, pcol;
      
      std::stack<ls_state> states;
      
    public:
      inline int get_line () const { return this->ln; }
      inline int get_column () const { return this->col; }
      
    public:
      /* 
       * Constructs a lexer stream on top of the specified input stream.
       */
      lexer_stream (std::istream& strm)
        : strm (strm)
      {
        this->ln = this->col = this->pcol = 1;
      }
      
    public:
      /*
       * Returns the next character or EOF, and advances the stream.
       */
      int
      get ()
      {
        char c = this->strm.get ();
        if (c == std::istream::traits_type::eof ())
          return EOF;
        
        if (c == '\n')
          {
            ++ this->ln;
            this->pcol = this->col;
            this->col = 1;
          }
        else
          ++ this->col;
        
        return c;
      }
      
      /* 
       * Returns the next character or EOF, without advancing the stream.
       */
      int
      peek ()
      {
        char c = this->strm.peek ();
        if (c == std::istream::traits_type::eof ())
          return EOF;
        return c;
      }
      
      /* 
       * Undoes the last get() call.
       */
      void
      unget ()
      {
        this->strm.unget ();
        if (this->peek () == '\n')
          {
            -- this->ln;
            this->col = this->pcol;
          }
      }
      
    //----
      
      void
      push ()
      {
        this->states.push ({
          .ln = this->ln,
          .col = this->col,
          .pcol = this->pcol,
          .strm_pos = this->strm.tellg (),
        });
      }
      
      void
      pop ()
      {
        this->states.pop ();
      }
      
      void
      restore ()
      {
        auto st = this->states.top ();
        this->ln = st.ln;
        this->col = st.col;
        this->pcol = st.pcol;
        this->strm.seekg (st.strm_pos, std::ios_base::beg);
        
        this->states.pop ();
      }
    };
  }
  
  
  
  // forward decs:
  static token _read_token (lexer_stream& strm);
  
  
  
  static bool
  _is_sigil (char c)
  {
    switch (c)
      {
      case '$': case '@': case '%': case '&':
        return true;
      
      default:
        return false;
      }
  }
  
  static bool
  _is_first_ident_char (char c)
  {
    return (std::isalpha (c) || c == '_');
  }
  
  static bool
  _is_ident_char (char c)
  {
    return (std::isalnum (c) || c == '_');
  }
  
  
  
  static void
  _skip_whitespace (lexer_stream& strm)
  {
    int c;
    for (;;)
      {
        c = strm.peek ();
        if (!std::isspace (c))
          {
            if (c == '#')
              {
                // single-line comment
                for (;;)
                  {
                    c = strm.get ();
                    if (c == EOF || c == '\n')
                      break;
                  }
                continue;
              }
            else
              break;
          }
        
        strm.get ();
      }
  }
  
  
  
  /* 
   * Attempts to read punctuation tokens ('(', '{', '=', etc...), and operators.
   */
  static bool
  _try_read_punctuation (lexer_stream& strm, token& tok)
  {
    int c = strm.get ();
    switch (c)
      {
      case '$':
        if (!_is_first_ident_char (strm.peek ()))
          {
            tok.typ = TOK_DOLLAR;
            return true;
          }
        strm.unget ();
        return false;
      case '@':
        if (!_is_first_ident_char (strm.peek ()))
          {
            tok.typ = TOK_AT;
            return true;
          }
        strm.unget ();
        return false;
      
      case '{': tok.typ = TOK_LBRACE; return true;
      case '}': tok.typ = TOK_RBRACE; return true;
      case '(': tok.typ = TOK_LPAREN; return true;
      case ')': tok.typ = TOK_RPAREN; return true;
      case '[': tok.typ = TOK_LBRACKET; return true;
      case ']': tok.typ = TOK_RBRACKET; return true;
      case ';': tok.typ = TOK_SCOL; return true;
      case ',': tok.typ = TOK_COMMA; return true;
      case '\\': tok.typ = TOK_BACKSLASH; return true;
      case '^': tok.typ = TOK_CARET; return true;
      
      case '.':
        c = strm.peek ();
        if (c == '.')
          {
            strm.get ();
            tok.typ = TOK_RANGE;
          }
        else
          tok.typ = TOK_DOT;
        return true;
      
      case '+':
        c = strm.peek ();
        if (c == '+')
          {
            strm.get ();
            tok.typ = TOK_INC;
          }
        else
          tok.typ = TOK_ADD;
        return true;
      
      case '*': tok.typ = TOK_MUL; return true;
      case '/': tok.typ = TOK_DIV; return true;
      case '%': tok.typ = TOK_MOD; return true;
      
      case '-':
        c = strm.peek ();
        if (c == '>')
          {
            strm.get ();
            tok.typ = TOK_LARROW;
          }
        else if (c == '-')
          {
            strm.get ();
            tok.typ = TOK_DEC;
          }
        else
          tok.typ = TOK_SUB;
        return true;
      
      case '=':
        c = strm.peek ();
        if (c == '=')
          {
            strm.get ();
            tok.typ = TOK_EQ;
          }
        else
          tok.typ = TOK_ASSIGN;
        return true;
      
      case '<':
        c = strm.peek ();
        if (c == '=')
          {
            strm.get ();
            tok.typ = TOK_LE;
          }
        else
          tok.typ = TOK_LT;
        return true;
      
      case '>':
        c = strm.peek ();
        if (c == '=')
          {
            strm.get ();
            tok.typ = TOK_GE;
          }
        else
          tok.typ = TOK_GT;
        return true;
      
      case '!':
        c = strm.peek ();
        if (c == '=')
          {
            strm.get ();
            tok.typ = TOK_NE;
          }
        else
          tok.typ = TOK_NOT;
        return true;
      
      default:
        strm.unget ();
        return false;
      }
  }
  
  
  /* 
   * Attempts to read a signed integer.
   */
  static bool
  _try_read_integer (lexer_stream& strm, token& tok)
  {
    strm.push ();
    
    bool neg = false;
    int c = strm.peek ();
    if (c == '-')
      {
        neg = true;
        strm.get ();
      }
    
    std::string str;
    while (std::isdigit (c = strm.peek ()))
      str.push_back (strm.get ());
    if (str.empty ())
      {
        strm.restore ();
        return false;
      }
    
    std::istringstream ss { str };
    ss >> tok.val.i64;
    tok.typ = TOK_INTEGER;
    if (neg)
      tok.val.i64 = -tok.val.i64;
    
    strm.pop ();
    return true;
  }
  
  
  
  static void
  _read_reg_string (lexer_stream& strm, token& tok)
  {
    strm.get (); // skip '
    
    int c;
    std::string str;
    for (;;)
      {
        c = strm.get ();
        if (c == EOF)
          throw lexer_error ("unexpected EOF in string",
            strm.get_line (), strm.get_column ());
        else if (c == '\'')
          break;
        
        str.push_back (c);
      }
    
    tok.typ = TOK_STRING;
    tok.val.str = new char [str.length () + 1];
    std::strcpy (tok.val.str, str.c_str ());
  }
  
  static void
  _read_interp_string (lexer_stream& strm, token& tok)
  {
    std::vector<token> toks;
    token_keeper tkeeper { toks };
    
    strm.get ();  // skip "
    toks.push_back ({ .typ = TOK_ISTR_BEGIN });
    
    std::string part;
    
    int c;
    for (;;)
      {
        c = strm.peek ();
        if (c == EOF)
          throw lexer_error ("unexpected EOF in string", strm.get_line (),
            strm.get_column ());
        
        if (c == '"')
          {
            if (!part.empty ())
              {
                token ptok;
                ptok.typ = TOK_ISTR_PART;
                ptok.val.str = new char [part.length () + 1];
                std::strcpy (ptok.val.str, part.c_str ());
                toks.push_back (ptok);
              }
            
            strm.get ();
            toks.push_back ({ .typ = TOK_ISTR_END });
            break;
          }
        else if (_is_sigil (c))
          {
            if (!part.empty ())
              {
                token ptok;
                ptok.typ = TOK_ISTR_PART;
                ptok.val.str = new char [part.length () + 1];
                std::strcpy (ptok.val.str, part.c_str ());
                toks.push_back (ptok);
                
                part.clear ();
              }
             
            toks.push_back (_read_token (strm));
          }
        else if (c == '{')
          {
            if (!part.empty ())
              {
                token ptok;
                ptok.typ = TOK_ISTR_PART;
                ptok.val.str = new char [part.length () + 1];
                std::strcpy (ptok.val.str, part.c_str ());
                toks.push_back (ptok);
                
                part.clear ();
              }
            
            token tok;
            for (;;)
              {
                tok = _read_token (strm);
                if (tok.typ == EOF)
                  throw lexer_error ("unexpected EOF in string", tok.ln, tok.col);
                
                toks.push_back (tok);
                if (tok.typ == TOK_RBRACE)
                  break;
              }
          }
        else if (c == '\\')
          {
            strm.get ();
            switch ((c = strm.get ()))
              {
                case 'a':   part.push_back ('\a'); break;
                case 'b':   part.push_back ('\b'); break;
                case 't':   part.push_back ('\t'); break;
                case 'n':   part.push_back ('\n'); break;
                case 'f':   part.push_back ('\f'); break;
                case 'r':   part.push_back ('\r'); break;
                
                case '$': case '@': case '%': case '&': case '{': case '\\':
                case '"':
                  part.push_back (c);
                  break;
                
                default:
                  throw lexer_error ("invalid escape sequence", strm.get_line (),
                    strm.get_column ());
              }
          }
        else
          {
            part.push_back (strm.get ());
          }
      }
    
    tkeeper.release ();
    
    toks.push_back ({ .typ = TOK_INVALID }); // marks the end
    tok.typ = TOK_MORE_TOKENS;
    tok.val.toks = new token [toks.size ()];
    for (unsigned int i = 0; i < toks.size (); ++i)
      tok.val.toks[i] = toks[i];
  }
  
  /* 
   * Attempts to read a string (handles both interpolated and regular strings).
   */
  static bool
  _try_read_string (lexer_stream& strm, token& tok)
  {
    int c = strm.peek ();
    if (c == '\'')
      {
        _read_reg_string (strm, tok);
        return true;
      }
    else if (c == '"')
      {
        _read_interp_string (strm, tok);
        return true;
      }
    
    return false;
  }
  
  
  
  /* 
   * Attempts to read an identifier name along with its sigil.
   */
  static bool
  _try_read_ident (lexer_stream& strm, token& tok)
  {
    token_type ident_type = TOK_IDENT_NONE;
    
    strm.push ();
    
    int c = strm.peek ();
    switch (c)
      {
      case '$':
        ident_type = TOK_IDENT_SCALAR;
        strm.get ();
        break;
      case '@':
        ident_type = TOK_IDENT_ARRAY;
        strm.get ();
        break;
      case '%':
        ident_type = TOK_IDENT_HASH;
        strm.get ();
        break;
      case '&':
        ident_type = TOK_IDENT_HANDLE;
        strm.get ();
        break;
      }
    
    if (!_is_first_ident_char (strm.peek ()))
      {
        strm.restore ();
        return false;
      }
    
    std::string str;
    while (_is_ident_char (c = strm.peek ()) || c == ':')
      {
        if (c == ':')
          {
            strm.get ();
            if (strm.peek () == ':')
              {
                strm.get ();
                str.append ("::");
              }
            else
              {
                strm.unget ();
                break;
              }
          }
        else
          str.push_back (strm.get ());
      }
    
    tok.typ = ident_type;
    tok.val.str = new char [str.length () + 1];
    std::strcpy (tok.val.str, str.c_str ());
    
    strm.pop ();
    return true;
  }
  
  
  
  /* 
   * Attempts to read a reserved keyword ('my', 'sub', 'use', etc...)
   */
  static bool
  _try_read_keyword (lexer_stream& strm, token& tok)
  {
    strm.push ();
    
    int c;
    std::string str;
    while (std::isalpha (c = strm.peek ()))
      str.push_back (strm.get ());
    if (str.empty ())
      {
        strm.restore ();
        return false;
      }
    
    static const std::unordered_map<std::string, token_type> _map {
      { "my", TOK_MY },
      { "undef", TOK_UNDEF },
      { "sub", TOK_SUBROUTINE },
      { "return", TOK_RETURN },
      { "if", TOK_IF },
      { "elsif", TOK_ELSIF },
      { "else", TOK_ELSE },
      { "while", TOK_WHILE },
      { "for", TOK_FOR },
      { "loop", TOK_LOOP },
      { "module", TOK_MODULE },
      { "package", TOK_PACKAGE },
      { "use", TOK_USE },
      { "eq", TOK_EQ_S },
      
      { "int", TOK_TYPE_INT_NATIVE },
      { "Int", TOK_TYPE_INT },
    };
    
    auto itr = _map.find (str);
    if (itr == _map.end ())
      {
        strm.restore ();
        return false;
      }
    tok.typ = itr->second;
    
    strm.pop ();
    return true;
  }
  
  
  
  token
  _read_token (lexer_stream& strm)
  {
    token tok { .typ = TOK_INVALID };
    
    _skip_whitespace (strm);
    tok.ln = strm.get_line ();
    tok.col = strm.get_column ();
    if (strm.peek () == EOF)
      {
        tok.typ = TOK_EOF;
        return tok;
      }
    
    if (_try_read_punctuation (strm, tok))
      return tok;
    
    if (_try_read_integer (strm, tok))
      return tok;
    
    if (_try_read_string (strm, tok))
      return tok;
    
    if (_try_read_keyword (strm, tok))
      return tok;
    
    if (_try_read_ident (strm, tok))
      return tok;
    
    throw lexer_error ("undefined token", tok.ln, tok.col);
  }
  
  /* 
   * Performs lexical analysis on the specified stream of characters.
   * Throws exceptions of type `lexer_error' on failure.
   */
  void
  lexer::tokenize (std::istream& strm)
  {
    lexer_stream lex { strm };
    
    this->toks.clear ();
    
    token tok;
    for (;;)
      {
        tok = _read_token (lex);
        
        if (tok.typ == TOK_MORE_TOKENS)
          {
            token *ptr = tok.val.toks;
            while (ptr->typ != TOK_INVALID)
              this->toks.push_back (*ptr++);
          }
        else
          {
            this->toks.push_back (tok);
            if (tok.typ == TOK_EOF)
              break;
          }
      }
  }
  
  
  
  /* 
   * Constructs and returns a new token sequence wrapped on top of the
   * lexer's tokens.
   */
  token_seq
  lexer::get_token_seq ()
  {
    return token_seq (this->toks);
  }
  
  
  
//------------------------------------------------------------------------------
  
  token_seq::token_seq (std::vector<token>& toks)
    : toks (toks)
  {
    this->pos = 0;
  }
  
  
  
  /* 
   * Returns the next token, and advances the stream.
   */
  token
  token_seq::next ()
  {
    token tok = this->toks[this->pos];
    if (this->pos <= this->toks.size ())
      ++ this->pos;
    return tok;
  }
  
  /* 
   * Returns the next token without advancing the stream.
   */
  token
  token_seq::peek_next ()
  {
    return this->toks[this->pos];
  }
  
  /* 
   * Returns true if the are more tokens in the stream that could be retreived
   * using next().
   */
  bool
  token_seq::has_next ()
  {
    return (this->pos < this->toks.size ());
  }
  
  
  
  /* 
   * State manipulation.
   */
  
  void
  token_seq::push ()
  {
    this->states.push (this->pos);
  }
  
  void
  token_seq::pop ()
  {
    this->states.pop ();
  }
  
  void
  token_seq::restore ()
  {
    this->pos = this->states.top ();
    this->states.pop ();
  }
}

