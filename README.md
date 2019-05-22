# INIpp

INIpp is an extension for good old INI format, designed specially for Custom Shaders Patch for Assetto Corsa, in attempt to make preparing configs easier. A change there, a tweak here, and it became incompatible with regular parsers, and then things only got worse, to a point where my initial idea of making specialized parsers for Python and JavaScript became a bit unrealistic. Maybe later I’ll get back to it, once format is established, but for now, here is some sort of preprocessor, which is basically just some code parsing those configs withing CSP.

In main mode, INIpp outputs data in JSON format, as not all data could be represented with INI (look at “Quotes for specific values” section). There is a flag to change it behavior though.

# Important note

Please don’t think you need to read any of this in order to work with configs. All of this is mainly meant for standard includable files, in order to make creating actual configs easier. Now, they would look like this:

```ini
[INCLUDE: common/materials_carpaint.ini]
CarPaintMaterial = Carpaint

[Material_CarPaint]
Skins = ?
ClearCoatThickness = 0.1

[Material_CarPaint_Gold]
Skins = yellow
```

And included file will unwrap it into regular `SHADER_REPLACEMENT_…` mess.

# Features

### Auto-indexing sections

Instead of writing `[SECTION_0]`, `[SECTION_1]`, `[SECTION_2]`, … and painfully keeping track of used indices to avoid conflicts, just use “...” (three dots, not “…”). Parser will automatically find next free index for each section, keeping them in order. You can combine both styles too, to force specific sections to have specific indices if required. No more conflics.

### Setting values to several sections at once

To reduce copy-paste, here, both sections will get “KEY_SHARED=VALUE” property:

```ini
[SECTION_0]
KEY = 0

[SECTION_1]
KEY = 1

[SECTION_0, SECTION_1]
KEY_SHARED = VALUE
```

### Quotes for specific values

With regular INI-files (at least the way they work in Assetto Corsa), it’s impossible to have a multi-line value, or a value with symbols like “[”, “]”, “;”, “,” or “//”. With INIpp, you can wrap values in quotes, either “"” or “'”, and it’ll parse value as it is:

```ini
[SECTION]
KEY_0 = "value, with; \"all\" [sorts] of=//symbols"
KEY_1 = \"here, quotes do nothing"
KEY_2 = as well as "here"
KEY_3 = and\, this\, is\, a\, single\, value
```

### Including

One INI-file might reference other files with `[INCLUDE]` sections, like so:

```ini
[INCLUDE]
INCLUDE = common/shared_file.ini

[INCLUDE]
; as a special section, you can define more than one of those, both files
; will be included
INCLUDE = common/another_shared_file.ini
```

Included file will be loaded in place of `[INCLUDE]` sections, although with some extra detail (see “Variables”). Important to note: if some property is set twice, second value will be used. So, you can overwrite values in included files with sections coming afterwards.

For shorter version, you can use this syntax:

```ini
[INCLUDE: common/shared_file.ini]
```

### Variables

In order to try and reduce copy-paste, included INI-files can use variables. Their values are set in `[INCLUDE]` sections, and the whole thing looks like this:

###### main.ini
```ini
[INCLUDE: extra.ini]
SomeVariable = 10

; value of “SomeVariable” won’t be available to “another_extra.ini”
[INCLUDE: another_extra.ini]
```

###### extra.ini
```ini
[SECTION_1]
KEY = $SomeVariable

[SECTION_2]
KEY = ${SomeVariable} ; both ways of referring to variable would work
```

**Important:** please prefer using CamelCase for variables (and, shown later in docs, templates). CSP, as most other apps, expects values in upper case, so there’ll be less chance of confict.

Included files are able to specify default values for variables like so:

```ini
[DEFAULTS]
SomeVariable = 10

[SECTION_1]
KEY = $SomeVariable
```

This way, you can also define and use variables in a main, not included anywhere file, if needed. Not quite sure where it might be needed, but it would work.

Variable, of course, could be set to a list of values, as everything else:

```ini
[DEFAULTS]
PointInSpace = 12.3, 14.6, -25.2

[SECTION_1]
POINT = $PointInSpace
NUMBER_OF_DIMENSIONS = ${PointInSpace:count}
COORD_X = ${PointInSpace:0}
COORD_Y = ${PointInSpace:1}
COORD_Z = ${PointInSpace:2}
COORD_LAST = ${PointInSpace:-1}
COORDS_XY = ${PointInSpace:0:2}      ; taking from 0th to 2th element excluding 2th
COORDS_XY_ALT = ${PointInSpace:0:-1} ; taking from 0th to 1th element from the end excluding it
COORDS_XZ = ${PointInSpace:0}, ${PointInSpace:2}
```

Another difference between referring to variable with and without curly quotes is that, if variable is missing, version without curly quotes will remain unchanged:

```ini
[SECTION_1]
VALUE_0 = $MissingValue    ; will resolve into “$MissingValue”
VALUE_1 = ${MissingValue}  ; will resolve into empty value
```

