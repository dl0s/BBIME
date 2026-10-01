# BBIME

Q10 原生应用内的自然码双拼测试，当前源码版本 `0.1.0.11`。
使用 Cascades 原生文本编辑控件和 libgooglepinyin 词库引擎。
不是系统级跨应用输入法，不修改系统键盘、输入服务、词库或权限。
现提供可随各自应用嵌入的最小原生模块，接入与所有权契约见
[模块说明](C:/Users/dove1/Documents/BBIME/module/README.md)。

## 风险收敛与模块化

`0.1.0.11` 延续此前未完成的阶段 A/B：加固解码器固定容量池，
保留固定失败串、邻近输入、正常输入恢复和 10,000 次随机回归；
使用单一 `ImeService`、每字段 `InputSession`、原生 TextArea/TextField
选区适配、带修订标识的候选及 Sym 组件。

菜单“原生模块”打开标题、正文和不接入的密码字段。两个普通字段默认不学习、
不保存到测试草稿；字段切换取消编码，关闭后恢复原输入模式。
旧测试页保持原来的 Custom 暂停行为。两页互斥使用一个解码服务。
模块不接管宿主保存和撤销，拒绝敏感/只读字段，失败候选可重试，
Ctrl、Alt+Enter、Alt+Backspace 由宿主统一路由。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-PhaseAB.ps1
```

加入 `-Device` 会执行现有 Q10 上的新建合成 ARM 核心测试，不安装 BAR或读取真实正文。
最新验收结果见 `research/phase-ab-validation-0.1.0.11.json`；
手机 UI、焦点、硬件键、普通宿主接入和共享权限仍须分别取得运行证据。

## 操作

打开手机上的 BBIME，默认进入自然码模式。

| 操作 | 行为 |
| --- | --- |
| 输入 `nihk`，空格 | 选择首项“你好” |
| 输入 `vsgo`，空格 | 选择首项“中国” |
| 空格 | 选择高亮项，默认第一项；无编码时输入空格 |
| 有编码时左/右 Shift 单独短按 | 上一个/下一个候选，列表平滑跟随高亮；空格确认 |
| 无编码时左/右 Shift 单独短按 | 左移/右移正文光标；中文及 English 均适用 |
| Alt 单独按下/释放 | 不输出字符，不提交或清除编码；Alt 组合键仍有效 |
| Alt 加印有 1–5 的键 | 选择当前页第 1–5 项 |
| 左右滑动候选栏 | 浏览全部候选，不提交、不改变键盘高亮 |
| 点击候选 | 按列表的真实索引提交，包括第 6 项以后，返回编辑框 |
| Shift+空格、`[` / `]` | 按每组 5 项移动高亮；不再保留屏幕翻页按钮 |
| 退格 | 删除一个编码；无编码时删除正文 |
| Alt/Shift+退格 | 取消编码 |
| Enter | 有编码时输出原始字母，否则换行 |
| Shift+Enter | 确认高亮项并换行 |
| Alt+Enter | 在当前中文方案与 English 之间切换 |
| 屏幕取消按钮 | 取消编码并返回编辑框 |
| 右上角状态/菜单按钮 | 输入启用时显示“中 / EN”，暂停时显示菜单图标；点击只切换菜单栏与输入开关 |
| 菜单显示期间 | 不输入文字/编码，不执行自建选词、Sym 或 Shift；不切到系统输入法 |
| Sym 实体键 / 屏幕 Sym | 首次打开符号网格；每次推进一组，一轮结束后关闭并返回原输入状态 |
| 符号网格 | 中文、English、数学三组，各 26 项，按 Q10 三行字母键排列；触屏或对应字母键选择 |
| 符号弹窗内退格 / Escape | 关闭弹窗，保留原编码和正文 |
| 顶部“全拼” | 使用同一解码引擎比较全拼 |
| 菜单“自检” | 在临时文本框测试解码、提交、重复键和选区替换 |

零声母包括 `aa/a`、`oo/o`、`ee/e`、`ai`、`an`、`ao`、`ou`、
`ah/ang`、`eg/eng`，兼容 `al/ai`、`aj/an`、`ak/ao`、`ob/ou`。
支持中文标点、Alt 数字/符号和半个音节。Sym 使用应用内符号面板，
不是调用原生输入法面板；尚未实现长按变音符。
符号面板用十列、三行定位字母键，保留退格、Alt、`$` 和 Enter 对应的空位；
空位不选符号，不把后面的键向左挤。每个符号仍对应同一个实体字母。
符号字母索引按 `qwertyuiopasdfghjklzxcvbnm` 排列。打开/关闭面板不提交编码；
再次按 Sym 轮换下一组；回到本轮起始组前关闭面板，不提交编码，长按不连续轮换。
中文起始顺序为中文、English、数学、返回；英文起始为 English、数学、中文、返回。
退格、Escape 或关闭按钮取消面板，不清空编码。
选择符号时先确认高亮候选，再插入符号，关闭面板并返回编辑框。
这里明确列出的规则不是对原生输入法所有细节完全一致的承诺。

## 测试范围

优先测试连续打字、选词后接着输入、逐码退格、Alt 选词、长按空格、切换英文、
触屏移动光标后输入、替换选区、切入后台再返回。

2026-10-01 设备验证：`10.3.3.3216`，32 位 ARM；
词库自检和合成 KeyEvent 测试均 PASS，词库包含 413 个拼写项。
主机回归覆盖其中 411 个双字母映射；独立 `m/n` 不是完整双字母音节。
`0.1.0.2` 的第二轮 40 次合成查询 P50 为 1.007 ms、P95 为 4.029 ms、最大值为 16.115 ms。
`0.1.0.8` 已确认解码、按键、光标、位置换算、开关和符号轮换合成自检全部通过，
并观察到 7 次真实编码更新，证明菜单隐藏后的自建输入路径已恢复。
这些数据只测同步解码，不包含物理按键、Cascades 渲染或屏幕刷新。
实体键盘手感、完整 Sym 行为、原生词库质量对等、长时间稳定性仍需人工验收。

首版 `0.1.0.1` 缺少 `QtQuick 1.0` 导入，运行时报
`Connections is not a type`，以退出码 2 结束。
`0.1.0.2` 已补导入、覆盖安装，手机确认界面加载及两轮自检成功，进程持续存活。
详细证据见 [测试记录](C:/Users/dove1/Documents/BBIME/research/APP_TEST_REPORT.md)。

## 构建与安装

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\dove1\Documents\BBIME\tools\Test-Decoder.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\dove1\Documents\BBIME\build.ps1 -Package
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\dove1\Documents\BBIME\tools\Deploy-Q10.ps1
```

