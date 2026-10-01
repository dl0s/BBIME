# BBIME 宿主接入规范与标准流程

适用基线：**0.1.0.15**。本规范描述共享模块和将来的宿主接入要求。
**BBnote、IntroOP、BBFile 仍暂停同步；本文不授权更新三款应用的源码、固定快照、BAR 或设备。**
此前[宿主检查记录](../research/HOST_INTEGRATION_REVIEW_2026-10-01.md)属于历史证据，
不能作为当前基线已同步、已部署或已完成实体键/UI 验收的结论。

本基线只公开 natural/english；默认资源不含自定义 Sym 界面。
`NativeController` 在有效输入会话中消费 Sym 并抑制 Shift 单按释放，不打开面板或输出符号。
研究用 `NativeSymbolPanel.qml` 不属于默认导出，也不是当前接入建议。
共享 `bbime::EditorFocusGate` 与本工程 `Backend` 的应用示例接线必须分开理解：
纯策略不连接宿主信号、不检查 UI、不启用控制器、不路由按键，不能替代完整接入。

## 1. 输入、右上角开关与候选底栏

公开输入、启动配置和 UI 只允许 `natural`（自然码）和 `english`（英文）。
宿主桥、InputSession/NativeController 拒绝 `full`、`system` 和未知模式。
引擎内部拼音展开和全拼回归用于自然码解码及崩溃保护，不增加第三种模式。

| 当前状态 | 右上角显示 | 点击后 | 语言 |
| --- | --- | --- | --- |
| 自然码启用 | 中，无图标 | 手动暂停输入，显示菜单 | 保留 natural |
| 英文启用 | EN，无图标 | 手动暂停输入，显示菜单 | 保留 english |
| 输入暂停 | 菜单图标，无文字 | 显式恢复许可，关闭菜单并重评估合法焦点 | 保留原语言 |

使用默认导出的 `ImeToggle.qml` 与 `ime-menu.png`。按钮固定 **76×64**、
`FocusPolicy.None`，图片独立固定 **36×36**，放在标题/工具行最右端。
preferred/min/max 尺寸必须从创建时生效，不依赖原生 Button 默认高度或图片自然尺寸。
组件只显示绑定状态并发出 `toggleRequested`，不保存第二份 enabled，不改变语言。
菜单图标和应用菜单表现同一个暂停/恢复状态。Alt+Enter 或两选项选择器单独切换语言，暂停时禁用。

```qml
ImeToggle {
    inputEnabled: ime.enabled
    inputMode: ime.mode
    enabled: ime.ready || ime.enabled
    onToggleRequested: host.toggleInputPause()
}
```

`host.toggleInputPause()` 是宿主接口：调用焦点策略 pause/resume 后重评估，不能简单执行
`ime.enabled = !ime.enabled`。本例要求 NativeController ready；独立英文故障路径只有在宿主
实际实现并验证后才能使用，不能以示例属性推定存在该能力。
资源路径变为 `assets/ime/` 等时，将确定性路径变换记入导入清单和补丁，不能手改已锁定快照。

候选固定 **72 px**，使用无焦点 `CandidateStrip`，置于页面内容竖向 StackLayout 的**最后一行**。
包含容器 `bottomPadding: 0`，正文/滚动编辑区域 `spaceQuota: 1`。
候选占用真实布局空间，不覆盖正文；候选之后不能再放状态栏、按钮或页脚。
ActionBar 在输入启用时隐藏，在暂停时显示，候选与它互斥；页脚等需要保留的控件放在候选之前。

```qml
Page {
    id: page
    property variant ime
    actionBarVisibility: ime.enabled ? ChromeVisibility.Hidden : ChromeVisibility.Visible
    Container {
        layout: StackLayout { orientation: LayoutOrientation.TopToBottom }
        bottomPadding: 0
        Container { /* 标题、工具和最右端 ImeToggle */ }
        Container {
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            /* 正文/滚动编辑区域 */
        }
        CandidateStrip { ime: page.ime } // 72 px，最后一行
    }
}
```

验收要检查创建首帧、输入中和暂停后布局。只检查 preferredHeight 或截图中的字体，
不能证明实际预留了底栏空间；还须核对正文底边、候选顶边及 ActionBar 状态。

## 2. 建立基线与字段矩阵

1. 读取适用 AGENTS.md，记录 BBIME/宿主 HEAD、版本、工作区状态和已有未提交修改。
2. 记录可回退 BAR 的版本、字节数及 SHA-256，保留旧快照、清单和补丁，不覆盖其他工作。
3. 按字段列出页面、稳定 ID、text/search/path 或宿主 Literal、长度、单/多行、学习授权、提交及敏感性。
4. 默认关闭学习和设置共享；不用真实正文、用户词库、连接配置、token 或设备日志充当测试数据。
5. 只向宿主 GUI 接入服务，不向 CLI、文件 worker、root 服务或系统输入协议接入。

