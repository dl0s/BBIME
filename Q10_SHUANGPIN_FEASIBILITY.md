# Q10 系统级中文双拼输入法可实施性报告

审查日期：2026-10-01，Asia/Shanghai。
对象：当前 Q10、现有 BB10 Native SDK / Momentics、已有 root SSH 管理入口。
性质：技术可行性评估，不是已经实现或部署完成的输入法。

## 1. 结论

**双拼中文输入可以做；但“应用内可用”“跨应用可用”“与原生系统输入法完全一致”是三个不同目标。**

| 目标 | 当前判断 | 关键条件 |
| --- | --- | --- |
| 在我们自己的原生应用中实现中文双拼 | 可实施，技术风险相对可控 | 自定义输入控件、双拼方案、候选引擎与文本编辑逻辑 |
| 在这台已有 root 的 Q10 上研究跨应用双拼 | 有明确实验基础，尚未证明完整可用 | 获取合法运行的特权服务上下文、按键替换、中文提交与焦点跟踪 |
| 使用公开 API 安装为普通第三方系统输入法 | 本轮没有发现受支持入口 | 公开 SDK 未见完整输入法提供者注册与系统候选栏接入契约 |
| 像原生输入法一样覆盖所有原生应用和 Android 应用 | 不能承诺 | IMF 私有接口或等效集成、兼容性、锁屏隔离与生命周期验收 |
| 保留原生词库、候选栏和选词操作，仅把全拼改为双拼 | 最值得优先研究的系统级路线 | 在正确位置把双拼转换给原生拼音引擎，且不破坏编辑状态 |

**建议把首个目标定义为“此台设备上的近原生双拼增强”，而不是“已经能够替换原生输入法”。**
只有系统集成验证通过后，才能扩大承诺。

本轮已通过固定主机记录的 SSH 查询、官方离线 SDK 文档核对和本地 ARM API 编译链接检查。
没有向手机上传或运行探针、安装 BAR、写入 PPS、注入按键、修改系统键盘、调整 ACL、
运行提权程序、停止服务或重启设备。[E1][E2]

## 2. 当前环境的实际基础

### 2.1 构建环境

本机存在以下可用材料：

- SDK：`C:\Users\dove1\Documents\BBarmin\sdk`。
- Host：`host_10_3_1_12\win32\x86`。
- Target：`target_10_3_1_995\qnx6`，BB10 10.3.1 SDK。
- 编译器：`qcc`，ARMv7 little-endian 目标。
- 原生界面：现有工程使用 Cascades / Qt 4，已有 Q10 安装及运行记录。[E4]
- 官方文档：`C:\bbndk\plugins` 中的 BlackBerry / QNX 帮助包。

本轮新建了不创建窗口、不创建上下文、不发送事件的链接探针。
使用 `4.8.3,gcc_ntoarmv7le` 编译链接通过，并通过 ELF 检查确认：

```text
screen_create_context
screen_create_session_type
screen_inject_event
screen_send_event
sqlite3_libversion
```

这证明相关声明、ARM 链接库和本地工具链可用。
**它不证明这些调用在手机上已获授权或运行成功，也不证明双拼引擎已经移植。**[E2]

现有 Cascades 工程另有 C++ ABI 约束：既有工程记录显示 GNU `libstdc++` 与
BB10 的 `libcpp.so.4` 混用曾造成启动崩溃；后续 GUI 应沿用已验证的
`4.6.3,gcc_ntoarmv7le_cpp` 路线，检查依赖，不能把本轮 C 探针的成功推广为
任意现代 C++ 输入引擎都能直接编译。[E4]

### 2.2 设备与 root

严格校验既有主机密钥后，`root@192.168.1.61:22` 登录成功。
本轮 `/proc/$$` 数字属主为 UID 0；设备版本文件为 `10.3.3.3216`，架构为 ARM。
这里的 UID 证据来自进程目录元数据，不替代完整有效 UID、进程能力和服务授权审计。[E1]

