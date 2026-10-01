# BBIME 嵌入多款自有 App 的当前可行性报告

评估日期：2026-10-01，Asia/Shanghai。
评估对象：当前 BBIME 自然码双拼、全拼、候选及 Q10 实体键盘交互。
源码基线：`0.1.0.10`。目标优先按现有 BB10 / Cascades 自有应用理解。
本文是实施决策报告，不表示已经接入其他 App，也不表示系统输入法已经实现。

**续作说明：**主体保留 `0.1.0.10` 初评及失败证据。`0.1.0.11` 已继续实施
风险收敛与最小原生模块，当前进度见
[阶段 A/B 实施记录](C:/Users/dove1/Documents/BBIME/research/PHASE_AB_IMPLEMENTATION.md)
和 `phase-ab-validation-0.1.0.11.json`。不能再将初评中的“尚无接口”
当作新版源码状态，也不能把新增源码/构建通过当作手机及双宿主已验收。

## 1. 决策摘要

**可以作为自有 App 内的公共输入模块建设；不建议把当前 Backend 原样复制到所有 App，也不建议把当前版本直接作为稳定 SDK 发布。**

推荐首版形态是“统一维护源码、每个 App 随包嵌入、每个进程一个解码服务、每个字段独立会话”。正常输入完全在手机本机完成，不依赖 BBIME 主应用正在运行、电脑、SSH、root 或网络。

当前有两个层次的结论：

| 层次 | 当前结论 | 决策 |
| --- | --- | --- |
| 在自有 BB10 原生文本控件里提供双拼 | 架构可行，有实际运行基础 | 可以进入组件化与试点 |
| 当前 `0.1.0.10` 直接批量嵌入并发布 | 证据不足，本轮新增回归失败 | 暂缓正式发布，先修复引擎边界 |
| BBnote 的 CodeMirror 正文编辑器 | 有条件可行，需要独立 WebView 桥接 | 单独设置接入闸门 |
| 自有 App 之间共享基础偏好 | 接口已实现，双应用权限验证未完成 | 作为可选能力，不作为输入前提 |
| 自有 App 之间直接共写个人词库 | 当前不可靠 | 首版禁止 |
| 未接入的系统 App 自动获得双拼 | 不是应用内模块的能力 | 不纳入本项目承诺 |
| Android、iOS、Windows App 原样复用 | 当前 BB10 UI 层不能原样复用 | 可研究核心移植，另建平台适配 |

最关键的新证据：本轮完整主机解码回归在上游引擎触发断言；固定测试串可在新进程、新建合成个人词库下独立复现。它阻止“当前版本已稳定可发布”的结论，但不否定应用内模块这条路线。详见第 2 节和 [验证记录](C:/Users/dove1/Documents/BBIME/research/multi-app-feasibility-validation-2026-10-01.json)。

## 2. 当前成熟度与证据

### 2.1 必须区分源码、安装和运行版本

| 证据对象 | 本地记录 | 本轮如何解释 |
| --- | --- | --- |
| 当前源码与描述符 | `0.1.0.10`，`buildId=10` | 本报告的源码基线 |
| 最近保存的手机安装记录 | `0.1.0.9`，安装及回读哈希 PASS | 不能推断 `0.1.0.10` 已安装 |
| 最近确认的设备运行结果 | `0.1.0.8`，解码与合成按键测试 PASS | 是既有运行基础，不是新版验收 |
| `0.1.0.9` 设备交互记录 | 新版验证 PENDING，观察运行版本为 `0.1.0.8` | 状态按钮与单轮 Sym 尚不能报实机通过 |
| `0.1.0.10` 多 App 输入、共享设置 | 未取得双 App 实测证据 | 不可报已经支持并验证 |

依据：[描述符](C:/Users/dove1/Documents/BBIME/bar-descriptor.xml)、[安装记录](C:/Users/dove1/Documents/BBIME/research/q10-app-deployment.json)、[交互状态](C:/Users/dove1/Documents/BBIME/research/q10-interaction-validation.json)、[设备测试报告](C:/Users/dove1/Documents/BBIME/research/APP_TEST_REPORT.md)。

### 2.2 本轮实际执行的检查

