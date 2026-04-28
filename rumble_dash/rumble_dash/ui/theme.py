"""
theme.py — DearPyGui light theme and colour palette.
"""

import dearpygui.dearpygui as dpg

# ── Palette ───────────────────────────────────────────────────────────────────

C_BG        = (245, 245, 245, 255)   # light gray / almost white
C_BG2       = (230, 230, 230, 255)   # panel background
C_BG3       = (215, 215, 215, 255)   # widget background
C_BORDER    = (180, 180, 180, 255)
C_TEXT      = (30,  30,  30,  255)   # dark text
C_DIM       = (100, 100, 100, 255)
C_ACCENT    = (40,  100, 180, 255)   # standard blue
C_ACCENT2   = (20,  80,  160, 255)
C_GREEN     = (40,  160, 40,  255)
C_AMBER     = (200, 130, 20,  255)
C_RED       = (180, 40,  40,  255)
C_BTN_ON    = (40,  100, 180, 255)
C_BTN_OFF   = (215, 215, 215, 255)
C_STICK_BG  = (230, 230, 230, 255)
C_STICK_RIM = (180, 180, 180, 255)
C_TRACE     = (40,  100, 180, 100)
C_DOT       = (60,  120, 200, 255)
C_DEADZONE  = (200, 200, 200, 100)
C_TRIG_BG   = (230, 230, 230, 255)
C_TRIG_FILL = (40,  100, 180, 255)
C_CIRC_REF  = (180, 180, 180, 200)
C_CIRC_PT   = (40,  100, 180, 180)


def apply() -> None:
    """Apply the global light theme to DearPyGui."""
    with dpg.theme() as global_theme:
        with dpg.theme_component(dpg.mvAll):
            dpg.add_theme_color(dpg.mvThemeCol_WindowBg,       C_BG)
            dpg.add_theme_color(dpg.mvThemeCol_ChildBg,        C_BG2)
            dpg.add_theme_color(dpg.mvThemeCol_PopupBg,        C_BG2)
            dpg.add_theme_color(dpg.mvThemeCol_FrameBg,        C_BG3)
            dpg.add_theme_color(dpg.mvThemeCol_FrameBgHovered, C_BG3)
            dpg.add_theme_color(dpg.mvThemeCol_FrameBgActive,  C_BG3)
            dpg.add_theme_color(dpg.mvThemeCol_TitleBg,        C_BG)
            dpg.add_theme_color(dpg.mvThemeCol_TitleBgActive,  C_BG2)
            dpg.add_theme_color(dpg.mvThemeCol_MenuBarBg,      C_BG)
            dpg.add_theme_color(dpg.mvThemeCol_ScrollbarBg,    C_BG)
            dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrab,  C_BG3)
            dpg.add_theme_color(dpg.mvThemeCol_CheckMark,      C_ACCENT)
            dpg.add_theme_color(dpg.mvThemeCol_SliderGrab,     C_ACCENT)
            dpg.add_theme_color(dpg.mvThemeCol_SliderGrabActive, C_ACCENT2)
            dpg.add_theme_color(dpg.mvThemeCol_Button,         C_BG3)
            dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered,  C_ACCENT2)
            dpg.add_theme_color(dpg.mvThemeCol_ButtonActive,   C_ACCENT)
            dpg.add_theme_color(dpg.mvThemeCol_Header,         C_BG3)
            dpg.add_theme_color(dpg.mvThemeCol_HeaderHovered,  C_ACCENT2)
            dpg.add_theme_color(dpg.mvThemeCol_HeaderActive,   C_ACCENT)
            dpg.add_theme_color(dpg.mvThemeCol_Separator,      C_BORDER)
            dpg.add_theme_color(dpg.mvThemeCol_Text,           C_TEXT)
            dpg.add_theme_color(dpg.mvThemeCol_Border,         C_BORDER)
            dpg.add_theme_color(dpg.mvThemeCol_PlotLines,      C_ACCENT)
            dpg.add_theme_color(dpg.mvThemeCol_PlotHistogram,  C_ACCENT)
            dpg.add_theme_style(dpg.mvStyleVar_WindowRounding,  0.0)
            dpg.add_theme_style(dpg.mvStyleVar_ChildRounding,   0.0)
            dpg.add_theme_style(dpg.mvStyleVar_FrameRounding,   0.0)
            dpg.add_theme_style(dpg.mvStyleVar_GrabRounding,    0.0)
            dpg.add_theme_style(dpg.mvStyleVar_WindowPadding,   8,  8)
            dpg.add_theme_style(dpg.mvStyleVar_ItemSpacing,     6,  4)
            dpg.add_theme_style(dpg.mvStyleVar_FramePadding,    4,  3)

    dpg.bind_theme(global_theme)
