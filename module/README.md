# 最小原生输入模块

源码基线：`0.1.0.15`。这是源码嵌入模块，服务和输入会话由各宿主进程持有。
公开模式只有 `natural`（自然码）和 `english`（英文）。默认导出不含自定义 Sym 界面；
`NativeController` 在有效输入会话内消费 Sym，不打开面板、不插入符号。
研究用 `NativeSymbolPanel.qml` 不在默认资源清单中，不能据此认为宿主已有符号输入能力。

**BBnote、IntroOP、BBFile 继续暂停同步。** 本文及[标准接入规范](INTEGRATION_WORKFLOW.md)
说明将来的接入要求，不授权更新这三款应用的快照、源码、包或设备。
编译、纯策略测试、SDK 合成 KeyEvent 和真实宿主的物理键盘/UI 验收分别记录。

## 所有权与可靠性

- 每进程只创建一个 `ImeService`，在同一指定输入线程创建、使用和销毁。
- 每字段一个 `InputSession`；`NativeController` 为已注册的原生控件管理会话。
- 宿主拥有正文、选区、撤销、保存、权限、业务快捷键和字段白名单。
- 字段切换默认取消旧字段的未提交编码；后台、休眠、页面失效和模态作用域必须释放输入。
  生命周期恢复时重新核对真实焦点，保留该焦点上的手动暂停，不能无条件抢焦点。
- 字典使用该宿主 BAR 内可信的 ARM32 副本，个人词库使用宿主私有路径。
  不跨进程共写个人词库，不要求共享文件权限或 BBIME 应用正在运行。
- 控制器和全部会话先于服务销毁；关闭或重开服务前先停用控制器。
  控件销毁自动解绑，宿主仍负责隐藏页面、导航、菜单和 Sheet 的生命周期接线。

## 导入与初始化

构建脚本引用 `module/BBIME-Sources.ps1`。源文件、moc 头、包含目录、资源和许可证清单
均为绝对路径；`BBIMEPolicyHeaders` 单独导出 `src/focusstate.h`。
原生目标使用 BB10 `libcpp.so.4` ABI。资源放入宿主 `assets/`，保留引擎许可证、版权及修改声明。
默认资源包含 `CandidateStrip.qml`、`ImeToggle.qml`、菜单/取消图标和 ARM 字典。
不要编入测试应用的 `Backend`、`main.cpp`、研究符号面板、草稿或诊断代码。

```cpp
// service 必须比 controller 活得更久；仅在指定输入线程执行。
bbime::ImeService service;
bbime::NativeController controller(service);
if (service.open(packagedDictionary, appPrivateUserDictionary) &&
    controller.initialize() && controller.selftest() &&
    controller.configure(localProfile, localLearningConsent)) {
    controller.setMode(QString::fromStdString(localProfile.startMode));
    // 暴露 controller 和宿主自己的接线接口，再创建/注册场景。
    // 创建回调完成后，由真实焦点重评估决定是否启用。
}
```

`configure()` 只在控制器停用且无活跃会话时接受严格校验后的偏好，不改变运行中的语言。
启动模式由宿主显式设置；`full`、`system` 和未知模式必须拒绝。
解码器内部展开拼音及全拼回归是引擎实现，不增加第三种公开模式。
可选的 `ModuleSettings` 使用 profile/version=2，已知私有 v1 的 `full` 可本地迁移为 `natural`；
下一次保存写 v2。共享 v1、未知键/版本/值整体拒绝，共享偏好不能授予学习或字段权限。
设置写入保留原子替换及失败不覆盖有效文件的约束；未使用共享设置时不申请 `access_shared`。

## 共享焦点策略与宿主接线

`bbime::EditorFocusGate` 是 `src/focusstate.h` 中无 UI 依赖的纯策略。
它保存观察到的焦点 owner 和手动暂停状态；相同 owner 的重评估保留暂停，真实 owner 变化才清除暂停。
它不会连接信号、检查 UI、启用控制器、路由按键，也不能单独构成完整接入。
本工程 `Backend` 使用该策略的接线是应用示例，不是模块默认导出。