最小内容为 Decoder/InputSession/ImeService、ModuleProfile、原生适配器/控制器、位置及相关头文件、
`src/focusstate.h`、引擎必要源码、ARM32 字典、候选/开关组件及许可证。
`module/BBIME-Sources.ps1` 的 `BBIMEPolicyHeaders` 导出纯策略头，默认资源不含自定义 Sym 面板。
ModuleSettings 是可选能力；未使用它的宿主可裁剪其持久化实现，不因接入输入法申请 `access_shared`。
不要复制测试 Backend/main、测试页面、研究面板、诊断截图或部署工具到宿主运行路径。

| 字段/原生状态 | 接入要求 |
| --- | --- |
| 普通 text | Plain、可编辑、启用且在当前合法作用域；学习需本地和字段双重授权 |
| search/path | ASCII 标点，不学习 |
| 宿主 Literal | 宿主单独英文直写白名单；不加入中文解码或学习，不构成第三种模式 |
| 密码/数值/敏感/未知策略 | 拒绝注册和接管 |
| 非 Plain、只读、禁用、隐藏/失效页面 | 不授予输入许可 |
| TextField 原 inputMode | 可临时租用 Default/Text/Chat/Custom |
| TextArea 原 inputMode | 可临时租用 Default/Text/Custom |

BBFile 的 mode/uid/gid/ACL 等技术字段和 BBnote 的账号/URL 等直写字段不能因为原生 inputMode 为
Custom 就扩大中文白名单。策略允许、真实焦点和可编辑性是不同条件，必须分别判断。

## 3. 固定快照与版本迁移

以下步骤只描述将来另行授权的宿主任务，不解除三款宿主当前暂停状态。

1. 根据清单选择必需文件，记录源提交、0.1.0.15 版本、dirty 状态及源/目标原始字节 SHA-256。
2. 宿主差异集中于适配层或独立版本化补丁，记录资源路径、换行、演示私有桥删除等基础转换。
3. 更新前校验现有快照和旧补丁哈希；出现漂移先解释差异，不用新哈希掩盖手工修改。
4. 在 staging 应用转换与补丁，执行 `git apply --check` 或精确匹配检查，全部成功后才替换快照。
5. 普通构建只使用宿主锁定的快照，不读取兄弟项目活动源码；归档旧清单和回退资源。

从旧版本迁移时需复核：

- 删除 `ModuleProfile.chineseMode` 和暂停→自然码→英文的三态按钮调用。
- 使用 profile/version=2，仅 natural/english。已知私有 v1 只在本地迁移 full→natural，
  保留合法英文/行为偏好和本地学习授权；下次保存写 v2。共享 v1 及未知键/版本/值整体拒绝。
- 删除旧补丁中已成为共享模块能力的 Custom/两模式重复片段，重新验证宿主 Literal、业务键和事件独占差异。
- 导入 `BBIMEPolicyHeaders`、共享开关和候选资源，移除运行中的研究 Sym 面板及其 attachedObjects/路由。
- 按共享策略补齐焦点、手动暂停、菜单/Sheet、前台/awake 与首键重评估，不能只换头文件。
- 对全部源/目标/补丁/资源重新锁定哈希，设备结果绑定最终包；旧版本 PASS 不沿用。

本规范不提供针对 BBnote、IntroOP、BBFile 的更新执行命令；将来具体操作须属于各自有效任务范围。

## 4. 初始化、真实焦点与生命周期

服务在同一 GUI 输入线程创建、打开、调用和销毁；每进程一个服务、每字段一个会话。
先创建服务再创建控制器，销毁顺序为会话/控制器 → 服务。异步编辑器桥使用同一服务串行解码。
启动顺序：打开可信包内字典和宿主私有词库 → 原生位置探测/自检 → 严格配置本地策略/模式 →
暴露控制器及宿主桥 → 创建并注册场景字段/设置 scene → 创建回调完成后重评估真实焦点。
隐藏页面创建回调不能覆盖当前场景许可；按钮、菜单、候选和冲突 Shortcut 绑定同一输入状态。
初始化失败保持停用并说明自然码不可用；仅可使用已实现且已验证的宿主英文路径，不能另开 Decoder owner。

共享策略的调用边界如下，`host.*` 全部由宿主实现：

```cpp
#include "focusstate.h"
// HostIme 的持久成员：bbime::EditorFocusGate focusGate;
void HostIme::reconcileFocus() {
    QObject *actual = host.actualFocusedEditor(); // 不先用 eligibility 过滤
    const bool foreground = host.foregroundAndAwake();
    const bool scope = host.editorScopeActive();
    if (foreground && scope) focusGate.observe(actual);
    controller.setEnabled(focusGate.permits(
        controller.ready() && host.eligibleEditor(actual), foreground, scope, actual));
}
```