| 检查 | 结果 | 覆盖边界 |
| --- | --- | --- |
| `tools/Test-ModuleContracts.ps1` | PASS，12 项 | 源码约束、Custom 模式、输入开关、共享字段和 ARM 词库哈希 |
| `tools/Test-Decoder.ps1` | FAIL | 主机字典生成和编译完成，回归运行触发引擎断言 |
| 固定种子 `910` 的追踪重跑 | FAIL，进程退出码 `3` | 在 iteration `1793`、全拼模式触发相同断言 |
| 独立全拼探针，新进程及合成词库 | FAIL，进程退出码 `3` | 单条查询即可触发，不要求先运行 1793 次 |
| 独立自然码 `nihk` 探针 | PASS，accepted=1，40 个候选 | 只证明这个基本查询成功 |

独立触发串为 37 个 ASCII 字符：

```text
nmecy'grgfxchsznrtnglcmmlz'jifycbiazx
```

失败位置：[matrixsearch.cpp:1444](C:/Users/dove1/Documents/BBIME/vendor/libgooglepinyin-0.1.2/src/matrixsearch.cpp:1444)，断言为 `0 != handles[0]`。它没有被当前 39 字节全拼长度检查拦截。

已有 [测试程序](C:/Users/dove1/Documents/BBIME/tests/decoder_test.cpp:74) 支持独立探针，可在完成主机编译后复核：

```powershell
$userDictionary = 'build/host/report-probe-' + [guid]::NewGuid().ToString('N') + '.dat'
& '.\build\host\decoder_test.exe' 'build/host/dict_pinyin.dat' $userDictionary "nmecy'grgfxchsznrtnglcmmlz'jifycbiazx" 'full'
```

这不是用户正文，是回归随机输入生成的合成测试串。测试只使用 `build/host/` 下的合成文件，没有读取手机草稿或实际个人词库。

**风险判断：**当前解码器与宿主在同一进程；若目标平台也触发同类断言，可能终止整个宿主，而不只是关闭输入栏。主机 Windows/64 位复现不是 Q10/ARM32 崩溃复现，设备是否同样触发仍待隔离测试。不能把这一点当作设备必崩的证明，也不能用两种 ABI 不同来忽略该风险。

**发布闸门：**修复或证明有效的防护、固定串和邻近边界回归、完整随机回归，以及 ARM 合成测试必须通过。不能仅关闭断言、删掉失败测试，或只屏蔽这一条字符串。默认自然码也不是未经验证即可排除引擎风险的理由。

主机编译另报告了上游同步导出数组边界警告；当前普通输入封装未调用该导出入口。开展个人词条共享前必须先审计它，不能把该警告解释成普通输入已复现的另一场崩溃。

## 3. 对现有自有 App 的接入判断

以下是对邻接项目公开说明和相关 UI 源码的只读抽查，不是这些应用已完成接入的声明。

| App / 项目 | 当前输入载体 | 适合接入的范围 | 相对难度与建议 |
| --- | --- | --- | --- |
| BBFile | Cascades `TextField`、`TextArea` | 中文文件名、搜索、普通文本编辑 | 中；优先试点普通文本编辑页，其次名称和搜索 |
| BBnote | 搜索等为原生字段；正文为 WebView + CodeMirror 5 | 原生搜索/名称可先接；Markdown 正文另接 | 正文较高；不能当作原生 TextArea 复用 |
| IntroOP，位于 `BBarmin/bb10-native` | 多个原生表单，已有业务按键路由 | 明确白名单中的中文说明字段 | 较高；先隔离业务快捷键和敏感字段，后接入 |
| BBattery | 当前主要是图表、筛选与诊断 | 将来新增中文备注等字段 | 当前未见明显文本输入需求，不建议为统一而强行增加 |
| Q10 管理器，位于 `BBIntroOP/windows` | Windows WPF | 有需要时单独移植 | 不属于 BB10 原生接入；需要重新编译核心及 Windows 编辑适配 |

**重要纠正：BBnote 的正文不是原生 Cascades 文本框。**其 [MarkdownEditor.qml](C:/Users/dove1/Documents/BBnote/assets/MarkdownEditor.qml:3) 已使用 `session/revision` 校验的 JavaScript 消息桥；[markdown-editor.js](C:/Users/dove1/Documents/BBnote/assets/markdown-editor.js:74) 创建 CodeMirror。应扩展已有协议，不要替换整个编辑器或丢弃 Markdown 高亮、撤销和草稿机制。