```cpp
#include "focusstate.h"
// HostIme 的持久成员：bbime::EditorFocusGate focusGate;

// 宿主适配层成员方法示意；host.* 均由宿主实现。
void HostIme::reconcileFocus() {
    QObject *actual = host.actualFocusedEditor(); // 只读取真实焦点身份
    const bool foreground = host.foregroundAndAwake();
    const bool scope = host.editorScopeActive();
    // 菜单/Sheet/后台通知不能伪造 observe(0) 清除手动暂停。
    if (foreground && scope) focusGate.observe(actual);
    const bool readyAndEligible = controller.ready() && host.eligibleEditor(actual);
    controller.setEnabled(focusGate.permits(readyAndEligible, foreground, scope, actual));
}
```

实际焦点身份与可编辑性必须分别计算。不要用 `eligible && focused` 生成 owner：
只读、禁用或可见性变化会被误判为离焦，随后清掉手动暂停。
`eligibleEditor` 应核对字段已注册、当前 scene/页面、白名单、可见/启用及可编辑状态。
字段转移可合并到事件循环末尾重评估，但唯一按键入口必须在 enabled 检查前同步核对焦点，
避免新字段的首键丢失或进入旧会话；不能因此激活隐藏、未注册或敏感字段。

宿主必须接入字段焦点/销毁/可用性、页面与 scene、菜单、Sheet opening/open/closing、
前台与 awake、首键以及手动暂停。打开菜单或 Sheet 前先禁用输入，直到其关闭完成并释放自己的作用域。
关闭菜单、回到前台或关闭 Sheet 时重新计算许可；相同真实焦点的手动暂停仍有效。
右上角开关通过宿主接口调用 `focusGate.pause()` / `resume()` 并重评估，不能直接翻转控制器 enabled。
显式恢复输入时仅可按宿主规则恢复仍有效的原字段焦点。

```qml
ImeToggle {
    inputEnabled: ime.enabled
    inputMode: ime.mode
    enabled: ime.ready || ime.enabled
    onToggleRequested: host.toggleInputPause()
}
TextField {
    id: search
    textFormat: TextFormat.Plain
    input.flags: TextInputFlag.VirtualKeyboardOff
    onCreationCompleted: ime.registerEditor(search, "search", false)
    onFocusedChanged: host.scheduleFocusReconcile()
    keyListeners: [
        KeyListener {
            onKeyEvent: host.routeKey(search, event)
        }
    ]
}
```

`host.toggleInputPause()`、`host.scheduleFocusReconcile()` 和 `host.routeKey()` 是宿主接口示意。
`routeKey` 必须先核对真实焦点，再统一处理业务键、模块键及被屏蔽事件的消费，不能建立第二条输入路径。
示例中的信号仅展示入口，完整生命周期接线仍须满足上述要求。

## 字段与输入模式租约

`registerEditor(control, policy, learn)` 只接受 `text/search/path`。
`text` 可按偏好使用中文标点；`search/path` 保留 ASCII 标点且不学习。
学习同时需要本地开关和该字段授权。密码、数值、敏感、未知策略、非 Plain、只读或禁用控件拒绝接管。
宿主 Literal 直写路径必须保持自己的白名单，不能视作第三种语言或扩大中文注册范围。

适配器在有效会话内临时将合格控件租为 Custom，生命周期释放时恢复原 inputMode。
预先配置为 Custom 的控件释放后仍为 Custom；TextField 的 Default/Text/Chat、
TextArea 的 Default/Text 可临时租用。宿主在租约期间改成其他模式时，释放代码保留其后续修改。
VirtualKeyboardOff、原生文本菜单及冲突内置快捷键仍由宿主管理；适配器不代管这些宿主策略。
输入启用时设置 `keysIgnoreFocusInActionBar`，按独占策略处理 `builtInShortcutsEnabled`，
禁用冲突 Shortcut；不增加 Custom 不支持的 prediction/spellcheck flags。
焦点启用和离焦停用由宿主接线决定，不能把恢复原 inputMode 等同于自动启用系统输入。

