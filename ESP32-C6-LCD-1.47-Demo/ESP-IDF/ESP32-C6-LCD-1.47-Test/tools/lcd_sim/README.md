# ESP32 LCD UI Simulator

320×172 横屏 UI 模拟器，用于在电脑上预览 `main/LVGL_UI/UI_*_320x172.svg` 里的界面设计，无需烧录设备。

## 安装与运行

推荐一键启动（自动创建 `.venv` 并安装依赖）：

```bash
cd tools/lcd_sim
chmod +x run.sh
./run.sh
```

或手动：

```bash
cd tools/lcd_sim
python3.12 -m venv .venv   # macOS 建议 3.12，3.14 可能需自行编译 pygame
.venv/bin/pip install -r requirements.txt
.venv/bin/python main.py
```

## 操作

| 按键 | 功能 |
|------|------|
| **1 2 3 4** | 模拟设备四键 K1–K4（SN / BD / OH / CH） |
| **Tab** | 切换 Bluetooth / FreePlay 界面 |
| **Space** | 切换播放 / 停止（波形动效） |
| **C** | 切换蓝牙连接状态（蓝点） |
| **N** | 切换演示歌名（含中文「纯音乐」） |
| **Esc** | 退出 |

## 文件

- `main.py` — 窗口与键盘输入
- `ui_renderer.py` — 与设备 UI 同布局的绘制逻辑
- `palette.py` — 颜色常量（对应 `ui_common.h`）

后续可在此扩展：菜单导航、组合键、与 ESP32 串口同步等。
