"""
theme.py — DearPyGui dark theme and colour palette.
"""

import dearpygui.dearpygui as dpg

# ── Palette ───────────────────────────────────────────────────────────────────

C_BG        = (15,  18,  22,  255)   # near-black
C_BG2       = (22,  27,  34,  255)   # panel background
C_BG3       = (30,  36,  46,  255)   # widget background
C_BORDER    = (45,  55,  70,  255)
C_TEXT      = (210, 220, 230, 255)
C_DIM       = (100, 115, 130, 255)
C_ACCENT    = (0,   200, 200, 255)   # cyan
C_ACCENT2   = (0,   160, 160, 255)
C_GREEN     = (60,  210, 100, 255)
C_AMBER     = (240, 170,  40, 255)
C_RED       = (220,  60,  60, 255)
C_BTN_ON    = (0,   200, 200, 255)
C_BTN_OFF   = (40,  50,  65,  255)
C_STICK_BG  = (30,  36,  46,  255)
C_STICK_RIM = (60,  75,  95,  255)
C_TRACE     = (0,   200, 200,  60)
C_DOT       = (0,   230, 230, 255)
C_DEADZONE  = (50,  60,  80, 100)
C_TRIG_BG   = (30,  36,  46,  255)
C_TRIG_FILL = (0,   200, 200, 255)
C_CIRC_REF  = (60,  75,  95,  200)
C_CIRC_PT   = (0,   200, 200, 180)


def apply() -> None:
    """Apply the global dark theme to DearPyGui."""
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
            dpg.add_theme_style(dpg.mvStyleVar_WindowRounding,  6.0)
            dpg.add_theme_style(dpg.mvStyleVar_ChildRounding,   4.0)
            dpg.add_theme_style(dpg.mvStyleVar_FrameRounding,   4.0)
            dpg.add_theme_style(dpg.mvStyleVar_GrabRounding,    4.0)
            dpg.add_theme_style(dpg.mvStyleVar_WindowPadding,   10, 10)
            dpg.add_theme_style(dpg.mvStyleVar_ItemSpacing,     6,  4)
            dpg.add_theme_style(dpg.mvStyleVar_FramePadding,    6,  4)

    dpg.bind_theme(global_theme)
