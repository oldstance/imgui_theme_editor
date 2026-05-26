// -----------------------------------------------------------------------------
// style_serializer.cpp
//
// Implementation notes:
// - We treat the JSON file as the source of truth at edit time, but only emit
//   keys that we know about. Unknown keys in a loaded file are ignored
//   gracefully so the format remains forward compatible.
// - ImGuiDir is serialized using its symbolic name ("None"/"Left"/...).
// - ImGuiHoveredFlags is serialized as an integer mirroring ImGui's flag bits.
// - Colors[] uses the canonical names returned by ImGui::GetStyleColorName so
//   removing/adding members across Dear ImGui versions doesn't shift indices.
// -----------------------------------------------------------------------------
#include "style_serializer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace ite {

namespace {

constexpr float kF32Max = 3.4028234663852886e+38f;

template <typename T>
T* MemberPtr(ImGuiStyle& style, std::size_t offset)
{
    return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(&style) + offset);
}

template <typename T>
const T* MemberPtr(const ImGuiStyle& style, std::size_t offset)
{
    return reinterpret_cast<const T*>(
        reinterpret_cast<const std::byte*>(&style) + offset);
}

// Strips trailing zeros from a default %g-like representation while keeping
// a single decimal point so the value is unambiguously a float literal.
std::string FloatLiteral(float v)
{
    if (std::isinf(v))
        return v < 0.0f ? "-FLT_MAX" : "FLT_MAX";
    if (std::isnan(v))
        return "0.0f";
    if (v >= kF32Max * 0.5f)
        return "FLT_MAX";
    if (v <= -kF32Max * 0.5f)
        return "-FLT_MAX";

    std::ostringstream os;
    os << std::setprecision(7) << v;
    std::string s = os.str();
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos &&
        s.find('E') == std::string::npos)
        s += ".0";
    s += 'f';
    return s;
}

std::string DirEnumLiteral(ImGuiDir dir)
{
    switch (dir) {
        case ImGuiDir_None:  return "ImGuiDir_None";
        case ImGuiDir_Left:  return "ImGuiDir_Left";
        case ImGuiDir_Right: return "ImGuiDir_Right";
        case ImGuiDir_Up:    return "ImGuiDir_Up";
        case ImGuiDir_Down:  return "ImGuiDir_Down";
        default:             return "ImGuiDir_None";
    }
}

ImGuiDir ParseDir(const std::string& s)
{
    if (s == "None")  return ImGuiDir_None;
    if (s == "Left")  return ImGuiDir_Left;
    if (s == "Right") return ImGuiDir_Right;
    if (s == "Up")    return ImGuiDir_Up;
    if (s == "Down")  return ImGuiDir_Down;
    return ImGuiDir_None;
}

} // namespace

const char* DirToString(ImGuiDir dir)
{
    switch (dir) {
        case ImGuiDir_None:  return "None";
        case ImGuiDir_Left:  return "Left";
        case ImGuiDir_Right: return "Right";
        case ImGuiDir_Up:    return "Up";
        case ImGuiDir_Down:  return "Down";
        default:             return "None";
    }
}

const char* CategoryName(StyleCategory category)
{
    switch (category) {
        case StyleCategory::Main:      return "Main";
        case StyleCategory::Window:    return "Window";
        case StyleCategory::Child:     return "Child";
        case StyleCategory::Popup:     return "Popup";
        case StyleCategory::Frame:     return "Frame";
        case StyleCategory::Item:      return "Item";
        case StyleCategory::Widget:    return "Widget";
        case StyleCategory::Tab:       return "Tab";
        case StyleCategory::Table:     return "Table";
        case StyleCategory::Separator: return "Separator";
        case StyleCategory::Tooltip:   return "Tooltip / Hover";
        case StyleCategory::Display:   return "Display";
        case StyleCategory::Rendering: return "Rendering";
    }
    return "Misc";
}

// ---------------------------------------------------------------------------
// Field descriptor table
// ---------------------------------------------------------------------------
//
// The order chosen here drives both the JSON layout and the section order in
// the editor UI. Keep them sorted by category for readability.
//
#define ITE_FIELD(field, type, cat, lo, hi, fmt)                          \
    { #field, offsetof(ImGuiStyle, field), StyleFieldType::type, cat, lo, \
      hi, fmt }

