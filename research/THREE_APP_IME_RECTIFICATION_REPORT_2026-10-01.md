# BBnote、IntroOP、BBFile 输入法整改报告

日期：2026-10-01（Asia/Shanghai）。审查对象：当前本地源码、固定模块快照、本地 BlackBerry 官方 SDK、已有验证记录，以及用户提供的实机现象。

**历史审查基线：本文记录修复实施前的状态。** 用户随后授权直接修复，基座和三个应用已修改；
当前完成情况、验证与剩余验收见 [修复实施结果](THREE_APP_IME_FIX_RESULTS_2026-10-01.md)。

**结论：三个应用的问题分为共享原生组件缺陷、BBnote 正文桥功能缺失和必须实机定位的 Sym 卡死。** 已确认 BBnote 正文没有实现左右 Shift 单按及 Sym；共享原生控制器缺少裸修饰键过滤；右上开关的固定尺寸约束与 SDK Button 行为冲突；三个宿主均未实现完整的焦点自动启停。BBnote 候选字体及占位存在独立实现差异。IntroOP / BBFile 的 Sym 卡死由用户实测确认，当前证据不能确定其运行时根因。

本轮交付整改方案、修改范围、证据和验收条件；运行代码修改、新功能落地、重新打包及部署尚未执行。下文将“源码已确认”“用户实测”“待运行定位”分别标明，不将既有编译或合成事件 PASS 当作修复完成。

## 1. 审查基线及三个应用的实际接入

| 项目 | 当前源码版本 / HEAD | 接入形态 | 本轮核对 |
| --- | --- | --- | --- |
| BBIME | 0.1.0.12 / `b3ec5e02132f24f560d929889c62918a89b9cbc0` | 共享核心、原生适配器、控制器和 QML 组件 | 开始审查时工作区干净；源码契约检查 24 项 PASS |
| BBnote | 1.0.0.8 / `2871cccd9d70c0159412a808025a1f9c2129c6be` | 原生字段 + 独立 CodeMirror 正文异步桥 | 工作区干净；0.1.0.12 快照及 index 验证 PASS，54 导入文件、4 接入输入 |
| IntroOP | 0.1.0.21 / `d95133150bddfbca4aec97bf5cc49dd37d09df45` | 原生 TextField / TextArea，宿主统一键路由 | 工作区干净；0.1.0.12 固定导入验证 PASS：58 文件；现有 JS 接入测试 PASS |
| BBFile | 1.0.0.18 / `15d3050d2be07ade7041c79af465d18231acbff5` | 原生字段 + 宿主 Literal / 焦点恢复补丁 | 导入清单 60 个目标文件 SHA-256 一致；工作区另有两项非 IME 改动，未触碰 |

三个宿主当前均以 BBIME 0.1.0.12 / `b3ec5e0` 为来源，但宿主补丁、资源转换、正文桥与字段白名单不同，不能因模块版本相同就推定表现相同。本文版本是本地源码版本；用户本次实测所用已安装包的哈希未重新读取，需要整改验收时对齐。

此前 [两款宿主检查](C:/Users/dove/Documents/BBIME/research/HOST_INTEGRATION_REVIEW_2026-10-01.md) 中 BBFile 1.0.0.17 / 模块 0.1.0.11 及早期 BBnote 未提交的描述属于历史快照，不能用于描述本轮当前状态。关键文件哈希和本轮检查范围另存于 [审查证据](C:/Users/dove/Documents/BBIME/research/three-app-ime-rectification-evidence-2026-10-01.json)。

| 应用 / 字段 | 当前输入通道 | 需要保留的接入规则 |
| --- | --- | --- |
| BBnote 搜索、路径、新建文件夹、重命名 | `ImeTextField` → `NoteIme` → `NativeController` | search/path 不学习，保留 ASCII 标点 |
| BBnote URL、账号、密码 | 宿主 literal 直写，密码遮罩 | 不纳入中文解码，不因自动聚焦扩大白名单 |
| BBnote Markdown 正文 | WebView 实体键桥 → CodeMirror → 异步提案 → 原文档事务 | 同一服务串行解码；保留选区、撤销及保存等待；需独立补齐 Sym / Shift |
| IntroOP 34 个编辑字段 | 原生 Custom 字段 → `IntroOPIme` 统一路由 | 继续使用既有字段策略，数值及字面量不自动进入中文解码 |
| BBFile 地址、搜索、名称、正文 | 原生 `NativeController` | 沿用 text/search/path 策略和唯一提交入口 |
| BBFile mode、uid、gid、ACL | 宿主 Literal 英文路径 | 保持技术字段直写，不解码、不学习 |
| 三应用列表、阅读页、设置、不可编辑区域 | 宿主浏览 / 操作通道 | 无合法编辑目标时停用应用输入，恢复适用的菜单和业务快捷键 |

