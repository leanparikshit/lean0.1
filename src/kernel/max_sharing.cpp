/*
Copyright (c) 2013 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include <tuple>
#include <unordered_set>
#include <functional>
#include "util/buffer.h"
#include "util/interrupt.h"
#include "kernel/max_sharing.h"

namespace lean {
/**
   \brief Implementation of the functional object for creating expressions with maximally
   shared sub-expressions.
*/
struct max_sharing_fn::imp {
    typedef typename std::unordered_set<expr, expr_hash, std::equal_to<expr>> expr_cache;

    expr_cache m_cache;

    void cache(expr const & a) {
        a.raw()->set_max_shared();
        m_cache.insert(a);
    }

    optional<expr> apply(optional<expr> const & a) {
        if (a)
            return some_expr(apply(*a));
        else
            return a;
    }

    expr apply(expr const & a) {
        check_system("max_sharing");
        auto r = m_cache.find(a);
        if (r != m_cache.end()) {
            lean_assert((*r).raw()->max_shared());
            return *r;
        }
        if (a.raw()->max_shared()) {
            m_cache.insert(a);
            return a;
        }
        expr res;
        switch (a.kind()) {
        case expr_kind::Constant:
            res = update_const(a, apply(const_type(a)));
            break;
        case expr_kind::Var: case expr_kind::Type: case expr_kind::Value:
            res = a;
            break;
        case expr_kind::HEq:
            res = update_heq(a, apply(heq_lhs(a)), apply(heq_rhs(a)));
            break;
        case expr_kind::Pair:
            res = update_pair(a, [=](expr const & f, expr const & s, expr const & t) {
                    return std::make_tuple(apply(f), apply(s), apply(t));
                });
            break;
        case expr_kind::Proj:
            res = update_proj(a, apply(proj_arg(a)));
            break;
        case expr_kind::App:
            res = update_app(a, [=](expr const & c) { return apply(c); });
            break;
        case expr_kind::Sigma:
        case expr_kind::Lambda:
        case expr_kind::Pi:
            res = update_abst(a, [=](expr const & t, expr const & b) { return std::make_pair(apply(t), apply(b)); });
            break;
        case expr_kind::Let:
            res = update_let(a, [=](optional<expr> const & t, expr const & v, expr const & b) {
                    return std::make_tuple(apply(t), apply(v), apply(b));
                });
            break;
        case expr_kind::MetaVar: {
            res = update_metavar(a, [=](local_entry const & e) -> local_entry {
                    if (e.is_inst())
                        return mk_inst(e.s(), apply(e.v()));
                    else
                        return e;
                });
            break;
        }}
        cache(res);
        return res;
    }
    expr operator()(expr const & a) { return apply(a); }
};

max_sharing_fn::max_sharing_fn():m_ptr(new imp) {}
max_sharing_fn::~max_sharing_fn() {}
expr max_sharing_fn::operator()(expr const & a) { return (*m_ptr)(a); }
void max_sharing_fn::clear() { m_ptr->m_cache.clear(); }

expr max_sharing(expr const & a) {
    if (a.raw()->max_shared())
        return a;
    else
        return max_sharing_fn::imp()(a);
}
} // namespace lean
