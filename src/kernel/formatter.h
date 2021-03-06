/*
Copyright (c) 2013 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#pragma once
#include <memory>
#include "util/sexpr/options.h"
#include "kernel/context.h"
#include "kernel/environment.h"

namespace lean {
/**
   \brief API for formatting expressions, contexts and environments.
*/
class formatter_cell {
public:
    virtual ~formatter_cell() {}
    /** \brief Format the given expression. */
    virtual format operator()(expr const & e, options const & opts) = 0;
    /** \brief Format the given context. */
    virtual format operator()(context const & c, options const & opts) = 0;
    /**
        \brief Format the given expression with respect to the given
        context.

        \remark If format_ctx == false, then the context is not formatted. It just provides names
        for the free variables
    */
    virtual format operator()(context const & c, expr const & e, bool format_ctx, options const & opts) = 0;
    /** \brief Format the given object */
    virtual format operator()(object const & obj, options const & opts) = 0;
    /** \brief Format the given environment */
    virtual format operator()(ro_environment const & env, options const & opts) = 0;

    /**
        \brief Return environment object associated with this formatter.
        Not every formatter has an associated environment object.
    */
    virtual optional<ro_environment> get_environment() const { return optional<ro_environment>(); }
};
/**
   \brief Smart-pointer for the actual formatter object (aka \c formatter_cell).
*/
class formatter {
    std::shared_ptr<formatter_cell> m_cell;
    formatter(formatter_cell * c):m_cell(c) {}
public:
    format operator()(expr const & e, options const & opts = options()) const { return (*m_cell)(e, opts); }
    format operator()(context const & c, options const & opts = options()) const { return (*m_cell)(c, opts); }
    format operator()(context const & c, expr const & e, bool format_ctx = false, options const & opts = options()) const { return (*m_cell)(c, e, format_ctx, opts); }
    format operator()(object const & obj, options const & opts = options()) const { return (*m_cell)(obj, opts); }
    format operator()(ro_environment const & env, options const & opts = options()) const { return (*m_cell)(env, opts); }
    optional<ro_environment> get_environment() { return m_cell->get_environment(); }

    template<typename FCell> friend formatter mk_formatter(FCell && fcell);
};

template<typename FCell> formatter mk_formatter(FCell && fcell) {
    return formatter(new FCell(std::forward<FCell>(fcell)));
}
}
