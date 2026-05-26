// -----------------------------------------------------------------------------
// theme_editor.cpp
// -----------------------------------------------------------------------------
#include "theme_editor.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <string>

namespace ite {

namespace {

// Case-insensitive substring match used by the color filter.
bool ContainsCI(const std::string& haystack, const std::string& needle)
{
    if (needle.empty())
        return true;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                   std::tolower(static_cast<unsigned char>(b));
        });
    return it != haystack.end();
}

template <typename T>
T* MemberPtr(ImGuiStyle& s, std::size_t offset)
{
    return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(&s) + offset);
}

} // namespace

// ---------------------------------------------------------------------------

ThemeEditor::ThemeEditor()
{
    // Start from ImGui's Dark preset so the editor has a sensible baseline.
    ImGuiStyle tmp;
    ImGui::StyleColorsDark(&tmp);
    style_ = tmp;
}

bool ThemeEditor::Draw()
{
    if (!show_window_)
        return false;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(720.0f, 720.0f), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("ImGui Theme Editor", &show_window_,
                      ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return show_window_;
    }

    DrawMenuBar();

    if (live_apply_)
        ApplyToImGui();

    if (ImGui::BeginTabBar("##theme_tabs",
                           ImGuiTabBarFlags_FittingPolicyScroll)) {
        if (ImGui::BeginTabItem("Sizes")) {
            DrawSizesTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Colors")) {
            DrawColorsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Preview")) {
            DrawPreviewTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Export")) {
            DrawExportTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    DrawStatusBar();

    ImGui::End();

    if (show_demo_)
        ImGui::ShowDemoWindow(&show_demo_);

    return show_window_;
}

void ThemeEditor::ApplyToImGui() const
{
    ImGui::GetStyle() = style_;
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------

void ThemeEditor::DrawMenuBar()
{
    if (!ImGui::BeginMenuBar())
        return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New (Dark preset)"))
            ApplyDarkPreset();
        if (ImGui::MenuItem("New (Light preset)"))
            ApplyLightPreset();
        if (ImGui::MenuItem("New (Classic preset)"))
            ApplyClassicPreset();
        ImGui::Separator();
        if (ImGui::MenuItem("Load JSON..."))
            DoLoadJson();
        if (ImGui::MenuItem("Save JSON..."))
            DoSaveJson();
        ImGui::Separator();
        if (ImGui::MenuItem("Export C++..."))
            DoExportCpp();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Live apply", nullptr, &live_apply_);
        ImGui::MenuItem("ImGui demo window", nullptr, &show_demo_);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        ImGui::TextDisabled("ImGui %s", IMGUI_VERSION);
        ImGui::Separator();
        ImGui::TextWrapped(
            "Edit any field, then use File > Save JSON to persist the "
            "current theme. Use File > Export C++ to emit a .cpp/.h pair "
            "that reproduces the same style at runtime.");
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

// ---------------------------------------------------------------------------
// Sizes tab (covers every non-color member of ImGuiStyle)
// ---------------------------------------------------------------------------

void ThemeEditor::DrawSizesTab()
{
    ImGui::TextWrapped(
        "Adjust the dimensional and behavioural members of ImGuiStyle. "
        "Changes apply live to every ImGui window when 'Live apply' is on.");
    ImGui::Spacing();

    StyleCategory current = static_cast<StyleCategory>(-1);
    bool open = false;
    for (const auto& d : StyleFieldDescs()) {
        if (d.category != current) {
            if (open) {
                ImGui::Unindent();
                ImGui::Spacing();
            }
            current = d.category;
            open = ImGui::CollapsingHeader(CategoryName(d.category),
                                           ImGuiTreeNodeFlags_DefaultOpen);
            if (open)
                ImGui::Indent();
        }
        if (open)
            DrawFieldEditor(d);
    }
    if (open)
        ImGui::Unindent();
}

void ThemeEditor::DrawFieldEditor(const StyleFieldDesc& d)
{
    ImGui::PushID(d.name);
    switch (d.type) {
        case StyleFieldType::Float: {
            float* p = MemberPtr<float>(style_, d.offset);
            ImGui::SliderFloat(d.name, p, d.slider_min, d.slider_max, d.fmt);
            break;
        }
        case StyleFieldType::Vec2: {
            ImVec2* p = MemberPtr<ImVec2>(style_, d.offset);
            ImGui::SliderFloat2(d.name, &p->x, d.slider_min, d.slider_max,
                                d.fmt);
            break;
        }
        case StyleFieldType::Bool: {
            bool* p = MemberPtr<bool>(style_, d.offset);
            ImGui::Checkbox(d.name, p);
            break;
        }
        case StyleFieldType::Dir: {
            ImGuiDir* p = MemberPtr<ImGuiDir>(style_, d.offset);
            static const char* kAll[] = {"None", "Left", "Right", "Up", "Down"};
            static const ImGuiDir kAllVal[] = {
                ImGuiDir_None, ImGuiDir_Left, ImGuiDir_Right,
                ImGuiDir_Up,   ImGuiDir_Down};
            int current_idx = 0;
            for (int i = 0; i < IM_ARRAYSIZE(kAllVal); ++i) {
                if (*p == kAllVal[i]) {
                    current_idx = i;
                    break;
                }
            }
            // WindowMenuButtonPosition only really supports None/Left/Right;
            // ColorButtonPosition only supports Left/Right. Limit the combo
            // accordingly to avoid confusing the user.
            int item_count = IM_ARRAYSIZE(kAllVal);
            if (std::string(d.name) == "WindowMenuButtonPosition")
                item_count = 3; // None/Left/Right
            else if (std::string(d.name) == "ColorButtonPosition")
                item_count = 3;
            if (ImGui::BeginCombo(d.name, kAll[current_idx])) {
                for (int i = 0; i < item_count; ++i) {
                    bool selected = (current_idx == i);
                    if (ImGui::Selectable(kAll[i], selected))
                        *p = kAllVal[i];
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            break;
        }
        case StyleFieldType::HoverFlags: {
            ImGuiHoveredFlags* p =
                MemberPtr<ImGuiHoveredFlags>(style_, d.offset);
            DrawHoverFlagsEditor(d.name, p);
            break;
        }
    }
    ImGui::PopID();
}

void ThemeEditor::DrawHoverFlagsEditor(const char* label,
                                       ImGuiHoveredFlags* flags)
{
    struct Item {
        const char*       name;
        ImGuiHoveredFlags bit;
    };
    static const Item kItems[] = {
        {"DelayNone",   ImGuiHoveredFlags_DelayNone},
        {"DelayShort",  ImGuiHoveredFlags_DelayShort},
        {"DelayNormal", ImGuiHoveredFlags_DelayNormal},
        {"Stationary",  ImGuiHoveredFlags_Stationary},
        {"NoSharedDelay", ImGuiHoveredFlags_NoSharedDelay},
    };

    // Render summary text and let the user toggle flag bits inside a tree.
    char preview[128];
    if (*flags == 0)
        std::snprintf(preview, sizeof(preview), "None");
    else
        std::snprintf(preview, sizeof(preview), "0x%X", static_cast<int>(*flags));

    if (ImGui::TreeNode(label, "%s : %s", label, preview)) {
        for (const auto& it : kItems) {
            bool on = (*flags & it.bit) != 0;
            if (ImGui::Checkbox(it.name, &on)) {
                if (on)
                    *flags |= it.bit;
                else
                    *flags &= ~it.bit;
            }
        }
        ImGui::TreePop();
    }
}

// ---------------------------------------------------------------------------
// Colors tab
// ---------------------------------------------------------------------------

void ThemeEditor::DrawColorsTab()
{
    ImGui::TextWrapped(
        "All %d ImGuiCol_ entries are listed below. Type in the filter to "
        "narrow the list.", static_cast<int>(ImGuiCol_COUNT));

    char buf[128] = {};
    std::snprintf(buf, sizeof(buf), "%s", color_filter_.c_str());
    if (ImGui::InputTextWithHint("##filter", "Filter (e.g. \"window\")", buf,
                                 sizeof(buf)))
        color_filter_ = buf;

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        color_filter_.clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Alpha preview", &color_alpha_preview_);
    ImGui::SameLine();
    ImGui::Checkbox("Hex", &color_show_hex_);

    ImGui::Separator();

    ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaBar |
                                ImGuiColorEditFlags_NoInputs |
                                ImGuiColorEditFlags_NoLabel;
    if (color_alpha_preview_)
        flags |= ImGuiColorEditFlags_AlphaPreviewHalf;
    if (color_show_hex_)
        flags |= ImGuiColorEditFlags_DisplayHex;

    if (ImGui::BeginTable("##colors", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_SizingStretchProp |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(0.0f, 400.0f))) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            const char* name = ImGui::GetStyleColorName(i);
            if (!ContainsCI(name, color_filter_))
                continue;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(i);
            ImGui::TextUnformatted(name);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::ColorEdit4("##c", reinterpret_cast<float*>(&style_.Colors[i]),
                              flags);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------
// Preview tab (showcases every widget so the user sees the impact)
// ---------------------------------------------------------------------------

void ThemeEditor::DrawPreviewTab()
{
    ImGui::TextWrapped(
        "Mini gallery of widgets so you can audit how the current style looks. "
        "Toggle the ImGui demo window from the View menu for the complete "
        "reference.");
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Basic widgets",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        static int   item_current = 0;
        static float slider_v     = 0.4f;
        static float drag_v       = 0.6f;
        static int   counter      = 0;
        static char  text_buf[128] = "Edit me";
        static bool  cb1 = true, cb2 = false;

        if (ImGui::Button("Button"))
            counter++;
        ImGui::SameLine();
        ImGui::SmallButton("Small");
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Checkbox("Checkbox A", &cb1);
        ImGui::SameLine();
        ImGui::Checkbox("Checkbox B", &cb2);

        ImGui::RadioButton("Radio 0", &item_current, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Radio 1", &item_current, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Radio 2", &item_current, 2);

        ImGui::SliderFloat("Slider", &slider_v, 0.0f, 1.0f);
        ImGui::DragFloat("Drag", &drag_v, 0.005f);
        ImGui::InputText("Input", text_buf, IM_ARRAYSIZE(text_buf));
        const char* items[] = {"AAAA", "BBBB", "CCCC", "DDDD"};
        ImGui::Combo("Combo", &item_current, items, IM_ARRAYSIZE(items));
    }

    if (ImGui::CollapsingHeader("Containers")) {
        if (ImGui::TreeNode("Tree node")) {
            ImGui::BulletText("Bullet A");
            ImGui::BulletText("Bullet B");
            if (ImGui::TreeNode("Nested"))
            {
                ImGui::Text("More content");
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
        if (ImGui::BeginChild("##child", ImVec2(0.0f, 120.0f),
                              ImGuiChildFlags_Borders)) {
            for (int i = 0; i < 20; ++i)
                ImGui::Text("Child row %d", i);
        }
        ImGui::EndChild();
    }

    if (ImGui::CollapsingHeader("Tabs and tables")) {
        if (ImGui::BeginTabBar("preview_tabs")) {
            if (ImGui::BeginTabItem("Tab 1")) {
                ImGui::TextWrapped("Hello from tab 1");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tab 2")) {
                ImGui::TextWrapped("Hello from tab 2");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tab 3")) {
                ImGui::TextWrapped("Hello from tab 3");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (ImGui::BeginTable("preview_table", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            for (int row = 0; row < 4; ++row) {
                ImGui::TableNextRow();
                for (int col = 0; col < 4; ++col) {
                    ImGui::TableSetColumnIndex(col);
                    ImGui::Text("r%d c%d", row, col);
                }
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Plots / progress / separator")) {
        static float progress = 0.6f;
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
        ImGui::SliderFloat("progress", &progress, 0.0f, 1.0f);

        static float arr[] = {0.6f, 0.1f, 1.0f, 0.5f, 0.92f, 0.1f, 0.2f};
        ImGui::PlotLines("Lines", arr, IM_ARRAYSIZE(arr));
        ImGui::PlotHistogram("Histogram", arr, IM_ARRAYSIZE(arr));

        ImGui::SeparatorText("SeparatorText");
        ImGui::TextLinkOpenURL("imgui repository",
                               "https://github.com/ocornut/imgui");
    }
}

// ---------------------------------------------------------------------------
// Export tab (lets the user configure C++ output)
// ---------------------------------------------------------------------------

namespace {

void TextInput(const char* label, std::string& s, const char* hint = nullptr)
{
    char buf[256] = {};
    std::snprintf(buf, sizeof(buf), "%s", s.c_str());
    if (hint && *hint)
        ImGui::InputTextWithHint(label, hint, buf, sizeof(buf));
    else
        ImGui::InputText(label, buf, sizeof(buf));
    s = buf;
}

} // namespace

void ThemeEditor::DrawExportTab()
{
    ImGui::TextWrapped(
        "Configure how the C++ output is generated. The .cpp file applies the "
        "style to ImGui::GetStyle(). The header declares the apply function.");
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("File paths",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        TextInput("JSON path",   json_path_,   "theme.json");
        TextInput("Source path", cpp_path_,    "theme.cpp");
        TextInput("Header path", header_path_, "theme.h (empty to skip)");
    }

    if (ImGui::CollapsingHeader("Code generation",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        TextInput("Function name",     export_opts_.function_name,    "ApplyTheme");
        TextInput("Namespace",         export_opts_.namespace_name,   "theme (empty for none)");
        TextInput("Theme display name", export_opts_.theme_display_name, "MyTheme");
        ImGui::Checkbox("#pragma once",
                        &export_opts_.use_pragma_once);
        ImGui::Checkbox("#include the header from the .cpp",
                        &export_opts_.include_header_in_source);
    }

    ImGui::Separator();
    if (ImGui::Button("Save JSON")) DoSaveJson();
    ImGui::SameLine();
    if (ImGui::Button("Load JSON")) DoLoadJson();
    ImGui::SameLine();
    if (ImGui::Button("Export C++"))  DoExportCpp();

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Preview generated C++ (.cpp)")) {
        ExportOptions effective = export_opts_;
        if (!header_path_.empty()) {
            auto pos = header_path_.find_last_of("/\\");
            effective.header_include = (pos == std::string::npos)
                                           ? header_path_
                                           : header_path_.substr(pos + 1);
        }
        std::string src = ExportStyleAsCppSource(style_, effective);
        ImGui::InputTextMultiline("##cpp_preview", src.data(), src.size() + 1,
                                  ImVec2(-1.0f, 240.0f),
                                  ImGuiInputTextFlags_ReadOnly);
    }
    if (ImGui::CollapsingHeader("Preview generated C++ (.h)")) {
        std::string hdr = ExportStyleAsCppHeader(export_opts_);
        ImGui::InputTextMultiline("##h_preview", hdr.data(), hdr.size() + 1,
                                  ImVec2(-1.0f, 160.0f),
                                  ImGuiInputTextFlags_ReadOnly);
    }
    if (ImGui::CollapsingHeader("Preview JSON")) {
        std::string j = StyleToJson(style_).dump(4);
        ImGui::InputTextMultiline("##j_preview", j.data(), j.size() + 1,
                                  ImVec2(-1.0f, 240.0f),
                                  ImGuiInputTextFlags_ReadOnly);
    }
}

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------

void ThemeEditor::DoSaveJson()
{
    std::string err;
    if (SaveStyleToFile(json_path_, style_, &err))
        SetStatus("Saved JSON to " + json_path_);
    else
        SetStatus("Save JSON failed: " + err, true);
}

void ThemeEditor::DoLoadJson()
{
    std::string err;
    ImGuiStyle staging = style_;
    if (LoadStyleFromFile(json_path_, staging, &err)) {
        style_ = staging;
        SetStatus("Loaded JSON from " + json_path_);
    } else {
        SetStatus("Load JSON failed: " + err, true);
    }
}

void ThemeEditor::DoExportCpp()
{
    std::string err;
    if (WriteStyleAsCpp(cpp_path_, header_path_, style_, export_opts_, &err)) {
        if (header_path_.empty())
            SetStatus("Exported C++ to " + cpp_path_);
        else
            SetStatus("Exported C++ to " + cpp_path_ + " + " + header_path_);
    } else {
        SetStatus("Export C++ failed: " + err, true);
    }
}

// ---------------------------------------------------------------------------
// Presets and status
// ---------------------------------------------------------------------------

void ThemeEditor::ApplyDarkPreset()
{
    ImGuiStyle tmp;
    ImGui::StyleColorsDark(&tmp);
    style_ = tmp;
    SetStatus("Loaded ImGui dark preset");
}

void ThemeEditor::ApplyLightPreset()
{
    ImGuiStyle tmp;
    ImGui::StyleColorsLight(&tmp);
    style_ = tmp;
    SetStatus("Loaded ImGui light preset");
}

void ThemeEditor::ApplyClassicPreset()
{
    ImGuiStyle tmp;
    ImGui::StyleColorsClassic(&tmp);
    style_ = tmp;
    SetStatus("Loaded ImGui classic preset");
}

void ThemeEditor::SetStatus(const std::string& msg, bool error)
{
    status_msg_      = msg;
    status_is_error_ = error;
    status_time_     = std::chrono::steady_clock::now();
}

void ThemeEditor::DrawStatusBar()
{
    if (status_msg_.empty())
        return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - status_time_)
            .count();
    if (elapsed > 6) {
        status_msg_.clear();
        return;
    }

    ImGui::Separator();
    if (status_is_error_)
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                           status_msg_.c_str());
    else
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s",
                           status_msg_.c_str());
}

} // namespace ite
