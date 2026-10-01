# BBIME 设备测试记录

日期：2026-10-01。设备：当前 Q10，BB10 `10.3.3.3216`。

## 启动问题及修复

首次安装 `0.1.0.1` 后，设备私有日志：

```text
BBIME: starting native test 0.1.0.1
BBIME QML: asset:///main.qml:139:9: Connections is not a type
```

`devmode_exitcode.txt` 为 `2`。QML 类型导入失败导致主动退出，
没有证据支持把此次问题归因于双拼引擎内存崩溃或 C++ ABI 混用。
修复是在 `assets/main.qml` 加入 `import QtQuick 1.0`，
与已有 BBFile、BBnote 工程使用 Connections 的导入方式一致。

修复版为 `0.1.0.2`，BAR SHA-256：
`A57CE3334E9ADC0FB2DDAAAD8A55A3D3372B026D6E9F6BF3FBEF426E81114E76`。
回读哈希一致；PPS 确认版本 `0.1.0.2`、进度 100、结果 success。
新版实际启动由手机完成，不把失败的 SSH Python 启动尝试当作验证。
手机的 Python 明确限制 root 执行未受信脚本及 `-c`；
没有为启动测试修改该限制，相关未使用辅助脚本已移除。

## 手机端结果

```text
BBIME: starting native test 0.1.0.2
BBIME: scene installed
BBIME: SELFTEST=PASS KEYS=PASS synthetic_decoder_p95_ms=2.014
BBIME: READY mapped_syllables=413
BBIME: SELFTEST=PASS KEYS=PASS synthetic_decoder_p95_ms=4.029
```

后续 `pidin ar` 确認应用进程 `260731018` 仍在运行，
进程名称是安装包 ID，而不是可执行文件名 `bbime`。
因此仅以小写 `grep bbime` 查进程会漏报，不能用它判定应用已退出。
新版没有对应的退出码文件。上述为短时启动验证，不代表长时间无崩溃。

第二轮私有诊断文件：

| 项目 | 结果 |
| --- | --- |
| 版本 | 0.1.0.2 |
| 指针字节数 | 4 |
| 字典拼写项 | 413 |
| 解码自检 | PASS |
| 合成 KeyEvent 自检 | PASS |
| 合成查询次数 | 40 |
| 合成解码 P50 | 1.007 ms |
| 合成解码 P95 | 4.029 ms |
| 合成解码最大值 | 16.115 ms |
| 当时实际编码更新样本数 | 19 |
| 当时编码更新解码 P95 | 3.022 ms |

合成按键测试覆盖自然码提交、重复空格不二次提交、退格、Alt+Enter 不重复切换、
英文大小写、选区替换、Alt 数字选词、长度拒绝时保留编码和 Enter 输出原码。
测试通过直接调用应用处理函数，不经硬件、Screen 注入或 QML KeyListener 分发，
不能据此宣布实体键盘流程全部通过。

日志有 `libpng warning: iCCP: known incorrect sRGB profile`；
当时应用仍正常运行，这不是本轮启动退出的原因。
未取得应用布局截图；不声称界面像素级验收已完成。

## 主机检查

- SDK QML 解析检查 PASS，但该检查只检查语法，不校验 Connections 的运行时类型导入。
- ARM 编译和链接 PASS，依赖 `libcpp.so.4`，没有 GNU `libstdc++`。
- 回归测试 PASS：411 个双字母音节映射、零声母、别名、词组、半音节、
  前缀选择和剩余编码、退格重算、无效输入、上游矩阵上限及全拼对照。
- 不能用 Windows 主机粗粒度 CPU 计时替代 Q10 实测。

## 证据

- [0.1.0.2 安装结果](C:/Users/dove1/Documents/BBIME/research/q10-app-deployment-0.1.0.2.json)
- [初版错误](C:/Users/dove1/Documents/BBIME/research/startup-failure.json)
- [0.1.0.2 运行日志及诊断快照](C:/Users/dove1/Documents/BBIME/research/q10-app-runtime-0.1.0.2.txt)

