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

#include "parser/parser.hpp"
#include "parser/lexer.hpp"
#include <sstream>
#include <memory>
#include <stack>


namespace arane {
  
  namespace {
    
    enum parser_context
    {
      PC_NONE,
      
      PC_SCALAR,
      PC_LIST,
    };
    
    struct parser_state
    {
      token_seq& toks;
      error_tracker& errs;
      
      std::stack<parser_context> ctxs;
      
    public:
      parser_state (token_seq& toks, error_tracker& errs)
        : toks (toks), errs (errs)
      {
        this->push_context (PC_SCALAR);
      }
      
      
      
      void
      push_context (parser_context ctx)
      {
        this->ctxs.push (ctx);
      }
      
      void
      pop_context ()
      {
        this->ctxs.pop ();
      }
      
      parser_context&
      top_context ()
      {
        return this->ctxs.top ();
      }
    };
    
    
    class context_block
    {
      parser_state& ps;
      parser_context ctx;
      
    public:
      context_block (parser_state& ps, parser_context ctx)
        : ps (ps)
      {
        this->ctx = ctx;
        if (ctx != PC_NONE)
          ps.push_context (ctx);
      }
      
      ~context_block ()
      {
        if (this->ctx != PC_NONE)
          ps.pop_context ();
      }
    };
  }
  
  
  
  parser::parser (error_tracker& errs)
    : errs (errs)
    { }
  
  
  // forward declarations:
  static ast_stmt* _parse_stmt (parser_state& ps);
  static ast_expr* _parse_expr (parser_state& ps);
  static ast_expr* _parse_atom (parser_state& ps);
  static ast_ident* _parse_ident (parser_state& ps);
  static ast_block* _parse_block (parser_state& ps);
  static ast_package* _parse_package_or_module (parser_state& ps);
  
  
  static bool
  _skip_scol (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    token tok = toks.peek_next ();
    if (tok.typ == TOK_SCOL || tok.typ == TOK_EOF)
      toks.next ();
    else if (tok.typ != TOK_RBRACE)
      {
        ps.errs.error (ES_PARSER, "expected ';'", tok.ln, tok.col);
        return false;
      }
    
    return true;
  }
  
  
  
  static basic_types
  _tok_type_to_basic (token_type typ)
  {
    switch (typ)
      {
      case TOK_TYPE_INT_NATIVE:     return TYPE_INT_NATIVE;
      case TOK_TYPE_INT:            return TYPE_INT;
      case TOK_TYPE_BOOL_NATIVE:    return TYPE_BOOL_NATIVE;
      case TOK_TYPE_STR:            return TYPE_STR;
      case TOK_TYPE_ARRAY:          return TYPE_ARRAY;
      
      default:
        return TYPE_INVALID;
      }
  }
  
  static bool
  _parse_type_info (parser_state& ps, type_info& ti)
  {
    token_seq& toks = ps.toks;
    
    ti.types.clear ();
    
    int parens = 0;
    token tok;
    for (;;)
      {
        tok = toks.next ();
        
        basic_type bt {
          .type = _tok_type_to_basic (tok.typ),
          .name = "",
        };
        if (bt.type == TYPE_INVALID)
          {
            ps.errs.error (ES_PARSER, "expected a type name", tok.ln, tok.col);
            return false;
          }
        ti.types.push_back (bt);
        
        tok = toks.peek_next ();
        if (tok.typ == TOK_COF)
          {
            toks.next ();
            tok = toks.next ();
            if (tok.typ != TOK_LPAREN)
              {
                ps.errs.error (ES_PARSER, "expected '(' after ':of'", tok.ln, tok.col);
                return false;
              }
            ++ parens;
          }
        else if (tok.typ == TOK_OF)
          toks.next ();
        else
          break;
      }
    
    // consume matching parentheses
    while (parens --> 0)
      {
        tok = toks.next ();
        if (tok.typ != TOK_RPAREN)
          {
            ps.errs.error (ES_PARSER, "expected matching ')' in type name", tok.ln, tok.col);
            return false;
          }
      }
    
    return true;
  }
  
  /* 
   * The type is expected to be positioned before an expression.
   * <type> <atom>
   */
  static ast_of_type*
  _parse_of_type_left (parser_state& ps)
  {
    type_info ti;
    if (!_parse_type_info (ps, ti))
      return nullptr;
    
    ast_expr *expr = _parse_atom (ps);
    if (!expr)
      return nullptr;
    
    return new ast_of_type (expr, ti);
  }
  
  /* 
   * <atom> of <type>
   * Assumes <atom> has already been consumed.
   */
  static ast_of_type*
  _parse_of_type_right (ast_expr *left, parser_state& ps)
  {
    token_seq& toks = ps.toks;
    toks.next (); // skip 'of'
    
    type_info ti;
    if (!_parse_type_info (ps, ti))
      return nullptr;
    
    return new ast_of_type (left, ti);
  }
  
  
  