**推荐试点顺序：**独立合成双字段页面 -> BBFile 普通编辑页 -> BBFile 名称/搜索及另一 App 原生字段 -> BBnote 正文桥接 -> IntroOP 受控中文说明字段。若你的第一目标确实是 BBnote 正文，可把 WebView 闸门提前，但不应沿用原生控件的低成本估计。

## 4. 推荐组件结构

建议先做源码形式的公共模块，按固定版本编入每个 BAR；在已有 PowerShell 构建里提供统一源文件/资源清单。现有项目不是统一 qmake 工程，不必为复用先迁移所有构建系统。

```text
宿主 App
  字段白名单、焦点、快捷键、正文、保存、权限、生命周期
      |
  宿主输入路由
      |
  InputSession：每字段编码、候选高亮、按键及修订号
      |
  ImeService：本进程唯一 Decoder，单一指定线程串行调用
      |
  libgooglepinyin + 本 App 包内基础词库 + 本 App 私有个人词库

  EditorAdapter
      + CascadesTextAreaAdapter
      + CascadesTextFieldAdapter
      + CodeMirrorAdapter，独立验证后加入

  CandidateStrip / SymbolPanel：复用 UI，不拥有业务正文
  ModuleSettings：本地偏好 + 可选的显式共享快照
```

`InputSession`、`ImeService`、各 Adapter 及可嵌入 UI 是建议新增的组件边界，**当前仓库并没有完成这些 SDK 接口**。

### 4.1 已有代码的复用程度

| 当前文件 | 可复用部分 | 仍需处理 |
| --- | --- | --- |
| `src/decoder.h/.cpp` | 双拼映射、候选、引擎封装 | 本轮断言风险、串行所有权契约、错误语义 |
| `src/moduleprofile.h` | 无 Qt 的偏好 schema 和校验 | 保持版本化，不混入字段安全授权 |
| `src/modulesettings.h/.cpp` | 私有设置、显式发布/读取 | 设备 I/O、权限失败和真正双应用验证 |
| `src/textpositions.h` | 码点与 UTF-16 索引换算 | 按不同编辑适配器确定单位，不是完整字素簇处理 |
| `src/backend.h/.cpp` | 键盘行为和交互经验 | 不能当作通用 SDK Backend 整体复用 |
| `assets/main.qml` | 候选滑动、Sym 网格等 UI 经验 | 提取组件，去掉独立测试 App 的页面和菜单绑定 |

### 4.2 为什么不能每个输入框直接创建 Backend

上游 [pinyinime.cpp](C:/Users/dove1/Documents/BBIME/vendor/libgooglepinyin-0.1.2/src/pinyinime.cpp:33) 的引擎状态为进程全局。当前 [Decoder](C:/Users/dove1/Documents/BBIME/src/decoder.cpp:110) 已拒绝第二个 owner；因此多个字段分别创建 Decoder 不是可用的会话隔离方式。

[attachEditor()](C:/Users/dove1/Documents/BBIME/src/backend.cpp:114) 当前只接受 `TextArea`，绑定时读取固定 `data/draft.txt` 并连接控件信号；没有完整的旧编辑器解绑协议。[撤销](C:/Users/dove1/Documents/BBIME/src/backend.cpp:411) 和编码状态也由同一个 Backend 持有。原样套用到表单可能带来字段内容覆盖、旧控件回调干扰及跨字段撤销。

宿主应继续拥有正文、业务保存和撤销；输入模块只提交当前选区的文字变更，不保存一份统一草稿、不读取宿主数据库、不负责“保存病例”或“保存笔记”。

### 4.3 首版会话规则

1. 只允许当前获得焦点、可编辑且在白名单中的字段激活输入。
2. A 字段切到 B 字段时，默认取消 A 未提交编码，不自动写入 B；保留正文。
3. 切换、控件销毁、页面关闭和后台切出时，解绑旧控件，清理按住键与符号面板。
4. 候选带 `sessionId + compositionRevision`；过期点击、延迟返回或旧页面回调必须拒绝。
5. 若未来保留每字段预编辑，恢复时重新解码；不能把旧候选索引当作当前引擎候选。
6. 提交先核对字段、选区修订号、可写状态和长度；编辑成功后才学习，失败不丢编码。

首版保持单一输入线程串行调用引擎。若以后将解码移到专用线程，open/search/choose/flush/close 也必须统一排队，并拒绝过期结果；仅把 search 移走不构成线程安全。