只核对本应用私有日志、诊断和进程；没有读取或保存用户输入正文。
人工测试仍需确认连续打字、候选翻页、光标与选区、Alt/Sym、后台恢复和长时间使用。

## 0.1.0.3 更新

用户已反馈 `0.1.0.2` 输入流畅，但底部菜单挡住部分界面。
`0.1.0.3` 增加输入活动计时收起/恢复菜单，缩小固定界面高度，增加顶部快捷按钮、
左右 Shift 独立短按候选高亮和只记录边界的布局检查。
ARM 构建、QML 语法和已有解码回归通过；覆盖安装成功，回读 BAR 哈希一致。
BAR SHA-256：`2EDCF191BDA6CDA25BDAF06E961B22A3EDBC8D20ED3EB188B27206E5871B8726`。

安装后即时查询仍返回 `0.1.0.2` 的旧日志，尚无新版启动和 `ui-layout.ini`，
所以不能将旧版 PASS 当作新版菜单/Shift 测试已经通过。
SDK 的标准 BPS invocation 调试请求没有在 8 秒内收到启动确认；
未把该尝试计为成功，也未修改系统启动策略。
本版设备启动、自检、菜单可见/隐藏时的布局以及独立 Shift 实体事件仍待手机图标启动后验证。

新键位行为和官方 API 依据见
[键位交互研究](C:/Users/dove1/Documents/BBIME/research/KEYBOARD_INTERACTION.md)；
最新版安装证据见
[当前安装结果](C:/Users/dove1/Documents/BBIME/research/q10-app-deployment.json)。

### 后续读取到的 0.1.0.3 实机证据

本轮修改前再次读取本应用私有日志，已观察到 `0.1.0.3` 启动和两次
`SELFTEST=PASS KEYS=PASS`，以及左 Shift 3 次、右 Shift 4 次、独立候选移动 7 次。
这些计数排除了合成测试，支持此设备路径能区分左右 Shift。
用户实测仍反馈动态菜单存在较多问题，因此该策略撤销，不以自检 PASS 为其背书。

旧布局文件的 `modes` 行没有有效测量，`nonoverlapping_rows_fit=false`
不能单独证明每一行都发生了重叠，也不能宣称已完成屏幕验收。
保存于 [本轮观察快照](C:/Users/dove1/Documents/BBIME/research/q10-app-runtime-0.1.0.3-observed.txt)。

## 0.1.0.4 更新

取消输入活动标志、菜单恢复计时器和动态 ActionBar 绑定。
底部栏永久隐藏，顶部菜单按钮保留；两行候选改成 72 px 高的原生水平列表，
取消左右候选按钮，文本不再人工截断。
全部候选均可滑动/点击，高亮使用全局索引；键盘切换以原生 Smooth 滚动跟随。
数据仅随编码刷新，Shift 切换不重建模型；Alt+1–5 保留五项组语义。

独立 Alt/Ctrl/CapsLock 不送入文字插入路径，其他私用区键符号也被过滤。
Sym 按键映射核对当前固件和 SDK 后，增加中文/英文/数学符号弹窗及屏幕入口。
选择符号时确认高亮候选并插入符号；关闭弹窗不清空编码。
符号网格可以触屏选择或对应字母选择，关闭后返回编辑框。

扩展设备合成自检覆盖：

- 第 8 个候选的全局索引提交、无效索引拒绝、跨组高亮及新编码复位。
- 左右 Alt 和 Qt Alt 单按不插字，也不提交已有编码。
- Sym 打开/关闭、长按不重复开关、符号选择后继续输入。
- 保留编码到符号选定时再确认、中英文及数学分组、Escape 关闭。
- 正文长度上限时保留面板、系统模式的按键和 Sym 不被拦截。

这些自检在临时文本框中执行，不打开真实弹窗、不重建真实列表，也不计入实体按键计数。
代码已编译，但下方实机状态为本次验收范围的准确边界。

### 已完成

