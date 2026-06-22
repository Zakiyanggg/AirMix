"""320x172 UI renderers matching LVGL_UI/UI_*_320x172.svg templates."""

from __future__ import annotations

import pygame

from palette import (
    C_BAT_BG,
    C_BG,
    C_BLUE,
    C_BORDER,
    C_CREAM,
    C_DARK,
    C_DIVIDER,
    C_MUTED,
    C_PAD_FILL,
    C_RED,
    C_TAB_BT,
    C_TAB_TRK,
    LCD_H,
    LCD_W,
)

FP_WAVE = [
    (10, 6), (6, 10), (2, 14), (8, 8), (4, 12), (9, 7), (0, 16), (5, 11),
    (11, 5), (3, 13), (7, 9), (10, 6), (4, 12), (8, 8), (1, 15), (6, 10),
    (10, 6), (4, 12), (8, 8), (2, 14), (9, 7), (5, 11), (10, 6), (3, 13),
    (7, 9), (10, 6), (4, 12), (8, 8), (6, 10), (9, 7),
]

BT_WAVE = [
    (14, 6), (10, 10), (4, 16), (8, 12), (12, 8), (6, 14), (14, 6), (2, 18),
    (10, 10), (6, 14), (12, 8), (4, 16), (8, 12), (14, 6), (10, 10), (6, 14),
    (2, 18), (14, 6), (8, 12), (12, 8),
]


def _hex_font(size: int, bold: bool = False) -> pygame.font.Font:
    names = [
        "Menlo",
        "Consolas",
        "Courier New",
        "Arial Unicode MS",
        "PingFang SC",
        "Helvetica Neue",
        "Arial",
    ]
    for name in names:
        path = pygame.font.match_font(name, bold=bold)
        if path:
            return pygame.font.Font(path, size)
    return pygame.font.SysFont("monospace", size, bold=bold)


class FontSet:
    def __init__(self) -> None:
        self.xs = _hex_font(8)
        self.sm = _hex_font(10)
        self.xs_b = _hex_font(8, bold=True)
        self.sm_b = _hex_font(10, bold=True)
        self.tab_b = _hex_font(11, bold=True)
        self.title_bt = _hex_font(13, bold=True)
        self.title_fp = _hex_font(16, bold=True)


def _box(surf: pygame.Surface, x: int, y: int, w: int, h: int, fill, border=None, bw=1) -> None:
    rect = pygame.Rect(x, y, w, h)
    pygame.draw.rect(surf, fill, rect)
    if border is not None and bw > 0:
        pygame.draw.rect(surf, border, rect, bw)