  static ast_sub_call*
  _parse_sub_call (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    token tok = toks.next ();
    if (tok.typ != TOK_IDENT_NONE)
      {
        // subroutine calls using handle sigils are not supported yet..
        ps.errs.error (ES_PARSER, "expected subroutine name", tok.ln, tok.col);
        return nullptr;
      }
    
    std::string name = tok.val.str;
    
    
    /* 
     * Read any parameters if any.
     */
    std::unique_ptr<ast_expr> params;
    try
      {
        // remember state
        toks.push ();
        ps.errs.silence (true, false);
        
        context_block ctxb { ps, PC_LIST }; // in list context
        std::unique_ptr<ast_expr> t;
        
        token tok = toks.peek_next ();
        if (tok.typ == TOK_LPAREN)
          t.reset (_parse_atom (ps));
        else
          t.reset (_parse_expr (ps));
        
        if (t.get ())
          params.reset (t.release ());
      }
    catch (const parse_error& ex)
      {
        // nope
      }
    
    ps.errs.silence (false, true);
    if (!params.get ())
      {
        // restore state
        toks.restore ();
      }
      
    bool got_list = (params && params->get_type () == AST_LIST);
    auto param_list = got_list ? static_cast<ast_list *> (params.get ()) : new ast_list ();
    if (!got_list)
      {
        if (params.get ())
          param_list->add_elem (params.get ());
      }
    
    params.release ();
    auto sub_call = new ast_sub_call (name, param_list);
    sub_call->set_pos (tok.ln, tok.col);
    return sub_call;
  }
  
  
  ast_ident*
  _parse_ident (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    ast_ident *ident;
    token tok = toks.peek_next ();
    switch (tok.typ)
      {
      case TOK_IDENT_SCALAR:
        toks.next ();
        ident = new ast_ident (tok.val.str, AST_IDENT_SCALAR);
        ident->set_pos (tok.ln, tok.col);
        return ident;
      case TOK_IDENT_ARRAY:
        toks.next ();
        ident = new ast_ident (tok.val.str, AST_IDENT_ARRAY);
        ident->set_pos (tok.ln, tok.col);
        return ident;
      case TOK_IDENT_HASH:
        toks.next ();
        ident = new ast_ident (tok.val.str, AST_IDENT_HASH);
        ident->set_pos (tok.ln, tok.col);
        return ident;
      case TOK_IDENT_HANDLE:
        toks.next ();
        ident = new ast_ident (tok.val.str, AST_IDENT_HANDLE);
        ident->set_pos (tok.ln, tok.col);
        return ident;
      
      default:
        ps.errs.error (ES_PARSER, "expected an identifier", tok.ln, tok.col);
        return nullptr;
      }
  }
  
  static ast_expr*
  _parse_ident_or_sub_call (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    token tok = toks.peek_next ();
    if (tok.typ == TOK_IDENT_NONE)
      {
        return _parse_sub_call (ps);
      }
    
    return _parse_ident (ps);
  }
  
  
  
  static ast_list*
  _parse_list (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    token tok = toks.next (); // skip (
    
    std::unique_ptr<ast_list> list { new ast_list () };
    list->set_pos (tok.ln, tok.col);
    for (;;)
      {
        tok = toks.peek_next ();
        if (tok.typ == TOK_RPAREN)
          {
            toks.next ();
            break;
          }
        
        ast_expr *elem = _parse_expr (ps);
        if (!elem)
          return nullptr;
        
        if (elem->get_type () == AST_LIST)
          {
            // flatten out
            for (ast_expr *e : (static_cast<ast_list *> (elem))->get_elems ())
              list->add_elem (e);
          }
        else
          list->add_elem (elem);
        
        tok = toks.peek_next (); 
        if (tok.typ == TOK_COMMA)
          toks.next ();
        else if (tok.typ != TOK_RPAREN)
          {
            ps.errs.error (ES_PARSER, "expected ',' or ')' inside list",
              tok.ln, tok.col);
            return nullptr;
          }
      }
    
    return list.release ();
  }
  
  
  
  static ast_anonym_array*
  _parse_anonym_array (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    token tok = toks.next (); // skip [
    
    std::unique_ptr<ast_anonym_array> arr { new ast_anonym_array () };
    arr->set_pos (tok.ln, tok.col);
    for (;;)
      {
        tok = toks.peek_next ();
        if (tok.typ == TOK_RBRACKET)
          {
            toks.next ();
            break;
          }
        
        ast_expr *elem = _parse_expr (ps);
        if (!elem)
          return nullptr;
        
        arr->add_elem (elem);
        
        tok = toks.peek_next (); 
        if (tok.typ == TOK_COMMA)
          toks.next ();
        else if (tok.typ != TOK_RBRACKET)
          {
            ps.errs.error (ES_PARSER, "expected ',' or ']' inside anonymous array",
              tok.ln, tok.col);
            return nullptr;
          }
      }
    
    return arr.release ();
  }
  
  
  
  /* 
   * Very similar to a subroutine call.
   * The difference is that named unary operators require parentheses in order
   * to bind to more than one operator.
   */
  static ast_named_unop*
  _parse_named_unop (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    token tok = toks.next ();
    int p_ln = tok.ln;
    int p_col = tok.col;
    
    ast_unop_type unop_type;
    switch (tok.typ)
      {
      case TOK_MY: unop_type = AST_UNOP_MY; break;
      
      default:
        throw std::runtime_error ("not a named unary operator");
      }
    
    ast_expr *param;
    tok = toks.peek_next ();
    if (tok.typ == TOK_LPAREN)
      param = _parse_list (ps);
    else
      param = _parse_atom (ps);
    if (!param)
      return nullptr;
    
    auto ast = new ast_named_unop (unop_type, param);
    ast->set_pos (p_ln, p_col);
    return ast;
  }
  
  
  
