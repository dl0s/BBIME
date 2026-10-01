# BBIME 宿主接入规范与标准流程

适用模块：**0.1.0.12**。整理日期：2026-10-01，Asia/Shanghai。
依据：本项目当前实现，以及 BBFile 1.0.0.17、BBnote 1.0.0.8 候选工作区的只读检查。
两款宿主已完成源码接入；检查期间 BBnote 的外部工作流程已升级到 0.1.0.12 并采用共享按钮。
这不等于两款都已提交、部署或完成发布验收。
宿主差异与证据见 [两款应用检查记录](../research/HOST_INTEGRATION_REVIEW_2026-10-01.md)。

## 1. 固定输入与按钮契约

公开输入模式只允许 `natural`（自然码）和 `english`（英文）。
界面、启动配置、宿主桥和 InputSession/NativeController 都必须拒绝 `full`、`system` 和未知模式。
解码器内部仍展开拼音并保留全拼回归，这是自然码引擎的实现与崩溃保护，不是第三种可选输入模式。
Sym 的中文、英文、数学分组属于符号面板，不属于输入模式。

| 当前状态 | 右上角显示 | 点击后的状态 | 语言结果 |
| --- | --- | --- | --- |
| 自然码输入启用 | 中，无图标 | 输入暂停，显示菜单图标/应用菜单 | 保留 natural |
| 英文输入启用 | EN，无图标 | 输入暂停，显示菜单图标/应用菜单 | 保留 english |
| 输入暂停 | 菜单图标，无文字 | 隐藏应用菜单，恢复输入和原字段焦点 | 恢复暂停前语言 |

使用导出的 `ImeToggle.qml` 和 `ime-menu.png`，按钮固定 **76×64**、`FocusPolicy.None`，
放在标题/工具行最右端，独立于候选滚动区域。组件只显示绑定的宿主状态并发出 `toggleRequested`；
不保存第二份开关，不改变语言，不调用三态 cycle，不以按钮文字推导模式。
菜单图标和应用菜单是同一个开关的表现，不是两步按钮操作。
模式单独通过 **Alt+Enter** 或仅含两个选项的模式选择器切换，暂停时禁止该键改变语言。

```qml
// hostIme 是宿主适配层，具有 enabled/mode/restoreFocus。
ImeToggle {
    inputEnabled: hostIme.enabled
    inputMode: hostIme.mode
    enabled: hostIme.ready || hostIme.englishReady || hostIme.enabled
    onToggleRequested: {
        hostIme.enabled = !hostIme.enabled;
        if (hostIme.enabled) hostIme.restoreFocus();
    }
}
```

`englishReady` 是已实现英文故障路径的宿主能力示意；直接使用 NativeController 时只检查 `ready || enabled`。
资源放在宿主 `assets/`；若命名空间改为 `assets/ime/`，将图标路径变换记录在导入清单及补丁中。
为候选条、Sym 取消图标做同样的确定性变换，不手改快照或遗漏 BAR 资源。

输入开启时隐藏 app menu/action bar，设置 `keysIgnoreFocusInActionBar`，禁用冲突的浏览快捷键。
手势展开菜单、打开普通设置/模态、导航、后台、休眠都必须暂停；菜单收起和回到前台不自动开启。
暂停清除预编辑、候选票据、符号面板与按键状态，保留正文、选区和当前语言。
宿主负责记录并恢复最后合法字段的焦点；控件被删除、页面不可见或应用不在前台时不能抢焦点。

接入的普通文本字段始终保持 **Custom + VirtualKeyboardOff**，暂停也不能恢复系统输入。
原生适配器支持已经设为 Custom 的控件，释放时恢复其原值，因而仍是 Custom。
`builtInShortcutsEnabled` 和冲突的原生文本菜单由宿主按独占输入策略处理；不增加不被 Custom 支持的 prediction/spellcheck flags。
未接入的密码等字段保留宿主原有控件策略，不注册进中文解码器。

## 2. 建立基线与字段矩阵

1. 读取适用的 AGENTS.md，记录 BBIME 与宿主 HEAD、版本、工作区状态和现有未提交修改。
2. 保存当前可回退 BAR 的版本、字节数、SHA-256；备份旧快照、清单和补丁，不覆盖其他工作。
3. 每个字段记录：页面、稳定 ID、text/search/path 或宿主 Literal、长度、单/多行、学习授权、提交行为、敏感性。
4. 默认关闭学习及设置共享。不要导入真实正文、用户词库、连接配置、token 或设备日志。
5. 只向宿主 GUI 接入输入服务；不向 CLI、文件 worker、root 服务或系统输入协议接入。

