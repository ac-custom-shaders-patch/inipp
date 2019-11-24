require "std"

print(vec4(1, 2, 3, 4) ^ 2)
print(2 ^ vec4(1, 2, 3, 4))

print(vec2(1, 2):dot(vec2(3, 4)))
print(dot(vec2(1, 2), 2))
print(dot(2, vec2(1, 2)))

print((2 ^ vec4(1, 2, 3, 4)):length())
print(#(2 ^ vec4(1, 2, 3, 4)))
print((2 ^ vec4(1, 2, 3, 4)):normalize())
local v = vec4(1, 2, 3, 4)
v:normalizeSelf()
print(v)

print(vec2(1, 2) + vec2(3, 7))
print(vec2(1, 2) + vec4(1, 2, 3, 4))
print(vec4(1, 2, 3, 4) + vec2(1, 2))
print(vec2(3, 5) < vec2(1, 2))
print(vec2(3, 5) <= vec2(1, 2))
print(-vec2(1, 2))
print(-vec2(1, 2) == vec4(-1, -2, 3, 4))
print(-vec4(1, 2, -3, -4) == vec4(-1, -2, 3, 4))
print((vec4(2, 3, 4, 5) - 1) == vec4(1, 2, 3, 4))