  static ast_interp_string*
  _parse_interp_string (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    token tok = toks.next ();     // skip TOK_ISTR_BEGIN
    
    std::unique_ptr<ast_interp_string> istr { new ast_interp_string () };
    istr->set_pos (tok.ln, tok.col);
    
    for (;;)
      {
        tok = toks.peek_next ();
        if (tok.typ == TOK_ISTR_END)
          {
            toks.next ();
            break;
          }
        else if (tok.typ == TOK_ISTR_PART)
          {
            toks.next ();
            istr->add_part (tok.val.str);
          }
        else if (tok.typ == TOK_LBRACE)
          {
            toks.next ();
            
            istr->add_expr (_parse_expr (ps));
            if ((tok = toks.next ()).typ != TOK_RBRACE)
              {
                ps.errs.error (ES_PARSER, "expected matching '}' in interpolated"
                  " string", tok.ln, tok.col);
                return nullptr;
              }
          }
        else
          {
            istr->add_expr (_parse_atom (ps));
          }
      }
    
    return istr.release ();
  }
  
  
  
  static ast_sub*
  _parse_subroutine (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    token tok = toks.next ();   // skip 'sub'
    int p_ln = tok.ln;
    int p_col = tok.col;
    
    type_info ret_type = type_info::none ();
    
    // subroutine name
    tok = toks.next ();
    if (tok.typ != TOK_IDENT_NONE)
      {
        ps.errs.error (ES_PARSER, "expected subroutine name after 'sub'",
          tok.ln, tok.col);
        return nullptr;
      }
    std::string name = tok.val.str;
    
    std::unique_ptr<ast_sub> sub { new ast_sub (name) };
    sub->set_pos (p_ln, p_col);
    
    /* 
     * Parameter list:
     */
    tok = toks.peek_next ();
    if (tok.typ == TOK_LPAREN)
      {
        toks.next ();
        for (;;)
          {
            tok = toks.peek_next ();
            if (tok.typ == TOK_RPAREN)
              {
                toks.next ();
                break;
              }
            else if (tok.typ == TOK_DLARROW)
              {
                // return type
                toks.next ();
                if (!_parse_type_info (ps, ret_type))
                  return nullptr;
                
                tok = toks.next ();
                if (tok.typ != TOK_RPAREN)
                  {
                    ps.errs.error (ES_PARSER, "expected ')' after subroutine "
                      "return type", tok.ln, tok.col);
                    return nullptr;
                  }
                
                break;
              }
            
            ast_expr *atom = _parse_atom (ps);
            if (atom)
              {
                if ((atom->get_type () == AST_IDENT) || (atom->get_type () == AST_OF_TYPE))
                  sub->add_param (atom);
                else
                  {
                    ps.errs.error (ES_PARSER, "expected an identifier or a type "
                      "name followed by an identifier in subroutine's parameter "
                      "list", tok.ln, tok.col);
                    return nullptr;
                  }
              }
            
            tok = toks.peek_next ();
            if (tok.typ == TOK_COMMA)
              {
                toks.next ();
              }
            else if (tok.typ != TOK_RPAREN && tok.typ != TOK_DLARROW)
              {
                ps.errs.error (ES_PARSER, "expected ',' or ')' after parameter "
                  "inside subroutine paramter list", tok.ln, tok.col);
                return nullptr;
              }
          }
      }
    
    /* 
     * Body:
     */
    tok = toks.peek_next ();
    if (tok.typ != TOK_LBRACE)
      {
        ps.errs.error (ES_PARSER, "expected '{' after subroutine parameter list",
          tok.ln, tok.col);
        return nullptr;
      }
    ast_block *body = _parse_block (ps);
    if (!body)
      return nullptr;
    
    sub->set_body (body);
    sub->set_return_type (ret_type);
    return sub.release ();
  }
  
  
  
  static ast_ref*
  _parse_ref (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    toks.next (); // skip backslash
    
    ast_expr *expr = _parse_atom (ps);
    if (!expr)
      return nullptr;
    return new ast_ref (expr);
  }
  
  
  
  static ast_deref*
  _parse_deref (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    toks.next (); // skip sigil
    
    token tok = toks.peek_next ();
    if (tok.typ != TOK_LPAREN)
      {
        ast_expr *expr = _parse_atom (ps);
        if (!expr)
          return nullptr;
        return new ast_deref (expr);
      }
    
    toks.next (); // skip (
    
    std::unique_ptr<ast_expr> expr { _parse_expr (ps) };
    if (!expr.get ())
      return nullptr;
    
    tok = toks.next ();
    if (tok.typ != TOK_RPAREN)
      {
        ps.errs.error (ES_PARSER, "expected matching ')' in dereference operator",
          tok.ln, tok.col);
        return nullptr;
      }
    
    return new ast_deref (expr.release ());
  }
  
  
  
  static ast_prefix*
  _parse_prefix (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    ast_prefix_type op;
    token tok = toks.next ();
    switch (tok.typ)
      {
      case TOK_INC:     op = AST_PREFIX_INC; break;
      case TOK_DEC:     op = AST_PREFIX_DEC; break;
      case TOK_TILDE:   op = AST_PREFIX_STR; break;
      
      default:
        throw std::runtime_error ("invalid prefix operator");
      }
    
    ast_expr *expr = _parse_expr (ps);
    if (!expr)
      return nullptr;
    
    auto ast = new ast_prefix (expr, op);
    ast->set_pos (tok.ln, tok.col);
    return ast;
  }
  
  
  
  static ast_postfix*
  _parse_postfix (ast_expr *left, parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    ast_postfix_type op;
    token tok = toks.next ();
    switch (tok.typ)
      {
      case TOK_INC:   op = AST_POSTFIX_INC; break;
      case TOK_DEC:   op = AST_POSTFIX_DEC; break;
      
      default:
        throw std::runtime_error ("invalid postfix operator");
      }
    
    auto ast = new ast_postfix (left, op);
    ast->set_pos (left->get_line (), left->get_column ());
    return ast;
  }
  
  
  