## 5. 控件、快捷键与界面适配

### 5.1 原生 TextArea / TextField

本机官方 SDK 明确提供 `Custom` 模式：控件不处理普通键输入，应用负责按键与编辑；原生 IMF 预测、拼写检查等不会自动继承。`TextField` 和 `TextArea` 在 Custom 下也不负责原有 submit keys / keyboard shortcuts。

因此 TextField 不只是将当前 TextArea 换一个类名：需要单行校验、长度限制、Enter 的 Search/Go/Next/Done 语义，以及宿主提交回调。中文文件名、路径、URL 的标点策略也应区分，不能普遍把 `.`、`:` 转为中文符号。

建议区分：

| 字段类型 | 双拼策略 | 学习与标点 |
| --- | --- | --- |
| 普通中文正文、中文名称 | 可启用 | 学习需本 App/字段允许；中文标点按字段配置 |
| 搜索框 | 可选启用 | 默认不学习；只对已提交文字发起搜索 |
| 文件名、路径 | 有中文需要时可选 | 保留 ASCII 路径分隔符和扩展名 |
| URL、邮件、命令、源代码 | 默认原宿主输入或英文策略 | 不做中文标点替换，不默认学习 |
| 密码、验证码、数字、金额、时间等 | 默认不接入 | 不进入模块，不采集、不学习、不共享 |
| 病例等敏感说明字段 | 明确白名单后再接入 | 首版禁止学习与个人词条导出 |

安全策略由宿主强制执行，不应由共享设置开启，也不能仅靠用户手动关闭词频学习。

### 5.2 快捷键路由是必要工作，不是体验微调

当前 [handleKey()](C:/Users/dove1/Documents/BBIME/src/backend.cpp:603) 会消费广泛的按键，并自行实现 Enter、Shift+Enter、Alt+Enter、Ctrl 操作。宿主不能再用另一个监听器独立执行相同事件。

| 组合 | BBIME 当前行为 | 已有宿主行为 | 接入处理 |
| --- | --- | --- | --- |
| Shift+Enter | 确认候选并换行 | BBnote 保存；IntroOP 表单保存 | 由宿主保留，先显式处理预编辑，成功后只保存一次 |
| Alt+Enter | 中英切换 | IntroOP 开始采集 | 不沿用全局键表；按页面/字段上下文分配或改键 |
| Alt+Backspace | 取消编码 | 多 App 返回/取消 | 明确定义预编辑与导航优先级，不能同时执行 |
| 单按左右 Shift | 候选/光标移动 | 可能参与宿主修饰键观察 | 只在输入上下文执行，禁止残留释放触发业务动作 |
| Enter | 原码输出或换行 | TextField 搜索/下一字段 | 区分多行、单行与未提交候选状态 |

以上冲突依据：[BBnote 保存快捷键](C:/Users/dove1/Documents/BBnote/assets/main.qml:390)、[IntroOP 采集命令](C:/Users/dove1/Documents/BBarmin/bb10-native/RecordingPage.qml:273)、[IntroOP 表单路由](C:/Users/dove1/Documents/BBarmin/bb10-native/DraftPage.qml:74)。

官方 `KeyListener` 文档说明事件从焦点控件向根传播。应使用一个按键决策入口，并根据输入状态显式禁用冲突 `Shortcut`；不能只假设子控件调用 `accept()` 后其他观察或业务路径都不会触发。

BBIME 当前把 ActionBar 显隐和输入开关联动，这是测试 App 的页面策略，不应成为模块要求。BBFile 和 IntroOP 仍需其业务工具栏：宿主决定菜单和工具栏布局；模块只在会话条件允许时显示候选，不占用整个页面或接管全 App 菜单。

### 5.3 BBnote 的 CodeMirror 适配闸门

建议在已有 JSON 桥上添加输入命令和候选结果，复用已有 `session/revision`。接入前先用合成页面证明：

