require "std"

print(__conv_result(vec4(2 * vec2(1,1) / vec2(1,1), -2 * vec2(0.5,0.5) / vec2(1,1))))

-- local inspect = require 'inspect'

-- print(vec4(1, 2, 3, 4) ^ 2)
-- print(2 ^ vec4(1, 2, 3, 4))

-- print(vec2(1, 2):dot(vec2(3, 4)))
-- print(dot(vec2(1, 2), 2))
-- print(dot(2, vec2(1, 2)))

-- print((2 ^ vec4(1, 2, 3, 4)):length())
-- print(#(2 ^ vec4(1, 2, 3, 4)))
-- print((2 ^ vec4(1, 2, 3, 4)):normalize())
-- local v = vec4(1, 2, 3, 4)
-- v:normalizeSelf()
-- print(v)

-- print(vec2(1, 2) + vec2(3, 7))
-- print(vec2(1, 2) + vec4(1, 2, 3, 4))
-- print(vec4(1, 2, 3, 4) + vec2(1, 2))
-- print(vec2(3, 5) < vec2(1, 2))
-- print(vec2(3, 5) <= vec2(1, 2))
-- print(-vec2(1, 2))
-- print(-vec2(1, 2) == vec4(-1, -2, 3, 4))
-- print(-vec4(1, 2, -3, -4) == vec4(-1, -2, 3, 4))
-- print((vec4(2, 3, 4, 5) - 1) == vec4(1, 2, 3, 4))

-- print(saturate(vec2(0.5, 1.1)))
-- print(clamp(vec2(0.5, 1.1), vec2(0.7, 0.9), 0.95))
-- print(vec4(-1, -2, 3, 4):sign())
-- print(lerp(vec2(0.5, 1.1), vec2(0.7, 0.9), 0.95))

-- print(inspect(__conv_result(vec2(1, 2))))
-- print(inspect({ 2 / vec2(0.1,0.1), -2 * vec2(0.05,0.05) / vec2(0.1,0.1) }))
-- print(inspect(__conv_result({ 2 / vec2(0.1,0.1), -2 * vec2(0.05,0.05) / vec2(0.1,0.1) })))
-- print(inspect(__conv_result(vec4(2 / vec2(0.1,0.1), -2 * vec2(0.05,0.05) / vec2(0.1,0.1)))))