  // forward dec:
  static ast_expr* _parse_range_partial (parser_state& ps);
  
  static ast_expr*
  _parse_atom_main (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    token tok = toks.peek_next ();
    switch (tok.typ)
      {
      case TOK_LPAREN:
        {
          toks.next ();
          std::unique_ptr<ast_expr> expr { _parse_expr (ps) };
          if ((tok = toks.next ()).typ != TOK_RPAREN)
            {
              ps.errs.error (ES_PARSER, "expected matching ')'", tok.ln, tok.col);
              return nullptr;
            }
          return expr.release ();
        }
      
      case TOK_LBRACKET:
        return _parse_anonym_array (ps);
      
      // named unary operators
      case TOK_MY:
        return _parse_named_unop (ps);
      
      case TOK_IDENT_NONE:
      case TOK_IDENT_SCALAR:
      case TOK_IDENT_ARRAY:
      case TOK_IDENT_HASH:
      case TOK_IDENT_HANDLE:
        return _parse_ident_or_sub_call (ps);
      
      case TOK_STRING:
        {
          toks.next ();
          auto ast = new ast_string (tok.val.str);
          ast->set_pos (tok.ln, tok.col);
          return ast;
        }
      
      case TOK_ISTR_BEGIN:
        return _parse_interp_string (ps);
        
      case TOK_INTEGER:
        {
          auto expr = new ast_integer (toks.next ().val.i64);
          expr->set_pos (tok.ln, tok.col);
          return expr;
        }
      
      case TOK_FALSE:
        {
          toks.next ();
          auto ast = new ast_bool (false);
          ast->set_pos (tok.ln, tok.col);
          return ast;
        }
      case TOK_TRUE:
        {
          toks.next ();
          auto ast = new ast_bool (true);
          ast->set_pos (tok.ln, tok.col);
          return ast;
        }
      
      case TOK_SUB:
        {
          // negative integer
          toks.next ();
          tok = toks.next ();
          if (tok.typ != TOK_INTEGER)
            {
              ps.errs.error (ES_PARSER, "expected integer after unary '-'",
                tok.ln, tok.col);
              return nullptr;
            }
          
          auto expr = new ast_integer (-(long long)tok.val.i64);
          expr->set_pos (tok.ln, tok.col);
          return expr;
        }
        break;
      
      case TOK_UNDEF:
        toks.next ();
        return new ast_undef ();
      
      case TOK_BACKSLASH:
        return _parse_ref (ps);
      
      case TOK_DOLLAR:
      case TOK_AT:
        return _parse_deref (ps);
        
      case TOK_CARET:
        return _parse_range_partial (ps);
      
      case TOK_TYPE_INT_NATIVE:
      case TOK_TYPE_INT:
      case TOK_TYPE_BOOL_NATIVE:
      case TOK_TYPE_STR:
      case TOK_TYPE_ARRAY:
        return _parse_of_type_left (ps);
        
      // prefix operators
      case TOK_INC:
      case TOK_DEC:
      case TOK_TILDE:
        return _parse_prefix (ps);
        
      default:
        toks.next (); // consume it
        ps.errs.error (ES_PARSER, "expected an atom expression", tok.ln, tok.col);
        return nullptr;
      }
  }
  
  static ast_expr*
  _parse_range (ast_expr *left, parser_state& ps)
  {
    token_seq& toks = ps.toks;
    bool lhs_exc = false;
    bool rhs_exc = false;
    
    token tok = toks.next ();
    if (tok.typ == TOK_CARET)
      {
        toks.next ();
        lhs_exc = true;
      }
    else if (tok.typ != TOK_RANGE)
      throw std::runtime_error ("expected ..");
    
    tok = toks.peek_next ();
    if (tok.typ == TOK_CARET)
      {
        toks.next ();
        rhs_exc = true;
      }
    
    std::unique_ptr<ast_expr> rhs { _parse_atom (ps) };
    if (!rhs.get ())
      return nullptr;
    
    return new ast_range (left, lhs_exc, rhs.release (), rhs_exc);
  }
  
  ast_expr*
  _parse_range_partial (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    toks.next (); // skip ^
    
    std::unique_ptr<ast_expr> rhs { _parse_atom (ps) };
    if (!rhs.get ())
      return nullptr;
    
    return new ast_range (new ast_integer (0), false, rhs.release (), true);
  }
  
  
  static ast_expr*
  _parse_atom_rest (ast_expr *left, parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    token tok = toks.peek_next ();
    switch (tok.typ)
      {
      case TOK_LBRACKET:
        {
          // array subscript
          toks.next ();
          
          std::unique_ptr<ast_expr> index { _parse_expr (ps) };
          if (!index.get ())
            return nullptr;
          
          tok = toks.next ();
          if (tok.typ != TOK_RBRACKET)
            {
              ps.errs.error (ES_PARSER, "expected matching ']'", tok.ln, tok.col);
              return nullptr;
            }
          
          std::unique_ptr<ast_subscript> subsc { new ast_subscript (left, index.get ()) };
          ast_expr *nexpr = _parse_atom_rest (subsc.get (), ps);
          if (!nexpr)
            return nullptr;
          index.release ();
          subsc.release ();
          return nexpr;
        }
      
      case TOK_CARET:
      case TOK_RANGE:
        return _parse_range (left, ps);
      
      case TOK_OF:
        return _parse_of_type_right (left, ps);
      
      // postfix operators
      case TOK_INC:
      case TOK_DEC:
        return _parse_postfix (left, ps);
      
      default: ;
      }
    
    return left;
  }
  
