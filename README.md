# INIpp

INIpp is an extension for good old INI format, designed specially for Custom Shaders Patch for Assetto Corsa, in attempt to make preparing configs easier. A change there, a tweak here, and it became incompatible with regular parsers, and then things only got worse, to a point where my initial idea of making specialized parsers for Python and JavaScript became a bit unrealistic. Maybe later I’ll get back to it, once format is established, but for now, here is some sort of preprocessor, which is basically just some code parsing those configs withing CSP.

In main mode, INIpp outputs data in JSON format, as not all data could be represented with INI (look at “Quotes for specific values” section). There is a flag to change it behavior though.

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
KEY = "value, with; all [sorts] of=//symbols"
```

### Including

One INI-file might reference other files, with `[INCLUDE]` or `[INCLUDE_...]` sections, like so:

```ini
[INCLUDE]
INCLUDE = common/shared_file.ini
```

Included file will be loaded in place of `[INCLUDE]` section, although with some extra detail (see “Variables”). Important to note: if some property is set twice, second value will be used. So, you can overwrite values in included files with sections coming afterwards.

### Variables

In order to try and reduce copy-paste, included INI-files can use variables. Their values are set in `INCLUDE` sections, and the whole thing looks like this:

###### main.ini
```ini
[INCLUDE]
INCLUDE = extra.ini
VAR_0 = SomeVariable, 10
```

###### extra.ini
```ini
[SECTION_1]
KEY = $SomeVariable

[SECTION_2]
KEY = ${SomeVariable} ; both ways of referring to variable would work
```

Included files are able to specify default values for variables like so:

```ini
[DEFAULTS]
VAR_0 = SomeVariable, 10

[SECTION_1]
KEY = $SomeVariable
```

This way, you can also define and use variables in a main, not included anywhere file, if needed. Not quite sure where it might be needed, but it would work.

Variable, of course, could be set to a list of values, as everything else:

```ini
[DEFAULTS]
VAR_0 = PointInSpace, 12.3, 14.6, -25.2

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

Concatenation is also supported:

```ini
[DEFAULTS]
VAR_0 = Prefix, ello

[SECTION_1]
GREETING_0 = H${Prefix} World   ; works either with curly braces
GREETING_1 = "H$Prefix World"   ; or with double quites
GREETING_FAILED_0 = H$Prefix World      ; this is not going to work
GREETING_FAILED_1 = 'H${Prefix} World'  ; and this
```

Concatenation with several values would behave as expected as well, and also, one variable can reference another:

```ini
[DEFAULTS]
VAR_0 = SomeVariable, A, B, C
VAR_1 = OtherVariable, $SomeVariable, "[$SomeVariable]"

[SECTION_1]
LETTERS_WITH_ZEROS = ${SomeVariable}0 ; A0, B0, C0
LETTERS_AND_LETTERS_IN_BRACKETS = $OtherVariable ; A, B, C, [A], [B], [C]
```

**Note:** at the moment, quotes are being resolved at the end, being removed from beginning and end of each item of each value. Because of this, concatenating string with variable which contains quotes will fail. 

### More to come

I’ll try to fix this ↑ bug first, and then continue.

# Licence

MIT.