构建沿用本机 BBarmin SDK 和 BB10 `4.6.3,gcc_ntoarmv7le_cpp`，
检查 ARM ELF 使用 `libcpp.so.4` 而非 GNU `libstdc++`。
安装只使用现有 Q10Manager SSH 配置和固定主机记录，核对 BAR 回读哈希和 PPS 安装结果。
安装后从手机图标启动。无需电脑在线或 SSH 参与每次输入。

语法检查可以使用现有官方 SDK 解析脚本：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\dove1\Documents\BBarmin\bb10-native\verify_qml_syntax.ps1 -Paths C:\Users\dove1\Documents\BBIME\assets\main.qml
```

语法检查不检查 QML 类型导入和设备运行状态；必须另查启动日志。
手机私有 `logs/log` 包含启动阶段和测试结果，不写入输入正文。

## 数据与限制

### 模块设置与共享边界

菜单“设置”保留启动方案、单按 Shift 的候选/光标行为、中文标点和本应用词频学习。
关闭设置页不自动开启输入，仍使用右上角按钮恢复。
设置独立保存于私有 `data/module-settings.ini`；读取失败时保留默认值或已加载值。
启动方案在下一次启动生效，不修改当前中英文状态；行为开关在恢复输入后生效。

“发布共享设置 / 读取共享设置”使用
`shared/documents/BBIME/module-profile.ini`，需要用户授予 `access_shared`。
读取方应用也需要共享目录权限及模块接入；同名 QSettings 不会跨沙箱自动共享。
仅交换版本化的行为偏好，不含个人学习授权、正文、按键、光标、候选或菜单开关。
默认不发布、不自动跟随共享文件；未知字段、版本及非法值整体拒绝。

基础词库仍来自包内；个人词库仍每应用私有。本版没有多应用共写词库，
不能调用未审计的上游同步入口来冒充已经支持词库共享。
解码器只允许本进程一个 owner，并要求调用方在一个输入线程串行使用。
自检不再更新真实词频，关闭学习也不删除已有学习。
完整审查和接入边界见
[模块可靠性与共享报告](C:/Users/dove1/Documents/BBIME/research/MODULE_RELIABILITY_AND_SHARING.md)。

### 输入数据与编辑限制

草稿保存于应用私有 `data/draft.txt`，用户词库为 `data/userdict.dat`。
`data/startup-diagnostics.ini` 保存合成测试和自检结果；
`data/typing-diagnostics.ini` 保存解码次数及最近 512 次查询的 P95，不含正文或原始按键。
输入正文最长 16384 个 UTF-16 单元，自然码最长 32 个字母。
底部 ActionBar 和自建输入法共享一个开关状态：隐藏时启用，Visible 时暂停。
没有输入活动计时或停顿恢复菜单；右上角保留 76 px 宽的状态/菜单按钮。
输入启用时显示“中 / EN”，暂停时显示菜单图标，点击始终只切换菜单栏和输入开关。
原独立中英文按钮已合并显示，中英文切换使用 Alt+Enter 或顶部方案选择。
全程使用 TextArea Custom，不切到 Text 或系统输入法；“系统”方案入口已删除。
显示菜单时应用不处理文字按下/释放事件，Custom 编辑框也不进行原生键盘输入，
不执行选词、应用 Sym、Shift 光标/候选操作或 Alt+Enter。
中英、取消、Sym、方案选择和候选触屏入口同步禁用，后端也有关闭保护，
不能用模式切换绕过总开关。复制、清空和自检仍是普通菜单操作。
通过原生手势展开操作菜单同样暂停自建输入法，收起菜单不会自动重新启用。
切换取消未提交编码和符号弹窗，清除按键状态，但保留正文、光标和选区。
自然码、全拼、English 方案在暂停期间保持原值，恢复时不重新选择模式。
顶部取消、Sym、状态/菜单按钮常驻，和候选滑动区域位于不同的行，
不会覆盖候选或随候选一起滚动。
候选只保留 72 px 高的单行横向列表，最多展示全部 40 项；文本不再人为截为 3/7 字。
每项按词长分配宽度，不超过候选视口；较长文本使用原生 FitToBounds 缩小字号。
Shift 或键盘翻组时用原生 Smooth 滚动跟随高亮；滑动浏览时不自动拉回。
候选只显示词语，不显示 1–5 序号。Alt+1–5 仍对应高亮所在的五项组，
单纯触屏滚动不改变该组。
单独 Shift 在释放时处理，超过 500 ms 的长按、与其他按键组合、双 Shift 同时按下
均不移动候选或光标，防止干扰 Shift+空格、Shift+Enter 和英文大写。
无编码时已有选区的左/右 Shift 分别收起选区到起点/终点；正文两端不越界，
移动时不落到 UTF-16 代理对中间。符号弹窗和暂停状态不执行应用的 Shift 光标操作。
`0.1.0.8` 用临时文本框探测原生光标单位，再与 QString 的 UTF-16 索引换算，
光标、退格、选区替换、全选和复制共用该换算，避免含 emoji 时跳过末字或选错范围。
原生插入光标由 TextEditor 移动并返回编辑焦点，触屏长按行为保留。
公开 SDK 未找到直接唤出长按圆形精细定位浮层的接口，本版未将其绑定到 Shift；
普通插入光标移动不能冒充圆形浮层已接入。
上述左右 Shift 行为依赖固件向 KeyListener 发出可区分两侧的事件，需实体键验证。

受上游引擎的矩阵上限约束，展开全拼最长 39 字节，先到的上限生效。
最多返回 40 项候选和 8 条歧义展开路径，不包含原生联想、模糊音和完整个人整句学习。

开发调试可放置私有 `data/layout-capture.once` 请求一次本应用窗口截图；
仅在启动后、正文及编码均为空且应用处于前台时执行，不截取其他应用。
`data/ui-layout.ini` 只记录各行控件的坐标与大小及输入法/菜单状态，用于检查单行候选布局是否放得下，
不包含正文。Shift 观察也仅记录左右键按下次数、候选及光标移动次数，不记录字母按键。
Alt/Sym 也仅保存实体键按下计数，不保存原始键值或字符。
本版构建、安装和设备验收状态以最新测试记录为准；
自动解码测试不能替代触屏滑动、菜单和 Sym 焦点的实机验收。
`0.1.0.7` 已观察到词库通过、光标自检失败，旧逻辑因此错误停用全部中文输入。
新版词库可用性只由载入和解码自检决定；交互自检失败仍记录 FAIL 并显示诊断提示，
不会将无关的光标测试失败改报为“词库自检失败”或停用已通过检查的解码器。
键位与菜单策略的依据见 [交互研究](C:/Users/dove1/Documents/BBIME/research/KEYBOARD_INTERACTION.md)。

## 来源

- 解码器：libgooglepinyin `0.1.2`，Apache-2.0；固定容量池的本地修改、
  源码和许可证保存在 `vendor`，修改记录为 `BBIME-MODIFICATIONS.md`。
- 源码归档：Debian `libgooglepinyin_0.1.2.orig.tar.bz2`。
  SHA-256：`1A339AE45721A60B9FADD15E43C34B9BB27AF3BB999C00ED0D88B4084CFD0637`。
- ARM 字典：Debian `libgooglepinyin0_0.1.2-8_armhf.deb`，采用其 32 位小端字典。
  归档 SHA-256：`157295DA0BB612DDF18486D5529CBA7B8431E88D542D77360B5DC67E3B48EBB2`。
- 包内字典 SHA-256：`179311C55AB9B912A07EF040E5A95AF97E704E2248B7B97735F17A3E05D13B67`。
- 自然码映射核对 Rime `double_pinyin.schema.yaml`，仅参考规则，不打包 Rime 引擎。
- 工具图标复用相邻 BBFile 项目的位图素材。

字典格式含原生 `size_t`，主机测试使用重新生成的 64 位字典；
不能将它覆盖到 ARM 应用的 `assets/dict_pinyin.dat`。
上游包含旧代码编译警告；本轮没有审计整个第三方引擎的所有未调用功能。
