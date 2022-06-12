function vecType(D){
  function repeat(cb, j = ', '){
    let r = [];
    for (let i = 0; i < D; i++) r.push(cb(i));
    return r.join(j);
  }
  
  function args(){
    return repeat(x => `a${x}`);
  }

  function op2(op, X){
    return `${op} = function (L, R) \
if type(R) ~= 'table' then return vec${D}(${repeat(x => `L[${x + 1}]${X}R`)}) \
elseif type(L) ~= 'table' then return vec${D}(${repeat(x => `L${X}R[${x + 1}]`)}) \
else return vec${D}(${repeat(x => `L[${x + 1}]${X}(R[${x + 1}] and R[${x + 1}] or 0)`)}) end \
end`;
  }

  let extras = '';
  if (D == 3){
    extras += `

    if key == 'cross' then return function (s, o) 
      return vec3(s[2] * o[3] - s[3] * o[2],
        s[3] * o[1] - s[1] * o[3],
        s[1] * o[2] - s[2] * o[1])
    end end`
  }

  return `local __vec${D}_mt = {
  ${op2('__add', '+')},
  ${op2('__sub', '-')},
  ${op2('__mul', '*')},
  ${op2('__div', '/')},
  ${op2('__mod', '%')},
  ${op2('__pow', '^')},
  ${op2('__lt', '<')},
  ${op2('__le', '<=')},
  __eq = function (L, R)
    if type(R) ~= 'table' then return ${repeat(x => `L[${x + 1}] == R`, ' and ')}
    elseif type(L) ~= 'table' then return ${repeat(x => `L == R[${x + 1}]`, ' and ')}
    else return ${repeat(x => `L[${x + 1}] == (R[${x + 1}] and R[${x + 1}] or 0)`, ' and ')} end
  end,
  __unm = function (s) return vec${D}(${repeat(x => `-s[${x + 1}]`)}) end,
  __tostring = function (s) return '{' ${repeat(x => `..s[${x + 1}]..`, '\', \'')} '}' end,
  __index = function (s, key)
    if key == 'len' or key == 'length' then return function (s) 
      return math.sqrt(${repeat(x => `s[${x + 1}]^2`, '+')}) 
    end end

    if key == 'normalize' then return function (s) 
      return s / s:length() 
    end end

    if key == 'normalizeSelf' then return function (s) 
      local m = 1 / s:length() 
      ${repeat(x => `s[${x + 1}] = s[${x + 1}] * m`, ' ')} 
      return s
    end end

    if key == 'dot' then return function (L, R)
      if type(R) ~= 'table' then return ${repeat(x => `L[${x + 1}] * R`, ' + ')}
      else return ${repeat(x => `L[${x + 1}] * (R[${x + 1}] and R[${x + 1}] or 0)`, ' + ')} end
    end end

    if key == 'clamp' then return function (L, min, max)
      return vec${D}(${repeat(x => `math.clamp(L[${x + 1}], 
        type(min) == 'table' and (min[${x + 1}] and min[${x + 1}] or 0) or min, 
        type(max) == 'table' and (max[${x + 1}] and max[${x + 1}] or 0) or max)`)})
    end end

    if key == 'lerp' then return function (L, R, s)
      return vec${D}(${repeat(x => `math.lerp(L[${x + 1}], 
        type(R) == 'table' and (R[${x + 1}] and R[${x + 1}] or 0) or R, 
        type(s) == 'table' and (s[${x + 1}] and s[${x + 1}] or 0) or s)`)})
    end end

    if key == 'sign' then return function (L)
      return vec${D}(${repeat(x => `math.sign(L[${x + 1}])`)})
    end end

    if key == 'x' or key == 'X' then return s[1] end
    if key == 'y' or key == 'Y' then return s[2] end
    if key == 'z' or key == 'Z' then return s[3] end
    if key == 'w' or key == 'W' then return s[4] end

    ${extras}
    return nil
  end
}

function vec${D}(${args()})
  local t = {}
  ${repeat(x => `if type(a${x}) == 'table' then
    for k, v in pairs(a${x}) do
      t[#t + 1] = v
    end
  else t[#t + 1] = a${x} end`, ' ')}
  setmetatable(t, __vec${D}_mt)
  return t
end
`;
}

let aliases = {
  abs: 'math.abs',
  acos: 'math.acos',
  asin: 'math.asin',
  atan: 'math.atan',
  ceil: 'math.ceil',
  cos: 'math.cos',
  deg: 'math.deg',
  exp: 'math.exp',
  floor: 'math.floor',
  fmod: 'math.fmod',
  mod: 'math.fmod',
  log: 'math.log',
  max: 'math.max',
  min: 'math.min',
  pi: 'math.pi',
  PI: 'math.pi',
  pow: 'math.pow',
  rad: 'math.rad',
  sin: 'math.sin',
  sqrt: 'math.sqrt',
  tan: 'math.tan',
  dot: 'math.dot',
  clamp: 'math.clamp',
  saturate: 'math.saturate',
  sign: 'math.sign',
  lerp: 'math.lerp',
  lerpInvSat: 'math.lerpInvSat',
  smoothstep: 'math.smoothstep',
  smootherstep: 'math.smootherstep',
};

let code = ['-- generated automatically, do not edit'];
code.push(vecType(2));
code.push(vecType(3));
code.push(vecType(4));
code.push(`math.dot = function(x, y) 
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
`);

code.push(`function __conv_result(arg)
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
`);

for (let n in aliases){
  code.push(`${n} = ${aliases[n]}`);
}

const codeFull = code.join('\n');
const codeMinified = codeFull.replace(/\s\s+/g, ' ').replace(/\s([=~]=|[<>*+=-])\s/g, '$1').replace(/([,)'\]{}])\s/g, '$1').replace(/\s([('\[{}])/g, '$1')

fs.writeFileSync('std.lua', codeMinified);
fs.writeFileSync('../utility/ini_parser_lua_lib.h', `#pragma once

namespace utils
{
\tconst char* LUA_STD_LUB = R""(` + codeMinified + `)"";
}`);

const luaJit = $[process.env['LUA_JIT']];
luaJit('test.lua');
luaJit('-bn', 'std', 'std.lua', 'std.raw');
