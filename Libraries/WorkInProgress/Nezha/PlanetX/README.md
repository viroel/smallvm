# Planet X 库整理与硬件验收

本目录是本次修改的目标，使用新的模块名和函数名，不提供旧项目兼容入口。主板库、`Nezha V1` 和 `Kits and Boards` 下的发布库不属于本次修改范围。

## 库的划分

| 库文件 / 模块名 | 内容 | 依赖 |
| --- | --- | --- |
| Planet X AI Lens | AI 摄像头：卡片、人脸、小球、颜色卡、巡线、自学习 | `_Planet X Support` |
| Planet X Gesture | 外接手势传感器 | `_Planet X Support` |
| Planet X Color | 外接颜色识别传感器 | `_Planet X Support` |
| Planet X Speech | 外接语音识别传感器 EF05037 | `_Planet X Support` |
| Planet X Input | 按钮、模拟量、温湿度、气体、RFID、RTC、四路巡线等 | `_Planet X Support` |
| Planet X Display | 数码管、彩虹灯、OLED、点阵屏 | `_Planet X Support` |
| Planet X Wireless | 外接 WiFi、SmartIoT、I²C 手柄 | `_Planet X Support` |
| Planet X Execute | LED、激光、风扇、MP3 | `_Planet X Support` |
| AI Lens Pro | AI Lens Pro 摄像头 | `_Planet X Support` |
| Planet X Support | 公共下拉菜单、I²C 初始化、FIFO 锁、读写函数及其状态变量 | 无 |

旧的 `AI Lens.ubl` 已拆分，不再提供。各库只保留自己使用的变量和专用菜单，重复菜单统一放入 Support。所有自定义函数具有 `planetX` 前缀；内部函数具有 `_planetX` 前缀并提供 `spec`。

Support 在 `depends` 中加下划线，作为实现库自动导入。正常使用不需要显示它；高级模式中开启显示实现积木和库，才会看到它。加载后共享 I²C 锁和队列只有一份。

## 公共下拉菜单

以下六组枚举只在 `Planet X Support.ubl` 定义一次，各硬件库通过 `depends '_Planet X Support'` 加载，并在 `spec` 中直接使用 `menu.枚举名称`。实现库隐藏不会影响下拉菜单查找。

| 枚举名称 | 按显示顺序排列的值 |
| --- | --- |
| `planetXPort` | J1、J2、J3、J4 |
| `planetXNumberCard` | 0–9（保留字符串类型） |
| `planetXLetterCard` | A、B、C、D、E |
| `planetXObjectValue` | x coordinate、y coordinate、width、height、confidence (%)、id |
| `planetXOnOff` | off、on |
| `planetXBasicColor` | red、green、blue、white、black、yellow |

手势和物体的坐标属性使用同一个 `planetXObjectValue`；两个摄像头的数字卡、字母卡和基本颜色也分别共用同一个菜单。原来顺序不同的开关、颜色菜单采用上表顺序，各积木原有的默认选项不变。

## 积木与接口调整

- 摄像头积木按采集、小球、人脸、卡片、颜色卡、巡线、自学习分组。
- 通用卡片取值标签为 `AI camera any card _ value`，同时保留独立的颜色卡取值积木。通用卡片取值支持数字、字母、交通、物品和颜色卡；原有计数逻辑保持不变，颜色卡数量通过独立的颜色卡计数积木读取。
- 手势库保留初始化和下拉菜单判断，删除固定返回英文名称的 `readGesture` 及其名称转换函数。
- 颜色库保留色相读取和颜色判断。
- 语音库的 `current code` 积木位于 `advanced` 区，其余三个积木正常显示。
- Input 删除旧参数顺序的气体、RTC 包装入口及参数顺序自动交换逻辑；四路巡线偏移量放在高级区。
- Wireless 删除旧的可选参数连接状态入口，普通连接状态积木直接返回布尔值。
- Display 删除未公开的旧刷新/单数字入口；保留渐变色内部实现。

常用的新函数名如下，完整接口以各库的 `spec` 为准：

| 功能 | 函数名 |
| --- | --- |
| 初始化手势传感器 | `planetXGestureInit` |
| 判断手势 | `planetXGestureDetected` |
| 读取颜色色相 | `planetXColorHue` |
| 判断颜色 | `planetXColorDetected` |
| 清空语音学习词条 | `planetXASRClearLearned` |
| 进入语音学习 | `planetXASREnterLearning` |
| 判断识别词条 | `planetXASRHeard` |
| 读取语音代码（高级） | `planetXASRReadCode` |
| 外接 WiFi 初始化 / 连接 | `planetX_wifiInit` / `planetX_wifiConnect` |
| 手柄按键 / 模拟量 | `planetX_joystickButton` / `planetX_joystickAnalog` |
| 气体 / RTC 设置 | `planetX_gasValue` / `planetX_rtcSetData` |
| OLED 文本 / 数字 | `planetX_oledShowText` / `planetX_oledShowNumber` |
| MP3 音量 | `planetX_mp3SetVolume` |

AI Lens Pro 的 `aicam_…` / `_aicam_…` 改为 `planetX_aiLensPro_…` / `_planetX_aiLensPro_…`，通信和识别逻辑保持原样。

## 语音学习模式