通过 `TextEditor::insertPlainText()` 替换选区，不用 `setText()` 重写整份正文或实现第二套撤销。
TextArea/TextField 分别探测位置单位，保护 UTF-16 代理对，不宣称完整字素簇编辑。
候选携带 session、预编辑 revision、文档/选区 revision；旧票据拒绝。
长度或写入失败不学习，失败候选保留编码供重试。
宿主回调关闭控件或切换字段时，正在执行的绑定延后释放，不将旧结果写入新字段。

## 按键、提交与布局

单按左右 Shift 按本地偏好移动候选或光标；组合、长按及跨字段释放不执行该动作。
单独 Alt/Ctrl/CapsLock/Meta 在 Unicode 转换前消费，不能输出非标准字符。
默认 Sym 消费后无动作，并抑制仍按住的 Shift 在释放时被误当成单按。
兼容符号接口保留，但不提供可用面板或符号提交；宿主不要绑定研究面板。

Ctrl 组合、Alt+Enter、Alt+Backspace 留给宿主唯一入口。Alt+Enter 可切换 natural/english，
暂停时不编辑内容或切换语言；右上角开关只暂停/恢复，保留语言。
单行 Enter 及 Shift+Enter 完成预编辑后发出一次 `submitRequested`，宿主定义 Search/Next/Done/Save；
多行 Enter 插入换行。保存或导航前调用 `prepareSubmit(targetControl)`，返回 false 时中止操作。
正文、选区、撤销和持久化仍归宿主；独立 Shortcut 不能重复处理同一事件。

`ImeToggle` 是无焦点的固定 Container：76×64，图片单独固定 36×36，位于标题行最右端。
初始布局就约束 preferred/min/max 尺寸；不要依赖原生 Button 的初始高度或图片自然尺寸。
`CandidateStrip` 无焦点且固定 **72 px**；必须是页面竖向布局的**最后一行**，与 ActionBar 互斥。
正文占弹性空间，底栏真实预留空间，不可浮盖已有文字；候选之后不再放按钮、状态栏等控件。

```qml
Page {
    id: page
    property variant ime
    actionBarVisibility: ime.enabled ? ChromeVisibility.Hidden : ChromeVisibility.Visible
    Container {
        layout: StackLayout { orientation: LayoutOrientation.TopToBottom }
        bottomPadding: 0
        Container { /* 标题、工具及 ImeToggle */ }
        Container {
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            /* 正文/滚动编辑区域 */
        }
        CandidateStrip { ime: page.ime } // 最后一行，组件内固定 72 px
    }
}
```

布局示意中的 `page.ime` 由宿主页面提供。候选和按钮状态绑定同一个控制器状态，
不保存第二份 UI 开关；单独文本/图片不会代替焦点与许可判断。

## 验证与范围

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-PhaseAB.ps1
# 仅在对应任务已授权时，额外运行配置设备上的 ARM 合成核心测试：
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-PhaseAB.ps1 -Device
```

验证脚本输出必须绑定实际版本、源码/资源/字典/包哈希，并如实标注 PASS/FAIL/NOT_RUN。
`-Device` 只上传合成测试程序和包内 ARM 字典到新的 `/tmp/bbime-validation/` 目录，
不安装 BAR、不读真实正文或词库、不修改服务/权限。SSH root 核心回归不能替代
普通应用身份的 UI、真实焦点、实体按键、首帧尺寸、长文档延迟或保存验收。
主机测试使用独立 64 位字典，绝不能覆盖包内 ARM32 字典。

BBIME 的旧编辑页和原生模块示例互斥使用同一服务；示例不是三个宿主的完整实现。
BBnote 的 CodeMirror 桥仍需单独验证异步事务、真实正文写入、撤销及保存等待。
后续接入工作须另行确定宿主任务范围；本文不解除 BBnote、IntroOP、BBFile 的暂停状态。