  /* 
   * Identifiers, numbers, and pretty much any type of literal.
   */
  ast_expr*
  _parse_atom (parser_state& ps)
  {
    std::unique_ptr<ast_expr> left { _parse_atom_main (ps) };
    if (!left.get ())
      return nullptr;
    
    ast_expr *expr = _parse_atom_rest (left.get (), ps);
    if (!expr)
      return nullptr;
    
    left.release ();
    return expr;
  }
  
  
  
  static ast_expr*
  _parse_expr_5_rest (ast_expr* parent, parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    token tok = toks.peek_next ();
    if (tok.typ == TOK_MUL || tok.typ == TOK_DIV || tok.typ == TOK_MOD)
      {
        toks.next ();
        
        ast_binop_type t = (tok.typ == TOK_MUL)
          ? AST_BINOP_MUL
          : (tok.typ == TOK_DIV)
            ? AST_BINOP_DIV
            : AST_BINOP_MOD;
        
        std::unique_ptr<ast_expr> right { _parse_atom (ps) };
        if (!right.get ())
          return nullptr;
         
        std::unique_ptr<ast_expr> nleft { new ast_binop (parent, right.get (), t) };
        nleft->set_pos (tok.ln, tok.col);
        ast_expr *expr = _parse_expr_5_rest (nleft.get (), ps);
        right.release ();
        nleft.release ();
        return expr;
      }
    
    return parent;
  }
  
  // binary ops:  * / %
  static ast_expr*
  _parse_expr_5 (parser_state& ps)
  {
    std::unique_ptr<ast_expr> left { _parse_atom (ps) };
    if (!left.get ())
      return nullptr;
    
    ast_expr *expr = _parse_expr_5_rest (left.get (), ps);
    if (!expr)
      return nullptr;
    
    left.release ();
    return expr;
  }
  
  
  static ast_expr*
  _parse_expr_4_rest (ast_expr* parent, parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    token tok = toks.peek_next ();
    if (tok.typ == TOK_ADD || tok.typ == TOK_SUB || tok.typ == TOK_TILDE)
      {
        toks.next ();
        
        ast_binop_type t = (tok.typ == TOK_ADD)
          ? AST_BINOP_ADD
          : (tok.typ == TOK_SUB)
            ? AST_BINOP_SUB
            : AST_BINOP_CONCAT;
        
        std::unique_ptr<ast_expr> right { _parse_expr_5 (ps) };
        if (!right.get ())
          return nullptr;
        
        std::unique_ptr<ast_expr> nleft { new ast_binop (parent, right.get (), t) };
        nleft->set_pos (tok.ln, tok.col);
        ast_expr *expr = _parse_expr_4_rest (nleft.get (), ps);
        right.release ();
        nleft.release ();
        return expr;
      }
    
    return parent;
  }
  
  // binary ops:  + - ~
  static ast_expr*
  _parse_expr_4 (parser_state& ps)
  {
    std::unique_ptr<ast_expr> left { _parse_expr_5 (ps) };
    if (!left.get ())
      return nullptr;
    
    ast_expr *expr = _parse_expr_4_rest (left.get (), ps);
    if (!expr)
      return nullptr;
    
    left.release ();
    return expr;
  }
  
  
  // binary ops:  < <= > >=
  static ast_expr*
  _parse_expr_3 (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    std::unique_ptr<ast_expr> left { _parse_expr_4 (ps) };
    if (!left.get ())
      return nullptr;
    
    token tok = toks.peek_next ();
    if (tok.typ == TOK_LT || tok.typ == TOK_LE ||
        tok.typ == TOK_GT || tok.typ == TOK_GE)
      {
        toks.next ();
        ast_binop_type t;
        switch (tok.typ)
          {
          case TOK_LT:  t = AST_BINOP_LT; break;
          case TOK_LE:  t = AST_BINOP_LE; break;
          case TOK_GT:  t = AST_BINOP_GT; break;
          case TOK_GE:  t = AST_BINOP_GE; break;
          
          default: ;
          }
        
        std::unique_ptr<ast_expr> right { _parse_expr_3 (ps) };
        if (!right.get ())
          return nullptr;
        
        auto binop = new ast_binop (left.release (), right.release (), t);
        binop->set_pos (tok.ln, tok.col);
        return binop;
      }
    
    return left.release ();
  }
  
  
  // binary ops:  == != eq
  static ast_expr*
  _parse_expr_2 (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    std::unique_ptr<ast_expr> left { _parse_expr_3 (ps) };
    if (!left.get ())
      return nullptr;
    
    token tok = toks.peek_next ();
    if (tok.typ == TOK_EQ || tok.typ == TOK_NE || tok.typ == TOK_EQ_S)
      {
        toks.next ();
        
        ast_binop_type t;
        switch (tok.typ)
          {
            case TOK_EQ:   t = AST_BINOP_EQ; break;
            case TOK_NE:   t = AST_BINOP_NE; break;
            case TOK_EQ_S: t = AST_BINOP_EQ_S; break;
            default:
              throw std::runtime_error ("_parse_expr_2: invalid binop op");
          }
        
        std::unique_ptr<ast_expr> right { _parse_expr_2 (ps) };
        if (!right.get ())
          return nullptr;
        
        auto binop = new ast_binop (left.release (), right.release (), t);
        binop->set_pos (tok.ln, tok.col);
        return binop;
      }
    
    return left.release ();
  }
  
  
  static parser_context
  _context_from_assign_lhs (ast_expr *lhs, parser_state& ps)
  {
    switch (lhs->get_type ())
      {
      case AST_IDENT:
        {
          ast_ident *ident = static_cast<ast_ident *> (lhs);
          if (ident->get_ident_type () == AST_IDENT_ARRAY)
            return PC_LIST;
        }
        break;
      
      case AST_NAMED_UNARY:
        {
          ast_named_unop *unop = static_cast<ast_named_unop *> (lhs);
          switch (unop->get_op ())
            {
            case AST_UNOP_MY:
              {
                ast_expr *param = unop->get_param ();
                if (param->get_type () == AST_IDENT)
                  {
                    ast_ident *ident = static_cast<ast_ident *> (param);
                    if (ident->get_ident_type () == AST_IDENT_ARRAY)
                      return PC_LIST;
                  }
                else if (param->get_type () == AST_LIST)
                  return PC_LIST;
              }
              break;
            
            default: ;
            }
        }
        break;
      
      default: ;
      }
    
    return PC_NONE;
  }
  