- SDK QML 语法检查 PASS；QtQuick 使用别名，Connections 保留且不与 Cascades ListView 混淆。
- ARM 编译、链接、BAR 打包 PASS，ABI 检查继续要求 `libcpp.so.4`。
- 已有主机解码回归 PASS，ARM 字典 SHA-256 未变。
- 覆盖安装 PASS，BAR 回读哈希一致，PPS 为 `0.1.0.4`、100、success。

BAR SHA-256：
`37C8F13E12AF8A67D1A9A3352B7F9BE8BEFBF587566ACF81054F1C4A35AFF62F`。

### 待实机启动验证

安装后即时读取的日志和诊断仍为 `0.1.0.3`，没有观察到新版进程，
不能把旧版 PASS 或旧版布局当作本版通过。
请从手机图标启动；重点验收菜单按钮始终可用、候选左右滑动与高亮跟随、
长候选显示、Alt 单按无字符、Sym 实体键和弹窗关闭后的焦点返回。
未取得本版截图，未验证本版触屏动画或实际 Sym 事件分发。

最新读取记录见
[0.1.0.4 安装后的读取快照](C:/Users/dove1/Documents/BBIME/research/q10-app-runtime-0.1.0.4.txt)；
该文件当前包含旧日志，文件名不是本版已运行的证据。

### 后续读取到的 0.1.0.4 实机证据

本轮构建前观察到 `0.1.0.4` 的启动日志：

```text
BBIME: starting native test 0.1.0.4
BBIME: scene installed
BBIME: INPUT_EXTENSIONS STRIP=PASS ALT=PASS SYMBOLS=PASS NATIVE=PASS
BBIME: SELFTEST=PASS KEYS=PASS synthetic_decoder_p95_ms=3.022
BBIME: READY mapped_syllables=413
```

该版本确已加载并通过设备合成自检。另有 Alt 按下 2 次、左右 Shift 各 7 次、
Sym 按下 12 次、独立候选移动 5 次；这些计数排除了合成测试。
计数证明当前控件路径观察到了这些键，不证明每次符号弹窗或滚动动画均通过视觉验收。
快照见 [本轮观察到的 0.1.0.4](C:/Users/dove1/Documents/BBIME/research/q10-app-runtime-0.1.0.4-observed.txt)。

## 0.1.0.5 更新

候选去掉全部序号前缀，只显示词语，Alt+1–5 的五项组选词逻辑保留。
符号从六列通用网格改为十列三行字母布局，保留退格、Alt、`$`、Enter 对应空位。
26 个字母映射不变；触屏点击按模型的 symbolIndex 转换，避免空位造成选错符号。
缩小弹窗高度，并分开符号和字母标签的垂直空间。

无编码时，单独短按左/右 Shift 左移/右移正文光标；
有编码时仍移动候选。已有选区分别收起到起点/终点，两端不越界，
不落入 UTF-16 代理对中间。按下时固定动作类型，组合键、长按、双 Shift、
符号面板和系统模式不执行新的光标操作，避免大写或取消编码后误移动。
合成测试补充上述情况和英文模式输入，结果输出新增 GRID、CURSOR 分组。
新增实体光标移动计数，不记录正文或原始按键。

### 原生定位显示的未完成部分

用户追加要求在移动光标时唤出系统长按后的圆形精细定位控件。
查阅本机公开 SDK 后，未找到 TextArea 的可靠显示入口。
虽然库内存在 FineCursorHandle / EnterSelectionModeAtCaret，
但不能把内部名字直接当作公开可调用接口，WebView 的对应机制也不适用于 TextArea。
本版只通过 TextEditor.setCursorPosition 移动原生插入光标并恢复焦点，
保留触屏长按；没有假冒圆形浮层已实现。
详细依据见 [原生定位浮层研究](C:/Users/dove1/Documents/BBIME/research/KEYBOARD_INTERACTION.md)。

### 构建与安装

- ARM 编译、链接和 BAR 打包 PASS，ABI 检查继续为 `libcpp.so.4`。
- SDK QML 语法检查 PASS；候选序号去除、十列网格及索引转换的源代码契约检查 PASS。
- 既有主机解码回归 PASS，ARM 字典哈希未变。
- 覆盖安装 PASS，BAR 回读哈希一致，PPS 为 `0.1.0.5`、100、success。