def _text(surf, font, text, x, y, color, align="left", width=None):
    if not text:
        return
    img = font.render(text, True, color)
    if align == "right" and width is not None:
        surf.blit(img, (x + width - img.get_width(), y))
    elif align == "center" and width is not None:
        surf.blit(img, (x + (width - img.get_width()) // 2, y))
    else:
        surf.blit(img, (x, y))


def _battery_pct_fill(battery: str) -> int:
    digits = "".join(ch for ch in battery if ch.isdigit())
    if not digits:
        return 10
    return max(4, min(16, int(digits) * 16 // 100))


def _bottom_bar(surf, fonts, fx_c, fx_t, drum_c, drum_t, battery="62%") -> None:
    y = 146
    fill_w = _battery_pct_fill(battery)

    _box(surf, 4, y, 76, 22, fx_c)
    _text(surf, fonts.tab_b, "FX", 4, y + 4, fx_t, "center", 76)
    _box(surf, 84, y, 76, 22, drum_c)
    _text(surf, fonts.tab_b, "DRUM", 84, y + 4, drum_t, "center", 76)
    _box(surf, 164, y, 76, 22, C_RED)
    _text(surf, fonts.tab_b, "MELODY", 164, y + 4, C_CREAM, "center", 76)

    _box(surf, 244, y, 72, 22, C_BAT_BG)
    _box(surf, 252, y + 6, 18, 10, C_BAT_BG, C_DARK, 1)
    _box(surf, 254, y + 8, fill_w, 6, C_DARK)
    _box(surf, 270, y + 9, 2, 4, C_DARK)
    _text(surf, fonts.sm_b, battery, 244, y + 4, C_DARK, "right", 64)


def _draw_wave(surf, bars, base_x: int, base_y: int, phase: int, animate: bool, bar_w: int = 2, step: int = 4) -> None:
    for i, (y_off, h) in enumerate(bars):
        bar_h = h
        bar_y = y_off
        if animate:
            bar_h = ((h + phase + i * 2) % 10) + 5
            bar_y = 18 - bar_h
        x = base_x + i * step
        y = base_y + bar_y
        _box(surf, x, y, bar_w, bar_h, C_CREAM)


def split_title(title: str) -> tuple[str, str]:
    if not title:
        return "", ""
    if len(title) <= 18:
        return title, ""
    split = len(title) // 2
    while split > 0 and title[split] != " ":
        split -= 1
    if split == 0:
        split = len(title) // 2
    return title[:split].strip(), title[split:].strip()


class BluetoothScreen:
    def __init__(self) -> None:
        self.connected = True
        self.audio_started = True
        self.title = "NEVER GONNA GIVE YOU UP"
        self.artist = "R.ASTLEY"
        self.style = "USER 01"
        self.fx = "LP -3"
        self.vol = "60"
        self.pos_cur = "01:23"
        self.pos_total = "03:45"
        self.battery = "62%"
        self.wave_phase = 0

    def tick(self) -> None:
        if self.audio_started:
            self.wave_phase += 1

    def render(self, surf: pygame.Surface, fonts: FontSet) -> None:
        surf.fill(C_BG)

        _box(surf, 4, 4, 154, 22, C_TAB_BT)
        _box(surf, 162, 4, 154, 22, C_TAB_TRK)
        dot_c = C_BLUE if self.connected else C_MUTED
        pygame.draw.circle(surf, dot_c, (18, 15), 3)
        _text(surf, fonts.tab_b, "BLUETOOTH", 4, 8, C_DARK, "center", 154)
        _text(surf, fonts.tab_b, "TRACK", 162, 8, C_DARK, "center", 154)

        lx, ly = 10, 34
        _box(surf, lx, ly + 2, 36, 22, C_BG, C_MUTED, 1)
        _box(surf, lx + 6, ly + 6, 8, 14, C_CREAM)
        _box(surf, lx + 16, ly + 6, 14, 14, C_BG, C_CREAM, 1)
        _text(surf, fonts.sm, "A2DP", lx + 46, ly + 6, C_MUTED)

        _draw_wave(surf, BT_WAVE, lx, ly + 32, self.wave_phase, self.audio_started, bar_w=4, step=6)

        _text(surf, fonts.xs, "POS", lx, ly + 66, C_MUTED)
        _box(surf, lx, ly + 72, 118, 6, C_BG, C_BORDER, 1)
        pos_w = 42 if self.audio_started else 10
        _box(surf, lx, ly + 72, pos_w, 6, C_CREAM)
        cur = self.pos_cur if self.audio_started else "01:23"
        _text(surf, fonts.sm_b, cur, lx, ly + 84, C_CREAM)
        _text(surf, fonts.sm, self.pos_total, lx, ly + 84, C_MUTED, "right", 118)

        pygame.draw.line(surf, C_DIVIDER, (138, 34), (138, 138), 1)

        rx, ry = 150, 34
        line1, line2 = split_title(self.title)
        _text(surf, fonts.title_bt, line1, rx, ry + 2, C_CREAM)
        _text(surf, fonts.title_bt, line2, rx, ry + 17, C_CREAM)

        rows = [
            ("ARTIST", self.artist, C_CREAM, fonts.sm_b),
            ("STYLE", self.style, C_CREAM, fonts.sm_b),
            ("FX", self.fx, C_TAB_TRK, fonts.sm_b),
        ]
        for i, (label, value, color, font) in enumerate(rows):
            row_y = ry + 46 + i * 16
            _text(surf, fonts.sm, label, rx, row_y, C_MUTED)
            _text(surf, font, value, rx, row_y, color, "right", 156)

        vol_y = ry + 94
        _text(surf, fonts.sm, "VOL", rx, vol_y, C_MUTED)
        _box(surf, rx + 38, vol_y - 8, 86, 9, C_BG, C_BORDER, 1)
        _box(surf, rx + 38, vol_y - 8, 51, 9, C_CREAM)
        _text(surf, fonts.sm_b, self.vol, rx, vol_y, C_CREAM, "right", 156)

        _bottom_bar(surf, fonts, C_CREAM, C_DARK, C_TAB_TRK, C_DARK, self.battery)


class FreePlayScreen:
    def __init__(self) -> None:
        self.kit_name = "NEON POP"
        self.bpm = "120"
        self.kit = "808"
        self.lead = "SYN1"
        self.fx = "PHASER"
        self.vol = "70"
        self.user = "01"
        self.drum_held = "DRUM HELD"
        self.battery = "85%"
        self.wave_phase = 0
        self.pad_active = 2

    def tick(self) -> None:
        self.wave_phase += 1

    def render(self, surf: pygame.Surface, fonts: FontSet) -> None:
        surf.fill(C_BG)

        _box(surf, 4, 4, 154, 22, C_TAB_BT)
        _box(surf, 162, 4, 154, 22, C_TAB_TRK)
        _text(surf, fonts.tab_b, "FREE PLAY", 4, 8, C_DARK, "center", 154)
        _text(surf, fonts.tab_b, "STYLE", 162, 8, C_DARK, "center", 154)

        lx, ly = 10, 34
        pads = [("HAT", 0), ("KIK", 1), ("SNR", 2), ("FX", 3)]
        for label, idx in pads:
            px = lx + idx * 30
            py = ly
            if idx == self.pad_active:
                _box(surf, px, py, 26, 22, C_TAB_TRK)
                _text(surf, fonts.xs_b, label, px, py + 6, C_DARK, "center", 26)
            else:
                _box(surf, px, py, 26, 22, C_BG, C_MUTED, 1)
                _box(surf, px + 3, py + 3, 20, 16, C_PAD_FILL)
                _text(surf, fonts.xs, label, px, py + 6, C_CREAM, "center", 26)

        _draw_wave(surf, FP_WAVE, lx, ly + 28, self.wave_phase, True, bar_w=2, step=4)
        pygame.draw.line(surf, C_DIVIDER, (lx, ly + 52), (lx + 118, ly + 52), 1)

        _text(surf, fonts.xs, "SUBMODE", lx, ly + 60, C_MUTED)
        _box(surf, lx, ly + 65, 56, 14, C_CREAM)
        _text(surf, fonts.sm_b, self.drum_held, lx, ly + 67, C_DARK, "center", 56)

        _text(surf, fonts.xs, "USER", lx, ly + 84, C_MUTED)
        _text(surf, fonts.sm_b, self.user, lx, ly + 84, C_CREAM, "right", 118)

        pygame.draw.line(surf, C_DIVIDER, (138, 34), (138, 138), 1)

        rx, ry = 150, 34
        _text(surf, fonts.title_fp, self.kit_name, rx, ry + 2, C_CREAM)
        rows = [
            ("BPM", self.bpm),
            ("KIT", self.kit),
            ("LEAD", self.lead),
            ("FX", self.fx),
        ]
        for i, (label, value) in enumerate(rows):
            row_y = ry + 32 + i * 16
            _text(surf, fonts.sm, label, rx, row_y, C_MUTED)
            _text(surf, fonts.sm_b, value, rx, row_y, C_CREAM, "right", 156)

        vol_y = ry + 98
        _text(surf, fonts.sm, "VOL", rx, vol_y, C_MUTED)
        _box(surf, rx + 38, vol_y - 8, 86, 9, C_BG, C_BORDER, 1)
        _box(surf, rx + 38, vol_y - 8, 60, 9, C_CREAM)
        _text(surf, fonts.sm_b, self.vol, rx, vol_y, C_CREAM, "right", 156)

        _bottom_bar(surf, fonts, C_BLUE, C_CREAM, C_CREAM, C_DARK, self.battery)


def render_lcd(screen_obj, fonts: FontSet) -> pygame.Surface:
    lcd = pygame.Surface((LCD_W, LCD_H))
    screen_obj.render(lcd, fonts)
    return lcd
