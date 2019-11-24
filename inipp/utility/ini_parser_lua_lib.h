#pragma once

namespace utils
{
	const char* LUA_STD_LUB = R""(local __vec2_mt = {
  __add = function (lhs, rhs) if type(rhs) ~= 'table' then return vec2(lhs[1]+rhs, lhs[2]+rhs) elseif type(lhs) ~= 'table' then return vec2(lhs+rhs[1], lhs+rhs[2]) else return vec2(lhs[1]+(rhs[1] ~= nil and rhs[1] or 0), lhs[2]+(rhs[2] ~= nil and rhs[2] or 0)) end end,
  __sub = function (lhs, rhs) if type(rhs) ~= 'table' then return vec2(lhs[1]-rhs, lhs[2]-rhs) elseif type(lhs) ~= 'table' then return vec2(lhs-rhs[1], lhs-rhs[2]) else return vec2(lhs[1]-(rhs[1] ~= nil and rhs[1] or 0), lhs[2]-(rhs[2] ~= nil and rhs[2] or 0)) end end,
  __mul = function (lhs, rhs) if type(rhs) ~= 'table' then return vec2(lhs[1]*rhs, lhs[2]*rhs) elseif type(lhs) ~= 'table' then return vec2(lhs*rhs[1], lhs*rhs[2]) else return vec2(lhs[1]*(rhs[1] ~= nil and rhs[1] or 0), lhs[2]*(rhs[2] ~= nil and rhs[2] or 0)) end end,
  __div = function (lhs, rhs) if type(rhs) ~= 'table' then return vec2(lhs[1]/rhs, lhs[2]/rhs) elseif type(lhs) ~= 'table' then return vec2(lhs/rhs[1], lhs/rhs[2]) else return vec2(lhs[1]/(rhs[1] ~= nil and rhs[1] or 0), lhs[2]/(rhs[2] ~= nil and rhs[2] or 0)) end end,
  __mod = function (lhs, rhs) if type(rhs) ~= 'table' then return vec2(lhs[1]%rhs, lhs[2]%rhs) elseif type(lhs) ~= 'table' then return vec2(lhs%rhs[1], lhs%rhs[2]) else return vec2(lhs[1]%(rhs[1] ~= nil and rhs[1] or 0), lhs[2]%(rhs[2] ~= nil and rhs[2] or 0)) end end,
  __pow = function (lhs, rhs) if type(rhs) ~= 'table' then return vec2(lhs[1]^rhs, lhs[2]^rhs) elseif type(lhs) ~= 'table' then return vec2(lhs^rhs[1], lhs^rhs[2]) else return vec2(lhs[1]^(rhs[1] ~= nil and rhs[1] or 0), lhs[2]^(rhs[2] ~= nil and rhs[2] or 0)) end end,
  __lt = function (lhs, rhs) if type(rhs) ~= 'table' then return vec2(lhs[1]<rhs, lhs[2]<rhs) elseif type(lhs) ~= 'table' then return vec2(lhs<rhs[1], lhs<rhs[2]) else return vec2(lhs[1]<(rhs[1] ~= nil and rhs[1] or 0), lhs[2]<(rhs[2] ~= nil and rhs[2] or 0)) end end,
  __le = function (lhs, rhs) if type(rhs) ~= 'table' then return vec2(lhs[1]<=rhs, lhs[2]<=rhs) elseif type(lhs) ~= 'table' then return vec2(lhs<=rhs[1], lhs<=rhs[2]) else return vec2(lhs[1]<=(rhs[1] ~= nil and rhs[1] or 0), lhs[2]<=(rhs[2] ~= nil and rhs[2] or 0)) end end,
  __eq = function (lhs, rhs)
    if type(rhs) ~= 'table' then return lhs[1] == rhs and lhs[2] == rhs
    elseif type(lhs) ~= 'table' then return lhs == rhs[1] and lhs == rhs[2]
    else return lhs[1] == (rhs[1] ~= nil and rhs[1] or 0) and lhs[2] == (rhs[2] ~= nil and rhs[2] or 0) end
  end,
  __len = function (s) return s:length() end,
  __unm = function (s) return vec2(-s[1], -s[2]) end,
  __tostring = function (s) return '{' ..s[1]..', '..s[2].. '}' end,
  __index = function (s, key)
    if key == 'len' or key == 'length' then return function (s) return math.sqrt(s[1]^2+s[2]^2) end end
    if key == 'normalize' then return function (s) return s / s:length() end end
    if key == 'normalizeSelf' then return function (s) local m = 1 / s:length() s[1] = s[1] * m s[2] = s[2] * m end end
    if key == 'dot' then return function (lhs, rhs)
      if type(rhs) ~= 'table' then return lhs[1] * rhs + lhs[2] * rhs
      elseif type(lhs) ~= 'table' then return lhs * rhs[1] + lhs * rhs[2]
      else return lhs[1] * (rhs[1] ~= nil and rhs[1] or 0) + lhs[2] * (rhs[2] ~= nil and rhs[2] or 0) end
    end end
    if key == 'x' or key == 'X' then return s[1] end
    if key == 'y' or key == 'Y' then return s[2] end
    if key == 'z' or key == 'Z' then return s[3] end
    if key == 'w' or key == 'W' then return s[4] end
    return nil
  end
}

function vec2(a0, a1)
  local t = {a0, a1}
  setmetatable(t, __vec2_mt)
  return t
end

local __vec3_mt = {
  __add = function (lhs, rhs) if type(rhs) ~= 'table' then return vec3(lhs[1]+rhs, lhs[2]+rhs, lhs[3]+rhs) elseif type(lhs) ~= 'table' then return vec3(lhs+rhs[1], lhs+rhs[2], lhs+rhs[3]) else return vec3(lhs[1]+(rhs[1] ~= nil and rhs[1] or 0), lhs[2]+(rhs[2] ~= nil and rhs[2] or 0), lhs[3]+(rhs[3] ~= nil and rhs[3] or 0)) end end,
  __sub = function (lhs, rhs) if type(rhs) ~= 'table' then return vec3(lhs[1]-rhs, lhs[2]-rhs, lhs[3]-rhs) elseif type(lhs) ~= 'table' then return vec3(lhs-rhs[1], lhs-rhs[2], lhs-rhs[3]) else return vec3(lhs[1]-(rhs[1] ~= nil and rhs[1] or 0), lhs[2]-(rhs[2] ~= nil and rhs[2] or 0), lhs[3]-(rhs[3] ~= nil and rhs[3] or 0)) end end,
  __mul = function (lhs, rhs) if type(rhs) ~= 'table' then return vec3(lhs[1]*rhs, lhs[2]*rhs, lhs[3]*rhs) elseif type(lhs) ~= 'table' then return vec3(lhs*rhs[1], lhs*rhs[2], lhs*rhs[3]) else return vec3(lhs[1]*(rhs[1] ~= nil and rhs[1] or 0), lhs[2]*(rhs[2] ~= nil and rhs[2] or 0), lhs[3]*(rhs[3] ~= nil and rhs[3] or 0)) end end,
  __div = function (lhs, rhs) if type(rhs) ~= 'table' then return vec3(lhs[1]/rhs, lhs[2]/rhs, lhs[3]/rhs) elseif type(lhs) ~= 'table' then return vec3(lhs/rhs[1], lhs/rhs[2], lhs/rhs[3]) else return vec3(lhs[1]/(rhs[1] ~= nil and rhs[1] or 0), lhs[2]/(rhs[2] ~= nil and rhs[2] or 0), lhs[3]/(rhs[3] ~= nil and rhs[3] or 0)) end end,
  __mod = function (lhs, rhs) if type(rhs) ~= 'table' then return vec3(lhs[1]%rhs, lhs[2]%rhs, lhs[3]%rhs) elseif type(lhs) ~= 'table' then return vec3(lhs%rhs[1], lhs%rhs[2], lhs%rhs[3]) else return vec3(lhs[1]%(rhs[1] ~= nil and rhs[1] or 0), lhs[2]%(rhs[2] ~= nil and rhs[2] or 0), lhs[3]%(rhs[3] ~= nil and rhs[3] or 0)) end end,
  __pow = function (lhs, rhs) if type(rhs) ~= 'table' then return vec3(lhs[1]^rhs, lhs[2]^rhs, lhs[3]^rhs) elseif type(lhs) ~= 'table' then return vec3(lhs^rhs[1], lhs^rhs[2], lhs^rhs[3]) else return vec3(lhs[1]^(rhs[1] ~= nil and rhs[1] or 0), lhs[2]^(rhs[2] ~= nil and rhs[2] or 0), lhs[3]^(rhs[3] ~= nil and rhs[3] or 0)) end end,
  __lt = function (lhs, rhs) if type(rhs) ~= 'table' then return vec3(lhs[1]<rhs, lhs[2]<rhs, lhs[3]<rhs) elseif type(lhs) ~= 'table' then return vec3(lhs<rhs[1], lhs<rhs[2], lhs<rhs[3]) else return vec3(lhs[1]<(rhs[1] ~= nil and rhs[1] or 0), lhs[2]<(rhs[2] ~= nil and rhs[2] or 0), lhs[3]<(rhs[3] ~= nil and rhs[3] or 0)) end end,
  __le = function (lhs, rhs) if type(rhs) ~= 'table' then return vec3(lhs[1]<=rhs, lhs[2]<=rhs, lhs[3]<=rhs) elseif type(lhs) ~= 'table' then return vec3(lhs<=rhs[1], lhs<=rhs[2], lhs<=rhs[3]) else return vec3(lhs[1]<=(rhs[1] ~= nil and rhs[1] or 0), lhs[2]<=(rhs[2] ~= nil and rhs[2] or 0), lhs[3]<=(rhs[3] ~= nil and rhs[3] or 0)) end end,
  __eq = function (lhs, rhs)
    if type(rhs) ~= 'table' then return lhs[1] == rhs and lhs[2] == rhs and lhs[3] == rhs
    elseif type(lhs) ~= 'table' then return lhs == rhs[1] and lhs == rhs[2] and lhs == rhs[3]
    else return lhs[1] == (rhs[1] ~= nil and rhs[1] or 0) and lhs[2] == (rhs[2] ~= nil and rhs[2] or 0) and lhs[3] == (rhs[3] ~= nil and rhs[3] or 0) end
  end,
  __len = function (s) return s:length() end,
  __unm = function (s) return vec3(-s[1], -s[2], -s[3]) end,
  __tostring = function (s) return '{' ..s[1]..', '..s[2]..', '..s[3].. '}' end,
  __index = function (s, key)
    if key == 'len' or key == 'length' then return function (s) return math.sqrt(s[1]^2+s[2]^2+s[3]^2) end end
    if key == 'normalize' then return function (s) return s / s:length() end end
    if key == 'normalizeSelf' then return function (s) local m = 1 / s:length() s[1] = s[1] * m s[2] = s[2] * m s[3] = s[3] * m end end
    if key == 'dot' then return function (lhs, rhs)
      if type(rhs) ~= 'table' then return lhs[1] * rhs + lhs[2] * rhs + lhs[3] * rhs
      elseif type(lhs) ~= 'table' then return lhs * rhs[1] + lhs * rhs[2] + lhs * rhs[3]
      else return lhs[1] * (rhs[1] ~= nil and rhs[1] or 0) + lhs[2] * (rhs[2] ~= nil and rhs[2] or 0) + lhs[3] * (rhs[3] ~= nil and rhs[3] or 0) end
    end end
    if key == 'x' or key == 'X' then return s[1] end
    if key == 'y' or key == 'Y' then return s[2] end
    if key == 'z' or key == 'Z' then return s[3] end
    if key == 'w' or key == 'W' then return s[4] end
    return nil
  end
}

function vec3(a0, a1, a2)
  local t = {a0, a1, a2}
  setmetatable(t, __vec3_mt)
  return t
end

local __vec4_mt = {
  __add = function (lhs, rhs) if type(rhs) ~= 'table' then return vec4(lhs[1]+rhs, lhs[2]+rhs, lhs[3]+rhs, lhs[4]+rhs) elseif type(lhs) ~= 'table' then return vec4(lhs+rhs[1], lhs+rhs[2], lhs+rhs[3], lhs+rhs[4]) else return vec4(lhs[1]+(rhs[1] ~= nil and rhs[1] or 0), lhs[2]+(rhs[2] ~= nil and rhs[2] or 0), lhs[3]+(rhs[3] ~= nil and rhs[3] or 0), lhs[4]+(rhs[4] ~= nil and rhs[4] or 0)) end end,
  __sub = function (lhs, rhs) if type(rhs) ~= 'table' then return vec4(lhs[1]-rhs, lhs[2]-rhs, lhs[3]-rhs, lhs[4]-rhs) elseif type(lhs) ~= 'table' then return vec4(lhs-rhs[1], lhs-rhs[2], lhs-rhs[3], lhs-rhs[4]) else return vec4(lhs[1]-(rhs[1] ~= nil and rhs[1] or 0), lhs[2]-(rhs[2] ~= nil and rhs[2] or 0), lhs[3]-(rhs[3] ~= nil and rhs[3] or 0), lhs[4]-(rhs[4] ~= nil and rhs[4] or 0)) end end,
  __mul = function (lhs, rhs) if type(rhs) ~= 'table' then return vec4(lhs[1]*rhs, lhs[2]*rhs, lhs[3]*rhs, lhs[4]*rhs) elseif type(lhs) ~= 'table' then return vec4(lhs*rhs[1], lhs*rhs[2], lhs*rhs[3], lhs*rhs[4]) else return vec4(lhs[1]*(rhs[1] ~= nil and rhs[1] or 0), lhs[2]*(rhs[2] ~= nil and rhs[2] or 0), lhs[3]*(rhs[3] ~= nil and rhs[3] or 0), lhs[4]*(rhs[4] ~= nil and rhs[4] or 0)) end end,
  __div = function (lhs, rhs) if type(rhs) ~= 'table' then return vec4(lhs[1]/rhs, lhs[2]/rhs, lhs[3]/rhs, lhs[4]/rhs) elseif type(lhs) ~= 'table' then return vec4(lhs/rhs[1], lhs/rhs[2], lhs/rhs[3], lhs/rhs[4]) else return vec4(lhs[1]/(rhs[1] ~= nil and rhs[1] or 0), lhs[2]/(rhs[2] ~= nil and rhs[2] or 0), lhs[3]/(rhs[3] ~= nil and rhs[3] or 0), lhs[4]/(rhs[4] ~= nil and rhs[4] or 0)) end end,
  __mod = function (lhs, rhs) if type(rhs) ~= 'table' then return vec4(lhs[1]%rhs, lhs[2]%rhs, lhs[3]%rhs, lhs[4]%rhs) elseif type(lhs) ~= 'table' then return vec4(lhs%rhs[1], lhs%rhs[2], lhs%rhs[3], lhs%rhs[4]) else return vec4(lhs[1]%(rhs[1] ~= nil and rhs[1] or 0), lhs[2]%(rhs[2] ~= nil and rhs[2] or 0), lhs[3]%(rhs[3] ~= nil and rhs[3] or 0), lhs[4]%(rhs[4] ~= nil and rhs[4] or 0)) end end,
  __pow = function (lhs, rhs) if type(rhs) ~= 'table' then return vec4(lhs[1]^rhs, lhs[2]^rhs, lhs[3]^rhs, lhs[4]^rhs) elseif type(lhs) ~= 'table' then return vec4(lhs^rhs[1], lhs^rhs[2], lhs^rhs[3], lhs^rhs[4]) else return vec4(lhs[1]^(rhs[1] ~= nil and rhs[1] or 0), lhs[2]^(rhs[2] ~= nil and rhs[2] or 0), lhs[3]^(rhs[3] ~= nil and rhs[3] or 0), lhs[4]^(rhs[4] ~= nil and rhs[4] or 0)) end end,
  __lt = function (lhs, rhs) if type(rhs) ~= 'table' then return vec4(lhs[1]<rhs, lhs[2]<rhs, lhs[3]<rhs, lhs[4]<rhs) elseif type(lhs) ~= 'table' then return vec4(lhs<rhs[1], lhs<rhs[2], lhs<rhs[3], lhs<rhs[4]) else return vec4(lhs[1]<(rhs[1] ~= nil and rhs[1] or 0), lhs[2]<(rhs[2] ~= nil and rhs[2] or 0), lhs[3]<(rhs[3] ~= nil and rhs[3] or 0), lhs[4]<(rhs[4] ~= nil and rhs[4] or 0)) end end,
  __le = function (lhs, rhs) if type(rhs) ~= 'table' then return vec4(lhs[1]<=rhs, lhs[2]<=rhs, lhs[3]<=rhs, lhs[4]<=rhs) elseif type(lhs) ~= 'table' then return vec4(lhs<=rhs[1], lhs<=rhs[2], lhs<=rhs[3], lhs<=rhs[4]) else return vec4(lhs[1]<=(rhs[1] ~= nil and rhs[1] or 0), lhs[2]<=(rhs[2] ~= nil and rhs[2] or 0), lhs[3]<=(rhs[3] ~= nil and rhs[3] or 0), lhs[4]<=(rhs[4] ~= nil and rhs[4] or 0)) end end,
  __eq = function (lhs, rhs)
    if type(rhs) ~= 'table' then return lhs[1] == rhs and lhs[2] == rhs and lhs[3] == rhs and lhs[4] == rhs
    elseif type(lhs) ~= 'table' then return lhs == rhs[1] and lhs == rhs[2] and lhs == rhs[3] and lhs == rhs[4]
    else return lhs[1] == (rhs[1] ~= nil and rhs[1] or 0) and lhs[2] == (rhs[2] ~= nil and rhs[2] or 0) and lhs[3] == (rhs[3] ~= nil and rhs[3] or 0) and lhs[4] == (rhs[4] ~= nil and rhs[4] or 0) end
  end,
  __len = function (s) return s:length() end,
  __unm = function (s) return vec4(-s[1], -s[2], -s[3], -s[4]) end,
  __tostring = function (s) return '{' ..s[1]..', '..s[2]..', '..s[3]..', '..s[4].. '}' end,
  __index = function (s, key)
    if key == 'len' or key == 'length' then return function (s) return math.sqrt(s[1]^2+s[2]^2+s[3]^2+s[4]^2) end end
    if key == 'normalize' then return function (s) return s / s:length() end end
    if key == 'normalizeSelf' then return function (s) local m = 1 / s:length() s[1] = s[1] * m s[2] = s[2] * m s[3] = s[3] * m s[4] = s[4] * m end end
    if key == 'dot' then return function (lhs, rhs)
      if type(rhs) ~= 'table' then return lhs[1] * rhs + lhs[2] * rhs + lhs[3] * rhs + lhs[4] * rhs
      elseif type(lhs) ~= 'table' then return lhs * rhs[1] + lhs * rhs[2] + lhs * rhs[3] + lhs * rhs[4]
      else return lhs[1] * (rhs[1] ~= nil and rhs[1] or 0) + lhs[2] * (rhs[2] ~= nil and rhs[2] or 0) + lhs[3] * (rhs[3] ~= nil and rhs[3] or 0) + lhs[4] * (rhs[4] ~= nil and rhs[4] or 0) end
    end end
    if key == 'x' or key == 'X' then return s[1] end
    if key == 'y' or key == 'Y' then return s[2] end
    if key == 'z' or key == 'Z' then return s[3] end
    if key == 'w' or key == 'W' then return s[4] end
    return nil
  end
}

function vec4(a0, a1, a2, a3)
  local t = {a0, a1, a2, a3}
  setmetatable(t, __vec4_mt)
  return t
end

math.dot = function(x, y) 
  if type(x) == 'table' then return x:dot(y) end 
  if type(y) == 'table' then return y:dot(x) end 
  return x * y
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
dot = math.dot)"";
}