BBnote 字段接入见 [main.qml](C:/Users/dove/Documents/BBnote/assets/main.qml:129)；正文入口见 [MarkdownEditor.qml](C:/Users/dove/Documents/BBnote/assets/MarkdownEditor.qml:7)。IntroOP 统一路由和合法目标判定见 [IntroOPIme](C:/Users/dove/Documents/IntroOP/bb10-native/introop_ime.cpp:44)，34 个字段分布为病例搜索 1、新病例 4、药库 4、术式设置 4、备份 5、记录及补记 / 复核 / 血压 16；自动模式必须分别保留 text、search、path 及 integer / date / time / literal 等已有字段策略。

BBFile 八字段注册见 [registerInputs](C:/Users/dove/Documents/BBFile/assets/main.qml:30)，可写 / 页面作用域检查及宿主按键入口见 [BBFileImeController](C:/Users/dove/Documents/BBFile/src/imecontroller.cpp:41)。IntroOP 当前 [focusChanged](C:/Users/dove/Documents/IntroOP/bb10-native/introop_ime.cpp:63) 和 BBFile [rememberFocus](C:/Users/dove/Documents/BBFile/src/imecontroller.cpp:29) 只记录目标；两者都没有据此自动切换 enabled。

## 2. 缺陷清单、判断与整改优先级

P0 表示会阻断继续使用或发布；P1 表示本次整改必须交付的功能及交互缺陷。

| 编号 | 现象 | 证据判断 | 优先级 / 责任范围 |
| --- | --- | --- | --- |
| R01 | IntroOP、BBFile 调出 Sym 卡死 | 用户实测；缺少与故障时刻对应的事件、UI 心跳及调用耗时轨迹，根因待定位 | P0；共享符号面板 + 两宿主 |
| R02 | BBnote 正文 Sym 无法输入 | 已确认正文桥没有 Sym 路由和符号事务；原生面板仅绑定原生控制器 | P1；BBnote 正文桥 |
| R03 | BBnote 左右 Shift 光标 / 候选失效 | 已确认桥接时两侧合并、仅传按下、JS 跳过 Shift / 释放 | P1；BBnote 正文桥 |
| R04 | IntroOP Alt 等修饰键输出异常字符 | 已确认共享 NativeController 无裸 Alt / Ctrl 等身份过滤，可能落入 Unicode 直写；具体实测码点未记录 | P1；共享键路由及宿主出口 |
| R05 | BBnote、IntroOP 首入右上按钮尺寸异常 | 已确认原生 Button 高度固定且不采用声明的 height 约束；图片 / 文本内容度量不同 | P1；共享 ImeToggle |
| R06 | BBnote 候选过大、影响已有文字 | DOM 字体及固定占位实现已确认；当前源码已有让位，实际遮挡触发条件待测 | P1；BBnote DOM 布局 |
| R07 | 聚焦自动启用、离焦自动停用 | 三宿主及共享控制器都没有完整状态协调 | P1；共享状态接口 + 各宿主焦点适配 |
| R08 | 既有验证未发现这些问题 | 验证确有覆盖缺口；源码字符串、mock 路由、合成事件不能验证首帧 / 物理键 / 弹窗完整生命周期 | P1；验收工具及归档 |

用户已认可 IntroOP / BBFile 的候选字号和原生 Shift 行为，应作为回归基线。此次主要调整 BBnote 正文候选，并验证三应用的位置及占位，避免对已合适的字号做统一缩小。

## 3. Sym 卡死：当前能确认的路径与定位步骤

共享 [cycleSymbols()](C:/Users/dove/Documents/BBIME/src/nativecontroller.cpp:229) 打开面板时读取当前会话票据，生成固定 30 个网格位置；这里没有直接查询词库或访问文件。选符号才进入 [chooseSymbol()](C:/Users/dove/Documents/BBIME/src/nativecontroller.cpp:257) 和 [insertLiteral()](C:/Users/dove/Documents/BBIME/src/inputmodule.cpp:151)，可能先提交已有编码。因此“调出立即卡死”与“选择后卡死”必须分别记录，不能先归因于词库规模或解码速度。