BAR SHA-256：
`DA40C78818290E9C9FFF0D2C070EE8231B50B797BD5BA58E402CBC42AC60A8F1`。

安装后的读取仍为 `0.1.0.4` 日志，没有观察到新版启动，
因此新增设备合成 GRID/CURSOR 自检、三行符号显示和实体 Shift 光标操作尚待验收。
请从手机图标打开新版，不将旧版 PASS 当作本版通过。
最新状态见 [交互验证记录](C:/Users/dove1/Documents/BBIME/research/q10-interaction-validation.json)，
读取快照见 [0.1.0.5 安装后快照](C:/Users/dove1/Documents/BBIME/research/q10-app-runtime-0.1.0.5.txt)；
该文件当前记录旧版日志，文件名不是运行成功的证明。

## 0.1.0.6 更新

右上角工具图标改为带“菜单 / 输入法”文字的总开关，
底部栏 Hidden 对应应用输入法启用，Visible 对应严格关闭并恢复 TextArea 原生 Text 模式。
不用输入停顿计时器，不使用覆盖内容的 Overlay。
关闭时取消未提交编码、候选及符号弹窗，清除按键和 Shift 待处理状态，
不提交原始字母，不改变已有正文、光标或选区。
重新启用恢复上一次自然码、全拼或 English；“系统”选项共用关闭路径。

关闭期间屏幕中英、Sym、取消、方案和候选入口禁用；
后端候选/符号/中英/模式设置也有独立保护，不能绕过总开关。
所有按键在 handleKey 接受事件前返回 false，原生侧滑打开操作菜单也关闭应用输入法。
复制、清空和自检保留；自检不向实际界面发布测试中间状态，也不写入临时测试正文到草稿。

设备合成 INPUT_EXTENSIONS 新增 GATE 分组，覆盖开关、模式恢复、
关闭时按下/释放及组合键放行、正文/选区保留、候选和符号的延迟回调、
Shift 残留释放及后台往返。这些测试须在 0.1.0.6 实际启动后读取结果，
不能用 ARM 编译成功或旧版本日志当作新版设备自检通过。

### 构建与安装结果

- ARM 编译、链接和 BAR 打包 PASS，继续使用 `libcpp.so.4`。
- SDK QML 语法检查 PASS；菜单/输入法单状态、屏幕禁用及自检隔离的源代码契约检查 PASS。
- 已有主机双拼解码回归 PASS，ARM 字典 SHA-256 未变。
- 覆盖安装 PASS，BAR 回读哈希一致，PPS 确认 `0.1.0.6`、100、success。
- 覆盖最新安装记录前，已另存 `0.1.0.5` 安装与交互验证记录。

BAR SHA-256：
`535C0CABB53E9D429379AA863DDCCD9913892824448956BFA12809E9C4E5F77D`。

### 实机验证边界

安装后读取的启动日志和诊断仍为 `0.1.0.4`，没有观察到 BBIME 应用进程；
`pidin ar | grep BBIME` 仅返回 grep 本身，不算应用存活。
不能把旧版 SELFTEST/KEYS PASS 或旧布局当作 0.1.0.6 的结果。
本版设备合成 GATE、自检时菜单状态不被扰动、右上角文字和两种布局均待新版启动验收。
没有读取用户草稿，也没有自动注入按键或尝试绕过系统启动限制。

从手机图标打开新版后的重点测试：

1. 输入 `ni` 后点击右上角“菜单”：底部栏显示，编码取消，正文不增加 `ni`。
2. 菜单显示期间按字母、Sym、Alt+Enter、两侧 Shift：只允许原生输入行为，
   不产生应用候选或应用符号面板，也不执行应用的独立 Shift 光标动作。
3. 点击右上角“输入法”：底部栏隐藏，恢复之前的应用模式，`nihk` 空格可继续提交。
4. 先建立正文选区再切换：正文和选区不被开关重建；长按/按住 Shift 跨切换不产生残留移动。
5. 切到 English 或全拼后关闭再开启：恢复对应模式，后台返回不自动切换开关。
6. 菜单显示期间执行“自检”：结束后仍保持系统模式和菜单可见，草稿不被测试文字覆盖。