最小必需部分：Decoder/InputSession/ImeService、ModuleProfile、原生适配器/控制器、位置与符号定义、
引擎必要源码/头文件、ARM32 字典、候选/Sym/右上角组件及许可证。
私有 ModuleSettings 是可选能力；未使用设置的宿主不需要复制其持久化实现或申请 `access_shared`。
不要复制演示 Backend/main、独立测试页面、草稿、诊断截图和部署工具到宿主运行路径。

上游仅注册 `text/search/path`，学习仅可用于经本地授权的普通 text。
Numeric/Password/Sensitive/Denied 和未知策略拒绝；只读、禁用和非 Plain 控件拒绝。
BBFile 的 mode/uid/gid/ACL 等技术字段走宿主 `Literal` 英文路径；它不是第三种语言，也不能中文解码或学习。
BBnote 的账号/URL 等宿主直写字段不得因为“所有字段都是 Custom”就自动扩大中文白名单。

## 3. 导入固定快照并处理版本升级

1. 按清单选择必需文件；记录源提交、源码版本、dirty 状态、每个源文件和目标文件原始字节 SHA-256。
2. 宿主修改集中在适配层或单独版本化补丁；记录资源路径、换行和演示私有桥删除等基础转换。
3. 更新前校验现有快照及旧补丁哈希；出现漂移先处理差异，不用新哈希掩盖手工变更。
4. 在 staging 应用基础转换和新补丁，执行 `git apply --check` 或精确匹配检查，全部成功后替换快照。
5. 构建只使用宿主固定快照，不在普通构建中读取兄弟项目源码；归档旧清单和回退资源。

本次 0.1.0.12 升级的具体兼容处理：

- 删除 `ModuleProfile.chineseMode` 的宿主赋值；共享偏好版本为 2，只有 natural/english 启动模式。
- 私有 v1 配置只在本地读取时迁移：full → natural，English 与行为偏好保留，本地学习授权保留。
  下一次保存或发布写 v2；共享 v1、未知键/版本/值仍整体拒绝，不转移学习授权。
- BBFile 旧补丁中的两模式及预设 Custom 修改已成为上游能力，重建补丁时去掉这些重复片段；
  保留并重新验证 Literal、独占事件消费、宿主业务键等必要差异。
- BBnote 的 `host-patches.json` 中 Custom 和 full 条件替换均已成为上游能力，必须删除/重制旧 overlay；
  其精确匹配构建会拒绝直接套用到新版。清除 `profile.chineseMode`，导入共享按钮并去掉 `noteIme.cycle()` 三态调用。
- 两个导入器均需把共享按钮/菜单图标加入复制和包装清单，更新版本及锁定哈希后重跑宿主验证。

以上是从 0.1.0.11 升级时的兼容清单。提交前复核时，BBnote 已完成删除旧 overlay/chineseMode/cycle
及导入共享按钮，54 文件快照校验通过；其合并记录仍需按实际新版同步，设备闸门仍待验收。

已有宿主的校验入口：

```powershell
# 只校验本地固定快照；这些命令不会更新模块。
python -B C:/Users/dove/Documents/BBFile/tools/import_bbime.py
python -B C:/Users/dove/Documents/BBnote/tools/Verify-IME-Import.py
```

升级命令只在上述补丁和兼容调整完成后，于对应宿主执行：

```powershell
# BBFile
python -B tools/import_bbime.py --source C:/Users/dove/Documents/BBIME --update
# BBnote
.\tools\Import-BBIME.ps1 -SourceRoot ..\BBIME -ModuleVersion 0.1.0.12 -Update
```

## 4. 初始化、焦点与生命周期

服务在同一 GUI 输入线程创建、打开、调用和销毁；每进程一个服务，每字段一个会话。
先声明服务再声明控制器，销毁顺序是会话/控制器 → 服务。异步编辑器桥也使用这个服务串行解码。

启动顺序：打开包内字典和宿主私有词库 → 探测位置单位/原生自检 → 设置本地策略与模式 →
向 QML 暴露宿主适配层 → 创建并注册场景字段 → 设置 scene → **所有创建回调结束后**才启用输入。
最后一步防止隐藏页面创建时调用 suspend 导致启动开关与菜单初态相反，BBFile 已遇到并修复此问题。
启动后由同一 enabled 状态绑定按钮、菜单、候选可用性和冲突 Shortcut。

销毁字段/换页使旧票据失效；普通模态、后台和休眠主动 suspend，前台恢复要求明确启用。
初始化失败显示自然码不可用；可启用已验证的宿主英文直写，否则保持暂停并提示。
不能将失败自动转换为系统输入或重开另一个 Decoder owner。

## 5. 唯一按键与提交入口

