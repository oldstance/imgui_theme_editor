// -----------------------------------------------------------------------------
// style_serializer.h
//
// JSON <-> ImGuiStyle conversion plus generation of ready-to-compile C++
// source/header files that reproduce a given ImGuiStyle at runtime.
//
// All metadata about ImGuiStyle is kept in a single descriptor table
// (kStyleFieldDescs) so JSON I/O, the UI editor and the C++ exporter stay in
// sync as new fields are added in future Dear ImGui releases.
// -----------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <imgui.h>
#include <nlohmann/json.hpp>

namespace ite {

// ---------------------------------------------------------------------------
// Field metadata
// ---------------------------------------------------------------------------

enum class StyleFieldType {
    Float,
    Vec2,
    Bool,
    Dir,
    HoverFlags,
};

enum class StyleCategory {
    Main,
    Window,
    Child,
    Popup,
    Frame,
    Item,
    Widget,
    Tab,
    Table,
    Separator,
    Tooltip,
    Display,
    Rendering,
};

struct StyleFieldDesc {
    const char*    name;       // member name, also the JSON key
    std::size_t    offset;     // offsetof(ImGuiStyle, <member>)
    StyleFieldType type;
    StyleCategory  category;
    float          slider_min; // used by the editor UI for floats / vec2 components
    float          slider_max;
    const char*    fmt;        // sprintf style format hint for sliders
};

const std::vector<StyleFieldDesc>& StyleFieldDescs();
const char* CategoryName(StyleCategory category);
const char* DirToString(ImGuiDir dir);

// ---------------------------------------------------------------------------
// JSON conversion
// ---------------------------------------------------------------------------

nlohmann::ordered_json StyleToJson(const ImGuiStyle& style);
bool JsonToStyle(const nlohmann::ordered_json& j, ImGuiStyle& out_style,
                 std::string* error = nullptr);

bool SaveStyleToFile(const std::string& path, const ImGuiStyle& style,
                     std::string* error = nullptr);
bool LoadStyleFromFile(const std::string& path, ImGuiStyle& out_style,
                       std::string* error = nullptr);

// ---------------------------------------------------------------------------
// C++ source export
// ---------------------------------------------------------------------------

struct ExportOptions {
    // Identifier used as the generated function name. Must be a valid C++ id.
    std::string function_name = "ApplyTheme";
    // Optional enclosing namespace. Leave empty for no namespace.
    std::string namespace_name = "theme";
    // Filename (or include path) used in the generated #include directive.
    // Only the basename is used.
    std::string header_include = "theme.h";
    // Human-readable name printed in the file header comment block.
    std::string theme_display_name = "MyTheme";
    // When true, prepend an `#include "<header_include>"` to the .cpp file.
    bool include_header_in_source = true;
    // When false, generated header uses traditional include guards.
    bool use_pragma_once = true;
};

std::string ExportStyleAsCppSource(const ImGuiStyle& style,
                                   const ExportOptions& opts);
std::string ExportStyleAsCppHeader(const ExportOptions& opts);

bool WriteStyleAsCpp(const std::string& source_path,
                     const std::string& header_path,
                     const ImGuiStyle& style,
                     const ExportOptions& opts,
                     std::string* error = nullptr);

} // namespace ite