`uname` 输出 QNX `8.0.0`。这是此 BB10 固件的运行时标识，
**不能据此按现代 QNX SDP 8 文档假设本机具有相同功能和安全策略。**
本报告优先依据实际安装的 BB10 SDK、设备元数据和同版本工程记录。

以下设备端点均存在，当前 root SSH 上下文的 `test -r` / `test -w` 均成功：

```text
/dev/screen/.provider
/dev/screen/.inject
/dev/screen/.winmgr
/dev/screen/.inmgr
```

这是比“只知道 root 可以读系统文件”更具体的系统输入实验基础。
但是访问检查不等于实际 `open`、`screen_create_context()`、服务策略判断和输入注入成功。
本轮没有通过调用这些 API 测试权限，也没有测量普通 BAR 应用的对应权限。[E1][H1]

### 2.3 实际输入服务

进程参数和库映射查询观察到：

| 实体 | 观察结果 | 能说明什么 |
| --- | --- | --- |
| `/base/bin/keyboard-imf` | 服务存在，启动参数含 `-U334:0,411,1002` | 物理键盘相关输入组件并非普通界面中的 KeyListener |
| `/base/bin/input_service` | 服务存在，使用 `libinput_method`、`libimfclient`、`libfluency_input_method` 等库 | 系统输入处理具有独立服务和引擎 |
| `sys.keyboard` | 系统键盘存在，库映射含 `libimfclient.so.1` | 系统键盘与 IMF 有集成 |
| `/pps/services/input` | 有 Keyboard、control、dictionary、options 等对象 | 存在输入服务状态或控制通道，不代表协议可直接照搬 |
| `/pps/system/keyboard` | 有 control、status | 存在系统键盘服务通道 |

服务启动参数是配置证据，不是本轮对其实际有效 UID 的证明。
本轮部分 `pidin` 身份查询不适用于此固件，没有据此声称已验证其完整身份或能力。
PPS 仅列出名称及权限，没有读取当前输入文本或向 control 对象写入请求。[E1][E5]

系统键盘的安装清单包含：

```text
Entry-Point-System-Actions:
run_native,hidden,permanent,gain_personal_group,
sys_service_keyboard,_sys_inject_events
```

这是一项重要差别：**原生系统键盘拥有系统级动作声明，不能假设普通应用在
`bar-descriptor.xml` 中写同样的字符串，就会得到同样的权限。**
清单中的声明也不单独证明授予过程、签名来源或启动策略。[E1]

## 3. 官方 API 能做到什么

在线查询未取得可用的 BB10 API 页面；实测旧开发者 URL 返回通用
“BlackBerry Developers”页面，而非所请求的参考文档。
以下结论依据本机安装包内的官方离线文档和 SDK 头文件，不冒充在线最新文档结论。
文档路径、页面标题和文件 SHA-256 见 [E3]。

### 3.1 Cascades：能控制自己的输入框，不是全局输入法

`KeyListener`、`KeyEvent`、`InputRouteProperties` 都围绕当前应用的控件树和焦点工作。
官方文档还说明，KeyListener 的信号沿控件树传播，没有手动停止这种传播的机制。
因此单独监听按键再把中文追加到 TextField，可能与默认输入处理重复，不能视为完整方案。[S1][H4]

SDK 提供 `TextFieldInputMode::Custom`：

- 忽略控件默认键盘输入，由应用处理按键。
- 可以通过 `TextEditor` 插入文字、维护光标和选区。
- 自定义模式下原生拼写检查和 IMF 预测功能被禁用，相关能力需要另外集成。
- 这只作用于本应用持有的控件，不会改变短信、Hub 或其他应用中的输入框。[H2][H3]

**因此应用内双拼有清晰入口；“自己的应用接收键盘事件”不等于“系统级接管键盘”。**

### 3.2 `TextInputProperties`：配置输入行为，不是注册引擎