1. 实体字母、Alt、两侧 Shift、Sym、重复键可正确识别，双拼字母不会同时进入原生 IMF 或 CodeMirror 正文。
2. 提交使用 CodeMirror 的选区替换/编辑事务，保持撤销和语法高亮；不逐次调用 `setValue()` 替换全文。
3. 请求只携带必要编码与标识，不逐键把完整笔记正文送到 C++。
4. 明确区分“文档修订号”与“预编辑修订号”；切换笔记、只读、保存、加载和 WebView 重建后拒绝旧回包。
5. 保存前完成一次受控预编辑处理和最终快照；不能只等待 900 ms 草稿定时器。
6. 候选区出现后重算 WebView 视口，验证 Q10 上键盘、候选与正文不遮挡。

本轮已核对包内 CodeMirror 版本为 `5.65.16`；[源码](C:/Users/dove1/Documents/BBnote/assets/vendor/codemirror/lib/codemirror.js:6289) 提供 `replaceSelection()`，也提供编辑事务和按键处理通知。这些接口存在不等于 BB10 WebView 的硬件事件链已验证。若最小实验无法可靠阻止双重输入，就先只接原生字段，不承诺正文已完成。

## 6. 多 App 设置与词库边界

**代码复用不等于共享运行会话，数据共享也不等于系统级输入法。**

| 对象 | 首版推荐 | 当前可行性与限制 |
| --- | --- | --- |
| 输入模块源码、键位和 UI 组件 | 统一维护、锁定版本 | 每 App 重新构建/发布，避免各自复制后漂移 |
| 基础词库 | 每 BAR 包含可信只读副本 | 最稳妥；现有 ARM 词库为 1,068,442 字节，约 1.02 MiB |
| 基础偏好 | 每 App 私有保存；共享为手动可选 | `ModuleSettings` 已有接口，真正双 App 权限实测待做 |
| 中英即时状态、编码、候选、按住键 | 每会话隔离 | 不放共享文件，不远程切换活跃会话 |
| 本 App 个人学习 | 私有 `data/userdict.dat` | 只在本 App/字段允许时启用 |
| 多 App 个人词条共享 | 后续版本另做 | 当前不能直接共写 `.dat` |

### 6.1 共享设置不是安装一次后自动影响所有 App

当前可发布/读取 `shared/documents/BBIME/module-profile.ini`。它是经过 schema 白名单校验的偏好快照，不含正文、编码、个人学习授权和词库加载路径。

每个读取 App 都需要代码接入和相应 `access_shared` 用户权限。BBIME 获授权不代表其他 App 自动获授权；没有权限时，本地双拼应仍可用。相同组织名、应用名或 QSettings 键名也不绕过沙箱。

首版只在用户明确操作、且会话空闲时导入。当前没有自动跟随、发布者签名或多发布者冲突合并；共享目录不能存密钥、敏感字段策略或安全授权。`access_shared` 的授权范围也不限于这一份配置文件，不应为所有 App 的纯本地输入无条件增加它。

### 6.2 不优先建设共享基础词库服务

单份现有基础词库约 1.02 MiB，为节省这部分体积引入跨 App 服务，会增加授权、启动、故障和版本依赖。每 App 随包发布固定词库更适合首版。

当前词库使用原生 `size_t` 和结构布局，不能把主机 64 位字典复制给 ARM32，也不能把该 `.dat` 当作跨平台交换格式。未来若做共享升级，应使用版本化不可变文件、可信来源校验、私有缓存及回退，不能原地覆盖正在使用的文件。

### 6.3 个人词库只选一种后续路线

轻量路线是授权后交换有版本、记录 ID 和修订语义的 UTF-8 词条，定义幂等合并与删除，不直接交换原始正文或反复累加累计频次。

强一致路线是单写入服务独占数据库，App 使用受限协议提交增量；需另验证普通身份 IPC、后台运行、冷启动、恢复和延迟。该服务也不是系统输入法。

当前上游只有进程内互斥与缓存写回；外加文件锁不能合并不同进程中的旧内存副本。原始 `userdict.dat` 多进程共写首版禁止。既有审计见 [模块可靠性与共享报告](C:/Users/dove1/Documents/BBIME/research/MODULE_RELIABILITY_AND_SHARING.md)。

## 7. 功能、性能与平台成本

当前支持自然码、全拼、英文、候选、应用内 Sym 及部分编辑操作，但不是所有双拼方案，也不是原生输入法能力的完整复制。现有限制包括：

