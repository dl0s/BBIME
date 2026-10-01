# Q10 键位与菜单交互研究

日期：2026-10-01。当前实现版本：`0.1.0.9`。
范围：本应用的 Custom TextArea，不修改全局键盘。

## 当前策略

用户实测动态隐藏/恢复菜单存在问题，`0.1.0.4` 已撤销该策略。
`0.1.0.7` 按最新要求改为右上角仅图标的 76 px 总开关。
`0.1.0.9` 保持同一固定宽度和点击职责：启用时只显示“中 / EN”，
暂停时只显示菜单图标，不同时显示图标和文字。
移除原独立中英文按钮；Alt+Enter 和顶部方案控件仍用于切换中英文。
按钮文本直接绑定后端 mode 和 imeEnabled，不维护独立显示状态。
唯一开关状态为后端 m_imeEnabled，QML 不维护第二份菜单可见性状态；
开关与 natural/full/english 方案独立，暂停时保留方案，不再设置 system 模式。
ActionBar 严格绑定为开启时 Hidden、关闭时 Visible，不使用覆盖正文的 Overlay。
自动隐藏保持 Disabled，菜单不会因输入停顿、滚动或后台返回自行恢复/切换。
没有输入活动标志、菜单恢复计时器或 typingChanged 信号。
正文草稿和诊断的去抖写盘计时器独立保留，不控制菜单。

### 手动切换的关闭保护

TextAreaInputMode 全程保持 Custom，不切换 Text，不启用系统文字输入或预测。
删除原有“系统”方案，setMode 也不接受 system 请求。
开关不修改编辑控件的 inputMode 或 text，因此正文、原生光标和选区不被重建。
暂停会取消未提交编码，不沿用 setMode 的 literalComposition 插入字母逻辑；
同时关闭应用符号面板，清除按键按下表、待处理 Shift 动作和应用选区锚点。
handleKey 在接受事件或写入按键状态前即返回 false，因此暂停期间
字母、空格、删除、Enter、Alt、Sym、Shift、方向键和 Ctrl 组合不进入自建输入路径。
官方 SDK 明确说明 Custom 忽略键盘文字输入，须由应用自行处理；
本版暂停时不处理这些事件，且不切回 Text，所以不会把文字交给系统输入法。
普通菜单命令和触屏原生文字选区仍可使用，不等于启用系统输入法。

候选点击、候选移动/翻组、符号选择/分组/打开和中英文切换均有后端保护，
屏幕入口也同步禁用。菜单可见时对 natural/full/english 的 setMode 请求不改变方案或开关，
必须通过右上角总开关恢复；system 始终是无效请求。
暂停不改变当前应用方案，包括 English，同时独立保留最近中文方案。
原生侧滑操作菜单开始展开时也走关闭路径；菜单收起后保持关闭，等待手动开启。
复制、清空和诊断仍可使用，不把普通编辑菜单视为应用输入法功能。

设备合成自检新增 GATE：关闭期间所有按下/释放均不接受，
未提交编码/面板/按键状态清理，正文与选区不变，
延迟候选和符号回调拒绝、Shift 残留释放、暂停后输入恢复、
三种应用方案保持、暂停状态在后台往返时保持，以及全程 Custom。
日志中的 PAUSED 表示事件未被自建输入处理且控件仍为 Custom，不再称为 NATIVE。
自检隔离 changed/metricsChanged 等 UI 信号和草稿写盘，结束后恢复原有总开关与模式记忆。
实际状态按钮、两种菜单状态下的布局及暂停期间无文字输入仍以新版实机结果为准。
`0.1.0.6` 的 CURSOR 合成测试实机返回 FAIL，本版非选区移动改为
TextEditor.setSelection(target, target)，明确收起旧选区并定位光标；
同时新增只记录合成光标位置的 CURSOR_CHECK 步骤日志，便于进一步定位设备差异。

候选使用原生水平 ListView、StackListLayout 和 ArrayDataModel，只保留一行。
顶部按钮不在列表内，不占候选宽度，也不覆盖候选。
列表包含全部候选，触屏点击使用全局索引；键盘 Alt+1–5 保留五项组语义。
`0.1.0.5` 删除全部候选的序号前缀，只显示完整词语，并减少每项预留的编号宽度。
滑动只浏览，不自动选中或提交。新编码刷新数据并回到起点；
Shift/翻组只改变高亮，不重建模型，以 Smooth 滚动让选中项进入视口。
按词长分配宽度，上限为列表视口，完整文本交给 LabelTextFitMode.FitToBounds，
不再使用旧版的人工省略号。长词实际字号和动画效果需实机验收。