其公开属性包括预测、自动纠错、自动大小写、遮罩、键盘布局和提交键等。
这些选项描述某个输入框对系统输入服务的请求。
本轮所查 API 未提供第三方候选引擎注册、系统候选栏供应或全局 composition 生命周期接口。[H5]

### 3.3 BPS `virtualkeyboard_*`：操作既有键盘，不是创建第三方 IME

公开接口可显示或隐藏触屏键盘、请求键盘状态事件、设置布局和 Enter 键类型。
布局枚举包括 URL、Email、数字、密码、电话等，
不是“小鹤双拼”“自然码双拼”等中文编码方案选择接口。[S2]

官方 Keyboard 示例是在本应用中处理 `SCREEN_EVENT_KEYBOARD`，
不能因为示例名称含 Keyboard 就把它理解为系统输入法开发模板。
文档也指出 Q10 物理键盘场景与触屏键盘显示场景不同。[S1]

### 3.4 Screen：具有特权输入基础，但不提供完整中文输入语义

官方 SDK 头文件定义：

| API / 上下文 | 用途 | 不能直接推出的能力 |
| --- | --- | --- |
| `SCREEN_APPLICATION_CONTEXT` | 本进程窗口及事件 | 默认不能操作其他应用；`.inject` 访问是有条件的额外能力 |
| `SCREEN_INPUT_PROVIDER_CONTEXT` | 向应用发送输入事件 | 不自动得到所有原始按键，也不自动阻止原按键 |
| `SCREEN_INPUT_MANAGER_CONTEXT` | 输入会话通知和部分属性管理 | 不是已验证的全局键盘过滤器 |
| `SCREEN_WINDOW_MANAGER_CONTEXT` | 窗口管理及通知 | 不自动得到输入框的光标、选区和安全输入属性 |
| `screen_inject_event()` | 向指定显示上的焦点窗口送事件 | 不是 UTF-8 字符串提交或 IMF composition API |
| `screen_send_event()` | 向指定进程送事件 | 不是对任意 TextField 的直接编辑接口 |
| `screen_create_session_type()` | 创建与上下文关联的输入会话 | 不是输入法提供者注册 |

普通公开帮助页只列出 application context；本轮 privileged context 和注入函数的依据
主要来自官方 SDK 头文件及库导出，不能把“随 SDK 出现”与“普通应用获支持授权”混为一谈。[S3][H1]

Screen 的焦点通常是窗口或输入会话级别。完整中文输入还需要知道：
预编辑文本、候选、提交确认、光标和选区、删除语义、输入框类型与安全状态。
**注入一个键盘事件成功，也不等于中文输入法已经工作。**

### 3.5 IMF：存在内部机制，未取得完整第三方开发契约

SDK 链接库 `libinput_client.so` 中可以观察到 `imf_client_init`、
`imfSetInputMode`、`ictrl_set_input_mode`、候选相关 hook 等导出；
设备也确有 IMF 相关库和服务。[E5]

这些符号能证明平台内部有对应机制，但不能证明：

- 第三方可以注册为系统默认输入法。
- 只凭函数名就能确定参数结构、消息协议、所有权及线程模型。
- 设置现有 input mode 相当于安装一个新编码引擎。
- SDK 10.3.1 内部 ABI 可无条件用于设备 10.3.3。

本轮在已查公开头文件和帮助包中，没有找到完整、受支持的第三方 IME 提供者接口。
这是有范围的审查结论，**不是证明此设备所有未公开接口都不可用**。

## 4. 三条实现路线

### A. 应用内双拼

```text
本应用的物理键盘事件
  -> 双拼编码和预编辑状态
  -> 候选引擎
  -> 本应用候选栏
  -> TextEditor 提交到当前控件
```

优点：有公开应用 API，易隔离测试，不需要 root；可以首先验证双拼方案与键盘交互。
限制：只覆盖我们自己的应用；原生词库及候选栏不会自动继承。
若只做一个编辑器再复制粘贴到其他应用，它仍是辅助输入工具，不是系统输入法。

