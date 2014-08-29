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

#ifndef _ARANE__AST__H_
#define _ARANE__AST__H_

#include "common/types.hpp"
#include <vector>
#include <string>


namespace arane {
  
  enum ast_type
  {
    AST_UNDEF,
    AST_IDENT,
    AST_INTEGER,
    AST_BOOL,
    AST_STRING,
    AST_INTERP_STRING,
    AST_LIST,
    AST_ANONYM_ARRAY,
    AST_MY,
    AST_SUB_CALL,
    AST_SUBSCRIPT,
    AST_BINARY,
    AST_NAMED_UNARY,
    AST_EXPR_STMT,
    AST_BLOCK,
    AST_PROGRAM,
    AST_SUB,
    AST_RETURN,
    AST_IF,
    AST_REF,
    AST_DEREF,
    AST_WHILE,
    AST_FOR,
    AST_RANGE,
    AST_LOOP,
    AST_MODULE,
    AST_PACKAGE,
    AST_USE,
    AST_CONDITIONAL,
    AST_OF_TYPE,
    AST_PREFIX,
    AST_POSTFIX,
  };
  
  
  /* 
   * The base class of all AST tree types.
   */
  class ast_node
  {
    // where this tree appears in the code.
    int ln, col;
    
  protected:
    std::vector<std::string> traits;
    
  public:
    inline int get_line () const { return this->ln; }
    inline int get_column () const { return this->col; }
    
    inline void set_pos (int ln, int col)
      { this->ln = ln; this->col = col; }
    
  public:
    ast_node ();
    virtual ~ast_node () { }
    
  public:
    inline std::vector<std::string>& get_traits () { return this->traits; }
    
    void
    add_trait (const std::string& str)
      { this->traits.push_back (str); }
    
  public:
    /* 
     * Returns the AST node's type.
     */
    virtual ast_type get_type () const = 0;
  };
  
  
  /* 
   * AST nodes that have a value associated with them.
   */
  class ast_expr: public ast_node
    { };
  
  
  /* 
   * An AST node that expresses some kind of action, and does not have a value
   * associated with it.
   */
  class ast_stmt: public ast_node
    { };
  
  
  /* 
   * An expression whose value is ignored (and thus becomes a statement).
   */
  class ast_expr_stmt: public ast_stmt
  {
    ast_expr *expr;
    
  public:
    virtual ast_type get_type () const override { return AST_EXPR_STMT; }
    
    inline ast_expr* get_expr () { return this->expr; }
    
  public:
    ast_expr_stmt (ast_expr *expr);
    ~ast_expr_stmt ();
  };
  
  
  
  /* 
   * A linear sequence of zero or more statements.
   */
  class ast_block: public ast_stmt
  {
    std::vector<ast_stmt *> stmts;
    
  public:
    virtual ast_type get_type () const override { return AST_BLOCK; }
    
    inline std::vector<ast_stmt *>& get_stmts () { return this->stmts; }
    
  public:
    ~ast_block ();
    
  public:
    void add_stmt (ast_stmt *stmt);
  };
  
  
  
  enum ast_ident_type
  {
    AST_IDENT_NONE,
    AST_IDENT_SCALAR,
    AST_IDENT_ARRAY,
    AST_IDENT_HASH,
    AST_IDENT_HANDLE,
  };
  
  /* 
   * An identifier ($name, @name, %name, ...)
   */
  class ast_ident: public ast_expr
  {
    std::string name;
    ast_ident_type type;
    
  public:
    virtual ast_type get_type () const override { return AST_IDENT; }
    
    inline const std::string& get_name () const { return this->name; }
    inline ast_ident_type get_ident_type () const { return this->type; }
    
    std::string get_decorated_name () const;
    
  public:
    ast_ident (const std::string& name, ast_ident_type type);
  };
  
  
  class ast_undef: public ast_expr
  {
  public:
    virtual ast_type get_type () const override { return AST_UNDEF; }
  };
  
  
  /* 
   * A 64-bit whole number.
   */
  class ast_integer: public ast_expr
  {
    long long val;
    
  public:
    virtual ast_type get_type () const override { return AST_INTEGER; }
    
    inline long long get_value () const { return this->val; }
    
  public:
    ast_integer (long long val);
  };
  
  
  /* 
   * A boolean value.
   */
  class ast_bool: public ast_expr
  {
    bool val;
    
  public:
    virtual ast_type get_type () const override { return AST_BOOL; }
    
    inline bool get_value () const { return this->val; }
    
  public:
    ast_bool (bool val);
  };
  
  
  /* 
   * A regular (non-interpolated) string.
   */
  class ast_string: public ast_expr
  {
    std::string str;
    
  public:
    virtual ast_type get_type () const override { return AST_STRING; }
    
