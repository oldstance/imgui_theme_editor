# imgui_theme_editor

An interactive editor for Dear ImGui's `ImGuiStyle` that can

- save/load the style under construction as **JSON** (for iterative editing), and
- export the finished style as a **C++ source/header pair** that reproduces it
  at runtime.

Built with C++17 / Dear ImGui v1.91.4 / GLFW 3.4 / nlohmann/json 3.11.3.

## Project layout

```
imgui_theme_editor/
├── CMakeLists.txt                 # Pulls all third-party deps via FetchContent
├── README.md                      # This file
└── imgui_theme_editor/
    └── sources/
        ├── main.cpp               # GLFW + OpenGL3 + Dear ImGui entry point
        ├── theme_editor.h/.cpp    # Editor UI (Sizes / Colors / Preview / Export)
        └── style_serializer.h/.cpp# JSON I/O + C++ code generator
```

## Building

The third-party dependencies (Dear ImGui, GLFW, nlohmann/json) are fetched
automatically by `FetchContent`, so no manual install is required. You need
CMake 3.24 or newer and a C++17-capable toolchain (Visual Studio 2022 has
been verified).

```powershell
cd D:\home\JUNKPOWERS\projects\PROJ_30007-GUNGNIR\04_sources\test\imgui_theme_editor

# Configure (the first run will git-clone ImGui/GLFW/nlohmann_json, ~minutes)
cmake -S . -B build/msvc-x64 -A x64

# Build (Release / Debug both work)
cmake --build build/msvc-x64 --config Release
```

The resulting binary lives at
`build/msvc-x64/Release/imgui_theme_editor.exe`.

### Reusing already fetched dependencies

If another local project (e.g. `modelconv`) has already fetched the same
libraries, you can point the editor at those copies to skip the git clones:

```powershell
cmake -S . -B build/msvc-x64 -A x64 `
    -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON="<path>/_deps/nlohmann_json-src" `
    -DFETCHCONTENT_SOURCE_DIR_GLFW="<path>/_deps/glfw-src" `
    -DFETCHCONTENT_SOURCE_DIR_IMGUI="<path>/_deps/imgui-src"
```

### Visual Studio (.vcxproj)

The bundled `imgui_theme_editor.slnx` / `imgui_theme_editor.vcxproj` are kept
for reference and are now set to C++17, but they do **not** include the
dependencies. The CMake build above is the recommended path because it
takes care of pulling and compiling Dear ImGui / GLFW for you.

## Using the editor

Launch `imgui_theme_editor.exe`. A single window titled
**ImGui Theme Editor** opens with four tabs and a menu bar.

### Tabs

- **Sizes** – every dimensional / behavioural member of `ImGuiStyle`
  (padding, rounding, borders, alignments, hover delays, anti-aliasing
  toggles, ...), grouped by category and edited with sliders / combo /
  checkboxes.
- **Colors** – all `ImGuiCol_COUNT` colors with a name filter, alpha-preview
  toggle, and optional hex display.
- **Preview** – a mini gallery of common widgets so the impact of your
  edits is visible in real time. Toggle `View → ImGui demo window` to open
  Dear ImGui's full demo as an additional reference.
- **Export** – paths and code-generation options plus live previews of the
  generated JSON, `.cpp` and `.h`.

### Menu bar

- **File**
  - *New (Dark / Light / Classic preset)* – reset to one of Dear ImGui's
    built-in presets.
  - *Load JSON…* / *Save JSON…* – read/write the JSON at the path
    configured in the Export tab.
  - *Export C++…* – emit the `.cpp` (and optionally `.h`) at the paths
    configured in the Export tab.
- **View**
  - *Live apply* – when on (the default), every edit is pushed into
    `ImGui::GetStyle()` immediately, so the rest of the UI reflects the
    in-progress theme.
  - *ImGui demo window* – toggles the Dear ImGui demo window.
- **Help** – shows the linked ImGui version and a brief usage hint.

### Export-tab settings

| Field                               | Meaning                                                      |
| ----------------------------------- | ------------------------------------------------------------ |
| `JSON path`                         | Destination for *Save JSON* / source for *Load JSON*.        |
| `Source path`                       | Destination for the generated `.cpp`.                        |
| `Header path`                       | Destination for the generated `.h`. Leave empty to skip header generation. |
| `Function name`                     | Name of the generated apply function (e.g. `ApplyTheme`).    |
| `Namespace`                         | Optional namespace wrapping the apply function. Leave empty for none. |
| `Theme display name`                | Label that appears in the generated file's comment header.   |
| `#pragma once`                      | If on, the generated header uses `#pragma once`; otherwise traditional include guards. |
| `#include the header from the .cpp` | If on, the generated `.cpp` adds `#include "<header>"` at the top. |

### Fields you can edit

Every field of `ImGuiStyle` in Dear ImGui v1.91.4 is exposed:

| Category        | Fields                                                       |
| --------------- | ------------------------------------------------------------ |
| Main            | `Alpha`, `DisabledAlpha`                                     |
| Window          | `WindowPadding`, `WindowRounding`, `WindowBorderSize`, `WindowMinSize`, `WindowTitleAlign`, `WindowMenuButtonPosition` |
| Child / Popup   | `ChildRounding`, `ChildBorderSize`, `PopupRounding`, `PopupBorderSize` |
| Frame           | `FramePadding`, `FrameRounding`, `FrameBorderSize`           |
| Item            | `ItemSpacing`, `ItemInnerSpacing`, `CellPadding`, `TouchExtraPadding`, `IndentSpacing`, `ColumnsMinSpacing` |
| Widget          | `ScrollbarSize`, `ScrollbarRounding`, `GrabMinSize`, `GrabRounding`, `LogSliderDeadzone`, `ColorButtonPosition`, `ButtonTextAlign`, `SelectableTextAlign` |
| Tab             | `TabRounding`, `TabBorderSize`, `TabMinWidthForCloseButton`, `TabBarBorderSize`, `TabBarOverlineSize` |
| Table           | `TableAngledHeadersAngle`, `TableAngledHeadersTextAlign`     |
| Separator       | `SeparatorTextBorderSize`, `SeparatorTextAlign`, `SeparatorTextPadding` |
| Tooltip / Hover | `HoverStationaryDelay`, `HoverDelayShort`, `HoverDelayNormal`, `HoverFlagsForTooltipMouse`, `HoverFlagsForTooltipNav` |
| Display         | `DisplayWindowPadding`, `DisplaySafeAreaPadding`, `MouseCursorScale` |
| Rendering       | `AntiAliasedLines`, `AntiAliasedLinesUseTex`, `AntiAliasedFill`, `CurveTessellationTol`, `CircleTessellationMaxError` |
| Colors          | `Colors[ImGuiCol_COUNT]` (every `ImGuiCol_` entry)           |

When a future Dear ImGui release adds a new `ImGuiStyle` member, only
`kDescs` in `style_serializer.cpp` has to be extended; the JSON I/O, the
editor UI and the C++ exporter all pick the new field up automatically.

## File formats

### JSON

```json
{
    "version": 1,
    "imgui_version": "1.91.4",
    "fields": {
        "Alpha": 1.0,
        "WindowPadding": [8.0, 8.0],
        "WindowMenuButtonPosition": "Left",
        "AntiAliasedLines": true,
        "HoverFlagsForTooltipMouse": 12288,
        "...": "..."
    },
    "colors": {
        "Text":     [1.0, 1.0, 1.0, 1.0],
        "WindowBg": [0.06, 0.06, 0.06, 0.94],
        "...":      []
    }
}
```

- Keys mirror the actual `ImGuiStyle` member names and the canonical color
  names returned by `ImGui::GetStyleColorName()`, so the format is robust
  to future enum reordering.
- Unknown keys are ignored on load, which makes the format forward
  compatible across ImGui versions.

### Generated C++

With `Function name = ApplyTheme`, `Namespace = theme`,
`Header path = theme.h`, `Source path = theme.cpp` the exporter produces
roughly:

```cpp
// theme.h ------------------------------------------------------------
#pragma once
namespace theme {
void ApplyTheme();
} // namespace theme

// theme.cpp ----------------------------------------------------------
#include "theme.h"
#include <cfloat>
#include <imgui.h>

namespace theme {

void ApplyTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // ---- Main ----
    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.6f;

    // ---- Window ----
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    // ...

    // ---- Colors ----
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    // ...
}

} // namespace theme
```

Drop the two files into your project, then call `theme::ApplyTheme()`
once your ImGui context exists (typically right after
`ImGui::CreateContext()` or after switching themes at runtime).

## Tips

- Set *Live apply* off when comparing presets side by side, then re-enable
  it to commit.
- The Export tab also has a *Preview JSON* / *Preview generated C++*
  section so you can inspect the exact text that will be written without
  touching the filesystem.
- The status bar at the bottom of the editor reports the last save / load
  / export result (green = success, red = error) for a few seconds.

## Credits

`imgui_theme_editor` would not exist without these excellent open-source
projects:

- **Dear ImGui** — Omar Cornut and contributors,
  <https://github.com/ocornut/imgui> (MIT License). The whole reason this
  tool exists; we edit `ImGuiStyle` and depend on the GLFW + OpenGL3
  backends.
- **GLFW** — Marcus Geelnard, Camilla Löwy and contributors,
  <https://github.com/glfw/glfw> (zlib/libpng License). Windowing,
  context creation and input.
- **nlohmann/json** — Niels Lohmann and contributors,
  <https://github.com/nlohmann/json> (MIT License). JSON read/write for
  the theme files.

All three are fetched verbatim via CMake `FetchContent`; their original
license terms apply to the corresponding source/binary artifacts. Please
keep their copyright notices intact when redistributing builds that
include them.

## License / terms of use

The `imgui_theme_editor` sources written in this repository (everything
under `imgui_theme_editor/sources/`, the `CMakeLists.txt`, the
`.vcxproj`/`.vcxproj.filters` and this `README.md`) are released under
the MIT License:

```
MIT License

Copyright (c) 2026 imgui_theme_editor contributors

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

In plain English: **feel free to use, modify, fork, embed, ship in
commercial or hobby projects, rebrand, and otherwise do whatever you
want with the editor sources.** Attribution is appreciated but not
required for the editor itself. The bundled third-party libraries
(Dear ImGui, GLFW, nlohmann/json) retain their own licenses listed in
*Credits*; make sure those notices ride along with any binary you
distribute.

Generated artifacts — both the JSON theme files and the `.cpp`/`.h`
output of *Export C++* — are **yours**. They are considered the user's
data, not derivative work of this editor, so you can drop them into any
project under any license without restriction.
