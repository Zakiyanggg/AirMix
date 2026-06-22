# AirMix 交互映射

## 手势到音效

| 手势 | 检测含义 | MVP 音效映射 | 主要参数 |
| --- | --- | --- | --- |
| tilt left | 设备向左倾斜 | High-pass filter sweep | 倾斜越明显，high-pass cutoff 越高 |
| tilt right | 设备向右倾斜 | Low-pass filter sweep | 倾斜越明显，low-pass cutoff 越低 |
| shake | 快速晃动 | Stutter / repeat | 触发半拍或四分之一拍长度的重复 |
| punch | 快速向前冲击 | Delay throw | 触发一次较明显的 echo |
| hit | 快速向下敲击 | Delay throw 或短 stutter | MVP 中与 punch 共用 delay throw |
| rotate | 快速旋转 | Gate / rhythmic mute | gate depth 增大，并跟随 tap tempo |
| lift | 上抬 | 增加亮度或轻微音量提升 | MVP 中轻微提高 high-pass 表现 |
| drop | 下落 | Volume ducking + low-pass | 瞬时降低音量并压暗声音 |

## 按钮功能

| 按钮 | 操作 | 功能 |
| --- | --- | --- |
| Effect Mode | 短按 | 在 High-pass / Low-pass / Gate / Delay / Stutter 间循环 |
| Hold | 按住 | 保持当前效果参数，新的手势不继续改变参数 |
| Tap | 连续短按 | 输入 tempo，供 gate 和 stutter 对齐节拍 |
| Bypass | 短按 | 开关总旁路，恢复原始播放或回到效果处理 |
| Reset | 短按，后续硬件可选 | 重置效果参数和控制状态 |

## 手势和按钮组合优先级

1. Bypass 最高优先级：开启后所有 DSP 效果停止处理，音频直接通过。
2. Reset 次高优先级：重置当前控制状态，再进入默认 High-pass 模式。
3. Tap Tempo 独立生效：无论当前效果模式是什么，都更新 BPM。
4. Hold 生效时冻结效果参数：按钮状态和 tempo 可以更新，但手势不改变 cutoff、gate depth、delay mix 等参数。
5. Effect Mode 改变基础模式：没有强手势触发时，当前模式由 Effect Mode 决定。
6. 强瞬时手势可临时覆盖模式：shake 可临时触发 stutter，punch/hit 可临时触发 delay throw。
7. 姿态类手势控制连续参数：tilt left/right、lift/drop 用于改变滤波或音量参数。

## 防误触策略

- 阈值分层：punch/hit、shake、rotate 使用更高阈值，tilt 使用较低阈值。
- 手势优先级：冲击类动作优先于倾斜类动作，避免敲击时同时触发 tilt。
- 冷却时间：真实硬件版本应在 shake、punch、hit、stutter 后加入短冷却窗口。
- 按钮去抖：GPIO 输入需要 5-20 ms 级 debounce，并区分按下边沿和保持状态。
- Hold 冻结：表演中需要稳定保持一个效果时，用户可按住 Hold 防止动作抖动继续改变参数。
- 扬声器震动补偿：内置扬声器工作时，IMU 数据需要 notch / low-pass / 自适应阈值，降低声学震动误触。
- 上下文约束：当设备静置或音频很小声时，可以提高强手势阈值，减少桌面震动触发。