- 自然码最多 32 字母，全拼展开最多 39 字节；这不是所有合法长度输入都安全的证明。
- 候选最多 40 项，歧义路径最多 8 条，未实现完整整句学习、预测或模糊音。
- 测试 Backend 的正文限制为 16384 个 UTF-16 单元，不应直接作为所有宿主业务限制。
- 现有光标处理保护代理对，但不是组合字符、旗帜和 ZWJ emoji 的完整字素簇处理。
- 系统长按圆形精细光标浮层尚未接入；普通光标移动不能称为完全等价。

既有 Q10 `0.1.0.8` 的 40 次合成解码 P95 为 3.022 ms，是有价值的起点，但不包含硬件分发、候选渲染、屏幕刷新或 WebView 桥接。它不是新版、多字段或新宿主的端到端延迟。

建议试点分别测量：首次启用时间、按键到候选、选词到正文、长文选区替换、内存峰值、后台恢复和连续输入。可先约定端到端 P95 <= 50 ms、P99 <= 100 ms 作为目标，再以真机基线调整；这两个数值是建议验收目标，不是已测结果。

构建沿用各 App 已验证的 ARMv7 / BB10 C++ ABI。现有脚本使用 `4.6.3,gcc_ntoarmv7le_cpp` 并检查 `libcpp.so.4`，不能混用 GNU `libstdc++`。首版不要求另外安装共享 `.so` 或维护 root 服务。

若目标扩大到 Windows、Android 或 iOS，应重新编译核心、生成目标架构字典，并各自建立编辑、键盘和生命周期适配。当前主机测试只是可移植性的部分证据，不是已完成这些平台 SDK 的证据。

发布包保留引擎许可证、版权及适用的 NOTICE/修改声明；当前源码和所用 Debian 二进制的版权元数据标注 Apache-2.0。以后新增词库、图标或引擎应单独核对来源与许可，不能把现有引擎的许可推广到其他资源。

## 8. 分阶段实施与投入估计

以下为一位熟悉这些项目的开发者、现有 SDK 和 Q10 设备可用时的规划估计，不是排期承诺。未开展完整引擎审计，第一阶段问题复杂度可能扩大。

| 阶段 | 交付物 | 参考投入 | 完成闸门 |
| --- | --- | --- | --- |
| A：风险收敛 | 引擎边界修复、固定失败用例、回归基线 | 2-5 人日；根因复杂时重新估算 | 主机完整回归及 ARM 合成测试通过 |
| B：最小原生模块 | 唯一服务、双字段会话、TextArea/TextField Adapter、候选组件 | 4-7 人日 | 字段隔离、敏感字段拒绝接入、提交失败可恢复 |
| C：两个原生宿主试点 | BBFile 及另一 App 的普通原生字段 | 3-5 人日 | 快捷键、业务保存、安装升级和端到端延迟通过 |
| D：发布加固 | 压力测试、生命周期、回退、共享设置可选验证 | 3-5 人日 | 两个普通身份 App 的完整验收 |
| E：BBnote 正文 | CodeMirror 协议、事务提交、保存与焦点桥接 | 另加 5-10 人日 | WebView 硬件事件、防重复输入、撤销及快照全部通过 |
| F：IntroOP | 业务键路由、字段白名单、隐私策略和业务回归 | 另加 3-6 人日 | 不能产生额外事件、误采集或改变剂量/时间字段 |

原生双 App 最小可靠版本约 12-22 人日；含 BBnote 正文和 IntroOP，需要进一步预算，不能理解为“每个 App 加一个文件就完成”。个人词条共享、跨 App headless 服务和其他操作系统移植不包含在这些估计中。

## 9. 发布前验收与回退

| 验收项 | 必须证明的结果 |
| --- | --- |
| 引擎边界 | 固定断言用例、邻近输入、随机和重复查询不崩溃；不能只测试 `nihk` |
| 多字段 | A 预编辑 -> B，不串字；旧候选/按键释放不作用于新字段 |
| 业务键 | 选词不保存/导航；保存、采集、返回只执行一次 |
| 编辑 | 中文/英文混合、选区替换、退格、撤销、长度拒绝及只读切换正确 |
| 密码及敏感字段 | 不创建输入会话，不学习，不记录，不导出 |
| 生命周期 | 菜单、弹窗、后台、休眠、控件销毁与重启均无残留会话 |
| WebView | 无原生 IMF/模块双输入；消息过期被拒；保存使用最终文本 |
| 设置共享 | 两个不同普通 App 身份验证；权限拒绝/撤回和损坏文件不破坏本地输入 |
| 存储故障 | 字典打不开、个人词库损坏、磁盘满时不损伤宿主文档 |
| UI 与性能 | Q10 720x720 实机候选、符号及业务栏无重叠，测量端到端延迟与内存 |