### 输入未恢复的原因及修复

实际读取 `0.1.0.7`：ime_enabled=true、action_bar_visible=false，
解码 SELFTEST=PASS、菜单 GATE=PASS，但 CURSOR/KEYS=FAIL。
失败步骤为 right_bound，QString UTF-16 长度为 5，原生光标末端实际为 4。
原代码混用两种索引，并把任意交互自检失败写入 m_ready=false；
handleKey 因 m_ready=false 吞掉中文输入。这解释了菜单隐藏仍无法输入，
不是菜单开关已经通过后再次调用系统输入法。

`0.1.0.8` 在临时 Custom TextArea 中用固定合成文本探测原生索引单位，
允许原生 Unicode 码点或 UTF-16 两种实现，然后用共用函数与 QString UTF-16 换算。
换算覆盖光标移动、退格、插入时的选区长度、全选及复制；
原生快照仍保留原生索引，避免撤销恢复时再次换算。
新增主机测试覆盖往返、边界、成对及孤立代理项；
设备测试增加从末尾左移再右移、整段 emoji 删除及长度上限内替换 emoji。
不删除原来的光标测试，新增 POSITIONS 分组和 CURSOR_UNITS 探测日志。

词库载入或解码自检失败仍阻止中文解码；
交互测试失败则保留 KEYS=FAIL、诊断和提示，但不将已通过的解码器设为不可用。
菜单隐藏/显示仍只由 m_imeEnabled 控制，全程仍为 Custom。
后续设备日志已确认 `0.1.0.8` 探测为 CODE_POINTS，全部合成分组及总 KEYS 通过，
decoder_ready=true，另观察到排除合成测试的 7 次编码更新。
这验证了解码路径恢复，不替代实体 Sym 轮换及完整菜单往返的视觉验收。

### Alt 与 Sym

旧路径直接将 KeyEvent.unicode 送入插入函数，没有先过滤修饰键或私用区键符号，
因此存在将 Alt 的非文字键值当作字符写入正文的风险。
新版先拦截独立 Alt、Ctrl、CapsLock，再过滤其他私用区字符。
Alt 单按不会插入字符、提交候选、清除编码或切换语言；原有 Alt 数字、符号和 Enter 组合保留。

读取本机固件只读键盘定义，未修改文件：

- `/etc/system/config/keypad_bb35.conf` 将 Sym 标为 HID `0x71`。
- SDK `sys/usbcodes.h` 将 `0x71` 定义为 F22。
- `/usr/photon/keyboard/QWERTY_bb35.kbd` 的偏移 `0x184c` 为
  `71 00 02 d3 f0 00 00`，将该键映射到 QNX 键符号 `0xf0d3`。
- 新版匹配 `KEYCODE_F1 + 21` 和 Qt F22；不能把原始 HID `0x71`
  当作 Cascades keycap，否则会错误拦截普通字母 q。

这证明固件有该映射，不证明当前中文 IMF 必定把 Sym 事件发送给 Custom TextArea。
屏幕 Sym 按钮使用同一面板，作为独立入口。实体观察只有按下计数，不记录用户正文或原始按键。

面板为本应用的 Cascades Dialog：中文、英文、数学各 26 项。
`0.1.0.5` 从六列改为与 Q10 字母键对齐的十列三行：

```text
q w e r t y u i o p
a s d f g h j k l _
_ z x c v b n m _ _
```

`_` 是功能键的预留空位，不显示符号，也不接收选符号动作。
它们分别保留退格、Alt、`$`、Enter 所在列，不改变原有 26 个字母与符号的索引关系。
显示模型共有 30 个网格位置，触屏使用每项的 symbolIndex，不能再将网格位置直接作为符号索引。
可触屏选择或用 `qwertyuiopasdfghjklzxcvbnm` 对应字母选择；
`0.1.0.8` 首次 Sym 打开面板，中文方案从中文组开始，English 从英文组开始。
`0.1.0.9` 改为单轮：中文 → English → 数学 → 关闭返回；
英文起始则为 English → 数学 → 中文 → 关闭返回。
记录本轮起始组，下一步将返回起始组时关闭，而不是无限绕回第一页。
每次重新按下只轮换一组，长按重复事件仍被去重。
轮换过程中只更新符号组，轮换结束关闭弹窗并恢复编辑焦点；
不切换原中英文方案，不提交或清空编码，不改变正文与光标。
提前关闭、选定符号、暂停输入或后台切出都会结束本轮；
下一次 Sym 从对应输入模式的起始组开始，不保留旧轮次。
退格、Escape 或关闭按钮取消面板，不清空编码。
选择符号时先确认当前高亮候选，必要时处理剩余编码，再插入精确符号。
长度上限拒绝插入时保持面板打开，不静默丢失原编码。
暂停模式不执行自建实体按键处理，编辑框仍为 Custom，不交给系统输入法。
合成自检不打开真实弹窗，也不重建真实候选模型，结束后还原状态。

