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

#include "parser/ast.hpp"
#include <cstring>


namespace arane {
  
  ast_node::ast_node ()
  {
    this->ln = 0;
    this->col = 0;
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_expr_stmt::ast_expr_stmt (ast_expr *expr)
  {
    this->expr = expr;
  }
  
  ast_expr_stmt::~ast_expr_stmt ()
  {
    delete this->expr;
  }
  
  
  
  ast_node*
  ast_expr_stmt::clone ()
  {
    return new ast_expr_stmt (static_cast<ast_expr *> (this->expr->clone ()));
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_node*
  ast_undef::clone ()
    { return new ast_undef (); }
  
  
  
//------------------------------------------------------------------------------
  
  ast_block::~ast_block ()
  {
    for (ast_stmt *stmt : this->stmts)
      delete stmt;
  }
  
  void
  ast_block::add_stmt (ast_stmt *stmt)
  {
    this->stmts.push_back (stmt);
  }
  
  
  
  ast_node*
  ast_block::clone ()
  {
    ast_block *copy = new ast_block ();
    for (auto stmt : this->stmts)
      copy->add_stmt (static_cast<ast_stmt *> (stmt->clone ()));
    return copy;
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_ident::ast_ident (const std::string& name, ast_ident_type type)
    : name (name)
  {
    this->type = type;
  }
  
  
  std::string
  ast_ident::get_decorated_name () const
  {
    switch (this->type)
      {
      case AST_IDENT_NONE: return this->name;
      case AST_IDENT_SCALAR: return "$" + this->name;
      case AST_IDENT_ARRAY: return "@" + this->name;
      case AST_IDENT_HASH: return "%" + this->name;
      case AST_IDENT_HANDLE: return "&" + this->name;
      
      default: return "<?>" + this->name;
      }
  }
  
  
  
  ast_node*
  ast_ident::clone ()
  {
    return new ast_ident (this->name, this->type);
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_integer::ast_integer (long long val)
  {
    this->val = val;
  }
  
  
  
  ast_node*
  ast_integer::clone ()
  {
    return new ast_integer (this->val);
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_bool::ast_bool (bool val)
  {
    this->val = val;
  }
  
  
  
  ast_node*
  ast_bool::clone ()
  {
    return new ast_bool (this->val);
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_string::ast_string (const std::string& str)
    : str (str)
    { }
  
  
  
  ast_node*
  ast_string::clone ()
  {
    return new ast_string (this->str);
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_interp_string::~ast_interp_string ()
  {
    for (entry ent : this->entries)
      {
        if (ent.type == ISET_PART)
          delete[] ent.val.str;
        else
          delete ent.val.expr;
      }
  }
  
  
  
  ast_node*
  ast_interp_string::clone ()
  {
    ast_interp_string *copy = new ast_interp_string ();
    
    for (auto ent : this->entries)
      if (ent.type == ISET_PART)
        copy->add_part (ent.val.str);
      else
        copy->add_expr (static_cast<ast_expr *> (ent.val.expr->clone ()));
    
    return copy;
  }
  
  
    
  void
  ast_interp_string::add_part (const std::string& str)
  {
    entry ent;
    ent.type = ISET_PART;
    ent.val.str = new char [str.length () + 1];
    std::strcpy (ent.val.str, str.c_str ());
    
    this->entries.push_back (ent);
  }
  
  void
  ast_interp_string::add_expr (ast_expr *expr)
  {
    entry ent;
    ent.type = ISET_EXPR;
    ent.val.expr = expr;
    
    this->entries.push_back (ent);
  }

  
  
//------------------------------------------------------------------------------
  
  ast_list::~ast_list ()
  {
    for (ast_expr *elem : this->elems)
      delete elem;
  }
  
  
  
  ast_node*
  ast_list::clone ()
  {
    auto copy = new ast_list ();
    for (auto elem : this->elems)
      copy->add_elem (static_cast<ast_expr *> (elem->clone ()));
    return copy;
  }
 
 
    
  void
  ast_list::add_elem (ast_expr *expr)
  {
    this->elems.push_back (expr);
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_node*
  ast_anonym_array::clone ()
  {
    auto copy = new ast_anonym_array ();
    for (auto elem : this->get_elems ())
      copy->add_elem (static_cast<ast_expr *> (elem->clone ()));
    return copy;
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_subscript::ast_subscript (ast_expr *expr, ast_expr *index)
  {
    this->expr = expr;
    this->index = index;
  }
  
  ast_subscript::~ast_subscript ()
  {
    delete this->expr;
    delete this->index;
  }
  
  
  
  ast_node*
  ast_subscript::clone ()
  {
    return new ast_subscript (static_cast<ast_expr *> (this->expr->clone ()),
      static_cast<ast_expr *> (this->index->clone ()));
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_binop::ast_binop (ast_expr *lhs, ast_expr *rhs, ast_binop_type type)
  {
    this->lhs = lhs;
    this->rhs = rhs;
    this->type = type;
  }
  
  ast_binop::~ast_binop ()
  {
    delete this->lhs;
    delete this->rhs;
  }
  
  
  
  ast_node*
  ast_binop::clone ()
  {
    return new ast_binop (static_cast<ast_expr *> (this->lhs->clone ()),
      static_cast<ast_expr *> (this->rhs->clone ()), this->type);
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_sub_call::ast_sub_call (const std::string& name, ast_list *params)
    : name (name)
  {
    this->params = params;
  }
  
  ast_sub_call::~ast_sub_call ()
  {
    delete this->params;
  }
  
  
  
  ast_node*
  ast_sub_call::clone ()
  {
    return new ast_sub_call (this->name,
      static_cast<ast_list *> (this->params->clone ()));
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_named_unop::ast_named_unop (ast_unop_type type, ast_expr *param)
  {
    this->type = type;
    this->param = param;
  }
  
  ast_named_unop::~ast_named_unop ()
  {
    delete this->param;
  }
  
  
  
  ast_node*
  ast_named_unop::clone ()
  {
    return new ast_named_unop (this->type,
      static_cast<ast_expr *> (this->param->clone ()));
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_sub::ast_sub (const std::string& name)
    : name (name)
  {
    this->body = nullptr;
    this->ret_type = type_info::none ();
  }
  
  ast_sub::~ast_sub ()
  {
    for (ast_sub_param& param : this->params)
      delete param.expr;
    delete this->body;
  }
  
  
  
  ast_node*
  ast_sub::clone ()
  {
    ast_sub *copy = new ast_sub (this->name);
    copy->set_return_type (this->ret_type);
    copy->set_body (static_cast<ast_block *> (this->body->clone ()));
    
    for (auto p : this->params)
      copy->add_param (static_cast<ast_expr *> (p.expr->clone ()));
    
    return copy;
  }
  
  
  
  void
  ast_sub::add_param (ast_expr *param)
  {
    this->params.push_back ({
      .expr = param,
    });
  }
  
  void
  ast_sub::set_body (ast_block *block)
  {
    this->body = block;
  }
  
  void
  ast_sub::set_return_type (const type_info& ti)
  {
    this->ret_type = ti;
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_return::ast_return (ast_expr *expr, bool implicit)
  {
    this->expr = expr;
    this->implicit = implicit;
  }
  
  ast_return::~ast_return ()
  {
    delete this->expr;
  }
  
  
  
  ast_node*
  ast_return::clone ()
  {
    return new ast_return (static_cast<ast_expr *> (this->expr->clone ()),
      this->implicit);
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_if::ast_if (ast_expr *cond, ast_block *body)
  {
    this->main_part = { cond, body };
    this->else_part = nullptr;
  }
  
  ast_if::~ast_if ()
  {
    delete this->main_part.cond;
    delete this->main_part.body;
    
    delete this->else_part;
    
    for (auto part : this->elsifs)
      {
        delete part.cond;
        delete part.body;
      }
  }
  
  
  
  ast_node*
  ast_if::clone ()
  {
    auto copy = new ast_if (
      static_cast<ast_expr *> (this->main_part.cond->clone ()),
      static_cast<ast_block *> (this->main_part.body->clone ()));
    
    if (this->else_part)
      copy->add_else (static_cast<ast_block *> (this->else_part->clone ()));
    
    for (auto p : this->elsifs)
      copy->add_elsif (static_cast<ast_expr *> (p.cond->clone ()),
        static_cast<ast_block *> (p.body->clone ()));
    
    return copy;
  }
  
  
  
  void
  ast_if::add_elsif (ast_expr *cond, ast_block *body)
  {
    this->elsifs.push_back ({ cond, body });
  }
  
  void
  ast_if::add_else (ast_block *body)
  {
    this->else_part = body;
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_deref::ast_deref (ast_expr *expr)
  {
    this->expr = expr;
  }
 
  ast_deref::~ast_deref ()
  {
    delete this->expr;
  }
  
  
  
  ast_node*
  ast_deref::clone ()
  {
    return new ast_deref (static_cast<ast_expr *> (this->expr->clone ()));
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_ref::ast_ref (ast_expr *expr)
  {
    this->expr = expr;
  }
 
  ast_ref::~ast_ref ()
  {
    delete this->expr;
  }
  
  
  
  ast_node*
  ast_ref::clone ()
  {
    return new ast_ref (static_cast<ast_expr *> (this->expr->clone ()));
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_while::ast_while (ast_expr *cond, ast_block *body)
  {
    this->cond = cond;
    this->body = body;
  }
  
  ast_while::~ast_while ()
  {
    delete this->cond;
    delete this->body;
  }
  
  
  
  ast_node*
  ast_while::clone ()
  {
    return new ast_while (static_cast<ast_expr *> (this->cond->clone ()),
      static_cast<ast_block *> (this->body->clone ()));
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_for::ast_for (ast_expr *arg, ast_ident *var, ast_block *body)
  {
    this->arg = arg;
    this->var = var;
    this->body = body;
  }
  
  ast_for::~ast_for ()
  {
    delete this->arg;
    delete this->var;
    delete this->body;
  }
  
  
  
  ast_node*
  ast_for::clone ()
  {
    return new ast_for (static_cast<ast_expr *> (this->arg->clone ()),
      static_cast<ast_ident *> (this->var->clone ()),
      static_cast<ast_block *> (this->body->clone ()));
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_range::ast_range (ast_expr *lhs, bool lhs_exc, ast_expr *rhs, bool rhs_exc)
  {
    this->lhs = lhs;
    this->rhs = rhs;
    this->lhs_exc = lhs_exc;
    this->rhs_exc = rhs_exc;
  }
  
  ast_range::~ast_range ()
  {
    delete this->lhs;
    delete this->rhs;
  }
  
  
  
  ast_node*
  ast_range::clone ()
  {
    return new ast_range (static_cast<ast_expr *> (this->lhs->clone ()),
      this->lhs_exc, static_cast<ast_expr *> (this->rhs->clone ()),
      this->rhs_exc);
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_loop::ast_loop (ast_block *body, ast_expr *init, ast_expr *cond, ast_expr *step)
  {
    this->body = body;
    this->init = init;
    this->cond = cond;
    this->step = step;
  }
  
  ast_loop::~ast_loop ()
  {
    delete this->body;
    delete this->init;
    delete this->cond;
    delete this->step;
  }
  
  
  
  ast_node*
  ast_loop::clone ()
  {
    return new ast_loop (static_cast<ast_block *> (this->body->clone ()),
      static_cast<ast_expr *> (this->init->clone ()),
      static_cast<ast_expr *> (this->cond->clone ()),
      static_cast<ast_expr *> (this->step->clone ()));
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_package::ast_package (const std::string& name, ast_block *body)
    : name (name)
  {
    this->body = body;
  }
  
  ast_package::~ast_package ()
  {
    delete this->body;
  }
  
  
  
  ast_node*
  ast_package::clone ()
  {
    return new ast_package (this->name,
      static_cast<ast_block *> (this->body->clone ()));
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_module::ast_module (const std::string& name, ast_block *body)
    : ast_package (name, body)
    { }
  
  
  
  ast_node*
  ast_module::clone ()
  {
    return new ast_module (this->get_name (),
      static_cast<ast_block *> (this->get_body ()->clone ()));
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_use::ast_use (const std::string& what)
    : what (what)
    { }
  
  
  
  ast_node*
  ast_use::clone ()
  {
    return new ast_use (this->what);
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_conditional::ast_conditional (ast_expr *test, ast_expr *conseq, ast_expr *alt)
  {
    this->test = test;
    this->conseq = conseq;
    this->alt = alt;
  }
  
  ast_conditional::~ast_conditional ()
  {
    delete this->test;
    delete this->conseq;
    delete this->alt;
  }
  
  
  
  ast_node*
  ast_conditional::clone ()
  {
    return new ast_conditional (static_cast<ast_expr *> (this->test->clone ()),
      static_cast<ast_expr *> (this->conseq->clone ()),
      static_cast<ast_expr *> (this->alt->clone ()));
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_of_type::ast_of_type (ast_expr *expr, const type_info& ti)
    : ti (ti)
  {
    this->expr = expr;
  }
  
  ast_of_type::~ast_of_type ()
  {
    delete this->expr;
  }
  
  
  
  ast_node*
  ast_of_type::clone ()
  {
    return new ast_of_type (static_cast<ast_expr *> (this->expr->clone ()),
      this->ti);
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_unop::ast_unop (ast_expr *expr)
  {
    this->expr = expr;
  }
  
  ast_unop::~ast_unop ()
  {
    delete this->expr;
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_prefix::ast_prefix (ast_expr *expr, ast_prefix_type op)
    : ast_unop (expr)
  {
    this->op = op;
  }
  
  
  
  ast_node*
  ast_prefix::clone ()
  {
    return new ast_prefix (static_cast<ast_expr *> (this->get_expr ()->clone ()),
      this->op);
  }
  
  
  
//------------------------------------------------------------------------------
  
  ast_postfix::ast_postfix (ast_expr *expr, ast_postfix_type op)
    : ast_unop (expr)
  {
    this->op = op;
  }
  
  
  
  ast_node*
  ast_postfix::clone ()
  {
    return new ast_postfix (static_cast<ast_expr *> (this->get_expr ()->clone ()),
      this->op);
  }
}