共享 [NativeSymbolPanel.qml](C:/Users/dove/Documents/BBIME/assets/NativeSymbolPanel.qml:4) 的现状是：`symbolsChanged` 直接根据 `panel.opened` 请求开 / 关；`onOpened` 请求网格焦点；`onClosed` 同时关闭控制器状态并恢复正文焦点。控制器 [closeSymbols()](C:/Users/dove/Documents/BBIME/src/nativecontroller.cpp:249) 又在 Dialog 动画完成前恢复焦点。

官方 SDK [AbstractDialog](C:/bbdevtools/target_10_3_1_995/qnx6/usr/include/bb/cascades/controls/abstractdialog.h:53) 确实提供 `opened` 布尔属性，这个用法合法。SDK 同时说明 [open / close](C:/bbdevtools/target_10_3_1_995/qnx6/usr/include/bb/cascades/controls/abstractdialog.h:77) 可异步完成，正在开 / 关时再次调用没有效果。因而当前缺少 opening / closing 和关闭后重新协调，属于需要整改的生命周期风险；**不能把重复 open 或属性不存在写成已证实的卡死根因**。

原生控制器 [focused()](C:/Users/dove/Documents/BBIME/src/nativecontroller.cpp:131) 已保留符号面板造成的编辑框失焦。两宿主目前也未发现“任意编辑框失焦立即 suspend”的确定闭环。候选票据参数经 moc 规范化为 `ulong`，没有证据支持直接判定 QML 无法调用 `unsigned long` 参数。

整改及排查步骤：

1. 在一个明确可写且位于前台的原生字段复现，分别测试无编码、有编码、英文、中文；记录“实体 Sym / 屏幕入口 → 面板是否显示 → 字母选择 / 触摸选择 → 关闭 / 返回”的具体失败环节。
2. 添加少量状态轨迹：单调时间、宿主页面 / 字段稳定 ID、enabled、active session、焦点归属、面板 epoch、open / close 请求及完成、模型重建次数、事件是否 accept。只记录状态与计数，不记录正文、候选内容或输入字母。
3. 以 GUI 定时心跳和操作响应区分：心跳继续但符号不工作时查路由 / 会话失效；心跳停顿且 CPU 高时查同步递归或重复刷新；心跳停顿且 CPU 低时查等待、锁及同步 I/O。必要时在故障窗口取得调用栈，保留与包哈希对应的时间线。
4. 将符号状态改为 Closed / Opening / Open / Closing，控制器可见意图与 Dialog 动画状态分别维护。在 opened / closed 完成后重新协调；旧面板回调必须检查 epoch，不能关闭或恢复后来建立的新会话。
5. 面板持有原合法编辑目标的受保护引用和会话票据；内部移焦不撤销会话。关闭完成后，只有原字段仍可见、可写、在当前页面且应用前台才恢复焦点。页面关闭、后台、禁用字段时撤销面板，拒绝延迟写入。
6. IntroOP 核对 main 页所挂的唯一 Dialog 与 Sheet / 栈页的实际承载关系；BBFile 核对关闭面板后立即 `prepareSubmit` 的时序。保持保存一次执行，不能靠多个恢复焦点调用保证提交成功。
7. 控制模型刷新次数：只有内容或票据变化才更新相应模型；保留票据失效校验，不为减少刷新而复用过期票据。日志不得在 GUI 线程每键同步刷盘。

BBFile 现有 [验证归档](C:/Users/dove/Documents/BBFile/docs/IME_VALIDATION_2026-10-01_1.0.0.18.json:123) 的 physicalKeysAndTouch、前台焦点及符号焦点恢复为 NOT_RUN。IntroOP 现有启动日志没有故障时刻轨迹。故 R01 在获得真实选择 / 关闭 / 连续调用证据前应保持未关闭状态。

## 4. BBnote 正文 Sym 与左右 Shift 的具体整改

BBnote 正文不通过原生 TextEditor 写入。原生字段的功能正常与否，不能代表正文桥已实现相同功能。

