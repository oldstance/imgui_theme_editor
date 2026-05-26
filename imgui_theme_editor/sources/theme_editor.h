// -----------------------------------------------------------------------------
// theme_editor.h
//
// Self-contained ImGui window that lets the user edit every member of
// ImGuiStyle (sizes, alignments, colors, behaviour flags) and persist the
// result either as JSON (for further editing) or as ready-to-include
// C++ source/header pair.
// -----------------------------------------------------------------------------
#pragma once

#include <chrono>
#include <string>

#include <imgui.h>

#include "style_serializer.h"

namespace ite {

class ThemeEditor {
public:
    ThemeEditor();

    // Renders the editor window. Returns false if the window has been closed.
    bool Draw();

    // Pushes the locally edited style into ImGui::GetStyle(). This is what
    // makes the changes visible everywhere immediately.
    void ApplyToImGui() const;

    // Direct access to the currently edited style.
    ImGuiStyle&       style()       { return style_; }
    const ImGuiStyle& style() const { return style_; }

private:
    // Editing
    void DrawMenuBar();
    void DrawSizesTab();
    void DrawColorsTab();
    void DrawPreviewTab();
    void DrawExportTab();

    void DrawFieldEditor(const StyleFieldDesc& d);
    void DrawHoverFlagsEditor(const char* label, ImGuiHoveredFlags* flags);

    // File I/O helpers
    void DoSaveJson();
    void DoLoadJson();
    void DoExportCpp();

    // Presets
    void ApplyDarkPreset();
    void ApplyLightPreset();
    void ApplyClassicPreset();

    // Status / feedback
    void SetStatus(const std::string& msg, bool error = false);
    void DrawStatusBar();

    // Pending UI state
    ImGuiStyle      style_{};                   // edited locally
    std::string     json_path_   = "theme.json";
    std::string     cpp_path_    = "theme.cpp";
    std::string     header_path_ = "theme.h";
    ExportOptions   export_opts_{};

    bool            show_window_ = true;
    bool            live_apply_  = true;
    bool            show_demo_   = false;

    std::string     color_filter_;
    bool            color_alpha_preview_ = true;
    bool            color_show_hex_      = false;

    std::string     status_msg_;
    bool            status_is_error_ = false;
    std::chrono::steady_clock::time_point status_time_{};
};

} // namespace ite