    inline const std::string& get_value () const { return this->str; }
    
  public:
    ast_string (const std::string& str);
  };
  
  
  
  class ast_interp_string: public ast_expr
  {
  public:
    enum entry_type
    {
      ISET_PART,
      ISET_EXPR,
    };
    
    struct entry
    {
      entry_type type;
      union
        {
          char *str;
          ast_expr *expr;
        } val;
    };
  
  private:
    std::vector<entry> entries;
  
  public:
    virtual ast_type get_type () const override { return AST_INTERP_STRING; }
    
    inline std::vector<entry>& get_entries () { return this->entries; }
    
  public:
    ~ast_interp_string ();
    
  public:
    void add_part (const std::string& str);
    void add_expr (ast_expr *expr);
  };
  
  
  
  /* 
   * A list of values.
   *     (a, b, c, ...)
   */
  class ast_list: public ast_expr
  {
    std::vector<ast_expr *> elems;
    
  public:
    virtual ast_type get_type () const override { return AST_LIST; }
    
    inline std::vector<ast_expr *>& get_elems () { return this->elems; }
    
  public:
    ~ast_list ();
    
  public:
    void add_elem (ast_expr *expr);
  };
  
  
  /* 
   *  [a, b, c, ...]
   */
  class ast_anonym_array: public ast_list
  {
  public:
    virtual ast_type get_type () const override { return AST_ANONYM_ARRAY; }
  };
  
  
  /* 
   * <expr>[<index>]
   */
  class ast_subscript: public ast_expr
  {
    ast_expr *expr;
    ast_expr *index;
    
  public:
    virtual ast_type get_type () const override { return AST_SUBSCRIPT; }
    
    inline ast_expr* get_expr () { return this->expr; }
    inline ast_expr* get_index () { return this->index; }
    
  public:
    ast_subscript (ast_expr *expr, ast_expr *index);
    ~ast_subscript ();
  };
  
  
  enum ast_binop_type
  {
    AST_BINOP_ASSIGN,             //   =
    AST_BINOP_ADD,                //   +
    AST_BINOP_SUB,                //   -
    AST_BINOP_MUL,                //   *
    AST_BINOP_DIV,                //   /
    AST_BINOP_MOD,                //   %
    AST_BINOP_CONCAT,             //   .
    
    AST_BINOP_EQ,                 //   ==
    AST_BINOP_NE,                 //   !=
    AST_BINOP_LT,                 //   <
    AST_BINOP_LE,                 //   <=
    AST_BINOP_GT,                 //   >
    AST_BINOP_GE,                 //   >=
    
    AST_BINOP_EQ_S,               //   eq
  };
  
  /* 
   * A binary operation.
   *     <lhs> OP <rhs>
   */
  class ast_binop: public ast_expr
  {
    ast_binop_type type;
    ast_expr *lhs;
    ast_expr *rhs;
    
  public:
    virtual ast_type get_type () const override { return AST_BINARY; }
    
    inline ast_binop_type get_op () { return this->type; }
    inline ast_expr* get_lhs () { return this->lhs; }
    inline ast_expr* get_rhs () { return this->rhs; }
    
  public:
    ast_binop (ast_expr *lhs, ast_expr *rhs, ast_binop_type type);
    ~ast_binop ();
  };
  
  
  /* 
   * Subroutine invocation.
   */
  class ast_sub_call: public ast_expr
  {
    std::string name;
    ast_list *params;
    
  public:
    virtual ast_type get_type () const override { return AST_SUB_CALL; }
    
    inline const std::string& get_name () const { return this->name; }
    inline ast_list* get_params () { return this->params; }
    
  public:
    ast_sub_call (const std::string& name, ast_list *params);
    ~ast_sub_call ();
  };
  
  
  enum ast_unop_type
  {
    AST_UNOP_MY,
    
    // types:
    AST_UNOP_TYPE_INT_NATIVE,
    AST_UNOP_TYPE_INT,
  };
  
  /* 
   * 'my', 'our', etc...
   * These are essentially subroutine calls that require parentheses for more
   * than one parameter.
   */
  class ast_named_unop: public ast_expr
  {
    ast_unop_type type;
    ast_expr *param;
    
  public:
    virtual ast_type get_type () const override { return AST_NAMED_UNARY; }
    
    inline ast_unop_type get_op () const { return this->type; }
    inline ast_expr* get_param () { return this->param; }
    
  public:
    ast_named_unop (ast_unop_type type, ast_expr *param);
    ~ast_named_unop ();
  };
  
  
  struct ast_sub_param
  {
    ast_expr *expr;
  };
  
