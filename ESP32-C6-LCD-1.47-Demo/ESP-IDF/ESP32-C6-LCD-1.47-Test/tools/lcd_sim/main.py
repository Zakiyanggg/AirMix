#!/usr/bin/env python3
"""ESP32 320x172 LCD UI simulator — keyboard 1-4 = physical keys K1-K4."""

from __future__ import annotations

import sys

import pygame

from palette import C_BG, C_CREAM, C_KEY_OFF, C_KEY_ON, C_MUTED, C_PANEL, LCD_H, LCD_W
from ui_renderer import BluetoothScreen, FontSet, FreePlayScreen, render_lcd

SCALE = 2
PANEL_W = 300
WIN_W = LCD_W * SCALE + PANEL_W + 40
WIN_H = LCD_H * SCALE + 80

PAD_BY_KEY = [2, 1, 0, 3]
PAD_LABELS = ["HAT", "KIK", "SNR", "FX"]

KEY_LABELS = [
    ("1", "K1", "SN", "Snare"),
    ("2", "K2", "BD", "Kick"),
    ("3", "K3", "OH", "Open Hat"),
    ("4", "K4", "CH", "Closed Hat"),
]

DEMO_TITLES = [
    ("NEVER GONNA GIVE YOU UP", "R.ASTLEY"),
    ("纯音乐", "未知艺术家"),
    ("ESP32-AUDIO", "PAIR & PLAY"),
]


def draw_scaled_lcd(dst: pygame.Surface, lcd: pygame.Surface, x: int, y: int) -> None:
    scaled = pygame.transform.scale(lcd, (LCD_W * SCALE, LCD_H * SCALE))
    frame = pygame.Rect(x, y, LCD_W * SCALE, LCD_H * SCALE)
    pygame.draw.rect(dst, (30, 30, 30), frame, 0)
    pygame.draw.rect(dst, (80, 80, 80), frame, 2)
    dst.blit(scaled, (x, y))


def draw_side_panel(
    dst: pygame.Surface,
    fonts: FontSet,
    x: int,
    y: int,
    screen_name: str,
    keys_down: list[bool],
    status_lines: list[str],
) -> None:
    panel = pygame.Rect(x, y, PANEL_W, LCD_H * SCALE)
    pygame.draw.rect(dst, C_PANEL, panel)
    pygame.draw.rect(dst, C_MUTED, panel, 1)

    _text = fonts.sm.render
    line_y = y + 12
    dst.blit(_text("LCD Simulator", True, C_CREAM), (x + 12, line_y))
    line_y += 28
    dst.blit(fonts.xs.render(f"Screen: {screen_name}", True, C_MUTED), (x + 12, line_y))
    line_y += 22

    dst.blit(fonts.xs_b.render("Keys (device K1-K4)", True, C_CREAM), (x + 12, line_y))
    line_y += 20
    for i, (key, gpio, abbr, name) in enumerate(KEY_LABELS):
        pressed = keys_down[i]
        color = C_KEY_ON if pressed else C_KEY_OFF
        btn = pygame.Rect(x + 12, line_y, PANEL_W - 24, 34)
        pygame.draw.rect(dst, color, btn, border_radius=4)
        label = f"[{key}]  {gpio}  {abbr} — {name}"
        dst.blit(fonts.sm.render(label, True, C_CREAM if pressed else C_MUTED), (x + 20, line_y + 8))
        line_y += 42

    line_y += 8
    dst.blit(fonts.xs_b.render("Shortcuts", True, C_CREAM), (x + 12, line_y))
    line_y += 18
    help_lines = [
        "Tab      Switch BT / FreePlay",
        "Space    Toggle play state",
        "C        Toggle BT connected",
        "N        Cycle demo title",
        "Esc      Quit",
    ]
    for hl in help_lines:
        dst.blit(fonts.xs.render(hl, True, C_MUTED), (x + 12, line_y))
        line_y += 16

    line_y += 10
    dst.blit(fonts.xs_b.render("Status", True, C_CREAM), (x + 12, line_y))
    line_y += 18
    for sl in status_lines:
        dst.blit(fonts.xs.render(sl, True, C_CREAM), (x + 12, line_y))
        line_y += 16


def main() -> int:
    pygame.init()
    pygame.display.set_caption("ESP32 LCD Simulator 320×172")
    screen = pygame.display.set_mode((WIN_W, WIN_H))
    clock = pygame.time.Clock()
    fonts = FontSet()

    bt = BluetoothScreen()
    fp = FreePlayScreen()
    active = "bluetooth"
    keys_down = [False, False, False, False]
    demo_idx = 0
    status_msg = "Ready"

    lcd_x = 20
    lcd_y = 20

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    running = False
                elif event.key == pygame.K_TAB:
                    active = "freeplay" if active == "bluetooth" else "bluetooth"
                    status_msg = f"Switched to {active}"
                elif event.key == pygame.K_SPACE:
                    bt.audio_started = not bt.audio_started
                    status_msg = "Playing" if bt.audio_started else "Stopped"
                elif event.key == pygame.K_c:
                    bt.connected = not bt.connected
                    status_msg = "BT connected" if bt.connected else "BT disconnected"
                elif event.key == pygame.K_n:
                    demo_idx = (demo_idx + 1) % len(DEMO_TITLES)
                    bt.title, bt.artist = DEMO_TITLES[demo_idx]
                    status_msg = f"Demo: {bt.title[:20]}..."
                elif event.key in (pygame.K_1, pygame.K_2, pygame.K_3, pygame.K_4):
                    idx = event.key - pygame.K_1
                    keys_down[idx] = True
                    abbr = KEY_LABELS[idx][2]
                    status_msg = f"Key {idx + 1} down → {abbr}"
                    if active == "freeplay":
                        fp.pad_active = PAD_BY_KEY[idx]
                        fp.drum_held = f"{PAD_LABELS[PAD_BY_KEY[idx]]} HELD"
            elif event.type == pygame.KEYUP:
                if event.key in (pygame.K_1, pygame.K_2, pygame.K_3, pygame.K_4):
                    idx = event.key - pygame.K_1
                    keys_down[idx] = False

        if active == "bluetooth":
            bt.tick()
            lcd = render_lcd(bt, fonts)
            screen_name = "Bluetooth (UI_02_320x172)"
        else:
            fp.tick()
            lcd = render_lcd(fp, fonts)
            screen_name = "FreePlay (UI_01_320x172)"

        screen.fill(C_BG)
        draw_scaled_lcd(screen, lcd, lcd_x, lcd_y)

        status_lines = [
            status_msg,
            f"BT conn={'Y' if bt.connected else 'N'}  play={'Y' if bt.audio_started else 'N'}",
            f"Title: {bt.title[:28]}",
        ]
        draw_side_panel(
            screen,
            fonts,
            lcd_x + LCD_W * SCALE + 20,
            lcd_y,
            screen_name,
            keys_down,
            status_lines,
        )

        hint = fonts.xs.render("320×172 landscape · scale 2× · SVG UI_01/UI_02", True, C_MUTED)
        screen.blit(hint, (lcd_x, lcd_y + LCD_H * SCALE + 12))

        pygame.display.flip()
        clock.tick(30)

    pygame.quit()
    return 0


if __name__ == "__main__":
    sys.exit(main())