最新证据见
[交互验证状态](C:/Users/dove1/Documents/BBIME/research/q10-interaction-validation.json)、
[0.1.0.6 安装结果](C:/Users/dove1/Documents/BBIME/research/q10-app-deployment.json)、
[安装后读取快照](C:/Users/dove1/Documents/BBIME/research/q10-app-runtime-0.1.0.6.txt)；
快照文件名只表示读取时安装的版本，不表示新版已实际启动。

### 后续观察到的 0.1.0.6

再次读取设备日志已观察到新版启动：

```text
BBIME: starting native test 0.1.0.6
BBIME: scene installed
BBIME: INPUT_EXTENSIONS STRIP=PASS ALT=PASS SYMBOLS=PASS GRID=PASS CURSOR=FAIL NATIVE=PASS GATE=PASS
BBIME: SELFTEST=PASS KEYS=FAIL synthetic_decoder_p95_ms=2.014
```

GATE 单组通过，不代表总按键自检通过；CURSOR 失败使总 KEYS 失败，
应用按原有策略将解码可用状态置为 false。不得将该版总体记为成功。
`0.1.0.6` 安装和验证 JSON 已归档，保留这次失败证据。

## 0.1.0.7 更新

按照最新要求，右上角恢复仅图标的 76 px 按钮，不再显示菜单/输入法文字。
唯一开关 m_imeEnabled 与方案模式分离，菜单 Hidden 对应自建输入启用，
Visible 对应自建输入暂停；自然码、全拼和 English 方案在暂停时不改变。

全程保持 TextAreaInputMode.Custom，删除 Text 模式切换代码和“系统”方案；
setMode("system") 也不接受。暂停期间不处理文字键，不提供原生输入替代路径，
保留普通菜单命令和触屏选区。重新开启清除残留按键/符号状态后继续自建输入。
设备合成测试用 PAUSED 取代 NATIVE，检查暂停和恢复全程均为 Custom。

针对 0.1.0.6 的 CURSOR 失败，非选区光标移动改为
setSelection(target, target)，明确收起已有选区并定位到目标；
增加只打印合成光标位置的步骤日志，不记录真实正文。
保留全部光标测试，不绕过失败或直接将结果设为 PASS。

### 构建与安装结果

- ARM 编译、链接及 BAR 打包 PASS，保持 `libcpp.so.4` ABI。
- QML 语法检查 PASS，双拼主机回归 PASS，ARM 字典哈希未变。
- 源代码契约检查 PASS：独立开关、图标固定宽度、无系统方案、
  无 Text 模式路径、暂停保护、光标选区收起及测试隔离。
- 覆盖安装 PASS，回读哈希一致，PPS 确认 `0.1.0.7`、100、success。

BAR SHA-256：
`EA3E7104EA264432854BA56BF901259559B6EA28810BCC8E6492C7C054FB2279`。

安装后实际读取的最新日志和诊断仍为 `0.1.0.6`，没有观察到 BBIME 应用进程。
该日志包含旧版 CURSOR/KEYS 失败，不能作为 0.1.0.7 的自检结果。
新版 PAUSED、GATE、CURSOR 合成测试及真实触屏按钮/布局仍待图标启动后验收。
未尝试自动启动、读取正文或注入系统输入。

新版验收重点：

1. 右上角仅图标；点击后菜单显示，当前编码取消，已有正文及选区不变。
2. 菜单显示时按字母、空格、Sym、Alt+Enter、两侧 Shift：
   不产生文字或自建候选/符号，也不启动系统输入法。
3. 再次点击隐藏菜单，恢复原有自然码、全拼或 English，
   输入 `nihk` 空格继续由自建输入法提交。
4. 菜单显示时执行自检或后台往返，不改变暂停状态，不把测试文字写入草稿。
5. 检查新日志 PAUSED/GATE/CURSOR 与总 KEYS 结果；若 CURSOR 仍失败，
   用 CURSOR_CHECK 的合成位置定位，不能把单组 GATE 通过当作全部通过。