Shift 缺口见 [noteime.cpp](C:/Users/dove/Documents/BBnote/src/noteime.cpp:166)：左右身份合并为 16；[MarkdownEditor.qml](C:/Users/dove/Documents/BBnote/assets/MarkdownEditor.qml:14) 仅派发 pressed；[ime-editor.js](C:/Users/dove/Documents/BBnote/assets/ime-editor.js:187) 跳过 Shift，释放事件另被忽略。接入文档也已记录 [正文未移植的能力](C:/Users/dove/Documents/BBnote/docs/ime/FEASIBILITY.md:82)。

需要实现：

- 桥消息保留左右实体身份、pressed / released、duration、重复状态及修饰组合；每文档 / 字段的按键状态隔离。
- 在释放时判定单独短按：左 Shift 上一候选 / 光标左移，右 Shift 下一候选 / 光标右移。已有选区时分别收起到起点 / 终点；边界及 emoji 不拆 UTF-16 代理对。
- 长按、Shift 与字母 / Enter / 空格组合、双 Shift、跨字段按下释放及失焦期间释放不触发单按动作；保留英文大写与原快捷键。
- 候选高亮使用真实候选索引，滚动跟随；避免异步解码返回后把用户刚移动的高亮无条件重置。

Sym 缺口见 [原生面板挂接](C:/Users/dove/Documents/BBnote/assets/main.qml:623)：面板仅绑定 `noteIme.native`，正文没有面板入口。需要新增正文符号事务及面板适配，或为共享面板提供经审计的正文接口；不能只改绑定，让原生控制器向影子字段插入就算完成。

正文符号请求须携带 document session、epoch、serial、文档修订、选区和 panel epoch。选择时验证仍属于当前文档，先确认当前候选，再以 CodeMirror 事务插入符号；成功只写一次并保留撤销。切组、取消、旧面板、旧选区、保存失败均不能向新文档串写。符号排列及中文 / English / 数学轮换按共享规范执行。

## 5. 修饰键异常：共享路由缺失

共享 [NativeController::handleKey()](C:/Users/dove/Documents/BBIME/src/nativecontroller.cpp:344) 在 Ctrl 组合及部分 Alt 业务组合之外，直接读取 Unicode，并在 [:392](C:/Users/dove/Documents/BBIME/src/nativecontroller.cpp:392) 后允许直写。只屏蔽私用区字符不等于屏蔽裸修饰键身份；如果固件事件携带其他可打印码点，仍可能写进正文。

BBIME 演示 [Backend](C:/Users/dove/Documents/BBIME/src/backend.cpp:714) 已显式过滤裸左右 Alt、Qt Alt / AltGr、Ctrl、CapsLock，嵌入的共享控制器没有对应处理。这是演示和宿主路径的明确不一致，可解释 IntroOP 实测的输入出口，但具体异常字符码点仍待实体事件验证。

整改在字符提取和 fallback 之前进行身份分类：裸 Alt / Ctrl / CapsLock 等按下、重复、释放都不写字；保留左右 Shift 单按状态机及 Sym 专用处理。Alt + 印字键仍按合法数字 / 符号语义处理，Alt+Enter、Alt+Backspace 和 Ctrl 业务组合由宿主唯一入口处理。未知不可打印事件不得通过宽泛 Unicode 兜底写入。

同时检查 IntroOP、BBFile、BBnote 原生和正文出口，确保同一事件只由一个入口处理，不被另一个 Shortcut、浏览器或原生通道重复输入。不得用“禁用全部 Alt”修补，避免破坏语言切换和选词。

## 6. 右上角按钮首入尺寸

共享 [ImeToggle.qml](C:/Users/dove/Documents/BBIME/assets/ImeToggle.qml:4) 使用原生 Button，声明宽 76、高 64，并在“菜单图片”和“中 / EN 文字”之间切换。菜单资源实测为 **81×81 像素**。

SDK [button.h](C:/bbdevtools/target_10_3_1_995/qnx6/usr/include/bb/cascades/controls/button.h:69) 明确说明：按钮默认尺寸与设备有关；图片可能限制最小宽度；原生按钮高度固定，不采用 preferredHeight / minHeight / maxHeight。故当前“固定 76×64”只是源码声明，不能保证首帧与切换后的实际尺寸。图片显示尺度、加载前后和宿主父布局的具体贡献仍需量测。