官方 [EF05037 WIKI](https://wiki.elecfreaks.com/microbit/planetx-series/sensors/Plant_X_EF05037) 的说明是：

1. 上电后说“你好，小恩”唤醒模块。
2. 录入一组新词条前，先清空已学习词条。
3. 执行进入学习模式积木，按照模块语音提示录入。
4. 学习完成后说“退出学习”，退出学习模式。

WIKI 列出的 I²C 命令为 `0x50`（进入学习）和 `0x60`（删除所有已学习词条），没有列出程序退出命令。因此本次不新增程序退出积木，也不将退出方式描述为自动退出。进入学习和清空学习仍沿用原有命令与等待时间。

WIKI 的识别码表为：唤醒 `0x01`，车辆指令 `0x10–0x18`，设备指令 `0x20–0x28`，功能指令 `0x31–0x32`，自学习词条 `0x50–0x59`。语音读取仍使用 100 kHz，识别码缓存仍为 250 ms；其他原有 I²C 事务继续使用各自的通信速度。

## 自动检查

在仓库根目录运行：

```sh
bash tools/test_planetx_libraries.sh
```

检查包含：

- 模块名与文件名一致，每个函数都有参数数量一致的 `spec`，菜单可从当前库或依赖库解析，默认值有效。
- 每个枚举名称和枚举值集合只有一处定义，包括仅排列顺序不同的重复集合。
- 函数和状态变量各有唯一归属，每个库的调用依赖闭合。
- 用真实 GP/MicroBlocks 解析器和编译器，分别导入并编译十个库，验证 Support 自动导入、实现库标记、跨库菜单查找和默认值。
- 正序和逆序组合导入，检查函数不丢失、没有重复定义。
- 模拟语音识别码、缓存、学习命令、手势检测、卡片类别、WiFi/SmartIoT 连接状态。
- 模拟并发 I²C FIFO、公平排队、互斥、超时取消，以及无竞争时没有额外固定等待。

模拟通过不能代替实物测试，也不能证明自定义固件中的硬件原语可用。

## 实物验收清单

记录测试使用的主板型号、固件烧录文件、MicroBlocks IDE 版本，以及下面每项的实际结果。每组从空白项目开始，按需加载库；不要把所有库一次性导入一个测试项目，当前 GP VM 的函数和脚本 chunk 总数上限为 255。

要验证自动依赖，需使用已包含本目录新库和 Support 的 IDE。只把一个 `.ubl` 拖进尚未包含这些库的旧 IDE，无法验证自动依赖查找。临时手动导入实测时，可先导入 `Planet X Support.ubl`，再导入要测试的硬件库。

| 通过 | 项目 | 操作和预期结果 |
| --- | --- | --- |
| ☐ | 单独导入 | 分别只添加一个新硬件库；所需 Support 自动加载；普通面板不出现其他三类智能传感器积木 |
| ☐ | 实现库显示 | 高级模式开启显示实现积木和库；能看到 Support 和内部函数；关闭后隐藏 |
| ☐ | 公共菜单 | 单独导入 Execute、Input、Display、Wireless、AI Lens 和 AI Lens Pro，检查端口、数字卡、字母卡、坐标属性、开关和基本颜色等适用菜单；Support 隐藏时选项完整，默认值正确 |
| ☐ | AI Lens 基础 | 初始化、切换模式、采集图像；识别结果与画面一致 |
| ☐ | AI Lens 卡片 | 分别测试数字、字母、交通、物品、颜色卡的判断、坐标、计数；颜色卡使用独立颜色卡计数 |
| ☐ | AI Lens 其他功能 | 小球颜色/坐标、人脸、巡线方向、自学习和清除学习均正常 |
| ☐ | 手势 | 初始化后分别测试右、左、上、下、前、后、顺时针、逆时针、挥手；下拉菜单判断正确 |
| ☐ | 颜色 | 测试红、绿、蓝、青、洋红、黄、白；色相和颜色判断与原版本表现一致 |
| ☐ | 语音预设词 | 唤醒后分别测试车辆、设备和功能组；高级代码显示与 WIKI 表一致 |
| ☐ | 语音学习 | 清空、进入学习、录入词条、说“退出学习”；随后能识别自学习词条；再次清空后不再匹配 |
| ☐ | Input | 逐个测试按钮、碰撞、超声波、PIR、DHT11、DS18B20、双路巡线、噪声、光照、土壤湿度、水位、紫外线、气体、PM2.5、灰尘、电位器 |
| ☐ | Input I²C | RFID 检卡/读写、BME280、RTC 设置/读取、四路巡线状态/灰度/偏移量均正常 |
| ☐ | Display | 数码管、彩虹灯、OLED、点阵屏分别测试；显示内容和刷新正常 |
| ☐ | Wireless | WiFi 连接、AT 响应、SmartIoT 上传/开关事件、手柄全部按键和四轴模拟量均正常 |
| ☐ | Execute | LED、激光、风扇开关，MP3 音量/播放/暂停/切曲/循环正常 |
| ☐ | AI Lens Pro | 初始化、切换模式、采集及各识别功能，与整理前相同 |
| ☐ | I²C 并发 | 先用 AI Lens + 手柄 + 语音运行多个脚本，再分组加入显示或 Input I²C 设备；持续操作无卡死、结果串扰或队列积压 |
| ☐ | 缺设备/重启 | 传感器未连接时调用不造成持续卡死；停止全部脚本后重启主板，再次运行正常 |

手势和语音判断的菜单可翻译；中文语音唤醒词和“退出学习”是模块实际支持的口令，不是需要翻译的程序返回值。