### B. 双拼前端复用原生拼音引擎

```text
中文文本输入会话中的物理按键
  -> 双拼状态机
  -> 正确的拼音 / 引擎输入表示
  -> 原生拼音引擎与 IMF
  -> 原生候选栏及中文提交
```

**这是最接近用户目标、应先做可行性闸门的路线。**
可望保留原生候选排序、词库、候选栏和选词操作，减少重新实现整个中文引擎的工作。
以上收益是架构上的预期，尚未通过本机原型证明。

必须先验证：

1. 转换点在原生拼音解码之前，而不是文字已经提交之后。
2. 能阻止原始双拼按键继续进入原生引擎，避免重复输入。
3. 转换后的输入不会再次被转换，避免事件反馈循环。
4. 有可靠的中文会话与输入框类型判定，英文、密码和数字场景不误处理。
5. 双拼一个码元与原生多个拼音字符之间可以进行正确退格、撤销和焦点切换。
6. 目标入口保留原生预编辑和候选流程，不仅能显示字母。

即使该路线成功，原生预编辑区可能显示展开后的全拼，而非用户输入的双拼码。
候选栏可以相同，但整个输入体验未必逐项完全相同。

### C. 独立系统双拼服务与自有候选界面

```text
受管特权输入适配器
  -> 双拼 / 拼音候选引擎
  -> 不抢编辑焦点的候选界面
  -> 经验证的目标文本提交通道
```

这条路线对词库、方案和候选展示更自由，但需要自行承担跨应用语义和兼容性。
Screen 叠加窗口与按键注入可以作为实验组件，不能单独替代 IMF。
普通中文事件是否被 Cascades、浏览器和 Android 桥接层正确解释，需要分别实测。

不应以全局剪贴板加粘贴作为最终等价方案：它涉及用户剪贴板、
跨应用粘贴限制、焦点竞争以及与原生预编辑不同的编辑行为。

## 5. 现有拼音数据的价值与边界

设备有以下静态资源：

```text
/base/usr/share/imf/data/swiftkey/zh_CN/pinyin/charactermap.json
/accounts/1000/appdata/sys.keyboard/app/native/data/keymaps/chinese_pinyin/pinyin.xml
/accounts/1000/appdata/sys.keyboard/app/native/data/predictionbar/pkb/res/720x720/chinese/portrait.xml
```

本轮用结构化解析检查了前两个文件：

- `charactermap.json` 顶层为 `charmap`、`multicharmap`、`variant`、`tags`，
  包含字母或字母组合到拼音相关表示的映射；是进一步分析原生引擎输入表示的线索。
- 338 字节的 `pinyin.xml` 仅见切换键及“拼音”显示标签，
  不是一份已确认的双拼解码表。
- 720×720 物理键盘中文候选栏资源存在，支持继续研究原生候选 UI。

**不能由这些资源存在就断言“改 JSON 或 XML 即可启用双拼”。**
还未确认引擎的数据加载方式、校验、缓存、映射含义、
物理键盘路径是否经过该映射、文件系统写入条件或动态切换能力。
检索到拼音音节 `shuang` 也不是支持双拼的证据。[E1][E5]

后续应先离线研究副本及使用路径，再考虑隔离测试；不要直接覆盖唯一可用设备的原始词库。

## 6. 双拼逻辑本身需要实现的内容

双拼不是简单地将每个字母固定替换为一串全拼字母。
正确实现需要方案特定的声母/韵母规则、零声母处理、合法音节约束，
以及半个音节、分词、连续多音节和中英文混输的状态管理。

应先固定一套方案，例如用户实际使用的小鹤双拼或自然码。
不同方案使用的韵母映射、零声母表示与标点键可能不同，不能混用。
音节映射、模糊音和候选排序应是不同层。

Q10 验收还要覆盖 Alt 数字/符号、Shift、Sym、Space、Enter、Backspace、
长按和重复键。选词键必须与系统快捷键、表单提交以及数字输入区分。
输入中文字后继续编辑既有英文、标点和选区，不能只测试向空白框追加汉字。