`observe()` 只观察实际焦点身份；ready、字段资格、前台及作用域分别传给 `permits()`。
`eligibleEditor` 须核对已注册、当前 scene/页面、白名单、可见/启用及可编辑状态。
同 owner 的观察不清手动暂停；真正新 owner/焦点 epoch 才可清除。
不要把 `eligible && focused` 当作 owner，也不要因为菜单、后台或场景暂不可用就调用 `observe(0)`：
这会把资格/生命周期变化误当成离焦，造成自动反弹。
当前实际焦点为空且处于可观察的前台编辑作用域时，才记录真实离焦。
适配层需保护 owner 生命周期，控件销毁/重新创建不能依赖悬空地址充当稳定身份。

| 宿主事件 | 必须完成的接线 |
| --- | --- |
| 合法字段获得/取消焦点 | 观察实际 owner，独立判断资格，重评估并启用/停用 |
| 字段间转移 | 合并同轮焦点通知；取消旧会话，不向新字段写旧编码 |
| 首个键事件 | enabled 检查前同步核对实际焦点、资格、作用域，防止首键丢失 |
| 手动暂停/恢复 | 调用 pause/resume，保留语言，再重评估；必要的焦点恢复仅针对仍合法字段 |
| 菜单打开/关闭 | 打开前使作用域失效；关闭后重评估，不能无条件清暂停或强制启用 |
| Sheet opening/open/closing | 打开调用前先禁用；整个过渡保持失效；关闭完成只释放该 Sheet 的作用域 |
| 导航/隐藏/销毁/scene 更换 | 主动停用并清旧会话票据，取消按键状态，重新注册/核对新作用域 |
| 前台/后台与 awake/asleep | 以当前实际前台且 awake 为许可；恢复重评估，不以生命周期通知清手动暂停 |
| 只读/启用/可见性/策略变化 | 仅重评估资格，实际焦点身份独立观察，不能伪造失焦 |

`EditorFocusGate` 没有宿主信号连接或自动 setEnabled。BBIME `Backend` 是以上机制的应用示例，
未由模块清单导出；复制该纯头文件或通过策略单测不代表已有完整宿主接入。
同时存在原生和异步编辑路径时，必须互斥使用服务，不能竞争活跃会话。

适配器激活时将合格原模式临时租为 Custom，离焦/停用/注销/销毁释放时恢复原 inputMode。
预设 Custom 释放后仍为 Custom；原 Default/Text/Chat 按对应控件支持范围恢复。
宿主在租约期间另改模式时，释放必须保留后续修改。
VirtualKeyboardOff、内置快捷键、原生文本菜单和浏览键策略由宿主维护，不能归给纯焦点策略或模式租约。
输入启用时设置 `keysIgnoreFocusInActionBar`，按独占策略处理 `builtInShortcutsEnabled` 和冲突 Shortcut；
不追加 Custom 不支持的 prediction/spellcheck flags。未接入的密码等字段保留宿主原有策略。

## 5. 唯一按键入口、提交与异步正文

宿主唯一入口先重评估真实焦点，再处理 Alt+Enter、Alt+Backspace、Ctrl/业务键，然后调用 NativeController。
模块将这些业务组合交回宿主；独立 Shortcut 不能再次执行同一事件。
切换/提交仅在首次按下执行，正确消费对应重复/释放；暂停时不得编辑内容或切换语言。
独占输入宿主必须消费被屏蔽内容事件，避免另一路原生/影子控件写入。

单独 Alt/Ctrl/CapsLock/Meta 在 Unicode 转换前消费，无字符输出。
左右 Shift 单按按本地偏好移动候选/光标，组合、长按、跨字段释放不触发动作。
Sym 在默认基线中消费后无动作；若此前按住 Shift，Sym 按下使其释放不再算单按。
不接入研究面板或把兼容符号接口误当作已支持的符号功能。

原生字段保存/Search/Next/Done 前调用 `prepareSubmit(target)`，成功后再读取最终 text。
false 表示中止保存/导航，保留可重试预编辑。单行 Enter 与 Shift+Enter 只派发一次业务操作；
多行 Enter 插入换行。正文、选区、撤销及持久化属于宿主，模块通过 TextEditor 替换选区，不能 setText 重写全文。
候选用 session/revision/document 票据；过期选择拒绝，长度或写入失败不学习，回调中退休绑定延后释放。
UTF-16 代理对保护不等于完整字素簇编辑保证。

CodeMirror 等异步正文需独立接入并验收：