整改应在共享组件中完成：使用能确定布局的外框、独立图片 / 文字控件和明确点击区域，显式设定图像显示框及内容对齐；保留无焦点策略、状态绑定、无障碍名称及最右位置。可继续以 76×64 作为目标触摸框，但必须测量实际布局，而非继续叠加原生 Button 的 height 属性。

启动时先完成 scene / 字段注册及焦点策略，再发布最终状态；图片框始终预留尺寸，文字 / 图标显隐不改变外框。记录首次可见帧、图标载入完成、首次开关、返回页面四个阶段的 layoutFrame 和图片边界，确认无跳变、无裁剪、不挤占候选。

现有 [源码契约测试](C:/Users/dove/Documents/BBIME/tools/Test-ModuleContracts.ps1:21) 只检查 maxWidth / maxHeight 字符串，仍会报告 fixed-size PASS。本轮实际重跑也通过；这证明应补充 SDK 语义和运行布局验收，不能将这项 PASS 作为尺寸正确的证据。

## 7. 候选框过大与遮挡正文

IntroOP / BBFile 使用共享原生 [CandidateStrip](C:/Users/dove/Documents/BBIME/assets/CandidateStrip.qml:12)，单行高 72、Small 字体、长词按宽度适配。用户认为其大小合适，保留该基线。

BBnote 正文 [nativeCandidates=false](C:/Users/dove/Documents/BBnote/assets/main.qml:369)，改用独立 DOM 候选条。[CSS](C:/Users/dove/Documents/BBnote/assets/markdown-editor.css:21) 将高度固定为 68 CSS px，标签 22 px、候选按钮 24 px；窄屏正文另缩为 18 px，候选没有相同响应规则。这是相对字体不协调的明确实现差异，CSS px 也不能直接与原生像素比较。

关于遮挡，当前 [ime-editor.js](C:/Users/dove/Documents/BBnote/assets/ime-editor.js:58) 已向宿主报告 68 的占位，[markdown-editor.js](C:/Users/dove/Documents/BBnote/assets/markdown-editor.js:165) 已下移正文并缩短视口。故不能写成“源码完全没有让位”。需要核对用户实测包、WebView 尺度、真实 DOM 外框高度和重布局 / 滚动时序。

整改要求：

1. 候选位于正文之外的独立布局区；显示时正文视口完整让位，隐藏时归还空间。候选外框与正文视口不得相交。
2. 用真实测量高度作为唯一占位来源，替代 CSS、JS 各写一次 68。计入 padding / border / 字体及 WebView 显示尺度；布局变化后刷新编辑器测量并保持活动光标及选区可见。
3. 候选字号跟随正文尺度，以用户认可的原生视觉大小为参照；维持单行横向滚动和足够点击范围。只缩字体而保留过高占位不能算完成。
4. 原生组件与 DOM 共用视觉参数规范，不要求两种控件使用相同的数值单位。实测短词、长词、预编辑提示和错误提示，避免内容溢出。
5. BBnote 当前 DOM 仅渲染前 20 个候选，而共享原生列表可展示 40 个；补齐浏览范围与高亮可见性，避免第 20 项后的候选成为不可触摸内容。

验收时记录实际可见边界、字体显示高度、正文视口及光标矩形，覆盖首行、中段、末行、选区、长文档和候选显隐。不得通过覆盖正文或吞掉当前行来节省布局高度。

## 8. 新增功能：按真实编辑焦点自动切换模式

“输入法模式”定义为应用自建输入通道活跃，按钮显示当前中 / EN，相关菜单及冲突快捷键按输入态协调；“非输入法模式”定义为自建文字通道停用，适用的宿主菜单及浏览操作恢复。离焦不自动启用系统输入法，原生接入字段继续遵守 Custom + VirtualKeyboardOff。

当前 [focused()](C:/Users/dove/Documents/BBIME/src/nativecontroller.cpp:131) 只 activate / deactivate 会话；activate 又要求 [enabled 已为 true](C:/Users/dove/Documents/BBIME/src/nativecontroller.cpp:98)。失焦没有改变 enabled，因此按钮 / 菜单状态可能与实际编辑焦点不一致。BBnote [focusEditor](C:/Users/dove/Documents/BBnote/src/noteime.cpp:127) 只记字段，DOM [focus / blur](C:/Users/dove/Documents/BBnote/assets/ime-editor.js:256) 也未向宿主同步编辑目标。三个应用需要同时改造，不能只给共享函数增加一个 setEnabled。

