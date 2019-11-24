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
    return `${op} = function (lhs, rhs) \
if type(rhs) ~= 'table' then return vec${D}(${repeat(x => `lhs[${x + 1}]${X}rhs`)}) \
elseif type(lhs) ~= 'table' then return vec${D}(${repeat(x => `lhs${X}rhs[${x + 1}]`)}) \
else return vec${D}(${repeat(x => `lhs[${x + 1}]${X}(rhs[${x + 1}] ~= nil and rhs[${x + 1}] or 0)`)}) end \
end`;
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
  __eq = function (lhs, rhs)
    if type(rhs) ~= 'table' then return ${repeat(x => `lhs[${x + 1}] == rhs`, ' and ')}
    elseif type(lhs) ~= 'table' then return ${repeat(x => `lhs == rhs[${x + 1}]`, ' and ')}
    else return ${repeat(x => `lhs[${x + 1}] == (rhs[${x + 1}] ~= nil and rhs[${x + 1}] or 0)`, ' and ')} end
  end,
  __len = function (s) return s:length() end,
  __unm = function (s) return vec${D}(${repeat(x => `-s[${x + 1}]`)}) end,
  __tostring = function (s) return '{' ${repeat(x => `..s[${x + 1}]..`, '\', \'')} '}' end,
  __index = function (s, key)
    if key == 'len' or key == 'length' then return function (s) return math.sqrt(${repeat(x => `s[${x + 1}]^2`, '+')}) end end
    if key == 'normalize' then return function (s) return s / s:length() end end
    if key == 'normalizeSelf' then return function (s) local m = 1 / s:length() ${repeat(x => `s[${x + 1}] = s[${x + 1}] * m`, ' ')} end end
    if key == 'dot' then return function (lhs, rhs)
      if type(rhs) ~= 'table' then return ${repeat(x => `lhs[${x + 1}] * rhs`, ' + ')}
      elseif type(lhs) ~= 'table' then return ${repeat(x => `lhs * rhs[${x + 1}]`, ' + ')}
      else return ${repeat(x => `lhs[${x + 1}] * (rhs[${x + 1}] ~= nil and rhs[${x + 1}] or 0)`, ' + ')} end
    end end
    if key == 'x' or key == 'X' then return s[1] end
    if key == 'y' or key == 'Y' then return s[2] end
    if key == 'z' or key == 'Z' then return s[3] end
    if key == 'w' or key == 'W' then return s[4] end
    return nil
  end
}

function vec${D}(${args()})
  local t = {${args()}}
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
};

let code = [];
code.push(vecType(2));
code.push(vecType(3));
code.push(vecType(4));
code.push(`math.dot = function(x, y) 
  if type(x) == 'table' then return x:dot(y) end 
  if type(y) == 'table' then return y:dot(x) end 
  return x * y
end
`);

code.push(`function __conv_result(arg)
  function conv_val(arg)
    if arg == nil then return '' end
    if type(arg) == 'boolean' then return arg and '1' or '0' end
    if type(arg) == 'table' then return arg end
    return tostring(arg)
  end

  if type(rhs) == 'table' then
    local ret = {}
    for k, v in pairs(arg) do
      ret[k] = conv_val(v)
    end
    return ret
  end

  return conv_val(arg)
end
`);

for (let n in aliases){
  code.push(`${n} = ${aliases[n]}`);
}

fs.writeFileSync('std.lua', code.join('\n'));
fs.writeFileSync('../utility/ini_parser_lua_lib.h', `#pragma once

namespace utils
{
\tconst char* LUA_STD_LUB = R""(` + code.join('\n') + `)"";
}`);
$['D:/Applications/Cygwin/bin/lua.exe']('test.lua');