1. 请求携带 session、epoch、请求序号、文档修订和选区，同一队列串行执行。
2. 返回提案再次核对这些值，拒绝旧文档、旧焦点、旧选区和旧语言的结果。
3. 在真实编辑器中用一次事务替换目标选区并保留撤销，不能向原生影子字段写入后宣称正文已提交。
4. 保存/暂存先完成预编辑并等待最终文档快照；失败/超时中止保存或关闭，销毁后拒绝回调。
5. Markdown/path/search 保持 ASCII 标点、不学习；原生字段回归不能代替 WebView 正文验证。

## 6. 本地验证、ABI 与包装

以下入口属于将来相应任务中的验证步骤，本文档更新不执行构建或部署：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-PhaseAB.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-SdkApi.ps1 -OutputPath build/audit/sdk-api.json
```

SDK 读取顺序为显式 `-SdkRoot` → 用户 `INTROOP_SDK_ROOT` → 进程同名变量。
BAR 包装单独解析兼容 Java 7/8：显式 `-JavaBin` → 用户/进程 `INTROOP_JAVA_BIN` → 已安装 JRE。
工具可查找 SDK 或同级 Momentics 的 JRE，最后检查 JAVA_HOME/PATH；不固定版本目录，
不把包装 JRE 当作编译器 SDK。Java 17 与旧包装器兼容性必须按实际运行结果判断。
原生目标 `4.6.3,gcc_ntoarmv7le_cpp`，ARM ELF 使用 `libcpp.so.4`，拒绝 GNU libstdc++。
当前包内 ARM 字典 SHA-256：`179311C55AB9B912A07EF040E5A95AF97E704E2248B7B97735F17A3E05D13B67`。
主机测试使用独立生成的 64 位字典和合成用户文件，不覆盖 ARM 字典。

验证须包括源码/快照、唯一提交入口、QML、核心回归、ARM 构建、ABI 和 BAR 内容。
覆盖两模式及非法模式拒绝、无/有预编辑、选区/emoji、旧票据、写入失败重试、拒绝字段、
手动暂停保持、资格变化、真实失焦/重新获焦、字段首键、菜单/Sheet 全过渡、后台/休眠、模式租约恢复。
布局检查覆盖首帧 76×64/36×36 开关、72 px 最后一行候选、ActionBar 互斥和正文不遮挡。
按键检查包含单独修饰键、Shift 组合和 Sym 无动作，不能把移除入口写成符号功能验收通过。
BBnote 另需异步提案、保存等待、撤销、Markdown 和长文档回归；既有业务、CLI/root 隔离也须验证。
记录实际 PASS/FAIL/NOT_RUN、版本及源码/资源/补丁/最终 BAR 哈希，不能沿用旧包结果。

设置启用时验证严格值/版本拒绝、私有迁移、共享无权限、坏文件、原子写入与写入失败保护。
学习及词库测试只用合成数据。引擎 GPL/LGPL 适用许可、COPYRIGHT 与本地修改声明随包保留；
第三方资源来源和修改记录不得因裁剪默认 Sym UI 而丢失。

## 7. 设备验收、归档与提交

设备操作按对应宿主任务的有效授权执行；另一应用的部署授权不能自动扩展。
当前三款宿主保持暂停，本文档不是同步、部署或恢复工作的授权。
将来部署须核对应用身份、版本、上传回读哈希、安装结果和运行资源，保留原数据及服务。
用普通应用身份、当前 QML 和真实 TextField/TextArea/WebView 验证菜单、焦点、候选与保存。
SDK 合成 KeyEvent、SSH root 核心回归、纯策略测试及 mock 测试分别记录，不能替代实体键/UI 验收。

`Test-PhaseAB.ps1 -Device` 仅将合成核心程序和 ARM 字典上传到新的 `/tmp/bbime-validation/`，
不安装 BAR、不读真实正文/用户词库、不修改权限或服务，因此不证明宿主已完成接入。
人工验收包含 720×720 首帧开关/底栏、触屏选词、物理 Shift/Alt/Sym、长按/连击、
选区/撤销、首键/字段转移、菜单/Sheet、后台/锁屏、写入失败和长文档持续输入延迟。
只有当前最终包满足全部适用闸门，才可写 `releaseReady=true`；未执行项保留 NOT_RUN。

归档包含需求、宿主/模块版本和提交、dirty 状态、导入清单/补丁哈希、字段矩阵、
冲突与修复、验证/未执行项、安装回读、人工结论及回退步骤。
提交仅含已审阅的接入源码、固定快照、补丁、必要资源、工具和文档，排除 build、凭据和真实正文/词库。
用户要求提交时在验证后完成本地 commit，推送和部署按任务范围执行。
回退使用记录的旧包/快照并保留身份与数据；存在无关未提交修改时不宽泛 reset。