## 7. “与原生一致”的验收定义

| 项目 | 原生级要求 | 本轮状态 |
| --- | --- | --- |
| 键盘输入 | 正确处理按下/释放、修饰键、重复及选词 | 未做行为验证 |
| 中文质量 | 有效词库、词组切分、排序、个人词频及混输 | 未移植或验证 |
| 候选栏 | 跟随当前会话，不抢焦点，不遮挡关键 UI | 仅确认原生资源存在 |
| 预编辑 | 按码元退格、提交/取消、选区和光标一致 | 未验证 IMF 接口 |
| 跨应用 | Hub、短信、系统搜索、浏览器、第三方原生应用 | 未逐项验证 |
| Android | 文本、密码、多行及 WebView 桥接路径单独通过 | 未验证，不能由原生结果推断 |
| 安全输入 | 密码/锁屏不采集、不记录、不沿用候选状态 | 尚未实现 |
| 稳定性 | 服务失败后恢复原生输入，不留下失效焦点或按键 | 尚未实现 |
| 生命周期 | 脱离 SSH、休眠唤醒、重启后按预期工作 | 未验证 |
| 性能 | 本机处理输入，候选与提交延迟和功耗可接受 | 无实测数据 |

因此，现在可以确认“有构建与研究基础”，不能确认“已经与原生一致”。
某个应用能收到一个汉字，只能证明该路径的基本提交，不是上述全部项目通过。

## 8. root 并不能解决的部分

SSH root 是管理入口，不自动将通过 Launcher 启动的 GUI 变为 root。
系统输入实验至少还有可信执行、实际 UID/能力、Screen/PPS ACL、
系统动作授权、会话归属和服务生命周期等条件。

邻近 BBFile 项目的现有 root 评估指出，该入口和已有服务依赖既有第三方启动链；
其冷启动独立性尚未完整验证。本轮没有重新追踪启动链，不把该历史评估写成本轮
已验证的新授权来源。[E6]

输入法应在手机本机运行，而不是每次按键通过电脑 SSH：
正常使用不能依赖电脑在线，SSH 查询耗时也不能代替输入延迟评测。

若继续开发，建议 GUI 保持普通身份，特权适配器仅保留经验证的必要权限。
不给它任意 shell 或文件操作接口；默认不记录原始按键和周边正文。
不为方便而放宽整个 Screen/PPS 权限或给普通应用复制系统键盘权限。

## 9. 推荐实施顺序与停止条件

### 第一阶段：应用内验证

完成一套双拼方案、自定义文本控件、候选和光标处理。
用合成文本测试，建立可回归的编码与编辑用例。
若不能正确处理退格、混输和选区，不进入系统集成。

### 第二阶段：系统接入可行性闸门

先准备恢复通道与原始资源备份，再在可恢复环境验证：

1. 特权程序获准执行，并成功创建所需 Screen 上下文。
2. 不改变输入流时，能够正确识别受控测试应用中的焦点/会话。
3. 仅在用户打开的合成文本测试框中，验证一次受控提交和接收结果。
4. 验证按键替换及原生拼音引擎前的接入位置。
5. 验证自有候选或复用原生候选的可行性。

本轮未执行这些有状态实验。
如果只取得事件注入，却无法可靠地处理中文会话与原按键阻止，应停止承诺系统级透明双拼，
转为应用内功能或明确标注的辅助输入工具。

### 第三阶段：近原生原型

优先采用路线 B；只有其接口与状态管理不成立时，才评估路线 C 的完整成本。
使用单独版本化适配层隔离私有接口，保留原生模式与立即停用能力。
先覆盖一组指定应用，再扩展到浏览器和 Android。

### 第四阶段：发布验收

验证脱离 SSH、休眠、重启、异常恢复、权限变化和所有目标应用。
采用事件驱动及本机 IPC，分别测量按键到候选、选词到提交的延迟和待机功耗。
没有这些测试数据，不提供“原生性能”“所有应用通用”或“升级后仍有效”的保证。