const std::vector<StyleFieldDesc>& StyleFieldDescs()
{
    static const std::vector<StyleFieldDesc> kDescs = {
        // ------------------------- Main -------------------------
        ITE_FIELD(Alpha,          Float, StyleCategory::Main, 0.20f,  1.00f, "%.2f"),
        ITE_FIELD(DisabledAlpha,  Float, StyleCategory::Main, 0.00f,  1.00f, "%.2f"),

        // ------------------------- Window -----------------------
        ITE_FIELD(WindowPadding,            Vec2,  StyleCategory::Window, 0.0f,  20.0f, "%.0f"),
        ITE_FIELD(WindowRounding,           Float, StyleCategory::Window, 0.0f,  12.0f, "%.1f"),
        ITE_FIELD(WindowBorderSize,         Float, StyleCategory::Window, 0.0f,   1.0f, "%.0f"),
        ITE_FIELD(WindowMinSize,            Vec2,  StyleCategory::Window, 0.0f, 1000.0f, "%.0f"),
        ITE_FIELD(WindowTitleAlign,         Vec2,  StyleCategory::Window, 0.0f,   1.0f, "%.2f"),
        ITE_FIELD(WindowMenuButtonPosition, Dir,   StyleCategory::Window, 0.0f,   0.0f, ""),

        // ------------------------- Child / Popup ----------------
        ITE_FIELD(ChildRounding,   Float, StyleCategory::Child, 0.0f, 12.0f, "%.1f"),
        ITE_FIELD(ChildBorderSize, Float, StyleCategory::Child, 0.0f,  1.0f, "%.0f"),

        ITE_FIELD(PopupRounding,   Float, StyleCategory::Popup, 0.0f, 16.0f, "%.1f"),
        ITE_FIELD(PopupBorderSize, Float, StyleCategory::Popup, 0.0f,  1.0f, "%.0f"),

        // ------------------------- Frame ------------------------
        ITE_FIELD(FramePadding,     Vec2,  StyleCategory::Frame, 0.0f, 20.0f, "%.0f"),
        ITE_FIELD(FrameRounding,    Float, StyleCategory::Frame, 0.0f, 12.0f, "%.1f"),
        ITE_FIELD(FrameBorderSize,  Float, StyleCategory::Frame, 0.0f,  1.0f, "%.0f"),

        // ------------------------- Item -------------------------
        ITE_FIELD(ItemSpacing,        Vec2,  StyleCategory::Item, 0.0f, 20.0f, "%.0f"),
        ITE_FIELD(ItemInnerSpacing,   Vec2,  StyleCategory::Item, 0.0f, 20.0f, "%.0f"),
        ITE_FIELD(CellPadding,        Vec2,  StyleCategory::Item, 0.0f, 20.0f, "%.0f"),
        ITE_FIELD(TouchExtraPadding,  Vec2,  StyleCategory::Item, 0.0f, 10.0f, "%.0f"),
        ITE_FIELD(IndentSpacing,      Float, StyleCategory::Item, 0.0f, 30.0f, "%.0f"),
        ITE_FIELD(ColumnsMinSpacing,  Float, StyleCategory::Item, 0.0f, 30.0f, "%.0f"),

        // ------------------------- Widget -----------------------
        ITE_FIELD(ScrollbarSize,        Float, StyleCategory::Widget, 1.0f, 20.0f, "%.0f"),
        ITE_FIELD(ScrollbarRounding,    Float, StyleCategory::Widget, 0.0f, 12.0f, "%.1f"),
        ITE_FIELD(GrabMinSize,          Float, StyleCategory::Widget, 1.0f, 20.0f, "%.0f"),
        ITE_FIELD(GrabRounding,         Float, StyleCategory::Widget, 0.0f, 12.0f, "%.1f"),
        ITE_FIELD(LogSliderDeadzone,    Float, StyleCategory::Widget, 0.0f, 12.0f, "%.0f"),
        ITE_FIELD(ColorButtonPosition,  Dir,   StyleCategory::Widget, 0.0f,  0.0f, ""),
        ITE_FIELD(ButtonTextAlign,      Vec2,  StyleCategory::Widget, 0.0f,  1.0f, "%.2f"),
        ITE_FIELD(SelectableTextAlign,  Vec2,  StyleCategory::Widget, 0.0f,  1.0f, "%.2f"),

        // ------------------------- Tab --------------------------
        ITE_FIELD(TabRounding,               Float, StyleCategory::Tab, 0.0f, 12.0f, "%.1f"),
        ITE_FIELD(TabBorderSize,             Float, StyleCategory::Tab, 0.0f,  1.0f, "%.0f"),
        ITE_FIELD(TabMinWidthForCloseButton, Float, StyleCategory::Tab, 0.0f, 100.0f, "%.0f"),
        ITE_FIELD(TabBarBorderSize,          Float, StyleCategory::Tab, 0.0f,  2.0f, "%.0f"),
        ITE_FIELD(TabBarOverlineSize,        Float, StyleCategory::Tab, 0.0f,  3.0f, "%.0f"),

        // ------------------------- Table ------------------------
        ITE_FIELD(TableAngledHeadersAngle,     Float, StyleCategory::Table, -50.0f, 50.0f, "%.1f"),
        ITE_FIELD(TableAngledHeadersTextAlign, Vec2,  StyleCategory::Table,   0.0f,  1.0f, "%.2f"),

        // ------------------------- Separator --------------------
        ITE_FIELD(SeparatorTextBorderSize, Float, StyleCategory::Separator, 0.0f, 10.0f, "%.0f"),
        ITE_FIELD(SeparatorTextAlign,      Vec2,  StyleCategory::Separator, 0.0f,  1.0f, "%.2f"),
        ITE_FIELD(SeparatorTextPadding,    Vec2,  StyleCategory::Separator, 0.0f, 40.0f, "%.0f"),

        // ------------------------- Tooltip / Hover --------------
        ITE_FIELD(HoverStationaryDelay, Float,      StyleCategory::Tooltip, 0.0f, 1.5f, "%.2f"),
        ITE_FIELD(HoverDelayShort,      Float,      StyleCategory::Tooltip, 0.0f, 1.5f, "%.2f"),
        ITE_FIELD(HoverDelayNormal,     Float,      StyleCategory::Tooltip, 0.0f, 1.5f, "%.2f"),
        ITE_FIELD(HoverFlagsForTooltipMouse, HoverFlags, StyleCategory::Tooltip, 0.0f, 0.0f, ""),
        ITE_FIELD(HoverFlagsForTooltipNav,   HoverFlags, StyleCategory::Tooltip, 0.0f, 0.0f, ""),

        // ------------------------- Display ----------------------
        ITE_FIELD(DisplayWindowPadding,   Vec2,  StyleCategory::Display, 0.0f, 30.0f, "%.0f"),
        ITE_FIELD(DisplaySafeAreaPadding, Vec2,  StyleCategory::Display, 0.0f, 30.0f, "%.0f"),
        ITE_FIELD(MouseCursorScale,       Float, StyleCategory::Display, 0.5f,  4.0f, "%.2f"),

        // ------------------------- Rendering --------------------
        ITE_FIELD(AntiAliasedLines,         Bool,  StyleCategory::Rendering, 0.0f,  0.0f, ""),
        ITE_FIELD(AntiAliasedLinesUseTex,   Bool,  StyleCategory::Rendering, 0.0f,  0.0f, ""),
        ITE_FIELD(AntiAliasedFill,          Bool,  StyleCategory::Rendering, 0.0f,  0.0f, ""),
        ITE_FIELD(CurveTessellationTol,     Float, StyleCategory::Rendering, 0.10f, 10.0f, "%.2f"),
        ITE_FIELD(CircleTessellationMaxError, Float, StyleCategory::Rendering, 0.10f, 5.0f, "%.2f"),
    };
    return kDescs;
}

