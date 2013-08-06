/*
Copyright (c) 2013 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include "environment.h"
#include "type_check.h"
#include "toplevel.h"
#include "builtin.h"
#include "arith.h"
#include "normalize.h"
#include "abstract.h"
#include "exception.h"
#include "trace.h"
#include "test.h"
using namespace lean;

static void tst1() {
    environment env;
    level u = env.define_uvar("u", level() + 1);
    level w = env.define_uvar("w", u + 1);
    lean_assert(!env.has_children());
    lean_assert(!env.has_parent());
    {
        environment child = env.mk_child();
        lean_assert(child.is_ge(w, u));
        lean_assert(child.is_ge(w, level() + 2));
        lean_assert(env.is_ge(w, level() + 2));
        lean_assert(env.has_children());
        lean_assert(child.has_parent());
        lean_assert(!env.has_parent());
        try {
            level o = env.define_uvar("o", w + 1);
            lean_unreachable();
        } catch (exception const & ex) {
            std::cout << "expected error: " << ex.what() << "\n";
        }
    }
    std::cout << "tst1 checkpoint" << std::endl;
    level o = env.define_uvar("o", w + 1);
    lean_assert(!env.has_children());
    env.display_uvars(std::cout);
}

static environment mk_child() {
    environment env;
    level u = env.define_uvar("u", level() + 1);
    return env.mk_child();
}

static void tst2() {
    environment child = mk_child();
    lean_assert(child.has_parent());
    lean_assert(!child.has_children());
    environment parent = child.parent();
    parent.display_uvars(std::cout);
    lean_assert(parent.has_children());
    std::cout << "uvar: " << child.get_uvar("u") << "\n";
}

static void tst3() {
    environment env;
    try {
        env.add_definition("a", Int, Const("a"));
        lean_unreachable();
    } catch (exception const & ex) {
        std::cout << "expected error: " << ex.what() << "\n";
    }
    env.add_definition("a", Int, iAdd(iVal(1), iVal(2)));
    expr t = iAdd(Const("a"), iVal(1));
    std::cout << t << " --> " << normalize(t, env) << "\n";
    lean_assert(normalize(t, env) == iVal(4));
    env.add_definition("b", Int, iMul(iVal(2), Const("a")));
    std::cout << "b --> " << normalize(Const("b"), env) << "\n";
    lean_assert(normalize(Const("b"), env) == iVal(6));
    try {
        env.add_definition("c", arrow(Int, Int), Const("a"));
        lean_unreachable();
    } catch (exception const & ex) {
        std::cout << "expected error: " << ex.what() << "\n";
    }
    try {
        env.add_definition("a", Int, iVal(10));
        lean_unreachable();
    } catch (exception const & ex) {
        std::cout << "expected error: " << ex.what() << "\n";
    }
    environment c_env = env.mk_child();
    try {
        env.add_definition("c", Int, Const("a"));
        lean_unreachable();
    } catch (exception const & ex) {
        std::cout << "expected error: " << ex.what() << "\n";
    }
    lean_assert(normalize(Const("b"), env) == iVal(6));
    lean_assert(normalize(Const("b"), c_env) == iVal(6));
    c_env.add_definition("c", Int, Const("a"));
    lean_assert(normalize(Const("c"), c_env) == iVal(3));
    try {
        expr r = normalize(Const("c"), env);
        lean_assert(r == iVal(3));
        lean_unreachable();
    } catch (exception const & ex) {
        std::cout << "expected error: " << ex.what() << std::endl;
    }
    std::cout << "end tst3" << std::endl;
}

static void tst4() {
    environment env;
    env.add_definition("a", Int, iVal(1), true); // add opaque definition
    expr t = iAdd(Const("a"), iVal(1));
    std::cout << t << " --> " << normalize(t, env) << "\n";
    lean_assert(normalize(t, env) == t);
    env.add_definition("b", Int, iAdd(Const("a"), iVal(1)));
    expr t2 = iSub(Const("b"), iVal(9));
    std::cout << t2 << " --> " << normalize(t2, env) << "\n";
    lean_assert(normalize(t2, env) == iSub(iAdd(Const("a"), iVal(1)), iVal(9)));
}

static void tst5() {
    environment env;
    env.add_definition("a", Int, iVal(1), true); // add opaque definition
    try {
        std::cout << infer_type(iAdd(Const("a"), Int), env) << "\n";
        lean_unreachable();
    } catch (exception const & ex) {
        std::cout << "expected error: " << ex.what() << "\n";
    }
}

static void tst6() {
    environment env;
    level u = env.define_uvar("u", level() + 1);
    level w = env.define_uvar("w", u + 1);
    env.add_var("f", arrow(Type(u), Type(u)));
    expr t = Const("f")(Int);
    std::cout << "type of " << t << " is " << infer_type(t, env) << "\n";
    try {
        infer_type(Const("f")(Type(w)), env);
        lean_unreachable();
    } catch (exception const & ex) {
        std::cout << "expected error: " << ex.what() << "\n";
    }
    try {
        infer_type(Const("f")(Type(u)), env);
        lean_unreachable();
    } catch (exception const & ex) {
        std::cout << "expected error: " << ex.what() << "\n";
    }
    t = Const("f")(Type());
    std::cout << "type of " << t << " is " << infer_type(t, env) << "\n";
    std::cout << infer_type(arrow(Type(u), Type(w)), env) << "\n";
    lean_assert(infer_type(arrow(Type(u), Type(w)), env) == Type(max(u+1, w+1)));
    std::cout << infer_type(arrow(Int, Int), env) << "\n";
    lean_assert(infer_type(arrow(Int, Int), env) == Type());
}

static void tst7() {
    environment env = mk_toplevel();
    env.add_var("a", Int);
    env.add_var("b", Int);
    expr t = If(Int, True, Const("a"), Const("b"));
    std::cout << t << " --> " << normalize(t, env) << "\n";
    std::cout << infer_type(t, env) << "\n";
    std::cout << "Environment\n" << env;
}

int main() {
    enable_trace("is_convertible");
    continue_on_violation(true);
    tst1();
    tst2();
    tst3();
    tst4();
    tst5();
    tst6();
    tst7();
    return has_violations() ? 1 : 0;
}