## 10. 总体建议

**可以立项，但应按“有条件的系统集成研究”立项。**

最优先的问题不是“能不能写出双拼映射”，而是：
**能否在这台固件上把双拼输入可靠地交给原生中文引擎，并保留其会话、候选和提交行为。**
当前 root、Screen 端点和原生引擎资源使该调查有实质意义；
当前公开 API 与只读实测还不足以给出完全原生等价的肯定答案。

## 11. 依据与复核

### 本轮证据

- [E1] [设备输入环境只读证据](C:/Users/dove1/Documents/BBIME/research/device-input-evidence.json)：
  严格主机记录、UID 元数据、系统版本、端点访问、服务、清单与资源。
- [E2] [ARM API 编译链接证据](C:/Users/dove1/Documents/BBIME/research/sdk-api-evidence.json)：
  编译目标、来源/产物哈希与 ELF 输出；未在设备执行。
- [E3] [官方 API 文档索引](C:/Users/dove1/Documents/BBIME/research/official-api-index.json)：
  官方离线包、页面标题、头文件及 SHA-256。
- [E4] [既有 Q10 部署和 ABI 记录](C:/Users/dove1/Documents/BBarmin/bb10-native/DEVICE_REPORT.md)、
  [构建脚本](C:/Users/dove1/Documents/BBarmin/bb10-native/build.ps1)：历史工程证据，非本轮重新部署。
- [E5] 本轮其他只读工具查询：`pidin -p <PID> libs`、SDK `ntoarm-nm -D`、
  固定资源读取后通过 PowerShell `ConvertFrom-Json` / XML 解析。
  相关输出未单独完整保存，不冒充已包含于 E1 的全部内容。
- [E6] [BBFile root 来源评估](C:/Users/dove1/Documents/BBFile/docs/ROOT_AUTHORIZATION_REVIEW.md)：
  既有本地评估，未在本轮独立复验完整启动链。

### 官方离线文档

- [S1] [Input methods 帮助包](C:/bbndk/plugins/com.qnx.doc.inputmethods_4.0.0.20150226/doc.zip)：
  `topic/index.html`、`topic/physical_keyboard.html`、`topic/keyboard_overview.html`。
- [S2] [BPS 参考帮助包](C:/bbndk/plugins/com.qnx.doc.bps.lib_ref_4.0.0.20150224/doc.zip)：
  `topic/virtualkeyboard_layout_t.html`、`topic/virtualkeyboard_change_options.html`。
- [S3] [Screen 参考帮助包](C:/bbndk/plugins/com.qnx.doc.screen.lib_ref_4.0.0.20150224/doc.zip)：
  `topic/group__screen__contexts_1Screen_Context_Types.html`、
  `topic/screen_get_event.html`、`topic/screen_create_session_type.html`。

### 官方 SDK 头文件

- [H1] [screen.h](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/screen/screen.h:164)：
  上下文类型；第 5680 行为 `screen_inject_event()` 声明，第 6581 行为会话创建声明。
- [H2] [textfieldinputmode.h](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/resources/textfieldinputmode.h:109)：
  Custom 输入模式及 IMF 限制。
- [H3] [texteditor.h](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/controls/texteditor.h:109)：
  光标与选区文本插入。
- [H4] [inputrouteproperties.h](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/controls/inputrouteproperties.h:13)：
  当前应用内控件事件路由。
- [H5] [textinputproperties.h](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/controls/input/textinputproperties.h:23)：
  文本输入属性。

复核脚本：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\dove1\Documents\BBIME\tools\Get-Q10InputEvidence.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\dove1\Documents\BBIME\tools\Test-SdkApi.ps1
```

第一个脚本只读当前固定设备的相关元数据；第二个只在电脑编译和检查 ARM ELF。
二者均不部署、运行手机端探针或改变输入服务。