#undef ITE_FIELD

// ---------------------------------------------------------------------------
// JSON serialisation
// ---------------------------------------------------------------------------

nlohmann::ordered_json StyleToJson(const ImGuiStyle& style)
{
    using nlohmann::ordered_json;
    ordered_json root;
    root["version"] = 1;
    root["imgui_version"] = IMGUI_VERSION;

    ordered_json& fields = root["fields"];
    for (const auto& d : StyleFieldDescs()) {
        switch (d.type) {
            case StyleFieldType::Float:
                fields[d.name] = *MemberPtr<float>(style, d.offset);
                break;
            case StyleFieldType::Vec2: {
                const ImVec2& v = *MemberPtr<ImVec2>(style, d.offset);
                fields[d.name] = {v.x, v.y};
                break;
            }
            case StyleFieldType::Bool:
                fields[d.name] = *MemberPtr<bool>(style, d.offset);
                break;
            case StyleFieldType::Dir: {
                ImGuiDir dir = *MemberPtr<ImGuiDir>(style, d.offset);
                fields[d.name] = DirToString(dir);
                break;
            }
            case StyleFieldType::HoverFlags:
                fields[d.name] =
                    static_cast<int>(*MemberPtr<ImGuiHoveredFlags>(style, d.offset));
                break;
        }
    }

    ordered_json& colors = root["colors"];
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        const char* name = ImGui::GetStyleColorName(i);
        const ImVec4& c = style.Colors[i];
        colors[name] = {c.x, c.y, c.z, c.w};
    }

    return root;
}