### 原生长按定位浮层

用户希望 Shift 移动光标时同时显示系统长按后的圆形精细定位控件。
核对 SDK 的 TextEditor、TextArea、AbstractTextControl、Control、
TextInputFlag、ContextMenuHandler 和 SystemShortcuts：
公开 TextEditor 提供 setCursorPosition 和 setSelection，未找到公开的显示/隐藏
FineCursorHandle 或进入圆形定位模式的接口。ContextMenuHandler 只有 closeMenu，
没有可用于此目的的 openMenu 或唤出定位控件接口；导航高亮也不是文字定位控件。

SDK libbbcascades.so.1 包含内部 FineCursorHandle、EnterSelectionModeAtCaret 等名字，
说明内部存在该机制，但它们不是 TextArea 的已声明可调用 API。
WebView 专用的 fine cursor 控制不能视为原生 TextArea 的入口。
没有调用猜测的私有槽、修改系统输入服务、注入全局触控或绘制仿制圆形控件。

本版用已公开的 TextEditor.setSelection(target, target) 移动并收起原生插入光标，随后 requestFocus；
原生触屏长按保持可用。圆形定位浮层的程序化唤出未完成，不能用插入光标、自检 PASS
或内部字符串存在作为实现成功的证据。需要后续独立验证可靠的控件内部调用路径。

## 历史菜单实验（已撤销）

用户实测 `0.1.0.2` 输入流畅，但底部菜单占用空间后部分界面不可见。
旧界面同时包含原生标题栏、方案选择、至少 160 px 高的编辑框、
预编辑、两行候选和状态行，在 Q10 正方形屏幕上固定高度过多。
没有截图，不能精确区分旧版本所有遮挡位置；以下同时处理空间预算和菜单生命周期。

官方 `Page` 头文件说明：默认和 Visible 菜单会压缩页面内容；
Overlay 才会覆盖内容，且应额外预留底部空间。
`ActionBarAutoHideBehavior.HideOnScroll` 根据主滚动控件的滚动活动工作，
不是“用户停止打字”的检测接口，因此本轮不依赖它实现输入停顿恢复。

`0.1.0.3` 当时采用如下策略，现已全部移除：

1. 按键或正文修改开始时将菜单设为 `ChromeVisibility.Hidden`。
2. 每次输入重置本机单次计时器；停顿约 1000 ms 后恢复 `Visible`。
3. 正文、光标、选区、未提交编码和高亮候选都不会因计时器恢复而清空。
4. 菜单打开时维持可见，不让计时器或候选活动强行隐藏已打开菜单。
5. 后台/休眠停止计时并复原状态；系统输入模式只观察活动，不消费原生按键。
6. 顶部常驻中英、取消、工具菜单按钮，菜单隐藏期间仍有操作入口。

原生高标题栏改为 64 px 头部，编辑框最小高度改为 88 px，
两行候选固定 72 px，长候选显示截断文本但提交完整内容。
`LayoutUpdateHandler` 上报相对父控件的边界，保存于私有 `ui-layout.ini`。
检查各行是否互不重叠、是否在实际内容高度以内；这是几何检查，不替代字体与触屏截图验收。

## 左右 Shift

SDK 的 `KeyEvent` 提供 keycap、key、按下/释放、duration 和 Shift/Alt/Ctrl 修饰状态。
QNX `sys/keycodes.h` 分别定义 `KEYCODE_LEFT_SHIFT`、`KEYCODE_RIGHT_SHIFT`。
因此 API 层有区分两侧 Shift 的表示能力，
但这不证明当前 Q10 的 keyboard-imf 一定把独立 Shift 事件送到本应用。

本轮选用“前后移动高亮候选”，不是按一下就直接提交：