| 事件 / 条件 | 目标状态与行为 |
| --- | --- |
| 前台活动页中，可见、可用、可写且已授权字段获得真实焦点 | 自动开启相应输入通道；普通字段保留用户自然码 / English 偏好 |
| Literal / 数值技术字段获得焦点 | 自动开启既有英文 / 字面量通道，不纳入中文解码；不覆盖普通字段的语言偏好 |
| 密码 / 敏感字段获得焦点 | 按既有宿主策略处理，拒绝中文候选、学习及共享正文状态 |
| 普通字段 A 切到 B | 一次协调目标转交；取消 A 未提交编码，拒绝 A 旧票据，不把内容写入 B |
| 点击列表、阅读区、导航、非编辑设置 | 停用文字路由、候选和符号；清按键状态；恢复适用业务操作 |
| 点击候选或 Sym 网格，焦点仍属于当前编辑操作 | 视为编辑目标的内部子焦点，保留会话，避免立即自动停用 |
| 点击保存 / 暂存 / 搜索提交 | 先封住新键输入，完成对应提交事务，再停用或导航；失败保留可重试状态 |
| 手动点击右上开关显示菜单 | 用户手动暂停优先；清掉 / 挂起编辑焦点，避免同一未变化焦点立即自动开启 |
| 应用后台、休眠、页面关闭、字段销毁或变只读 | 立即撤销目标及面板，不允许恢复到旧页 |
| 返回前台 | 重新验证页面和可写目标；新的合法用户聚焦可自动开启，不能仅因旧 focus 位仍为 true 就抢占焦点 |

实施方式：宿主集中维护编辑目标、页面有效性、应用前台状态、内部面板焦点、提交事务和手动暂停原因，导出一个有效输入状态。所有按钮、菜单、候选、Shortcut 与路由使用该状态；语言偏好独立保存。

焦点及 visible / editable / enabled / scope / 前台变化统一触发 reevaluate。A→B 的 focus-out / focus-in 在同一事件轮次协调，避免同步回调中反复 suspend / requestFocus。只监听 focusedChanged 不够：BBFile Sheet 的 requestFocus 可能早于 opened / scopeActive；IntroOP 需在所有 scene 创建回调结束后评估启动焦点。

BBnote 另加带当前文档 session / epoch 的 DOM 焦点消息；旧 WebView blur 不能停用后来原生字段。候选触摸及符号子焦点需保护。正文点击标题栏保存 / 暂存已经有保留预编辑并等待快照的流程，不能在 blur 中直接清队列；应将普通键路由停用与提交事务完成分别协调。

此要求更新原规范的“启动全局开启 / 暂停后靠手动恢复”行为。整改时同步 [标准接入流程](C:/Users/dove/Documents/BBIME/module/INTEGRATION_WORKFLOW.md)、宿主文档和测试期望，避免新焦点策略与旧菜单规则互相打断。

## 9. 按工程分配修改范围与执行顺序

| 工程 | 必须修改的内容 | 应保留的能力 |
| --- | --- | --- |
| BBIME 共享模块 | 裸修饰键分类；符号生命周期及关闭后恢复协调；可控开关组件；焦点策略接口；候选布局参数及对应验收 | 单服务、会话票据、Custom 独占、自然码 / English、UTF-16 保护、失败可重试 |
| BBnote | 物理 Shift / Sym 消息；正文符号事务；DOM 焦点同步；候选真实占位及字体；保存离焦协调；导入共享修复 | CodeMirror 事务和撤销、异步票据、保存等待、Markdown ASCII 标点、私密字段边界 |
| IntroOP | 更新固定模块 / 资源；字段和页面焦点协调；Sym 跨页面承载验证；修饰键唯一入口 | 当前正常的原生 Shift、合适候选字号、既有业务和字段策略 |
| BBFile | 更新模块及 r2 宿主补丁；Sheet opened / scope 焦点评估；面板关闭后提交 / 恢复协调 | 正常原生 Shift / 候选字号、八字段白名单、唯一提交入口、GUI 与 root 服务隔离 |

建议顺序：先用现有包取得 R01 故障轨迹；随后修共享模块并验证；三个宿主按固定快照导入和重制补丁；补齐 BBnote 正文功能、候选及三个宿主焦点协调；最后对最终包完成实机验收。不能仅升级模块版本就将 BBnote 正文缺口标为已修复。