  // binary ops:  =
  static ast_expr*
  _parse_expr_1 (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    std::unique_ptr<ast_expr> left { _parse_expr_2 (ps) };
    if (!left.get ())
      return nullptr;
    
    token tok = toks.peek_next ();
    if (tok.typ == TOK_ASSIGN)
      {
        toks.next ();
        
        context_block ctxb { ps, _context_from_assign_lhs (left.get(), ps) };
        std::unique_ptr<ast_expr> right { _parse_expr_1 (ps) };
        if (!right.get ())
          return nullptr;
        
        auto binop = new ast_binop (left.release (), right.release (), AST_BINOP_ASSIGN);
        binop->set_pos (tok.ln, tok.col);
        return binop;
      }
    
    return left.release ();
  }
  
  
  
  static ast_list*
  _parse_list_no_parens (ast_expr *first, parser_state& ps)
  {
    std::unique_ptr<ast_list> lst { new ast_list () };
    lst->add_elem (first);
    
    token_seq& toks = ps.toks;
    token tok = toks.peek_next ();
    if (tok.typ == TOK_COMMA)
      {
        toks.next ();
        
        ast_expr *elem = _parse_expr (ps);
        if (!elem)
          return nullptr;
        if (elem->get_type () == AST_LIST)
          {
            // flatten out
            ast_list *sublst = static_cast<ast_list *> (elem);
            for (ast_expr *e : sublst->get_elems ())
              lst->add_elem (e);
          }
        else
          lst->add_elem (elem);
      }
    
    return lst.release ();
  }
  
  
  
  static ast_conditional*
  _parse_conditional (ast_expr *left, parser_state& ps)
  {
    token_seq& toks = ps.toks;
    toks.next (); // skip ??
    
    std::unique_ptr<ast_expr> conseq { _parse_expr (ps) };
    if (!conseq.get ())
      return nullptr;
    
    token tok = toks.next ();
    if (tok.typ != TOK_DEXC)
      {
        ps.errs.error (ES_PARSER, "expected '!!' in conditional expression",
          tok.ln, tok.col);
        return nullptr;
      }
    
    std::unique_ptr<ast_expr> alt { _parse_expr (ps) };
    if (!alt.get ())
      return nullptr;
    
    return new ast_conditional (left, conseq.release (), alt.release ());
  }
  
  
  static ast_expr*
  _parse_expr_rest (ast_expr *left, parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    token tok = toks.peek_next ();
    if (tok.typ == TOK_COMMA)
      {
        if (ps.top_context () != PC_LIST)
          ; // TODO: handle regular commas.
        else
          {
            // parse list
            return _parse_list_no_parens (left, ps);
          }
      }
    else if (tok.typ == TOK_DQ)
      return _parse_conditional (left, ps);
    
    return left;
  }
  
  ast_expr*
  _parse_expr (parser_state& ps)
  {
    std::unique_ptr<ast_expr> expr { _parse_expr_1 (ps) };
    if (!expr.get ())
      return nullptr;
    
    ast_expr *nexpr = _parse_expr_rest (expr.get (), ps);
    if (!nexpr)
      return nullptr;
    
    expr.release ();
    return nexpr;
  }
  
  
  
  static ast_return*
  _parse_return (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    toks.next (); // skip 'return'
    
    std::unique_ptr<ast_expr> expr;
    try
      {
        // remember state
        toks.push ();
        ps.errs.silence (true, false);
        
        context_block ctxb { ps, PC_LIST }; // in list context
        std::unique_ptr<ast_expr> t { _parse_expr (ps) };
        if (t.get ())
          expr.reset (t.release ());
      }
    catch (const parse_error& ex)
      {
        // nope
      }
    
    ps.errs.silence (false, true);
    if (!expr.get ())
      {
        // restore state
        toks.restore ();
      }
    
    if (!_skip_scol (ps))
      return nullptr;
    
    return new ast_return (expr.release ());
  }
  
  
  