宿主先处理 Alt+Enter、Alt+Backspace 和 Ctrl/业务键，再交给 NativeController；这些键的语义在宿主层统一定义。
NativeController 保留将 Ctrl、Alt+Enter、Alt+Backspace 交回宿主的边界，不能再由第二个 Shortcut 执行。
只在首次按下执行切换/提交，处理重复/释放事件；暂停时禁止内容编辑和语言切换。
独占输入宿主必须消费被屏蔽的内容事件，避免另一路内容处理重复写入。

原生字段统一调用 `prepareSubmit(target)`，成功后才读取最终 text 并执行 Search/Next/Done/Save。
失败保留可重试预编辑，不保存、不导航。单行 Enter 与 Shift+Enter 只派发一次业务操作；正文 Enter 换行。
正文、选区、撤销和持久化属于宿主；模块通过 TextEditor 替换选区，不用 setText 改写整份文档。

CodeMirror 等异步正文需要额外适配：

1. 请求携带 session、epoch、请求序号、文档修订及选区；同一队列串行请求。
2. 返回提案再次核对这些值，拒绝旧文档、旧焦点、旧选区或旧语言的结果。
3. 在真实编辑器内用一次事务替换目标选区并保留撤销；不能向原生影子控件写字就认定正文已提交。
4. 保存/暂存先完成预编辑并等待最终文档快照，失败/超时中止保存或关闭；页面销毁后拒绝回调。
5. Markdown/path/search 保持 ASCII 标点，学习保持关闭；不能沿用原生字段测试冒充 WebView 正文验证。

## 6. 本地验证与包装

BBIME 标准入口：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-PhaseAB.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-SdkApi.ps1 -OutputPath build/audit/sdk-api.json
```

SDK 读取顺序是显式 `-SdkRoot` → 用户 `INTROOP_SDK_ROOT` → 进程同名变量，本次有效目录为 `C:\bbdevtools`。
BAR 包装单独解析兼容 Java 7/8：显式 `-JavaBin` → 用户/进程 `INTROOP_JAVA_BIN` → 已安装 JRE。
自动查找 SDK 或同级 Momentics `bbndk/features` 的 JRE，最后检查 JAVA_HOME/PATH。
本机 Java 17 因 SDK 包装器访问内部 XML 类失败，使用已安装的兼容 Momentics JRE；
不固定其版本目录，也不将它作为编译器 SDK 根目录。
目标为 `4.6.3,gcc_ntoarmv7le_cpp`，ARM ELF 使用 `libcpp.so.4`，拒绝 GNU libstdc++。
ARM 字典 SHA-256 固定为 `179311C55AB9B912A07EF040E5A95AF97E704E2248B7B97735F17A3E05D13B67`。
主机测试必须使用独立生成的 64 位字典和合成用户文件，不能覆盖包内 ARM 字典。

宿主再执行自己的源码/快照检查、唯一提交入口用例、QML、核心回归、ARM 构建、ABI 与 BAR 内容检查。
覆盖无预编辑/有预编辑、自然码/英文启停、full 拒绝、选区/emoji、过期票据、写入失败重试、只读/敏感字段。
包含按钮最右位置/尺寸/焦点策略、菜单及 Shortcut 同步、符号、销毁/后台；验证既有业务及 CLI/root 隔离。
BBnote 额外执行异步提案、保存等待、撤销、Markdown 和长文档回归。
记录每项实际 PASS/FAIL/NOT_RUN、版本、源码/资源/补丁/最终 BAR 哈希；不得沿用旧包结果。

## 7. 设备验收、归档与提交

设备部署按该宿主任务的有效授权执行；另一个应用的既往部署授权不能自动扩展到本项目。
部署时核对应用身份、版本、上传回读哈希、安装结果和全部运行资源，保留数据与原有服务。
使用普通应用身份加载当前 QML、真实 TextField/TextArea/WebView 并检查菜单、焦点、候选和保存。
SDK 合成 KeyEvent 与 SSH root 核心执行分别记录，不能当作实体键或普通应用权限验收。

人工闸门：720×720 顶部按钮/候选布局、触屏选词、物理 Shift/Alt/Sym、长按/连击、选区/撤销、
后台/锁屏/模态返回、写入失败、长文档延迟及连续输入。共享功能启用时另测无权限/坏文件/原子写入。
只有当前包满足全部适用闸门，才能写 `releaseReady=true`。

归档合并记录包含需求、宿主和模块版本/提交、dirty 状态、导入清单/补丁哈希、字段矩阵、
冲突与修复、验证与未执行项、安装回读、人工结论及回退步骤。
提交仅包含已审阅的接入源码、固定快照、补丁、必要资源、工具和文档；排除 build、凭据、真实正文/用户词库。
用户要求提交时在验证后完成本地 commit，推送及部署单独按任务范围执行。
回退用记录的旧包/快照，保留身份与数据，说明旧版本输入策略；有无关未提交改动时不宽泛 reset。