Concatenation is also supported:

```ini
[DEFAULTS]
Prefix = ello

[SECTION_1]
GREETING_0 = H${Prefix} World   ; works either with curly braces
GREETING_1 = "H$Prefix World"   ; or with double quotes
GREETING_2 = H$Prefix World     ; or even without quotes
GREETING_FAILED_1 = 'H${Prefix} World'  ; but not with single quotes
```

Concatenation with several values would behave as expected as well, and also, one variable can reference another:

```ini
[DEFAULTS]
SomeVariable = A, B
OtherVariable = $SomeVariable, "[$SomeVariable]"

[SECTION_1]
LETTERS_WITH_ZEROS = ${SomeVariable}0 ; A0, B0
LETTERS_AND_LETTERS_IN_BRACKETS = prefix $OtherVariable ; prefix A, prefix B, prefix [A], prefix [B]
```

### Skipping

If needed, you can skip entire sections with `ACTIVE=0`. With this properly found, parser will skip all following properties, while keeping that one. That might be especially helpful with variables:

```ini
[SHADER_REPLACEMENT_...]
ACTIVE=$UseSpecificReplacement  ; skip section if variable wasn’t set to 1
MATERIALS=$SomeMaterialsToTweak
SHADER=ksPerPixel

[SHADER_REPLACEMENT_...]
ACTIVE=${OtherMaterialsToTweak:count}  ; if materials are not set, whole section will be skipped
MATERIALS=$OtherMaterialsToTweak
SHADER=ksPerPixelNM
```

### Templates

With templates, you can create your own type of section which will unwrap into something different. Like so:

```ini
[TEMPLATE: Material_CarPaint]   ; prefix “TEMPLATE:” specifies it’s a template
SHADER = smCarPaint             ; this property will be copied to any section based on this template
MATERIALS = $CarPaintMaterial   ; templates can have their own variables

[SHADER_REPLACEMENT_... : Material_CarPaint]  ; with “:”, it marks section as based on a template
CarPaintMaterial = CarPaint_EXT               ; this is how section can specify template properties
SKINS = new_skin                              ; plus, it can define its own properties
```

This will be read into this:

```ini
[SHADER_REPLACEMENT_0]
MATERIALS = CarPaint_EXT
SHADER = smCarPaint
SKINS = new_skin
```

**Important:** please prefer using CamelCase for templates and variables. CSP, as most other apps, expects values in upper case, so there’ll be less chance of confict.

Templates, of course, can also specify default section name, which will be used if no specific is set, with `@OUTPUT` property:

```ini
[TEMPLATE: Material_CarPaint]
CarPaintShader = smCarPaint                 ; this is how pattern can define value by default
@OUTPUT = SHADER_REPLACEMENT_0CARPAINT_...  ; I highly recommend to use “...” in there: the whole idea 
                                            ; of templates was to be able to use them more than once
SHADER = $CarPaintShader
MATERIALS = $CarPaintMaterial

[Material_CarPaint]
CarPaintMaterial = CarPaint_EXT
```

Which produces:

```ini
[SHADER_REPLACEMENT_0CARPAINT_0]
MATERIALS = CarPaint_EXT
SHADER = smCarPaint
```

On top of that, templates can inherit one another:

```ini
[TEMPLATE: Material_CarPaint]
@OUTPUT = SHADER_REPLACEMENT_0CARPAINT_...
SHADER = smCarPaint
MATERIALS = $CarPaintMaterial

[TEMPLATE: Material_CarPaint_Old EXTENDS Material_CarPaint]
SHADER = smCarPaint_old

[Material_CarPaint_Old]
CarPaintMaterial = CarPaint_EXT

[Material_CarPaint_Old]  ; notice how we use the same template twice, with different parameters
CarPaintMaterial = CarPaint_EXT2
SKINS = some_skin
```

Those lines result it:

```ini
[SHADER_REPLACEMENT_0CARPAINT_0]
MATERIALS = CarPaint_EXT
SHADER = smCarPaint_old

[SHADER_REPLACEMENT_0CARPAINT_1]
MATERIALS = CarPaint_EXT2
SHADER = smCarPaint_old
SKINS = some_skin
```

Isn’t that nice? And consider that in everyday usage, mess that is templates will be hidden in included files.

### Mixins

Mixins are similar to templates, but could be deactivated with a condition, making them helpful in some specific cases. For example, imagine a template receiving either list of meshes or materials, and it needs to set `MESHES = ${Meshes}` or `MATERIALS = ${Materials}`, but with a condition that empty list shouldn’t be set at all (`MESHES=` could mean that thing should be applied to none meshes). With mixins, it’s an easy thing to set:

```ini
[MIXIN: _MeshesFilter]
@ACTIVE = ${Meshes:count}
MESHES = ${Meshes}

[MIXIN: _MaterialsFilter]
@ACTIVE = ${Materials:count}
MATERIALS = ${Materials}

[TEMPLATE: Material_DigitalScreen]
@OUTPUT = SHADER_REPLACEMENT_0_SCREEN_...
ACTIVE = $" ${Meshes:count} + ${Materials:count} "
@MIXIN = _MeshesFilter
@MIXIN = _MaterialsFilter
```