  ast_block*
  _parse_block (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    toks.next ();     // skip '{'
    
    std::unique_ptr<ast_block> block { new ast_block () };
    
    token tok;
    while (toks.has_next ())
      {
        tok = toks.peek_next ();
        if (tok.typ == TOK_RBRACE)
          {
            // end of block
            toks.next ();
            break;
          }
        
        // continue parsing when errors are encountered to accumulate them.
        ast_stmt *stmt = _parse_stmt (ps);
        if (stmt)
          block->add_stmt (stmt);
      }
    
    return block.release ();
  }
  
  
  static ast_if*
  _parse_if (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    toks.next ();     // skip 'if'
    
    /* 
     * main part.
     */
    
    std::unique_ptr<ast_expr> mcond { _parse_expr (ps) };
    if (!mcond.get ())
      return nullptr;
    
    token tok = toks.peek_next ();
    if (tok.typ != TOK_LBRACE)
      {
        ps.errs.error (ES_PARSER, "expected block ('{') after if condition expression",
          tok.ln, tok.col);
        return nullptr;
      }
    
    std::unique_ptr<ast_block> mbody { _parse_block (ps) };
    if (!mbody.get ())
      return nullptr;
    
    std::unique_ptr<ast_if> ast { new ast_if (mcond.release (), mbody.release ()) };
    
    /* 
     * elsif parts.
     */
    for (;;)
      {
        tok = toks.peek_next ();
        if (tok.typ != TOK_ELSIF)
          break;
        
        toks.next ();
        
        std::unique_ptr<ast_expr> cond { _parse_expr (ps) };
        if (!cond.get ())
          return nullptr;
        
        token tok = toks.peek_next ();
        if (tok.typ != TOK_LBRACE)
          {
            ps.errs.error (ES_PARSER, "expected block ('{') after elsif condition expression",
              tok.ln, tok.col);
            return nullptr;
          }
        
        std::unique_ptr<ast_block> body { _parse_block (ps) };
        if (!body.get ())
          return nullptr;
        
        ast->add_elsif (cond.release (), body.release ());
      }
    
    /*
     * else part.
     */
    
    tok = toks.peek_next ();
    if (tok.typ == TOK_ELSE)
      {
        toks.next ();
        
        token tok = toks.peek_next ();
        if (tok.typ != TOK_LBRACE)
          {
            ps.errs.error (ES_PARSER, "expected block ('{') after elsif condition expression",
              tok.ln, tok.col);
            return nullptr;
          }
        
        std::unique_ptr<ast_block> body { _parse_block (ps) };
        if (!body.get ())
          return nullptr;
        
        ast->add_else (body.release ());
      }
    
    return ast.release ();
  }
  
  
  
  static ast_while*
  _parse_while (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    toks.next ();     // skip 'while'
    
    std::unique_ptr<ast_expr> cond { _parse_expr (ps) };
    if (!cond.get ())
      return nullptr;
    
    token tok = toks.peek_next ();
    if (tok.typ != TOK_LBRACE)
      {
        ps.errs.error (ES_PARSER, "expected block ('{') after while condition expression",
          tok.ln, tok.col);
        return nullptr;
      }
    
    std::unique_ptr<ast_block> body { _parse_block (ps) };
    if (!body.get ())
      return nullptr;
    
    return new ast_while (cond.release (), body.release ());
  }
  
  
  
  static ast_for*
  _parse_for (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    toks.next ();     // skip 'for'
    
    std::unique_ptr<ast_expr> arg { _parse_expr (ps) };
    if (!arg.get ())
      return nullptr;
    
    token tok = toks.next ();
    if (tok.typ != TOK_LARROW)
      {
        ps.errs.error (ES_PARSER, "expected '->' after for expression argument",
          tok.ln, tok.col);
        return nullptr;
      }
    
    std::unique_ptr<ast_ident> var { _parse_ident (ps) };
    if (!var.get ())
      return nullptr;
    
    tok = toks.peek_next ();
    if (tok.typ != TOK_LBRACE)
      {
        ps.errs.error (ES_PARSER, "expected block ('{') after for variable",
          tok.ln, tok.col);
        return nullptr;
      }
    
    std::unique_ptr<ast_block> body { _parse_block (ps) };
    if (!body.get ())
      return nullptr;
    
    return new ast_for (arg.release (), var.release (), body.release ());
  }
  
  
  
  static ast_loop*
  _parse_loop (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    toks.next ();     // skip 'loop'
    
    std::unique_ptr<ast_expr> init;
    std::unique_ptr<ast_expr> cond;
    std::unique_ptr<ast_expr> step;
    
    token tok = toks.peek_next ();
    if (tok.typ == TOK_LPAREN)
      {
        // (<init>; <cond>; <step>)
        toks.next ();
        
        // init
        tok = toks.peek_next ();
        if (tok.typ != TOK_SCOL)
          {
            init.reset (_parse_expr (ps));
            if (!init.get ())
              return nullptr;
            
            tok = toks.peek_next ();
            if (tok.typ != TOK_SCOL)
              {
                ps.errs.error (ES_PARSER,
                  "expected ';' after <init> expression in loop statement",
                  tok.ln, tok.col);
                return nullptr;
              }
          }
        toks.next ();
        
        // cond
        tok = toks.peek_next ();
        if (tok.typ != TOK_SCOL)
          {
            cond.reset (_parse_expr (ps));
            if (!cond.get ())
              return nullptr;
            
            tok = toks.peek_next ();
            if (tok.typ != TOK_SCOL)
              {
                ps.errs.error (ES_PARSER,
                  "expected ';' after <cond> expression in loop statement",
                  tok.ln, tok.col);
                return nullptr;
              }
          }
        toks.next ();
        
        // step
        tok = toks.peek_next ();
        if (tok.typ != TOK_RPAREN)
          {
            step.reset (_parse_expr (ps));
            if (!step.get ())
              return nullptr;
            
            tok = toks.peek_next ();
            if (tok.typ != TOK_RPAREN)
              {
                ps.errs.error (ES_PARSER,
                  "expected ')' after <step> expression in loop statement",
                  tok.ln, tok.col);
                return nullptr;
              }
          }
        toks.next ();
      }
    
    // body
    tok = toks.peek_next ();
    if (tok.typ != TOK_LBRACE)
      {
        ps.errs.error (ES_PARSER, "expected block ('{')", tok.ln, tok.col);
        return nullptr;
      }
    std::unique_ptr<ast_block> body { _parse_block (ps) };
    if (!body.get ())
      return nullptr;
    
    return new ast_loop (body.release (), init.release (), cond.release (),
      step.release ());
  }
  
  
  
