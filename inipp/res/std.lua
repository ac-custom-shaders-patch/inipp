-- generated automatically, do not edit
local __vec2_mt = {
  __add = function (L, R) if type(R) ~= 'table' then return vec2(L[1]+R, L[2]+R) elseif type(L) ~= 'table' then return vec2(L+R[1], L+R[2]) else return vec2(L[1]+(R[1] and R[1] or 0), L[2]+(R[2] and R[2] or 0)) end end,
  __sub = function (L, R) if type(R) ~= 'table' then return vec2(L[1]-R, L[2]-R) elseif type(L) ~= 'table' then return vec2(L-R[1], L-R[2]) else return vec2(L[1]-(R[1] and R[1] or 0), L[2]-(R[2] and R[2] or 0)) end end,
  __mul = function (L, R) if type(R) ~= 'table' then return vec2(L[1]*R, L[2]*R) elseif type(L) ~= 'table' then return vec2(L*R[1], L*R[2]) else return vec2(L[1]*(R[1] and R[1] or 0), L[2]*(R[2] and R[2] or 0)) end end,
  __div = function (L, R) if type(R) ~= 'table' then return vec2(L[1]/R, L[2]/R) elseif type(L) ~= 'table' then return vec2(L/R[1], L/R[2]) else return vec2(L[1]/(R[1] and R[1] or 0), L[2]/(R[2] and R[2] or 0)) end end,
  __mod = function (L, R) if type(R) ~= 'table' then return vec2(L[1]%R, L[2]%R) elseif type(L) ~= 'table' then return vec2(L%R[1], L%R[2]) else return vec2(L[1]%(R[1] and R[1] or 0), L[2]%(R[2] and R[2] or 0)) end end,
  __pow = function (L, R) if type(R) ~= 'table' then return vec2(L[1]^R, L[2]^R) elseif type(L) ~= 'table' then return vec2(L^R[1], L^R[2]) else return vec2(L[1]^(R[1] and R[1] or 0), L[2]^(R[2] and R[2] or 0)) end end,
  __lt = function (L, R) if type(R) ~= 'table' then return vec2(L[1]<R, L[2]<R) elseif type(L) ~= 'table' then return vec2(L<R[1], L<R[2]) else return vec2(L[1]<(R[1] and R[1] or 0), L[2]<(R[2] and R[2] or 0)) end end,
  __le = function (L, R) if type(R) ~= 'table' then return vec2(L[1]<=R, L[2]<=R) elseif type(L) ~= 'table' then return vec2(L<=R[1], L<=R[2]) else return vec2(L[1]<=(R[1] and R[1] or 0), L[2]<=(R[2] and R[2] or 0)) end end,
  __eq = function (L, R)
    if type(R) ~= 'table' then return L[1] == R and L[2] == R
    elseif type(L) ~= 'table' then return L == R[1] and L == R[2]
    else return L[1] == (R[1] and R[1] or 0) and L[2] == (R[2] and R[2] or 0) end
  end,
  __len = function (s) return s:length() end,
  __unm = function (s) return vec2(-s[1], -s[2]) end,
  __tostring = function (s) return '{' ..s[1]..', '..s[2].. '}' end,
  __index = function (s, key)
    if key == 'len' or key == 'length' then return function (s) 
      return math.sqrt(s[1]^2+s[2]^2) 
    end end

    if key == 'normalize' then return function (s) 
      return s / s:length() 
    end end

    if key == 'normalizeSelf' then return function (s) 
      local m = 1 / s:length() 
      s[1] = s[1] * m s[2] = s[2] * m 
      return s
    end end

    if key == 'dot' then return function (L, R)
      if type(R) ~= 'table' then return L[1] * R + L[2] * R
      else return L[1] * (R[1] and R[1] or 0) + L[2] * (R[2] and R[2] or 0) end
    end end

    if key == 'clamp' then return function (L, min, max)
      return vec2(math.clamp(L[1], 
        type(min) == 'table' and (min[1] and min[1] or 0) or min, 
        type(max) == 'table' and (max[1] and max[1] or 0) or max), math.clamp(L[2], 
        type(min) == 'table' and (min[2] and min[2] or 0) or min, 
        type(max) == 'table' and (max[2] and max[2] or 0) or max))
    end end

    if key == 'lerp' then return function (L, R, s)
      return vec2(math.lerp(L[1], 
        type(R) == 'table' and (R[1] and R[1] or 0) or R, 
        type(s) == 'table' and (s[1] and s[1] or 0) or s), math.lerp(L[2], 
        type(R) == 'table' and (R[2] and R[2] or 0) or R, 
        type(s) == 'table' and (s[2] and s[2] or 0) or s))
    end end

    if key == 'sign' then return function (L)
      return vec2(math.sign(L[1]), math.sign(L[2]))
    end end

    if key == 'x' or key == 'X' then return s[1] end
    if key == 'y' or key == 'Y' then return s[2] end
    if key == 'z' or key == 'Z' then return s[3] end
    if key == 'w' or key == 'W' then return s[4] end

    
    return nil
  end
}