证据见
[当前安装结果](C:/Users/dove1/Documents/BBIME/research/q10-app-deployment.json)、
[最新交互状态](C:/Users/dove1/Documents/BBIME/research/q10-interaction-validation.json)、
[0.1.0.7 安装后读取快照](C:/Users/dove1/Documents/BBIME/research/q10-app-runtime-0.1.0.7.txt)。

### 后续观察到的 0.1.0.7

用户反馈菜单隐藏后无法输入。读取本应用私有诊断确认：
ime_enabled=true、action_bar_visible=false、decoder_samples=0。
本版确已启动，进程存在，但日志为：

```text
BBIME: starting native test 0.1.0.7
BBIME: CURSOR_CHECK right_bound expected=5 actual=4 start=4 end=4
BBIME: INPUT_EXTENSIONS STRIP=PASS ALT=PASS SYMBOLS=PASS GRID=PASS CURSOR=FAIL PAUSED=PASS GATE=PASS
BBIME: SELFTEST=PASS KEYS=FAIL synthetic_decoder_p95_ms=2.014
```

旧代码将光标测试失败一并置为 m_ready=false，导致通过检查的词库被停用。
按键仍被消费，但中文编码不进入解码器。开关本身的 GATE 测试通过不能发现此全局状态问题。
本次失败证据保存在
[0.1.0.7 实际运行快照](C:/Users/dove1/Documents/BBIME/research/q10-app-runtime-0.1.0.7-observed.txt)，
安装及验证 JSON 已按版本归档，不把旧版日志作为新版结果。

## 0.1.0.8 更新

保留图标开关和全程 Custom；修复原生索引与 QString UTF-16 混用：
启动合成测试用临时文本框探测原生位置单位，再统一换算光标、退格、
选区长度、全选和复制。保留原有光标测试并增加末尾往返、emoji 删除、
长度上限内用两字替换 emoji 的 POSITIONS 分组。

修正词库可用性判断：只有载入或解码自检失败才停用解码器。
交互失败仍输出 KEYS=FAIL 和诊断提示，不清除失败记录，也不停用已通过的词库。
这不是把失败改成通过；本版设备测试结果须另行读取。

Sym 首次打开，之后循环中文、English、数学三组；长按不连续轮换。
轮换保持弹窗、编码及正文，退格/Escape/关闭按钮仍可关闭，暂停状态仍禁用应用 Sym。
补充循环回首组、带预编辑轮换、英文组起始和数学符号选择的设备合成测试。

### 构建与安装

- ARM 编译、链接及 BAR 打包 PASS，继续使用 `libcpp.so.4`。
- QML 语法检查 PASS；开关、Custom 限制、Sym 轮换和可用性判断源代码契约 PASS。
- 主机双拼回归及新增光标索引换算测试 PASS，ARM 字典哈希未变。
- 覆盖安装 PASS，BAR 回读哈希一致，PPS 确认 `0.1.0.8`、100、success。

BAR SHA-256：
`0D8F0CA038E119310380A1F77A772F179422F5569AC1FABB64FCDC7153AF71E1`。

### 已确认的设备运行结果

本次读取已确认实际启动和诊断均为 `0.1.0.8`，应用进程存在：

```text
BBIME: starting native test 0.1.0.8
BBIME: CURSOR_UNITS=CODE_POINTS probe_end=3
BBIME: INPUT_EXTENSIONS STRIP=PASS ALT=PASS SYMBOLS=PASS GRID=PASS CURSOR=PASS POSITIONS=PASS PAUSED=PASS GATE=PASS
BBIME: SELFTEST=PASS KEYS=PASS synthetic_decoder_p95_ms=3.022
BBIME: READY mapped_syllables=413
```

固定合成文本的探测确认此设备 TextEditor 使用码点索引。
诊断中 decoder_ready=true、ime_enabled=true、input_mode=natural、
editor_input_mode=Custom；不再因先前的末端测试失败停用中文输入。
40 次合成解码 P95 为 3.022 ms，指针为 4 字节，字典映射 413 项。

