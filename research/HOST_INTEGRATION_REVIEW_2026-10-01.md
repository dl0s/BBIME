# 两款宿主接入检查与规范收敛记录

日期：2026-10-01，Asia/Shanghai。BBIME 本次目标：**0.1.0.12**。
本轮只读 BBFile、BBnote 的源码、固定快照、现有日志和文档；重新执行各自的只读快照校验。
没有覆盖宿主工作区、重新导入、重建宿主包、连接设备或沿用此前授权部署。
结构化哈希、HEAD、工作区和检查结果见 [宿主检查证据](host-integration-review-2026-10-01.json)。
原始检查快照为 19:23；提交前发现 BBnote 被外部工作流程更新，最新复核追加在 JSON 的 latestFollowup 中。
这是有时间边界的只读检查，不保证另一工作流程在检查结束后停止修改。

## 结论与完成度

| 项目 | BBFile | BBnote |
| --- | --- | --- |
| 实际源码接入 | 已完成；工作区版本 1.0.0.17 | 已完成；工作区候选 1.0.0.8 |
| 宿主 HEAD | 9c25b558294d15891320063ad175d5c05e18f1c0 | 8df16470a401fc68019ec2d0f313541cef81ebd0 |
| 接入提交 | 尚未提交，工作区有其他修改 | 尚未提交，工作区有其他修改 |
| 来源 | 固定 BBIME 0.1.0.11，54 文件和宿主 patch | 最新复核：固定 BBIME 0.1.0.12，54 文件，旧 overlay 已移除 |
| 本轮快照校验 | PASS：54 文件及补丁无漂移 | PASS：原 52 文件；更新后 54 文件及全部当前源文件哈希一致 |
| 输入模式 | natural/english；技术 Literal 固定英文 | 新版公开接口及核心会话 natural/english，schema v2 |
| 右上角按钮 | 中/EN ↔ 菜单，只改变启用状态 | 已改用与 BBIME 字节一致的 ImeToggle，只翻转 enabled；实际设备行为待验收 |
| 字段及适配 | 八原生字段，四普通+四 Literal | 原生字段及 CodeMirror 正文异步提案 |
| 学习/共享 | 关闭，词库私有，无 IME root 接口 | 学习关闭，ASCII 标点，词库私有 |
| 现有设备证据 | 1.0.0.17 部署及 29 项 SDK 事件检查 PASS，55 安装文件匹配 | 未找到当前候选的可信设备验收证据 |
| 发布状态 | releaseReady=false，实体键/触屏等未执行 | 本地候选，releaseReady=false；归档验证 JSON 仍缺失 |

初次项目检查的 17:04 快照早于本轮宿主接入成果，其“未找到接入标记”不能作为现在的状态。
历史 `CURRENT_PROJECT_AUDIT_2026-10-01.md/json` 保留当时结果，本记录承接并更新当前结论。
不使用百分比推断可靠性：源码最小化、自动回归、设备运行与人工发布分别判定。

## BBFile 可复用经验

来源文件：`docs/IME_INTEGRATION_WORKFLOW.md`、`docs/IME_MERGE_RECORD_2026-10-01_1.0.0.17.md`、
`docs/IME_VALIDATION_2026-10-01_1.0.0.17.json`、`vendor/bbime/manifest.json`、`module/BBFile-IME.patch`。

GUI 线程唯一服务、控制器先析构；八字段保持 Custom + VirtualKeyboardOff，关闭原生冲突输入。
地址/搜索/名称/正文允许自然码和英文；mode/uid/gid/ACL 的 Literal 路径禁止中文解码和学习。
单一 submitInput 完成预编辑后才执行文件业务，菜单/模态/后台暂停，开关保留语言。
实际按钮实现已满足本次规范，尺寸 76×64、无焦点、最右，中英文由 Alt+Enter 单独切换。

导入器核对现有快照和补丁，在 staging 应用变换及 git apply --check 后记录源/目标哈希。
首次启动曾被隐藏字段创建时的 suspend 打断；启用移到 scene 创建完成之后，成为统一启动顺序。
现有 GUI 与 root 服务隔离，IME 不增加 root 协议。合成按键只改未保存 UI，不执行真实 chmod/chown/ACL。