bool JsonToStyle(const nlohmann::ordered_json& j, ImGuiStyle& out_style,
                 std::string* error)
{
    try {
        // Apply known fields. Missing keys leave whatever was in out_style
        // before, which is convenient for partial overrides.
        if (j.contains("fields") && j["fields"].is_object()) {
            const auto& fields = j["fields"];
            for (const auto& d : StyleFieldDescs()) {
                if (!fields.contains(d.name))
                    continue;
                const auto& v = fields.at(d.name);
                switch (d.type) {
                    case StyleFieldType::Float:
                        if (v.is_number())
                            *MemberPtr<float>(out_style, d.offset) =
                                v.get<float>();
                        break;
                    case StyleFieldType::Vec2:
                        if (v.is_array() && v.size() == 2 &&
                            v[0].is_number() && v[1].is_number())
                            *MemberPtr<ImVec2>(out_style, d.offset) =
                                ImVec2(v[0].get<float>(), v[1].get<float>());
                        break;
                    case StyleFieldType::Bool:
                        if (v.is_boolean())
                            *MemberPtr<bool>(out_style, d.offset) =
                                v.get<bool>();
                        break;
                    case StyleFieldType::Dir:
                        if (v.is_string())
                            *MemberPtr<ImGuiDir>(out_style, d.offset) =
                                ParseDir(v.get<std::string>());
                        else if (v.is_number_integer())
                            *MemberPtr<ImGuiDir>(out_style, d.offset) =
                                static_cast<ImGuiDir>(v.get<int>());
                        break;
                    case StyleFieldType::HoverFlags:
                        if (v.is_number_integer())
                            *MemberPtr<ImGuiHoveredFlags>(out_style, d.offset) =
                                static_cast<ImGuiHoveredFlags>(v.get<int>());
                        break;
                }
            }
        }

        if (j.contains("colors") && j["colors"].is_object()) {
            const auto& colors = j["colors"];
            for (int i = 0; i < ImGuiCol_COUNT; ++i) {
                const char* name = ImGui::GetStyleColorName(i);
                if (!colors.contains(name))
                    continue;
                const auto& c = colors.at(name);
                if (c.is_array() && c.size() == 4 && c[0].is_number()) {
                    out_style.Colors[i] = ImVec4(
                        c[0].get<float>(), c[1].get<float>(),
                        c[2].get<float>(), c[3].get<float>());
                }
            }
        }
    } catch (const std::exception& e) {
        if (error)
            *error = e.what();
        return false;
    }
    return true;
}

bool SaveStyleToFile(const std::string& path, const ImGuiStyle& style,
                     std::string* error)
{
    try {
        std::ofstream out(path);
        if (!out.is_open()) {
            if (error)
                *error = "Cannot open output file: " + path;
            return false;
        }
        out << StyleToJson(style).dump(4) << std::endl;
        return true;
    } catch (const std::exception& e) {
        if (error)
            *error = e.what();
        return false;
    }
}

bool LoadStyleFromFile(const std::string& path, ImGuiStyle& out_style,
                       std::string* error)
{
    try {
        std::ifstream in(path);
        if (!in.is_open()) {
            if (error)
                *error = "Cannot open input file: " + path;
            return false;
        }
        nlohmann::ordered_json j;
        in >> j;
        return JsonToStyle(j, out_style, error);
    } catch (const std::exception& e) {
        if (error)
            *error = e.what();
        return false;
    }
}

// ---------------------------------------------------------------------------
// C++ source export
// ---------------------------------------------------------------------------

