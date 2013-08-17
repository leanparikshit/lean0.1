/*
Copyright (c) 2013 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#pragma once
#include "exception.h"
#include "context.h"
#include "environment.h"

namespace lean {
class environment;
/** \brief Base class for all kernel exceptions. */
class kernel_exception : public exception {
protected:
    environment m_env;
public:
    kernel_exception(environment const & env):m_env(env) {}
    kernel_exception(environment const & env, char const * msg):exception(msg), m_env(env) {}
    virtual ~kernel_exception() noexcept {}
    environment const & get_environment() const { return m_env; }
};

/** \brief Base class for unknown universe or object exceptions. */
class unknown_name_exception : public kernel_exception {
    name m_name;
public:
    unknown_name_exception(environment const & env, name const & n):kernel_exception(env), m_name(n) {}
    virtual ~unknown_name_exception() {}
    name const & get_name() const { return m_name; }
};

/** \brief Exception used to report that a universe variable is not known in a given environment. */
class unknown_universe_variable_exception : public unknown_name_exception {
public:
    unknown_universe_variable_exception(environment const & env, name const & n):unknown_name_exception(env, n) {}
    virtual char const * what() const noexcept { return "unknown universe variable"; }
};

/** \brief Exception used to report that an object is not known in a given environment. */
class unknown_object_exception : public unknown_name_exception {
public:
    unknown_object_exception(environment const & env, name const & n):unknown_name_exception(env, n) {}
    virtual char const * what() const noexcept { return "unknown object"; }
};

/** \brief Base class for already declared universe or object. */
class already_declared_exception : public kernel_exception {
    name m_name;
public:
    already_declared_exception(environment const & env, name const & n):kernel_exception(env), m_name(n) {}
    virtual ~already_declared_exception() noexcept {}
    name const & get_name() const { return m_name; }
};

/** \brief Exception used to report that a universe variable has already been declared in a given environment. */
class already_declared_universe_exception : public already_declared_exception {
public:
    already_declared_universe_exception(environment const & env, name const & n):already_declared_exception(env, n) {}
    virtual char const * what() const noexcept { return "invalid universe variable declaration, it has already been declared"; }
};

/** \brief Exception used to report that an object has already been declared in a given environment. */
class already_declared_object_exception : public already_declared_exception {
public:
    already_declared_object_exception(environment const & env, name const & n):already_declared_exception(env, n) {}
    virtual char const * what() const noexcept { return "invalid object declaration, environment already has an object the given name"; }
};

/**
    \brief Exception used to report that a update has been tried on a
    read-only environment.

    An environment is read-only if it has "children environments".
*/
class read_only_environment_exception : public kernel_exception {
public:
    read_only_environment_exception(environment const & env):kernel_exception(env) {}
    virtual char const * what() const noexcept { return "environment cannot be updated because it has children environments"; }
};

/** \brief Base class for type checking related exceptions. */
class type_checker_exception : public kernel_exception {
public:
    type_checker_exception(environment const & env):kernel_exception(env) {}
};

/**
    \brief Exception used to report an application argument type
    mismatch.

    Explanation:
    In the environment get_environment() and local context
    get_context(), the argument get_arg_pos() of the application
    get_application() has type get_given_type(), but it is expected to
    have type get_expected_type().
*/
class app_type_mismatch_exception : public type_checker_exception {
    context  m_context;
    expr     m_app;
    unsigned m_arg_pos;
    expr     m_expected_type;
    expr     m_given_type;
public:
    app_type_mismatch_exception(environment const & env, context const & ctx, expr const & app, unsigned pos, expr const & expected, expr const & given):
        type_checker_exception(env), m_context(ctx), m_app(app), m_arg_pos(pos),
        m_expected_type(expected), m_given_type(given) {}
    virtual ~app_type_mismatch_exception() {}
    context const & get_context() const { return m_context; }
    expr const & get_application() const { return m_app; }
    unsigned get_arg_pos() const { return m_arg_pos; }
    expr const & get_expected_type() const { return m_expected_type; }
    expr const & get_given_type() const { return m_given_type; }
    virtual char const * what() const noexcept { return "application argument type mismatch"; }
};

/**
   \brief Exception used to report than an expression that is not a
   function is being used as a function.

   Explanation:
   In the environment get_environment() and local context
   get_context(), the expression get_expr() is expected to be a function.
*/
class function_expected_exception : public type_checker_exception {
    context m_context;
    expr    m_expr;
public:
    function_expected_exception(environment const & env, context const & ctx, expr const & e):
        type_checker_exception(env), m_context(ctx), m_expr(e) {}
    virtual ~function_expected_exception() {}
    context const & get_context() const { return m_context; }
    expr const & get_expr() const { return m_expr; }
    virtual char const * what() const noexcept { return "function expected"; }
};

/**
   \brief Exception used to report than an expression that is not a
   type is being used where a type is expected.

   Explanation:
   In the environment get_environment() and local context
   get_context(), the expression get_expr() is expected to be a type.
*/
class type_expected_exception : public type_checker_exception {
    context m_context;
    expr    m_expr;
public:
    type_expected_exception(environment const & env, context const & ctx, expr const & e):
        type_checker_exception(env), m_context(ctx), m_expr(e) {}
    virtual ~type_expected_exception() {}
    context const & get_context() const { return m_context; }
    expr const & get_expr() const { return m_expr; }
    virtual char const * what() const noexcept { return "type expected"; }
};

/**
    \brief Exception used to report a definition type mismatch.

    Explanation:
    In the environment get_environment(), the declaration with name
    get_name(), type get_type() and value get_value() is incorrect
    because the value has type get_value_type() and it not matches
    the given type get_type().
*/
class def_type_mismatch_exception : public type_checker_exception {
    name m_name;
    expr m_type;
    expr m_value;
    expr m_value_type;
public:
    def_type_mismatch_exception(environment const & env, name const & n, expr const & type, expr const & val, expr const & val_type):
        type_checker_exception(env), m_name(n), m_type(type), m_value(val), m_value_type(val_type) {}
    virtual ~def_type_mismatch_exception() {}
    name const & get_name() const { return m_name; }
    expr const & get_type() const { return m_type; }
    expr const & get_value() const { return m_value; }
    expr const & get_value_type() const { return m_value_type; }
    virtual char const * what() const noexcept { return "definition type mismatch"; }
};
}