记录的最终部署时间是 **2026-10-01 18:40:02 +08:00**，BAR SHA-256：
`04BA5DDB97402608D794E80D13011B7AFF67206EC702092947E889C92BCE4EDA`。
这些是宿主既有归档记录，本轮未重新核验设备。SDK 构造事件不是物理按键或触屏验收。
仍需实体长按/连击、触屏候选、后台/锁屏、磁盘故障和大文档性能。

## BBnote 可复用经验、原始偏差与后续修正

来源文件：`docs/ime/INTEGRATION_PROCESS.md`、`MERGE_RECORD.md`、`upstream-manifest.json`、
`host-patches.json`、`src/noteime.cpp`、`src/imeproposal.h`、`assets/ImeControls.qml` 和 `assets/ime-editor.js`。

原生字段与 CodeMirror 共用单一服务；异步提案携带 session/epoch/serial/revision/selection，
真正写入时复核并以 CodeMirror 事务替换选区，保存等待最终快照。这是异步正文接入的必要独立步骤。
现有日志报告核心回归、18 项异步编辑器用例、7 QML 和候选包构建通过；本轮只读取历史日志，未重跑这些用例。

19:23 原始快照中，NoteIme 和正文桥只暴露 natural/english，但上游会话仍接受 full，controller 限制由 overlay 实施。
右上角当时调用 `noteIme.cycle()`，暂停后重设语言，造成与 BBFile 的差异；早期文档另有“三模式”和“系统回退”描述。
这些是本轮识别的历史偏差，不能用来描述后续更新后的代码。

提交前复核发现外部工作流程已导入 **0.1.0.12、54 文件**，全部源哈希与当前 BBIME 一致。
已采用共享 ImeToggle 并只翻转 enabled，删除 NoteIme::cycle、profile.chineseMode 赋值，host-patches 清单为空。
新的宿主日志报告自然码/英文核心回归、8 QML 和 ARM 包完成；本轮只读取这些日志并重跑只读快照校验。
合并记录此时仍写 0.1.0.11/52 文件/旧循环按钮，须同步实际版本、补丁与规范。

`MERGE_RECORD.md` 链接的 `docs/ime/validation-2026-10-01.json` 本轮未找到，
因此不能仅凭链接把候选包或设备验收判为已完成。需要生成匹配最终源码/包的结构化归档。
设备启动、WebView/原生焦点、真实保存/撤销、后台、实体键与触屏均仍需当前包证据。

## 本项目完成的收敛

- 公开界面、Backend、NativeController、InputSession 和新配置只保留自然码/英文，非法模式不改变会话。
- 配置 schema v2 删除 chineseMode；仅私有已知 v1 本地配置迁移全拼，共享导入严格拒绝旧结构和本地授权。
- 导出并打包无自身状态的 ImeToggle/菜单资源；两个演示页面共用 76×64 最右按钮。
- 原生适配器支持预设 Custom，演示注册字段暂停仍 Custom；Alt+Enter 由宿主路由，语言在页面间同步。
- 内部全拼/自然码展开与历史崩溃/随机回归保留；有限符号轮换、过期票据、失败重试和线程所有权保留。
- SDK 根目录统一解析用户变量 `INTROOP_SDK_ROOT=C:\bbdevtools`；打包单独发现兼容 JRE，不固定旧版本目录。

本项目局部实现的验证见 [0.1.0.12 阶段记录](phase-ab-validation-0.1.0.12.json)。
配置持久化与原生控件自检已编译进当前包，实际运行仍属于未执行设备闸门。

## 下一步标准操作

详细步骤与状态表已固化于 [模块接入规范](../module/INTEGRATION_WORKFLOW.md)。
标准顺序为：保留基线 → 重制重复补丁/overlay → 导入新版组件及配置 → 对齐宿主开关与文档 →
完整本地验证 → 当前包设备及人工验收 → 归档 → 提交。
BBFile 仍须升级 0.1.0.12，保留 Literal 与独占业务路由；BBnote 已完成新版源码/按钮/overlay 同步，
下一步核对最终本地验证、更新合并记录并补齐结构化与设备证据。
旧包 PASS 不覆盖新版行为；本轮没有代替宿主提交或部署。