另已观察到 7 次真实编码更新，当前解码 P95 为 1.007 ms；
左右 Shift 分别按下 4/3 次，候选移动 7 次。这些计数排除合成测试，
支持自建输入与候选键已在实际 KeyListener 路径恢复。
计数不记录正文，不等于完整连续输入、提交内容或屏幕动画已视觉验收。

读取时 physical_sym_presses 和 symbol_group_cycles 均为 0，
因此 Sym 循环状态机的设备合成检查通过，但不能声称本版实体轮换/面板显示已人工通过。
仍需确认实际菜单往返、Sym 循环、关闭后继续输入和长时间稳定性。

证据：
[0.1.0.8 运行快照](C:/Users/dove1/Documents/BBIME/research/q10-app-runtime-0.1.0.8.txt)、
[当前交互验证状态](C:/Users/dove1/Documents/BBIME/research/q10-interaction-validation.json)、
[当前安装结果](C:/Users/dove1/Documents/BBIME/research/q10-app-deployment.json)。

## 0.1.0.9 更新

右上角 76 px 按钮在自建输入启用时显示当前“中 / EN”，暂停时显示菜单图标，
两种内容不同时显示。点击始终只切换菜单栏和自建输入开关，不执行语言切换。
移除原独立中英文按钮，Alt+Enter 和顶部方案选择保留。
状态直接绑定后端 mode/imeEnabled，不引入另一份 UI 状态。

Sym 从无限循环改为单轮：
中文起始为中文、English、数学、返回；英文起始为 English、数学、中文、返回。
返回时关闭弹窗、恢复编辑焦点，保留原输入方案、正文、光标和未提交编码。
提前关闭、选定符号、输入暂停或后台切出清理轮次；下一次从当前输入方案的起始组开始。
长按导致的重复事件不能在一轮结束后又立即打开下一轮。

设备合成测试补充中文/英文完整退出、带预编辑及光标的状态保留、
结束键长按去重和新一轮正确起始。
自检仍在临时 TextArea 中运行，恢复原有符号起始组，不扰动实际弹窗。

### 构建与安装结果

- ARM 编译、链接和 BAR 打包 PASS，保持 `libcpp.so.4` ABI。
- QML 语法、已有解码及索引换算回归 PASS，ARM 字典哈希未变。
- 状态按钮及单轮符号源代码契约检查 PASS。
- 覆盖安装 PASS，BAR 回读哈希一致，PPS 确认 `0.1.0.9`、100、success。
- 最新记录覆盖前已归档 `0.1.0.8` 的安装和交互验证证据。

BAR SHA-256：
`64A98E430C6D7F8289875AD406F2E2278253E3BFD0BF2798923A4C4661C1AA2D`。

安装后读取仍为 `0.1.0.8` 日志与诊断，没有观察到应用进程。
不能将旧版全部 PASS 当作本版按钮显示和单轮退出已通过。
新版 SYMBOLS/KEYS 自检、按钮文字/图标切换及实体 Sym 退出后的焦点仍待图标启动验收。

验收重点：

1. 自然码/全拼显示“中”，Alt+Enter 或 English 方案显示“EN”；
   点击这个按钮只显示菜单并暂停输入，不切换语言。
2. 暂停时按钮只显示菜单图标，再点击隐藏菜单，恢复之前的“中 / EN”。
3. 中文模式按 Sym 四次：中文、English、数学、退出，回到原输入方案。
4. English 模式按 Sym 四次：English、数学、中文、退出，保持英文输入。
5. 有未提交编码时完成一轮：编码、正文、光标保持；退出后可继续输入或空格确认。
6. 按住最后一次 Sym 不重新打开下一轮；释放后再按才开启新一轮。

证据：
[0.1.0.9 安装后快照](C:/Users/dove1/Documents/BBIME/research/q10-app-runtime-0.1.0.9.txt)、
[当前交互验证状态](C:/Users/dove1/Documents/BBIME/research/q10-interaction-validation.json)、
[当前安装结果](C:/Users/dove1/Documents/BBIME/research/q10-app-deployment.json)。
