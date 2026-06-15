# 冰箱 PID 温度控制系统

> **计算机系统课程设计 · 最终项目**  
> 基于 ESP32-S3 的冷冻室 PID 温度闭环控制系统，集成 Wokwi 在线仿真、嵌入式 Web 控制面板与微信小程序远程操控。

本项目 fork 自 [ashllll/klipper_pid_esp32s3](https://github.com/ashllll/klipper_pid_esp32s3)（MIT License），在其 FreeRTOS 多任务 PID 温控框架基础上，针对**冰箱冷冻室制冷场景**进行了大量改造与功能扩展。

---

## 与原项目的差异

原项目定位为 3D 打印机热床/挤出头**加热**温控。本项目在保留其 ESP32-S3 + FreeRTOS + ADS1115 + SHT40 + SSD1306 + EC11 硬件架构的基础上，做了以下系统性改造：

### 1. 应用场景与 PID 方向

| 对比维度 | 原项目 | 本项目 |
|---------|--------|--------|
| 控制对象 | 加热器（3D打印机热床等） | 冰箱冷冻室制冷 |
| PID 方向 | DIRECT（输出↑→温度↑） | **REVERSE**（输出↑→温度↓） |
| 执行器 | MOSFET PWM 加热 | 压缩机继电器 + 除霜继电器 + PWM 风扇 |
| PID 参数 | 自动调谐获得 | 手动整定（Kp=2.0, Ki=5.0, Kd=1.0） |

### 2. 新增：四模式状态机

原项目仅通过控制状态（加热/调谐/运行）管理 PID。本项目新增了**四模式工作状态机**，覆盖冰箱完整运行场景：

- **制冷模式** — PID 正常控制压缩机启停
- **除霜模式** — 关闭压缩机、开启加热丝，温度升至 -5°C 或 20 分钟后自动退出
- **节能模式** — 目标温度提高 2°C、关闭风扇，门开事件触发退出
- **故障模式** — 传感器异常或温度超限时自动关闭所有执行器并触发声光报警

### 3. 新增：压缩机保护机制

- 最小停机间隔 3 分钟，防止频繁启动损坏压缩机电机
- 最小运行时长 5 分钟，避免短周期运行
- PID 输出滞回区间（10~30），避免设定值附近频繁开关

### 4. 新增：Wifi 通信与远程控制

原项目为纯本地独立运行设备。本项目新增了完整的网络通信层：

- **ESP32 SoftAP 热点**（SSID: FridgeControl），无需路由器即可直连
- **HTTP RESTful API**（6 个端点）：`/api/status`、`/api/setpoint`、`/api/mode`、`/api/pid`、`/api/history`、CORS 预检
- **嵌入式 Web 控制面板** — HTML5 + Plotly.js 实时温度曲线，深蓝渐变玻璃态 UI，存储在 PROGMEM 中不占用 RAM
- **微信小程序** — 4 个功能页面（设备连接、控制面板、温度曲线、PID 调参），TabBar 导航，支持离线 Mock 模式

### 5. 新增：Wokwi 在线仿真平台

原项目仅支持物理硬件部署。本项目新增了完整的 Wokwi 仿真环境：

- **温度物理模拟引擎** — 根据压缩机/除霜/自然回温状态，以不同速率（-0.5/+0.8/+0.1 °C/s）动态变化
- **三按钮编码器模拟** — A/B/SW 按钮替代旋转编码器
- **串口键盘交互** — 支持 `'a'/'b'/'t'/'s'` 等按键模拟物理操作
- **双主程序编译隔离** — `main.cpp`（物理版，FreeRTOS 7 任务）与 `wokwi_main.cpp`（仿真版，单循环），通过 `build_src_filter` 互斥编译

### 6. 新增模块（独立文件）

原项目核心逻辑集中在一个 `main.cpp` 中。本项目对所有功能进行了模块化拆分：

| 模块 | 文件 | 职责 |
|------|------|------|
| 传感器 | `sensors_sim.cpp/h` | 模拟/真实 NTC 温度读取、EMA 滤波 |
| 执行器 | `hardware.cpp` / `actuator.h` | 压缩机/除霜/风扇/LED/蜂鸣器统一接口 |
| PID 控制 | `pid_control.cpp/h` | REVERSE 方向 PID + 滞回控制 |
| 状态机 | `mode_defs.cpp` / `state_machine.h` | 四模式定义与切换逻辑 |
| 系统控制 | `system_control.cpp/h` | 安全检查、模式更新、全局复位 |
| OLED 显示 | `oled_display.cpp/h` | U8g2 帧缓冲渲染、菜单交互 |
| 输入处理 | `input_handler.cpp/h` | 三按钮 + 串口双通道输入 |
| 温度历史 | `temp_history.cpp/h` | 60 点环形缓冲区 + JSON 序列化 |
| Web 服务 | `wifi_webserver.cpp/h` | HTTP 路由注册、CORS、JSON 响应 |
| Web 界面 | `web_interface.h` | 嵌入式 HTML5 页面（PROGMEM） |
| 引脚配置 | `pin_config.h` | Wokwi 仿真版引脚宏定义 |

### 7. 功能对比总表

| 功能 | 原项目 | 本项目 |
|------|:---:|:---:|
| FreeRTOS 多任务 | ✅ | ✅ |
| NTC 温度采集 + ADS1115 | ✅ | ✅ |
| SHT40 环境监测 | ✅ | ✅ |
| SSD1306 OLED + U8g2 | ✅ | ✅（5页 UI） |
| EC11 旋转编码器 | ✅ | ✅ |
| PID 自动调谐 | ✅ | ❌（已移除） |
| NVS 参数持久化 | ✅ | ❌（已移除） |
| EMA 滤波 | ✅ | ✅ |
| 制冷 REVERSE PID | ❌ | ✅ |
| 四模式状态机 | ❌ | ✅ |
| 压缩机延时保护 | ❌ | ✅ |
| WiFi SoftAP + HTTP API | ❌ | ✅ |
| 嵌入式 Web 控制面板 | ❌ | ✅ |
| 微信小程序（4页） | ❌ | ✅ |
| Wokwi 仿真 + 温度模拟 | ❌ | ✅ |
| 环形缓冲温度历史 | ❌ | ✅ |
| 多级 LED + 蜂鸣器报警 | ❌ | ✅ |
| 安全检查（NaN/越界） | ❌ | ✅ |
| 双平台编译隔离 | ❌ | ✅ |

---

## 系统架构

```
┌─────────────────────────────────────────────────────────┐
│                    用户交互层                              │
│  微信小程序 (设备连接/温度控制/PID调参/曲线查看)            │
│  Web浏览器 (实时监控面板/Plotly.js图表)                    │
│  OLED 显示屏 (本地5页UI/编码器交互)                        │
├─────────────────────────────────────────────────────────┤
│                    通信服务层                              │
│  WiFi Web Server (HTTP API: /api/status|setpoint|mode...)  │
│  ESP32 SoftAP (192.168.4.1)                               │
├─────────────────────────────────────────────────────────┤
│                    核心业务层                              │
│  PID 控制器 | 状态机 (四模式) | 系统控制 | 安全检查          │
│  温度物理模拟 | NTC传感器读取 | EMA滤波                     │
├─────────────────────────────────────────────────────────┤
│                    硬件驱动层                              │
│  GPIO | I2C | ADC | PWM | Relay | OLED(SSD1306) | Buzzer  │
└─────────────────────────────────────────────────────────┘
```

---

## 快速开始

### 环境要求

- **IDE**：VS Code + PlatformIO
- **编译器**：espressif32（Arduino Framework）
- **仿真**：Wokwi for VS Code 插件
- **小程序**：微信开发者工具

### 依赖库

U8g2、GyverEncoder、PID_v1、ArduinoJson、Adafruit_SHT4x、Adafruit_ADS1X15

### Wokwi 仿真运行

1. 在 VS Code 中安装 Wokwi 插件
2. 打开项目，按 `F1` → `Wokwi: Start Simulation`
3. 仿真启动后，可通过以下方式交互：
   - **串口监视器** — 键盘输入 `a/b/s/t` 模拟按钮操作
   - **浏览器** — 访问 `http://192.168.4.1` 打开 Web 控制面板

### 物理硬件部署

1. 按 `include/pin_config.h` 连接外围器件
2. 在 `platformio.ini` 中确保 `build_src_filter` 排除 `wokwi_main.cpp`
3. 编译并上传到 ESP32-S3-DevKitC-1
4. 手机连接 WiFi 热点 `FridgeControl`（密码 `12345678`）
5. 微信小程序输入设备 IP 进行连接

---

## 微信小程序

小程序提供 4 个功能页面：

- **设备连接页** — IP 地址输入、扫码连接、实时状态显示、操作日志
- **控制面板页** — 温度显示、模式切换、温度调节、快捷操作
- **温度曲线页** — Canvas 2D 折线图、时间范围切换、温度统计
- **PID 调参页** — Kp/Ki/Kd 滑块调节、预设参数加载、参数说明

---

## 项目结构

```
fridge_pid_control/
├── include/                # 头文件
│   ├── actuator.h          # 执行器控制接口
│   ├── input_handler.h     # 输入处理接口
│   ├── oled_display.h      # OLED 显示接口
│   ├── pid_control.h       # PID 控制接口
│   ├── pin_config.h        # 引脚配置（Wokwi 仿真版）
│   ├── sensors_sim.h       # 传感器模拟接口
│   ├── state_machine.h     # 状态机控制接口
│   ├── system_control.h    # 系统控制接口
│   ├── temp_history.h      # 温度历史记录接口
│   ├── web_interface.h     # 嵌入式网页 HTML
│   └── wifi_webserver.h    # Web 服务器接口
├── src/                    # 源文件
│   ├── main.cpp            # 物理硬件版主程序（FreeRTOS）
│   ├── wokwi_main.cpp      # Wokwi 仿真版主程序（单循环）
│   ├── hardware.cpp        # 执行器驱动
│   ├── sensors_sim.cpp     # 温度传感器模拟
│   ├── pid_control.cpp     # PID 控制器
│   ├── oled_display.cpp    # OLED 显示
│   ├── input_handler.cpp   # 按钮/串口输入
│   ├── system_control.cpp  # 安全 + 模式管理
│   ├── temp_history.cpp    # 环形缓冲区历史
│   ├── wifi_webserver.cpp  # HTTP API 服务
│   └── mode_defs.cpp       # 状态机定义
├── wechatapp/              # 微信小程序
│   └── miniprogram/
│       ├── app.js / app.json
│       └── pages/
│           ├── device/     # 设备连接页
│           ├── control/    # 控制面板页
│           ├── chart/      # 温度曲线页
│           └── pid/        # PID 调参页
├── docs/                   # 课程设计报告
├── platformio.ini          # PlatformIO 配置
├── diagram.json            # Wokwi 仿真电路图
└── wokwi.toml              # Wokwi 配置
```

---

## 许可证

本项目基于 [ashllll/klipper_pid_esp32s3](https://github.com/ashllll/klipper_pid_esp32s3)（MIT License）修改，继续沿用 **MIT License** 发布。原始版权归属原项目作者 ashllll，本项目的修改及新增部分版权归属课程设计小组。详见 [LICENSE](./LICENSE) 文件。

---

## 参考资料

- 原项目：[ashllll/klipper_pid_esp32s3](https://github.com/ashllll/klipper_pid_esp32s3)
- PID 库：[br3ttb/Arduino-PID-Library](https://github.com/br3ttb/Arduino-PID-Library)
- 显示库：[olikraus/U8g2](https://github.com/olikraus/u8g2)
- JSON 库：[bblanchon/ArduinoJson](https://arduinojson.org/)