Not only templates, but any section can refer to a mixin. Also, mixins can include other mixins as well. Plus, `extends` keyword allows to build one mixins on top of others similar to templates.

### Expressions

This one is simple. If you add “$” in front of a string, its value will be calculated, thanks to Lua interpreter:

```ini
[TEST]
SQUARE_ROOT_OF_TWO = $" sqrt(2) "
```

On its own, it’s not very helpful, but could be convenient once variables join in:

```ini
[TEMPLATE: Material_CarPaint]
@OUTPUT = SHADER_REPLACEMENT_0CARPAINT_...
SHADER = smCarPaint
MATERIALS = $CarPaintMaterial
Reflectiveness = 0.8
FresnelF0 = 0.2
PROP_0 = fresnelMaxLevel, $Reflectiveness
PROP_1 = fresnelC, $" $Reflectiveness * $FresnelF0 "
PROP_2 = fresnelEXP, 5  ; that’s the correct value for this one in most cases, by the way, at least 
                        ; with common approach to PBR rendering
                        
[Material_CarPaint]
SKINS = some_very_reflective_skin
CarPaintMaterial = Carpaint
Reflectiveness = 0.8

[Material_CarPaint]
SKINS = not_that_reflective_skin
CarPaintMaterial = Carpaint
Reflectiveness = 0.4
```

And it generates this configuration:

```ini
[SHADER_REPLACEMENT_0CARPAINT_0]
SHADER = smCarPaint
MATERIALS = Carpaint
SKINS = some_very_reflective_skin
PROP_0 = fresnelMaxLevel, 0.8
PROP_1 = fresnelC, 0.16
PROP_2 = fresnelEXP, 5

[SHADER_REPLACEMENT_0CARPAINT_1]
SHADER = smCarPaint
MATERIALS = Carpaint
SKINS = not_that_reflective_skin
PROP_0 = fresnelMaxLevel, 0.4
PROP_1 = fresnelC, 0.08
PROP_2 = fresnelEXP, 5
```

**Important:** variables substitution works differently for expressions. Strings get wrapped in quotes, and if variable is a list, it’ll be passed as a table. Also, of course, if expression returns table, it’ll turn into a list.

### Functions

To simplify writing expressions, it’s possible to define functions. Here is an example:

```ini
; this function turns color from HEX or 0–255 format into regular normalized numbers
[FUNCTION: ParseColor]
ARGUMENTS = v  ; input arguments to be used in code below
PRIVATE = 0    ; if set to 1, Lua state will be reset before loading next file
CODE='
  if type(v) == "string" and v:sub(1, 1) == "#" then 
    if #v == 7 then return { tonumber(v:sub(2, 3), 16) / 255, tonumber(v:sub(4, 5), 16) / 255, tonumber(v:sub(6, 7), 16) / 255 } end
    if #v == 4 then return { tonumber(v:sub(2, 2), 16) / 15, tonumber(v:sub(3, 3), 16) / 15, tonumber(v:sub(4, 4), 16) / 15 } end
    error("Invalid color: "..v)
  end
  if type(v) == "table" and #v == 3 and ( v[1] > 1 or v[2] > 1 or v[3] > 1 ) then return { v[1] / 255, v[2] / 255, v[3] / 255 } end
  return v'
  
[DEFAULTS]
ColorHex = #abcdef
ColorHexShort = #f80
ColorRgb = 255, 127, 0
ColorRel = 1, 0.5, 0

[TEST]
VALUE1 = $" ParseColor( $ColorHex ) "       ; 0.67, 0.804, 0.937
VALUE2 = $" ParseColor( $ColorHexShort ) "  ; 1.0, 0.53, 0.0
VALUE3 = $" ParseColor( $ColorRgb ) "       ; 1.0, 0.5, 0.0
VALUE4 = $" ParseColor( $ColorRel ) "       ; 1.0, 0.5, 0.0
```

Some common functions could be moved into an external Lua-file, which can be included like this:

```
[USE: common/functions_base.lua]  ; search is done the same way as for included INI-files
PRIVATE = 0                       ; again, if set to 1, Lua state will be reset before loading next file
```

# Known issues and plans

- Auto-indexing values might have been a nice addition;
- There is no support for vectors or colors in expressions. At least they can receive and output tables now;
- Possibly, scripts could have read & write access to whole section.

# Credits

- For working with files and values, some code from [Reshade](https://github.com/crosire/reshade) by crosire is used;
- For sorting, [alphanum.hpp](http://www.davekoelle.com/alphanum.html) by David Koelle;
- JSON is serialized with [library by Niels Lohmann](https://github.com/nlohmann/json);
- Expressions are made possible by [Lua](https://www.lua.org/).