| 情况 | 行为 |
| --- | --- |
| 有编码，单独短按左 Shift | 高亮前一个候选 |
| 有编码，单独短按右 Shift | 高亮后一个候选 |
| 无编码，单独短按左/右 Shift | 正文光标左移/右移，已有正文不影响判断 |
| 无编码但有选区，单独短按左/右 Shift | 收起选区到起点/终点 |
| 越过当前页首/尾 | 自动调整页码 |
| 空格 | 确认高亮项 |
| Alt 数字或触屏候选 | 仍按所选编号/项目提交，不受高亮位置影响 |
| Shift+空格 | 保留翻页行为 |
| Shift+Enter | 确认高亮项后换行 |
| Shift+字母 | 不另行移动候选；English 大写逻辑保留 |
| 长按超过 500 ms 或同时按两侧 Shift | 不移动候选或光标 |
| English | 单独 Shift 移动光标，Shift+字母仍输入大写 |
| 符号弹窗或暂停模式 | 不执行应用的 Shift 光标/候选操作 |

仅在释放时触发；按下期间收到其他按键则取消独立 Shift 动作。
`0.1.0.5` 在按下时区分候选动作、光标动作和无动作，释放时再次检查编码与弹窗状态。
候选按下后编码被屏幕操作清空，不能在释放时误变成移动光标。
光标操作沿用 UTF-16 代理对边界保护，不重建空候选列表。
候选高亮不抢编辑焦点；`0.1.0.4` 改用列表内背景和文字颜色表示。
自检使用构造的 KeyEvent 验证应用状态机，不冒充实体键事件验证。
私有诊断中 `physical_left_shift_presses`、`physical_right_shift_presses`、
`standalone_shift_candidate_moves` 只保存计数，且排除合成测试。

手机验证方法：输入 `ni`，短按右 Shift，观察高亮是否从第 1 项移到第 2 项；
再按左 Shift 返回。接着测试 Shift+空格及 English 下 Shift+字母。
若两侧计数始终为 0，则当前控件路径未观察到支持的独立事件，
不能仅凭候选状态机 PASS 宣布实体键已支持。

## 其他键位

| 键位 | 当前选择与边界 |
| --- | --- |
| Alt+Enter | 中英文切换，与屏幕按钮共用同一函数 |
| Alt+印有 1–5 的键 | 编号选词，优先使用事件的逻辑 unicode，不能把物理字母误当编码 |
| Alt+其他符号键 | 保留印刷符号，不转换为双拼字母 |
| Shift/Alt+退格 | 取消预编辑 |
| 普通退格 | 逐码删除；无编码时编辑正文 |
| Enter | 原码输出/换行，与候选确认分开 |
| `[` / `]` | 有编码时前后翻页 |
| 空格长按 | 不重复提交候选或追加空格 |
| 双拼字母 a–z | 保留给编码，不绑定应用快捷操作 |
| Ctrl+A/C/V/Z | 软件支持；不能假设 Q10 实体键盘有独立 Ctrl 键 |
| Tab / 方向键 | API 有表示，但不作为 Q10 实体键盘必备交互 |
| Sym | 使用应用内符号网格，不调用原生 IMF 面板；`0.1.0.9` 单轮三组后自动关闭返回，亦可退格/Escape/关闭按钮提前退出 |
| Alt 单按 | `0.1.0.4` 明确消费修饰键，不输出字符；不改变原有组合键 |
| 单 Shift 切中英、双击 Shift 锁定 | 暂不新增，避免与候选移动和平台大写锁定习惯冲突 |

优先保留 Alt 的既有数字/符号职责，再给中文候选增加独立 Shift 动作。
所有快捷操作仅在本测试应用生效。

## 官方依据

- [Page 菜单可见性及自动隐藏说明](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/controls/page.h:166)
- [ChromeVisibility 枚举](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/controls/chromevisibility.h:40)
- [KeyEvent 字段与事件构造](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/core/keyevent.h:47)
- [左右 Shift 键码](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/sys/keycodes.h:136)
- [LayoutUpdateHandler 本地坐标语义](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/layouts/layoutupdatehandler.h:173)
- [Custom TextArea 限制](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/resources/textareainputmode.h:71)
- [水平 StackListLayout](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/layouts/stacklistlayout.h:110)
- [ListView.scrollToItem 和焦点说明](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/controls/listview.h:1175)
- [Smooth 滚动](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/resources/scrollanimation.h:53)
- [LabelTextFitProperties](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/controls/labeltextfitproperties.h:34)
- [Cascades Dialog](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/controls/dialog.h:24)
- [HID F22](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/sys/usbcodes.h:198)
- [TextEditor 公开接口](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/controls/texteditor.h:145)
- [ContextMenuHandler 公开操作](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/core/contextmenuhandler.h:108)

网络检索没有取得可用的旧 BB10 API 页面；以上采用实际安装的官方 SDK 头文件。
