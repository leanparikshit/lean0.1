assert(not pcall(function() name(mpz(10)) end))
assert(not pcall(function() name(function() return 1 end) end))
assert(Const{"x", name("y", 1), 1}:fields() == name("x", "y", 1, 1))
assert(not pcall(function() Const({"x", function() return 1 end}) end))
assert(name("x", 1):hash() == name("x", 1):hash())