  static ast_use*
  _parse_use (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    toks.next (); // skip 'use'
    
    // TODO: support ::s
    token tok = toks.next ();
    if (tok.typ != TOK_IDENT_NONE)
      {
        ps.errs.error (ES_PARSER, "expected a name after `use'", tok.ln, tok.col);
        return nullptr;
      }
    std::string what = tok.val.str;
    
    if (!_skip_scol (ps))
      return nullptr;
    
    return new ast_use (what);
  }
  
  
  
  ast_stmt*
  _parse_stmt (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    switch (toks.peek_next ().typ)
      {
      case TOK_LBRACE:
        return _parse_block (ps);
      
      case TOK_SUBROUTINE:
        return _parse_subroutine (ps);
      
      case TOK_RETURN:
        return _parse_return (ps);
      
      case TOK_IF:
        return _parse_if (ps);
      
      case TOK_WHILE:
        return _parse_while (ps);
      
      case TOK_FOR:
        return _parse_for (ps);
      
      case TOK_LOOP:
        return _parse_loop (ps);
      
      case TOK_USE:
        return _parse_use (ps);
      
      // empty statement
      case TOK_SCOL:
        {
          toks.next ();
          return new ast_expr_stmt (new ast_undef ());
        }
      
      default:
        {
          std::unique_ptr<ast_expr> expr { _parse_expr (ps) };
          if (!expr.get ())
            return nullptr;
          
          if (!_skip_scol (ps))
            return nullptr;
          
          return new ast_expr_stmt (expr.release ());
        }
      }
  }
  
  
  static ast_block*
  _parse_package_body (parser_state& ps, bool got_braces)
  {
    token_seq& toks = ps.toks;
    token tok;
    
    if (got_braces)
      {
        tok = toks.next ();
        if (tok.typ != TOK_LBRACE)
          {
            ps.errs.error (ES_PARSER, "expected '{'", tok.ln, tok.col);
            return nullptr;
          }
      }
    
    std::unique_ptr<ast_block> body { new ast_block () };
    while (toks.has_next ())
      {
        tok = toks.peek_next ();
        if (tok.typ == TOK_EOF)
          {
            if (got_braces)
              {
                ps.errs.error (ES_PARSER, "unexpected EOF inside module body",
                  tok.ln, tok.col);
                return nullptr;
              }
            break;
          }
        else if (got_braces && tok.typ == TOK_RBRACE)
          {
            toks.next ();
            break;
          }
        
        // handle top-level statements
        switch (tok.typ)
          {
          case TOK_MODULE:
          case TOK_PACKAGE:
            {
              ast_package *pkg = _parse_package_or_module (ps);
              if (pkg)
                body->add_stmt (pkg);
            }
            break;
          
          default:
            {
              ast_stmt *stmt = _parse_stmt (ps);
              if (stmt)
                body->add_stmt (stmt);
            }
            break;
          }
      }
    
    return body.release ();
  }
  
  ast_package*
  _parse_package_or_module (parser_state& ps)
  {
    token_seq& toks = ps.toks;
    
    token ftok = toks.next (); // skip 'module' or 'package'
    
    // name
    // TODO: handle ::s
    token tok = toks.next ();
    if (tok.typ != TOK_IDENT_NONE)
      {
        ps.errs.error (ES_PARSER, "expected module name after `module'",
          tok.ln, tok.col);
        return nullptr;
      }
    std::string name = tok.val.str;
    
    // body
    bool as_block = false;
    tok = toks.peek_next ();
    if (tok.typ == TOK_SCOL)
      toks.next ();
    else
      as_block = true;
    ast_block *body = _parse_package_body (ps, as_block);
    if (!body)
      return nullptr;
    
    if (ftok.typ == TOK_PACKAGE)
      return new ast_package (name, body);
    else if (ftok.typ == TOK_MODULE)
      return new ast_module (name, body);
    throw std::runtime_error ("expected module or package");
  }
  
  
  static ast_program*
  _parse_program (parser_state& ps)
  {
    std::unique_ptr<ast_program> program { new ast_program () };
    std::unique_ptr<ast_block> body { _parse_package_body (ps, false) };
    if (!body.get ())
      return nullptr;
    
    program->set_body (body.release ());
    return program.release ();
  }
  
  
  
  /* 
   * Parses the Perl code contained in the specified input stream and
   * returns an AST tree representation.
   */
  ast_program*
  parser::parse (std::istream& strm)
  {
    lexer lex {};
    lex.tokenize (strm);
    
    token_seq toks = lex.get_token_seq ();
    parser_state ps { toks, this->errs };
    return _parse_program (ps);
  }
  
  /* 
   * Parses the Perl code contained in the specified string and
   * returns an AST tree representation.
   */
  ast_program*
  parser::parse (const std::string& str)
  {
    std::istringstream ss { str };
    return this->parse (ss);
  }
}