  /* 
   * Subroutine definition.
   */
  class ast_sub: public ast_stmt
  {
    std::string name;
    std::vector<ast_sub_param> params;
    ast_block *body;
    type_info ret_type;
    
  public:
    virtual ast_type get_type () const override { return AST_SUB; }
    
    inline const std::string& get_name () const { return this->name; }
    inline std::vector<ast_sub_param>& get_params () { return this->params; }
    inline ast_block* get_body () { return this->body; }
    inline const type_info& get_return_type () { return this->ret_type; }
    
  public:
    ast_sub (const std::string& name);
    ~ast_sub ();
    
  public:
    void add_param (ast_expr *param);
    void set_body (ast_block *block);
    
    void set_return_type (const type_info& ti);
  };
  
  
  /* 
   * return <expr>;
   */
  class ast_return: public ast_stmt
  {
    ast_expr *expr;
    bool implicit;
    
  public:
    virtual ast_type get_type () const override { return AST_RETURN; }
    
    inline ast_expr* get_expr () { return this->expr; }
    inline bool is_implicit () const { return this->implicit; }
    
  public:
    ast_return (ast_expr *expr = nullptr, bool implicit = false);
    ~ast_return ();
  };
  
  
  /* 
   * Represents a whole module.
   */
  class ast_program: public ast_sub
  {
  public:
    virtual ast_type get_type () const override { return AST_PROGRAM; }
    
  public:
    ast_program ()
      : ast_sub ("#PROGRAM")
      { }
  };
  
  
  
  struct ast_if_part 
  {
    ast_expr *cond;
    ast_block *body;
  };
  
  /* 
   * An if statement.
   *     if <expr> {
   *        <body>
   *     }
   *     elsif <expr> {
   *        <body>
   *     }
   *     ...
   *     elsif <expr> {
   *        <body>
   *     }
   *     else {
   *        <body>
   *     }
   * 
   * All but the if clause are optional.
   */
  class ast_if: public ast_stmt
  {
    ast_if_part main_part;
    std::vector<ast_if_part> elsifs;
    ast_block *else_part;
    
  public:
    virtual ast_type get_type () const override { return AST_IF; }
    
    inline ast_if_part get_main_part () { return this->main_part; }
    inline ast_block* get_else_part () { return this->else_part; }
    inline std::vector<ast_if_part>& get_elsif_parts () { return this->elsifs; }
    
  public:
    ast_if (ast_expr *cond, ast_block *body);
    ~ast_if ();
    
  public:
    void add_elsif (ast_expr *cond, ast_block *body);
    void add_else (ast_block *body);
  };
  
  
  /* 
   * \<expr>
   */
  class ast_ref: public ast_expr
  {
    ast_expr *expr;
    
  public:
    virtual ast_type get_type () const override { return AST_REF; }
    
    inline ast_expr* get_expr () { return this->expr; }
    
  public:
    ast_ref (ast_expr *expr);
    ~ast_ref ();
  };
  
  
  /* 
   * ${<expr>}
   */
  class ast_deref: public ast_expr
  {
    ast_expr *expr;
    
  public:
    virtual ast_type get_type () const override { return AST_DEREF; }
    
    inline ast_expr* get_expr () { return this->expr; }
    
  public:
    ast_deref (ast_expr *expr);
    ~ast_deref ();
  };
  
  
  /* 
   * while <expr> {
   *     <body>
   * }
   */
  class ast_while: public ast_stmt
  {
    ast_expr *cond;
    ast_block *body;
    
  public:
    virtual ast_type get_type () const override { return AST_WHILE; }
    
    inline ast_expr* get_cond () { return this->cond; }
    inline ast_block* get_body () { return this->body; }
    
  public:
    ast_while (ast_expr *cond, ast_block *body);
    ~ast_while ();
  };
  
  
  /* 
   * for <expr> -> {var} {
   *     <body>
   * }
   */
  class ast_for: public ast_stmt
  {
    ast_expr *arg;
    ast_ident *var;
    ast_block *body;
    
  public:
    virtual ast_type get_type () const override { return AST_FOR; }
    
    inline ast_expr* get_arg () { return this->arg; }
    inline ast_ident* get_var () { return this->var; }
    inline ast_block* get_body () { return this->body; }
    
  public:
    ast_for (ast_expr *arg, ast_ident *var, ast_block *body);
    ~ast_for ();
  };
  
  
  /* 
   * <start> ^..^ <end>
   * (carets (^) are optional on either side)
   */
  class ast_range: public ast_expr
  {
    ast_expr *lhs, *rhs;
    bool lhs_exc, rhs_exc;
    
  public:
    virtual ast_type get_type () const override { return AST_RANGE; }
    
    inline ast_expr* get_lhs () { return this->lhs; }
    inline ast_expr* get_rhs () { return this->rhs; }
    
