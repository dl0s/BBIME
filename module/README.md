# 最小原生输入模块

源码版本：`0.1.0.12`。这是源码嵌入模块，不是系统输入法或独立跨应用服务。
测试证据由 `tools/Test-PhaseAB.ps1` 写入 `research/phase-ab-validation-0.1.0.12.json`。
编译成功不表示原生控件、硬件键盘或两个真实宿主已经完成验收。
完整操作过程、右上角按钮状态表和升级兼容要求见 [标准接入规范](INTEGRATION_WORKFLOW.md)。
BBFile/BBnote 的接入已完成源码工作，设备与按钮差异见 [两款宿主检查](../research/HOST_INTEGRATION_REVIEW_2026-10-01.md)。

## 所有权

- 每个进程只创建一个 `ImeService`，在同一指定输入线程创建、使用和销毁。
- 每个字段一个 `InputSession`；`NativeController` 为原生控件自动管理会话。
- 宿主拥有字段正文、选区、撤销、保存、权限、业务键和字段白名单。
- 会话切换默认取消未提交编码，不向另一字段写字。后台切出关闭模块，
  恢复后需宿主明确启用，不自动抢占焦点。
- 字典使用该宿主 BAR 的可信 ARM32 副本，个人词库使用该宿主私有路径。
  不跨进程共写个人词库，不要求共享文件权限或 BBIME 应用正在运行。
- 控制器和全部会话必须先于服务销毁。控件销毁自动解绑；页面隐藏、导航、
  模态窗口和宿主输入路径切换时仍须主动调用 `suspend()`。
  关闭或重开服务前也先暂停控制器，不能只关闭引擎而留下 Custom 字段。

## 接入

构建脚本引用 `module/BBIME-Sources.ps1`；其中源文件、moc 头、包含目录、
资源和许可证清单均为绝对路径。原生目标使用既有 BB10 `libcpp.so.4` ABI。
将组件放入宿主 `assets/`，保留许可证及本地修改声明。不要编入 BBIME
测试应用的 `Backend`、`main.cpp`、草稿和诊断代码。

```cpp
// service 必须比 controller 活得更久；只在指定输入线程执行。
bbime::ImeService service;
bbime::NativeController controller(service);
if (service.open(packagedDictionary, appPrivateUserDictionary) &&
    controller.initialize() && controller.selftest()) {
    controller.configure(localProfile, localLearningConsent);
    controller.setMode(QString::fromStdString(localProfile.startMode));
    // 将 controller 放入宿主 QML 上下文，而不是创建另一个 Backend。
}
```

```qml
TextField {
    id: search
    textFormat: TextFormat.Plain
    inputMode: TextFieldInputMode.Custom
    input.flags: TextInputFlag.VirtualKeyboardOff
    onCreationCompleted: ime.registerEditor(search, "search", false)
    keyListeners: [
        KeyListener {
            onKeyEvent: {
                // 宿主业务键先判断，且只由这一个入口决定是否执行。
                if (!host.routeKey(search, event))
                    ime.handleKey(search, event);
            }
        }
    ]
}
CandidateStrip { ime: ime }
// 将 NativeSymbolPanel { ime: ime } 放在页面 attachedObjects 中。
// 最右端使用 ImeToggle：inputEnabled/inputMode 绑定宿主，toggleRequested 只切换 enabled。
```

上例的 `host.routeKey` 是宿主接口示意，不是模块内置方法。
`configure()` 只在控制器停用、无活跃会话时接受严格校验后的偏好；
它不改变正在运行的语言模式。启动模式由宿主显式设置。
公开输入和启动模式只有 natural/english。共享 profile/version=2，不再含 chineseMode；
已知私有 v1 设置可迁移 full→natural，下一次保存为 v2，共享 v1 整体拒绝。
资源清单导出 ImeToggle.qml/ime-menu.png，按钮 76×64、无焦点，只切换输入/菜单并保留语言。
Alt+Enter 在宿主唯一按键入口实现；不得将暂停→自然码→英文循环绑定到按钮。

`registerEditor(control, policy, learn)` 只接受 `text/search/path`：
`text` 可配置中文标点，`search/path` 保留 ASCII 标点且不学习。
学习还必须同时通过本地开关与该字段授权。共享偏好不能授予学习或字段权限。
密码、数值、敏感和未知策略拒绝注册；实际密码/数值输入模式、只读、
禁用及非纯文本控件也不接管。敏感中文字段当前不接入。

## 编辑与按键

接入字段由宿主预设 Custom + VirtualKeyboardOff，适配器支持预设 Custom，退出时仍恢复 Custom。
适配器也支持暂借其他合格原模式，退出时恢复原模式；宿主随后改成密码等
其他模式时不会被恢复代码覆盖。通过 `TextEditor::insertPlainText()` 替换选区，
不调用 `setText()` 重写整个宿主文档，也不读写统一草稿或实现第二套撤销。
位置单位分别探测 TextArea/TextField；保护 UTF-16 代理对，不宣称完整字素簇编辑。

候选与符号携带会话、预编辑和文档/选区修订标识；符号另带面板修订号，
关闭、重开及轮换后拒绝旧面板结果。过期请求拒绝。
长度或写入失败不学习，失败候选保留编码，可调整限制后重试。
宿主文字回调关闭控件或切换字段时，正在执行的绑定延后释放，不重解码到新字段。

Ctrl 组合、Alt+Enter、Alt+Backspace 留给宿主。模块不继承测试应用的 Alt+Enter
语言切换。单行 Enter 及 Shift+Enter 完成剩余预编辑后发出一次 `submitRequested`，
由宿主定义 Search/Next/Done/Save；普通多行 Enter 插入换行。
业务保存前使用 `prepareSubmit(targetControl)`，返回 false 时不要继续保存或导航。
宿主须禁用冲突的独立 Shortcut，不能再让另一监听器执行同一事件。

单按左右 Shift 根据本地偏好移动候选或光标，组合、长按及跨字段释放不触发动作。
Sym 使用中文/English/数学单轮面板；Esc/退格取消面板，保留编码。
宿主负责布局；右上角开关遵循共享组件契约，候选条与符号面板按字段能力启用。

## 验证与范围

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-PhaseAB.ps1
# 额外执行配置好的 Q10 上的 ARM 合成核心测试：
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-PhaseAB.ps1 -Device
```

设备脚本只上传合成测试程序与包内 ARM 字典到新的 `/tmp/bbime-validation/` 目录，
不安装 BAR，不读真实正文/词库，不修改系统服务或权限。SSH root 的核心回归
不是普通应用身份的权限、原生 UI、焦点、硬件按键或延迟验收。
主机使用单独生成的 64 位字典，绝不能覆盖包内 ARM 词库。

本工程菜单“原生模块”提供标题、正文和不接入的密码字段，字段均不学习、
不自动持久化。保留原来的单编辑框测试页；两个页面互斥使用同一解码服务。
模块页面打开期间禁止测试应用的旧调试窗口截图。
下一闸门是当前版本 Q10 原生页面、两款宿主升级及其设备验收；
BBnote 已有 CodeMirror 桥，需另测异步事务和保存等待，不能用原生字段回归代替。
