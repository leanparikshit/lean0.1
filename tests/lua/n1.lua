n = name("foo", 1, 2)
print(n)
print(name(n, "bla"))
print(n == name("foo", 1))
print(n == name("foo", 2))
print(n < name("foo", 2))
print(n < name("foo", 0))
print(name(nil))