    inline bool lhs_exclusive () { return this->lhs_exc; }
    inline bool rhs_exclusive () { return this->rhs_exc; }
    
  public:
    ast_range (ast_expr *lhs, bool lhs_exc, ast_expr *rhs, bool rhs_exc);
    ~ast_range ();
  };
  
  
  /* 
   * loop (<init>; <cond>; <step>) {
   *     <body>
   * }
   * 
   * or just:
   * 
   * loop {
   *     <body>
   * }
   */
  class ast_loop: public ast_stmt
  {
    ast_expr *init;
    ast_expr *cond;
    ast_expr *step;
    ast_block *body;
    
  public:
    virtual ast_type get_type () const override { return AST_LOOP; }
    
    inline ast_expr* get_init () { return this->init; }
    inline ast_expr* get_cond () { return this->cond; }
    inline ast_expr* get_step () { return this->step; }
    inline ast_block* get_body () { return this->body; }
    
  public:
    ast_loop (ast_block *body, ast_expr *init, ast_expr *cond, ast_expr *step);
    ~ast_loop ();
  };
  
  
  
  /* 
   * package <name>;
   * or:
   * package <name> {
   *    <body>
   * }
   */
  class ast_package: public ast_stmt
  {
    std::string name;
    ast_block *body;
    
  public:
    virtual ast_type get_type () const override { return AST_PACKAGE; }
    
    inline const std::string& get_name () { return this->name; }
    inline ast_block* get_body () { return this->body; }
    
  public:
    ast_package (const std::string& name, ast_block *body);
    ~ast_package ();
  };
  
  
  
  /* 
   * Special type of package.
   */
  class ast_module: public ast_package
  {
  public:
    virtual ast_type get_type () const override { return AST_MODULE; }
    
  public:
    ast_module (const std::string& name, ast_block *body);
  };
  
  
  
  /* 
   * use <module>;
   */
  class ast_use: public ast_stmt
  {
    std::string what;
    
  public:
    virtual ast_type get_type () const override { return AST_USE; }
    
    inline const std::string& get_value () { return this->what; }
    
  public:
    ast_use (const std::string& what);
  };
  
  
  
  /* 
   * <expr> ?? <expr> !! <expr>;
   */
  class ast_conditional: public ast_expr
  {
    ast_expr *test;
    ast_expr *conseq;
    ast_expr *alt;
    
  public:
    virtual ast_type get_type () const override { return AST_CONDITIONAL; }
    
    inline ast_expr *get_test () { return this->test; }
    inline ast_expr *get_conseq () { return this->conseq; }
    inline ast_expr *get_alt () { return this->alt; }
    
  public:
    ast_conditional (ast_expr *test, ast_expr *conseq, ast_expr *alt);
    ~ast_conditional ();
  };
  
  
  
  /*
   * Associates an expression with a type.
   * <expr>:of(<type>)
   * <expr> of <type>
   */
  class ast_of_type: public ast_expr
  {
    ast_expr *expr;
    type_info ti;
    
  public:
    virtual ast_type get_type () const override { return AST_OF_TYPE; }
    
    inline ast_expr* get_expr () { return this->expr; }
    inline const type_info& get_typeinfo () { return this->ti; }
    
  public:
    ast_of_type (ast_expr *expr, const type_info& ti);
    ~ast_of_type ();
  };
  
  
  /* 
   * Unary operator.
   */
  class ast_unop: public ast_expr
  {
    ast_expr *expr;
    
  public:
    inline ast_expr* get_expr () { return this->expr; }
    
  public:
    ast_unop (ast_expr *expr);
    ~ast_unop ();
  };
  
  
  
  enum ast_prefix_type
  {
    AST_PREFIX_INC,       // ++<expr>
    AST_PREFIX_DEC,       // --<expr>
    AST_PREFIX_STR,       // ~<expr>
  };
  
  class ast_prefix: public ast_unop
  {
    ast_prefix_type op;
    
  public:
    virtual ast_type get_type () const override { return AST_PREFIX; }
    
    inline ast_prefix_type get_op () const { return this->op; }
    
  public:
    ast_prefix (ast_expr *expr, ast_prefix_type op);
  };
  
  
  
  enum ast_postfix_type
  {
    AST_POSTFIX_INC,       // <expr>++
    AST_POSTFIX_DEC,       // <expr>--
  };
  
  class ast_postfix: public ast_unop
  {
    ast_postfix_type op;
    
  public:
    virtual ast_type get_type () const override { return AST_POSTFIX; }
  
    inline ast_postfix_type get_op () const { return this->op; }
    
  public:
    ast_postfix (ast_expr *expr, ast_postfix_type op);
  };
}

#endif