新版本及补丁号在实现完成时分配，不把当前 0.1.0.12 的源码 / BAR 哈希改写成已经修复。保留各宿主导入前清单及可回退包，逐个归档实际源码、模块、补丁、资源及安装包身份。

## 10. 发布前验收矩阵

下列次数及响应值为本项目建议的验收目标，需要在整改阶段采用相同方法实际测量。

| 验收项 | 覆盖范围 | 通过条件 |
| --- | --- | --- |
| Sym 基本操作 | 三应用；BBnote 原生 / 正文分别；中文 / 英文，有 / 无编码；触摸 / 实体字母 | 26 个位置对应正确；空位不输入；符号只插一次；原编码及撤销符合规则 |
| Sym 稳定性 | 每路径至少连续开关 / 轮换 100 轮；快按、长按、后台、关闭页、换字段 | 不崩溃、不持续无响应、不重复开关；旧回调 / 票据不写新目标；可关闭并继续输入 |
| 卡顿量测 | Sym 请求、opened、选择、closed、焦点恢复分段；带编码和无编码分开 | 记录 P50 / P95 / 最大值；建议关键输入响应 P95 ≤100 ms、最大值 ≤250 ms；动画单独计时，无持续超过 1 s 的冻结 |
| Shift | 两侧短按、长按 >500 ms、组合、双 Shift、跨字段释放；候选及空编码 | 单按各执行一次；组合不误触发；边界、选区、emoji 正确 |
| 修饰键 | 裸 Alt / Ctrl / CapsLock 按下、重复、释放；Alt 印字、选词及宿主组合 | 裸修饰键零字符；组合语义正确且仅执行一次 |
| 自动模式 | 每白名单字段及正文；列表 / 阅读区；只读 / 隐藏 / 禁用；Sheet 打开时序 | 聚焦自动开启、真正离焦停用；按钮 / 菜单 / Shortcut 同步；手动暂停不立即反弹 |
| 内部焦点及保存 | 候选点击、Sym、标题栏保存 / 暂存；提交失败 / 延迟响应 | 会话不误清；保存等待最终文档；失败不保存 / 关闭；不串写、不丢预编辑 |
| 开关首帧 | 至少 10 次冷启动、图片载入后、首次开关、回到页面 | 外框及图像框符合目标；无大小跳变、无裁剪、无候选挤占 |
| 候选空间 | BBnote 窄屏及长文，首 / 中 / 末行、选区；其他两应用回归 | 候选与正文视口边界不相交，光标可见；字号合适；全部候选可达 |
| 生命周期及业务 | 后台 / 锁屏 / 页面销毁，搜索 / 文件业务 / IntroOP 提交 | 不恢复旧焦点，不重复提交；不破坏原业务、撤销及字段权限 |

现有回归继续保留，但增加实际 QML 类型 / 布局及实体事件覆盖。IntroOP 的 JS 测试 [mock 路由](C:/Users/dove/Documents/IntroOP/bb10-native/test_ime_integration.js:32) 不经过 C++ 真实按键；BBnote 桌面 CodeMirror 测试不包含 Q10 WebView 尺度；BBFile 合成 KeyEvent 不包含实体键及触摸。三者都不能替代本表。

每个最终应用分别记录 PASS / FAIL / NOT_RUN、源码及 BAR 哈希、设备版本、复现步骤、布局测量和响应轨迹。用户已报告的 Sym 卡死、正文功能缺失及遮挡没有取得对应复验 PASS 前，保持 releaseReady=false。

## 11. 本轮完成情况与剩余交付

已完成：逐应用源码 / 固定快照审查，官方 SDK 行为核对，关键现有验证范围复核，缺陷分级、新增焦点需求设计、工程修改清单及验收矩阵。

未完成：Sym 故障时刻的独立设备复现 / 调用栈，新代码实现与宿主导入，最终包构建 / 安装及实体键、首帧、触摸、焦点和候选布局复验。本轮没有修改三应用运行代码或宣称这些问题已消除。

本报告的整改关闭条件是：共享组件缺陷修复，BBnote 正文 Sym / Shift 与真实占位补齐，三宿主焦点自动模式落地，以及 IntroOP / BBFile Sym 卡死在当前最终包上完成定位与复验。