当前 BBIME 的“暂停”保持 Custom 且不输入，不等于生产宿主的故障回退。宿主应保留原有编辑路径，并在模块初始化失败、用户停用或适配失败时，受控解绑模块后恢复原字段模式；该切换必须处理未提交编码和按住键，防止双路径同时输入。

但普通同进程代码无法在已经触发致命断言后再执行回退。先消除可复现的断言风险；若未来要求引擎崩溃绝不影响宿主，需要另评估进程隔离及其性能、权限、部署成本。

## 10. 最终建议

**立项方向：自有应用内的公共双拼输入模块。当前阶段：可开展组件化和试点，不可直接认定为稳定可批量发布。**

首版范围收紧为：原生文本适配、进程唯一引擎、多字段隔离、候选与 Sym、宿主可配置键位、私有词库、可选偏好导入。先修复本轮回归失败，再完成两个真实 App 的接入验收。

不要把共享词库服务、系统输入替换或跨平台全覆盖作为首版依赖。BBnote 正文与 IntroOP 分别是 WebView 和业务快捷键/敏感字段两类独立适配工作，应保留各自闸门。

## 11. 依据与复核边界

本地实现与本轮执行：

- [Decoder](C:/Users/dove1/Documents/BBIME/src/decoder.cpp)、[Backend](C:/Users/dove1/Documents/BBIME/src/backend.cpp)、[ModuleSettings](C:/Users/dove1/Documents/BBIME/src/modulesettings.cpp)、[主机回归](C:/Users/dove1/Documents/BBIME/tests/decoder_test.cpp)。
- [本轮验证与源码哈希](C:/Users/dove1/Documents/BBIME/research/multi-app-feasibility-validation-2026-10-01.json)；状态记录是本轮结果，不覆盖既有版本的安装和运行历史。
- [BBFile UI](C:/Users/dove1/Documents/BBFile/assets/main.qml)、[BBnote UI](C:/Users/dove1/Documents/BBnote/assets/main.qml)、[CodeMirror 桥](C:/Users/dove1/Documents/BBnote/assets/MarkdownEditor.qml)、[IntroOP UI](C:/Users/dove1/Documents/BBarmin/bb10-native/RecordingPage.qml)。
- [BBattery 项目说明](C:/Users/dove1/Documents/BBattery/README.md)、[Windows 管理器项目说明](C:/Users/dove1/Documents/BBIntroOP/README.md)。

本轮重新查阅的官方 BB10 SDK：

- [TextField Custom 模式](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/resources/textfieldinputmode.h:109)。
- [TextArea Custom 模式](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/resources/textareainputmode.h:68)。
- [KeyListener 事件传播](C:/Users/dove1/Documents/BBarmin/sdk/target_10_3_1_995/qnx6/usr/include/bb/cascades/core/keylistener.h:24)。
- 官方离线 `C:/bbndk/plugins/com.qnx.doc.native_sdk.devguide_4.0.0.20150226/doc.zip` 的 `topic/r_barfile_dtd_ref_permission.html`，确认 `access_shared` 用户权限描述符示例。
- [引擎许可证](C:/Users/dove1/Documents/BBIME/vendor/libgooglepinyin-0.1.2/LICENSE)、[Debian 版权元数据](C:/Users/dove1/Documents/BBIME/build/debian-arm/usr/share/doc/libgooglepinyin0/copyright)。

在线检索尝试未取得本轮可引用的 BB10 原始 API 页面；BB10 控件和权限结论以实际安装的官方旧版 SDK 为准，未用现代 QNX/Qt 文档推定此固件行为。CodeMirror 可复核的官方手册地址为 `https://codemirror.net/5/doc/manual.html`，本文对应接口需以 BBnote 包内的固定版本及 Q10 运行结果再核实。

本轮只读查看源码、运行本地主机测试并撰写报告。没有修改业务源码、部署 BAR、登录手机、改系统键盘/权限、读取实际输入正文或执行跨 App 输入实验。实际设备授权、WebView 事件、新版 UI、ARM 失败串及多 App 共存仍是未完成的验收，不作为 PASS。