function vec2(a0, a1)
  local t = {}
  if type(a0) == 'table' then
    for k, v in pairs(a0) do
      t[#t + 1] = v
    end
  else t[#t + 1] = a0 end if type(a1) == 'table' then
    for k, v in pairs(a1) do
      t[#t + 1] = v
    end
  else t[#t + 1] = a1 end
  setmetatable(t, __vec2_mt)
  return t
end

local __vec3_mt = {
  __add = function (L, R) if type(R) ~= 'table' then return vec3(L[1]+R, L[2]+R, L[3]+R) elseif type(L) ~= 'table' then return vec3(L+R[1], L+R[2], L+R[3]) else return vec3(L[1]+(R[1] and R[1] or 0), L[2]+(R[2] and R[2] or 0), L[3]+(R[3] and R[3] or 0)) end end,
  __sub = function (L, R) if type(R) ~= 'table' then return vec3(L[1]-R, L[2]-R, L[3]-R) elseif type(L) ~= 'table' then return vec3(L-R[1], L-R[2], L-R[3]) else return vec3(L[1]-(R[1] and R[1] or 0), L[2]-(R[2] and R[2] or 0), L[3]-(R[3] and R[3] or 0)) end end,
  __mul = function (L, R) if type(R) ~= 'table' then return vec3(L[1]*R, L[2]*R, L[3]*R) elseif type(L) ~= 'table' then return vec3(L*R[1], L*R[2], L*R[3]) else return vec3(L[1]*(R[1] and R[1] or 0), L[2]*(R[2] and R[2] or 0), L[3]*(R[3] and R[3] or 0)) end end,
  __div = function (L, R) if type(R) ~= 'table' then return vec3(L[1]/R, L[2]/R, L[3]/R) elseif type(L) ~= 'table' then return vec3(L/R[1], L/R[2], L/R[3]) else return vec3(L[1]/(R[1] and R[1] or 0), L[2]/(R[2] and R[2] or 0), L[3]/(R[3] and R[3] or 0)) end end,
  __mod = function (L, R) if type(R) ~= 'table' then return vec3(L[1]%R, L[2]%R, L[3]%R) elseif type(L) ~= 'table' then return vec3(L%R[1], L%R[2], L%R[3]) else return vec3(L[1]%(R[1] and R[1] or 0), L[2]%(R[2] and R[2] or 0), L[3]%(R[3] and R[3] or 0)) end end,
  __pow = function (L, R) if type(R) ~= 'table' then return vec3(L[1]^R, L[2]^R, L[3]^R) elseif type(L) ~= 'table' then return vec3(L^R[1], L^R[2], L^R[3]) else return vec3(L[1]^(R[1] and R[1] or 0), L[2]^(R[2] and R[2] or 0), L[3]^(R[3] and R[3] or 0)) end end,
  __lt = function (L, R) if type(R) ~= 'table' then return vec3(L[1]<R, L[2]<R, L[3]<R) elseif type(L) ~= 'table' then return vec3(L<R[1], L<R[2], L<R[3]) else return vec3(L[1]<(R[1] and R[1] or 0), L[2]<(R[2] and R[2] or 0), L[3]<(R[3] and R[3] or 0)) end end,
  __le = function (L, R) if type(R) ~= 'table' then return vec3(L[1]<=R, L[2]<=R, L[3]<=R) elseif type(L) ~= 'table' then return vec3(L<=R[1], L<=R[2], L<=R[3]) else return vec3(L[1]<=(R[1] and R[1] or 0), L[2]<=(R[2] and R[2] or 0), L[3]<=(R[3] and R[3] or 0)) end end,
  __eq = function (L, R)
    if type(R) ~= 'table' then return L[1] == R and L[2] == R and L[3] == R
    elseif type(L) ~= 'table' then return L == R[1] and L == R[2] and L == R[3]
    else return L[1] == (R[1] and R[1] or 0) and L[2] == (R[2] and R[2] or 0) and L[3] == (R[3] and R[3] or 0) end
  end,
  __len = function (s) return s:length() end,
  __unm = function (s) return vec3(-s[1], -s[2], -s[3]) end,
  __tostring = function (s) return '{' ..s[1]..', '..s[2]..', '..s[3].. '}' end,
  __index = function (s, key)
    if key == 'len' or key == 'length' then return function (s) 
      return math.sqrt(s[1]^2+s[2]^2+s[3]^2) 
    end end

    if key == 'normalize' then return function (s) 
      return s / s:length() 
    end end

    if key == 'normalizeSelf' then return function (s) 
      local m = 1 / s:length() 
      s[1] = s[1] * m s[2] = s[2] * m s[3] = s[3] * m 
      return s
    end end

    if key == 'dot' then return function (L, R)
      if type(R) ~= 'table' then return L[1] * R + L[2] * R + L[3] * R
      else return L[1] * (R[1] and R[1] or 0) + L[2] * (R[2] and R[2] or 0) + L[3] * (R[3] and R[3] or 0) end
    end end

    if key == 'clamp' then return function (L, min, max)
      return vec3(math.clamp(L[1], 
        type(min) == 'table' and (min[1] and min[1] or 0) or min, 
        type(max) == 'table' and (max[1] and max[1] or 0) or max), math.clamp(L[2], 
        type(min) == 'table' and (min[2] and min[2] or 0) or min, 
        type(max) == 'table' and (max[2] and max[2] or 0) or max), math.clamp(L[3], 
        type(min) == 'table' and (min[3] and min[3] or 0) or min, 
        type(max) == 'table' and (max[3] and max[3] or 0) or max))
    end end

    if key == 'lerp' then return function (L, R, s)
      return vec3(math.lerp(L[1], 
        type(R) == 'table' and (R[1] and R[1] or 0) or R, 
        type(s) == 'table' and (s[1] and s[1] or 0) or s), math.lerp(L[2], 
        type(R) == 'table' and (R[2] and R[2] or 0) or R, 
        type(s) == 'table' and (s[2] and s[2] or 0) or s), math.lerp(L[3], 
        type(R) == 'table' and (R[3] and R[3] or 0) or R, 
        type(s) == 'table' and (s[3] and s[3] or 0) or s))
    end end

    if key == 'sign' then return function (L)
      return vec3(math.sign(L[1]), math.sign(L[2]), math.sign(L[3]))
    end end

    if key == 'x' or key == 'X' then return s[1] end
    if key == 'y' or key == 'Y' then return s[2] end
    if key == 'z' or key == 'Z' then return s[3] end
    if key == 'w' or key == 'W' then return s[4] end

    

    if key == 'cross' then return function (s, o) 
      return vec3(s[2] * o[3] - s[3] * o[2],
        s[3] * o[1] - s[1] * o[3],
        s[1] * o[2] - s[2] * o[1])
    end end
    return nil
  end
}

function vec3(a0, a1, a2)
  local t = {}
  if type(a0) == 'table' then
    for k, v in pairs(a0) do
      t[#t + 1] = v
    end
  else t[#t + 1] = a0 end if type(a1) == 'table' then
    for k, v in pairs(a1) do
      t[#t + 1] = v
    end
  else t[#t + 1] = a1 end if type(a2) == 'table' then
    for k, v in pairs(a2) do
      t[#t + 1] = v
    end
  else t[#t + 1] = a2 end
  setmetatable(t, __vec3_mt)
  return t
end

local __vec4_mt = {
  __add = function (L, R) if type(R) ~= 'table' then return vec4(L[1]+R, L[2]+R, L[3]+R, L[4]+R) elseif type(L) ~= 'table' then return vec4(L+R[1], L+R[2], L+R[3], L+R[4]) else return vec4(L[1]+(R[1] and R[1] or 0), L[2]+(R[2] and R[2] or 0), L[3]+(R[3] and R[3] or 0), L[4]+(R[4] and R[4] or 0)) end end,
  __sub = function (L, R) if type(R) ~= 'table' then return vec4(L[1]-R, L[2]-R, L[3]-R, L[4]-R) elseif type(L) ~= 'table' then return vec4(L-R[1], L-R[2], L-R[3], L-R[4]) else return vec4(L[1]-(R[1] and R[1] or 0), L[2]-(R[2] and R[2] or 0), L[3]-(R[3] and R[3] or 0), L[4]-(R[4] and R[4] or 0)) end end,
  __mul = function (L, R) if type(R) ~= 'table' then return vec4(L[1]*R, L[2]*R, L[3]*R, L[4]*R) elseif type(L) ~= 'table' then return vec4(L*R[1], L*R[2], L*R[3], L*R[4]) else return vec4(L[1]*(R[1] and R[1] or 0), L[2]*(R[2] and R[2] or 0), L[3]*(R[3] and R[3] or 0), L[4]*(R[4] and R[4] or 0)) end end,
  __div = function (L, R) if type(R) ~= 'table' then return vec4(L[1]/R, L[2]/R, L[3]/R, L[4]/R) elseif type(L) ~= 'table' then return vec4(L/R[1], L/R[2], L/R[3], L/R[4]) else return vec4(L[1]/(R[1] and R[1] or 0), L[2]/(R[2] and R[2] or 0), L[3]/(R[3] and R[3] or 0), L[4]/(R[4] and R[4] or 0)) end end,
  __mod = function (L, R) if type(R) ~= 'table' then return vec4(L[1]%R, L[2]%R, L[3]%R, L[4]%R) elseif type(L) ~= 'table' then return vec4(L%R[1], L%R[2], L%R[3], L%R[4]) else return vec4(L[1]%(R[1] and R[1] or 0), L[2]%(R[2] and R[2] or 0), L[3]%(R[3] and R[3] or 0), L[4]%(R[4] and R[4] or 0)) end end,
  __pow = function (L, R) if type(R) ~= 'table' then return vec4(L[1]^R, L[2]^R, L[3]^R, L[4]^R) elseif type(L) ~= 'table' then return vec4(L^R[1], L^R[2], L^R[3], L^R[4]) else return vec4(L[1]^(R[1] and R[1] or 0), L[2]^(R[2] and R[2] or 0), L[3]^(R[3] and R[3] or 0), L[4]^(R[4] and R[4] or 0)) end end,
  __lt = function (L, R) if type(R) ~= 'table' then return vec4(L[1]<R, L[2]<R, L[3]<R, L[4]<R) elseif type(L) ~= 'table' then return vec4(L<R[1], L<R[2], L<R[3], L<R[4]) else return vec4(L[1]<(R[1] and R[1] or 0), L[2]<(R[2] and R[2] or 0), L[3]<(R[3] and R[3] or 0), L[4]<(R[4] and R[4] or 0)) end end,
  __le = function (L, R) if type(R) ~= 'table' then return vec4(L[1]<=R, L[2]<=R, L[3]<=R, L[4]<=R) elseif type(L) ~= 'table' then return vec4(L<=R[1], L<=R[2], L<=R[3], L<=R[4]) else return vec4(L[1]<=(R[1] and R[1] or 0), L[2]<=(R[2] and R[2] or 0), L[3]<=(R[3] and R[3] or 0), L[4]<=(R[4] and R[4] or 0)) end end,
  __eq = function (L, R)
    if type(R) ~= 'table' then return L[1] == R and L[2] == R and L[3] == R and L[4] == R
    elseif type(L) ~= 'table' then return L == R[1] and L == R[2] and L == R[3] and L == R[4]
    else return L[1] == (R[1] and R[1] or 0) and L[2] == (R[2] and R[2] or 0) and L[3] == (R[3] and R[3] or 0) and L[4] == (R[4] and R[4] or 0) end
  end,
  __len = function (s) return s:length() end,
  __unm = function (s) return vec4(-s[1], -s[2], -s[3], -s[4]) end,
  __tostring = function (s) return '{' ..s[1]..', '..s[2]..', '..s[3]..', '..s[4].. '}' end,
  __index = function (s, key)
    if key == 'len' or key == 'length' then return function (s) 
      return math.sqrt(s[1]^2+s[2]^2+s[3]^2+s[4]^2) 
    end end

    if key == 'normalize' then return function (s) 
      return s / s:length() 
    end end

    if key == 'normalizeSelf' then return function (s) 
      local m = 1 / s:length() 
      s[1] = s[1] * m s[2] = s[2] * m s[3] = s[3] * m s[4] = s[4] * m 
      return s
    end end

    if key == 'dot' then return function (L, R)
      if type(R) ~= 'table' then return L[1] * R + L[2] * R + L[3] * R + L[4] * R
      else return L[1] * (R[1] and R[1] or 0) + L[2] * (R[2] and R[2] or 0) + L[3] * (R[3] and R[3] or 0) + L[4] * (R[4] and R[4] or 0) end
    end end

    if key == 'clamp' then return function (L, min, max)
      return vec4(math.clamp(L[1], 
        type(min) == 'table' and (min[1] and min[1] or 0) or min, 
        type(max) == 'table' and (max[1] and max[1] or 0) or max), math.clamp(L[2], 
        type(min) == 'table' and (min[2] and min[2] or 0) or min, 
        type(max) == 'table' and (max[2] and max[2] or 0) or max), math.clamp(L[3], 
        type(min) == 'table' and (min[3] and min[3] or 0) or min, 
        type(max) == 'table' and (max[3] and max[3] or 0) or max), math.clamp(L[4], 
        type(min) == 'table' and (min[4] and min[4] or 0) or min, 
        type(max) == 'table' and (max[4] and max[4] or 0) or max))
    end end

    if key == 'lerp' then return function (L, R, s)
      return vec4(math.lerp(L[1], 
        type(R) == 'table' and (R[1] and R[1] or 0) or R, 
        type(s) == 'table' and (s[1] and s[1] or 0) or s), math.lerp(L[2], 
        type(R) == 'table' and (R[2] and R[2] or 0) or R, 
        type(s) == 'table' and (s[2] and s[2] or 0) or s), math.lerp(L[3], 
        type(R) == 'table' and (R[3] and R[3] or 0) or R, 
        type(s) == 'table' and (s[3] and s[3] or 0) or s), math.lerp(L[4], 
        type(R) == 'table' and (R[4] and R[4] or 0) or R, 
        type(s) == 'table' and (s[4] and s[4] or 0) or s))
    end end

    if key == 'sign' then return function (L)
      return vec4(math.sign(L[1]), math.sign(L[2]), math.sign(L[3]), math.sign(L[4]))
    end end

    if key == 'x' or key == 'X' then return s[1] end
    if key == 'y' or key == 'Y' then return s[2] end
    if key == 'z' or key == 'Z' then return s[3] end
    if key == 'w' or key == 'W' then return s[4] end

    
    return nil
  end
}

function vec4(a0, a1, a2, a3)
  local t = {}
  if type(a0) == 'table' then
    for k, v in pairs(a0) do
      t[#t + 1] = v
    end
  else t[#t + 1] = a0 end if type(a1) == 'table' then
    for k, v in pairs(a1) do
      t[#t + 1] = v
    end
  else t[#t + 1] = a1 end if type(a2) == 'table' then
    for k, v in pairs(a2) do
      t[#t + 1] = v
    end
  else t[#t + 1] = a2 end if type(a3) == 'table' then
    for k, v in pairs(a3) do
      t[#t + 1] = v
    end
  else t[#t + 1] = a3 end
  setmetatable(t, __vec4_mt)
  return t
end

math.dot = function(x, y) 
  if type(x) == 'table' then return x:dot(y) end 
  if type(y) == 'table' then return y:dot(x) end 
  return x * y
end

math.clamp = function(x, min, max)
  if type(x) == 'table' then return x:clamp(min, max) end 
  if x < min then return min end
  if x > max then return max end
  return x
end

math.saturate = function(x) return math.clamp(x, 0, 1) end

math.sign = function(x) 
  if type(x) == 'table' then return x:sign() end 
  if x < 0 then 
    return -1
  elseif x > 0 then 
    return 1
  else 
    return 0 
  end 
end

math.lerp = function(x, y, s)
  if type(x) == 'table' then return x:lerp(y, s) end 
  return x * (1 - s) + y * s
end

math.lerpInvSat = function (s, min, max) return math.saturate((s - min) / (max - min)) end
math.smoothstep = function (x) return x * x * (3 - 2 * x) end
math.smootherstep = function (x) return x * x * x * (x * (x * 6 - 15) + 10) end

function __conv_result(arg)
  function conv_val(arg)
    if arg == nil then return '' end
    if type(arg) == 'boolean' then return arg and '1' or '0' end
    if type(arg) == 'number' then return arg end
    if type(arg) == 'table' then return arg end
    return tostring(arg)
  end

  function conv_table(ret, arg)
    if type(arg) == 'table' then
      for k, v in pairs(arg) do
        conv_table(ret, v)
      end
    elseif type(arg) ~= 'function' then
      ret[#ret + 1] = conv_val(arg)
    end
  end

  if type(arg) == 'table' then
    local ret = {}
    conv_table(ret, arg)
    return ret
  end

  return conv_val(arg)
end

function discard() error('__discardError__') end
function def(i, x) if i ~= nil then return i end return x end
function def2(i, x, y) if i ~= nil then return i end return vec2(x, y) end
function def3(i, x, y, z) if i ~= nil then return i end return vec3(x, y, z) end
function def4(i, x, y, z, w) if i ~= nil then return i end return vec4(x, y, z, w) end

function ParseColor(v)
  if type(v) == "string" and v:sub(1, 1) == "#" then 
    if #v == 7 then 
      return { tonumber(v:sub(2, 3), 16) / 255, tonumber(v:sub(4, 5), 16) / 255, tonumber(v:sub(6, 7), 16) / 255 } 
    end
    if #v == 4 then 
      return { tonumber(v:sub(2, 2), 16) / 15, tonumber(v:sub(3, 3), 16) / 15, tonumber(v:sub(4, 4), 16) / 15 } 
    end
    error("Invalid color: "..v)
  end
  if type(v) == "table" and #v == 3 and ( v[1] > 1 or v[2] > 1 or v[3] > 1 ) then 
    return { v[1] / 255, v[2] / 255, v[3] / 255 } 
  end
  return v
end

abs = math.abs
acos = math.acos
asin = math.asin
atan = math.atan
ceil = math.ceil
cos = math.cos
deg = math.deg
exp = math.exp
floor = math.floor
fmod = math.fmod
mod = math.fmod
log = math.log
max = math.max
min = math.min
pi = math.pi
PI = math.pi
pow = math.pow
rad = math.rad
sin = math.sin
sqrt = math.sqrt
tan = math.tan
dot = math.dot
clamp = math.clamp
saturate = math.saturate
sign = math.sign
lerp = math.lerp
lerpInvSat = math.lerpInvSat
smoothstep = math.smoothstep
smootherstep = math.smootherstep