# M3DE Project — Claude Code Instructions

## Project Overview

- **Description:** M3DE (Micro-3D Engine) — DOS-era 3D graphics engine with GFX-13 2D library
- **Status:** Fully restored — code has been modernized (PascalCase API, corrected normals/lighting) and builds cleanly under BCC 3.1
- **Language:** C and C++ (Borland Turbo C++ 3.1)
- **Platform:** DOS / DOSBox
- **Memory model:** Large (`-ml`) — required for `far` pointers and `farmalloc`

## Build

- **Compiler:** Borland Turbo C++ 3.1 (`BCC`)
- **Build command:** `build` (runs `build.bat`)
- **Clean command:** `build clean`
- **Build log:** `build.log` (created on each build)

## File Structure

| File       | Language | Description                                    |
|------------|----------|------------------------------------------------|
| gfx13.h   | C        | GFX-13 header (VGA Mode 13h graphics library)  |
| gfx13.c   | C        | GFX-13 implementation (inline assembly)        |
| m3de.h    | C++      | M3DE header (3D engine structs and functions)   |
| m3de.cpp  | C++      | M3DE implementation                             |
| test.cpp  | C++      | Test program (3D shape gallery with lighting)   |
| build.bat | Batch    | Build script for BCC 3.1                        |
| clean.bat | Batch    | Clean build artifacts (OBJ, EXE, LOG files)    |

## Code Conventions

- **Function names:** PascalCase (e.g., `TranslateWorld`, `GenerateGeosphere`)
- **Struct members:** Mixed case with underscores (e.g., `Number_Faces`, `Light_Type`)
- **Bracket style:** Spaced brackets `Face [ Fc ].v [ 0 ]`, spaced function calls `cos ( t + ti )`
- **Comment style:** `//****` file headers, `//----` section separators (78 chars), structured Function/Description/Arguments/Returns blocks
- **Line endings:** CRLF (`\r\n`)

---

## Lessons Learned

### DOS / DOSBox Build Environment

#### DO

- Use `-2` flag when compiling code that contains 286+ instructions (e.g., `REP OUTSB`). BCC defaults to 8086 mode and will reject these instructions without it.
- Compile `.c` files as C and `.cpp` files as C++ — BCC selects mode by file extension automatically.
- Use `extern "C" { }` wrappers in C headers (like `gfx13.h`) to ensure correct linkage when included from C++ code.
- Use the large memory model (`-ml`) for any code using `far` pointers or `farmalloc`.

#### DON'T

- Don't use `2>&1` stderr redirection in batch files — DOS `COMMAND.COM` does not support it. BCC writes all output (errors included) to stdout, so `>> build.log` alone is sufficient.
- Don't assume BCC enables extended instruction sets by default. Always check inline assembly for 286/386 instructions and add the appropriate flag (`-1`, `-2`, `-3`).

### Code Refactoring

#### DO

- When renaming functions globally, watch for name collisions with struct members. Use a placeholder strategy: temporarily replace struct member references, perform the global rename, then restore the placeholders. (Example: `Camera_Translation` was both a function name and a WORLD struct member.)
- When fixing spelling in identifiers, apply changes to both `.h` and `.cpp` files simultaneously to keep declarations and definitions in sync.
- When formatting code with regex-based scripts, verify edge cases manually — expressions like `2*M_PI/Sides` or struct initializers `{0,0,0}` are easily missed by simple spacing rules.

#### DON'T

- Don't run Python inline heredocs (`python3 << 'PYEOF'`) in MINGW64 bash — quoting issues cause `unexpected EOF` errors. Write the script to a temp file and execute it instead.
- Don't rename struct members when renaming functions to PascalCase, even if they share the same name as a function. Struct members are data, not API surface.

### Normals, Backface Culling, and Lighting

#### DO

- Understand how `flip_normal` works in the render loop: for `flip_normal == TRUE` faces, `GetNormal` is called with reversed vertex order to swap `n1`/`n2`. This means `n1` already points in the correct direction for both inner and outer faces. Always pass `LightTranslation(n1, n2, ...)` unconditionally — do not swap `n1`/`n2` based on `flip_normal`.
- When debugging lighting issues on inner faces (tubes, tori), verify that the problem is not in the normal computation (`GetNormal` vertex order) before touching the `LightTranslation` call.

#### DON'T

- Don't swap `n1`/`n2` arguments to `LightTranslation` for `flip_normal` faces. The `GetNormal` vertex-order swap already accounts for the reversed winding. An additional swap in `LightTranslation` inverts the illumination — faces that should be bright become dark and vice versa.

### GFX-13 / M3DE Integration

#### DO

- Cross-reference M3DE's function calls against the actual GFX-13 header (`gfx13.h`) before building. Previous versions of M3DE used underscore-separated names (e.g., `Fill_Triangle`) that don't match the current GFX-13 API (`FillTriangle`).
- Check that VGA palette functions use the correct calling convention: `SetPalette(col, count, segment, offset)` with `FP_SEG()` and `FP_OFF()` for far pointer decomposition.

#### DON'T

- Don't assume M3DE was originally written for the same version of GFX-13 in the repository. The API may have changed between versions.