namespace {

std::string BaseName(const std::string& path)
{
    auto pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string MakeIncludeGuard(const std::string& header)
{
    std::string name = BaseName(header);
    std::string out;
    out.reserve(name.size() + 8);
    for (char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9'))
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        else
            out += '_';
    }
    return out + "_INCLUDED_";
}

void EmitHeaderComment(std::ostringstream& os, const ExportOptions& opts,
                       const char* file_kind)
{
    os << "// "
       << "----------------------------------------------------------------------\n";
    os << "// " << file_kind << " generated by imgui_theme_editor.\n";
    os << "// Theme : " << opts.theme_display_name << "\n";
    os << "// ImGui : " << IMGUI_VERSION << "\n";
    os << "// "
       << "----------------------------------------------------------------------\n";
}

} // namespace

std::string ExportStyleAsCppHeader(const ExportOptions& opts)
{
    std::ostringstream os;
    EmitHeaderComment(os, opts, "Header");

    std::string guard;
    if (opts.use_pragma_once) {
        os << "#pragma once\n\n";
    } else {
        guard = MakeIncludeGuard(opts.header_include);
        os << "#ifndef " << guard << "\n#define " << guard << "\n\n";
    }

    if (!opts.namespace_name.empty())
        os << "namespace " << opts.namespace_name << " {\n\n";

    os << "// Applies the \"" << opts.theme_display_name
       << "\" theme to ImGui::GetStyle().\n";
    os << "void " << opts.function_name << "();\n";

    if (!opts.namespace_name.empty())
        os << "\n} // namespace " << opts.namespace_name << "\n";

    if (!opts.use_pragma_once)
        os << "\n#endif // " << guard << "\n";

    return os.str();
}

std::string ExportStyleAsCppSource(const ImGuiStyle& style,
                                   const ExportOptions& opts)
{
    std::ostringstream os;
    EmitHeaderComment(os, opts, "Source");

    if (opts.include_header_in_source)
        os << "#include \"" << BaseName(opts.header_include) << "\"\n\n";

    os << "#include <cfloat>   // FLT_MAX\n";
    os << "#include <imgui.h>\n\n";

    if (!opts.namespace_name.empty())
        os << "namespace " << opts.namespace_name << " {\n\n";

    os << "void " << opts.function_name << "()\n";
    os << "{\n";
    os << "    ImGuiStyle& style = ImGui::GetStyle();\n\n";

    // Emit scalar / vec2 / bool / dir / flags fields grouped by category.
    StyleCategory current = static_cast<StyleCategory>(-1);
    for (const auto& d : StyleFieldDescs()) {
        if (d.category != current) {
            current = d.category;
            os << "\n    // ---- " << CategoryName(d.category) << " ----\n";
        }
        os << "    style." << d.name << " = ";
        switch (d.type) {
            case StyleFieldType::Float:
                os << FloatLiteral(*MemberPtr<float>(style, d.offset));
                break;
            case StyleFieldType::Vec2: {
                const ImVec2& v = *MemberPtr<ImVec2>(style, d.offset);
                os << "ImVec2(" << FloatLiteral(v.x) << ", "
                   << FloatLiteral(v.y) << ")";
                break;
            }
            case StyleFieldType::Bool:
                os << (*MemberPtr<bool>(style, d.offset) ? "true" : "false");
                break;
            case StyleFieldType::Dir:
                os << DirEnumLiteral(*MemberPtr<ImGuiDir>(style, d.offset));
                break;
            case StyleFieldType::HoverFlags: {
                int flags = static_cast<int>(
                    *MemberPtr<ImGuiHoveredFlags>(style, d.offset));
                if (flags == 0)
                    os << "ImGuiHoveredFlags_None";
                else
                    os << "static_cast<ImGuiHoveredFlags>(" << flags << ")";
                break;
            }
        }
        os << ";\n";
    }

    // Colors block
    os << "\n    // ---- Colors ----\n";
    os << "    ImVec4* colors = style.Colors;\n";
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        const char* name = ImGui::GetStyleColorName(i);
        const ImVec4& c = style.Colors[i];
        os << "    colors[ImGuiCol_" << name << "] = ImVec4("
           << FloatLiteral(c.x) << ", " << FloatLiteral(c.y) << ", "
           << FloatLiteral(c.z) << ", " << FloatLiteral(c.w) << ");\n";
    }

    os << "}\n";

    if (!opts.namespace_name.empty())
        os << "\n} // namespace " << opts.namespace_name << "\n";

    return os.str();
}

bool WriteStyleAsCpp(const std::string& source_path,
                     const std::string& header_path,
                     const ImGuiStyle& style,
                     const ExportOptions& opts,
                     std::string* error)
{
    try {
        // Adjust the header include name based on the actual header path so
        // the generated #include matches what was written to disk.
        ExportOptions effective = opts;
        if (!header_path.empty())
            effective.header_include = BaseName(header_path);

        if (!header_path.empty()) {
            std::ofstream hout(header_path);
            if (!hout.is_open()) {
                if (error)
                    *error = "Cannot open header output: " + header_path;
                return false;
            }
            hout << ExportStyleAsCppHeader(effective);
        }

        std::ofstream sout(source_path);
        if (!sout.is_open()) {
            if (error)
                *error = "Cannot open source output: " + source_path;
            return false;
        }
        sout << ExportStyleAsCppSource(style, effective);
        return true;
    } catch (const std::exception& e) {
        if (error)
            *error = e.what();
        return false;
    }
}

} // namespace ite
