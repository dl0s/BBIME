# BBIME 0.1.0.15 基准代码记录

范围：独立 BBIME 小验证应用及本仓库内的可复用底座。仅支持 BBIME 内置自然码／English；BBnote、IntroOP、BBFile 保持暂停。本轮不向三个宿主导入代码，也不构建或部署它们。

## 本版基准

| 项目 | 固化行为 | 实现与验收程度 |
| --- | --- | --- |
| 候选位置 | 主页面最后一行固定 72 px，底部 padding 为 0，弹性正文让位，编码和状态在其上方；输入启用时隐藏 ActionBar，暂停时隐藏候选 | 代码已完成；本地布局契约和 QML 验证见下方证据；0.15 手机首帧、长文与动画尚未验收 |
| 原生验证页候选 | 共享 CandidateStrip 同样固定 72 px、最后一行、暂停隐藏；原生页没有底部 actions，贴齐 Sheet 可用内容底边 | 代码已完成；不能声称替换了此页不存在的实际菜单栏，也不能把 72 px 当作系统 ActionBar 的测量高度 |
| 焦点自动启停 | 仅真实聚焦、可输入、已注册且当前范围允许的字段启用；失焦、后台、菜单或 Sheet 动画撤销输入；保留自然码／English 当前语言 | 0.14 本地回归、设备启动自检和用户流畅反馈已有证据；0.15 策略语义保持，尚待本版本实体与全生命周期验收 |
| 手动暂停 | 同一焦点重复通知、资格变化、后台恢复不清除暂停；明确恢复或真正新编辑焦点可解除 | 纯 C++98 生产策略和生产 QML 接线回归覆盖；真实 SDK 事件组合仍需验收 |
| 默认 Sym | 两页无自定义按钮、菜单或 Dialog，BAR 与默认模块清单均不含 NativeSymbolPanel；NativeController 直接调用也消费 Sym，兼容符号接口无操作 | 代码与资源核对；Shift＋Sym 不作为单独 Shift；研究源码仍保留但不默认导出 |
| 候选提交 | 主页面保存 Down 时 generation；共享候选条保存 Down 时完整 session／revision／document／index。触发只提交原票据，过期提交由后端拒绝 | 生产处理函数和输入会话回归覆盖；无 Down 快照的辅助触发采用当前行；真实滚动／取消手势仍需设备验证 |
| 初始右上按钮 | 外框 76×64，独立图像框 36×36；不随启停重算框大小 | 保留已实现固定约束与触摸测试；0.15 冷启动真实尺寸需验收 |
| 其他输入功能 | 保留左右 Shift、Alt 组合、候选滑动、草稿、设置和自检，原生字段私有验证 | 本地解码与会话回归覆盖，不能据此承诺全部硬件交互已验收 |

候选固定区占独立布局行，不叠加到编辑框或光标上。顶部模式、编码和状态仍占用各自行高，因而可见正文空间随布局变化。宿主将来接入必须先为正文分配弹性空间，再把候选放在内容根容器末行；不得把候选塞进 Overlay 或在候选下方继续摆控件。

## 底座代码与接线边界

- `src/focusstate.h` 导出 `bbime::EditorFocusGate`，通过 `module/BBIME-Sources.ps1` 的 `BBIMEPolicyHeaders` 提供绝对路径。它不依赖 Qt，也不自动注册字段或监听 SDK。
- `Backend` 展示实际字段资格、焦点、生命周期、菜单和 Sheet 接线。它仍属于验证应用，不作为共享模块导出，也不把草稿、剪贴板或页面所有权带入宿主。
- 真实焦点身份与输入资格分开。资格暂时失效不能被误报为新焦点以解除手动暂停；范围关闭时不凭临时焦点空值重置暂停。
- 获得焦点立即协调，失焦合并到事件循环尾部；按键入口在启用判断前同步校准。启停同值赋值无副作用，普通启停不 requestFocus，只有明确恢复及有效提交后的合法恢复可以请求焦点。
- NativeController 和 NativeEditorAdapter 继续负责每字段会话与原始输入模式租用／恢复；注册敏感、只读、隐藏或禁用字段的资格限制保持。焦点策略的导出不代表三个宿主已完成此接线。

详见 [底座说明](C:/Users/dove/Documents/BBIME/module/README.md) 与 [接入准则](C:/Users/dove/Documents/BBIME/module/INTEGRATION_WORKFLOW.md)。

## 可追溯验证

本地完整验证 **PASS**：解码与输入会话回归、10,000 次随机查询、37 项源码契约、21 项真实生产 C++98 焦点策略检查、20 组生产 QML 处理函数与布局规则测试、按钮触摸处理、ARM 构建／ABI／BAR 包装、官方 SDK QML parser 均通过。旧 Sym 4 组研究测试单独标为 `RESEARCH_ONLY_PASS`，不计入当前产品能力。构建成功与 QML 语法通过只证明本地对应检查，不证明 Cascades 的首帧、原生手势或硬件键。

- [本地回归记录](C:/Users/dove/Documents/BBIME/research/phase-ab-validation-0.1.0.15.json)：状态 `LOCAL_PASS_BBIME_DEVICE_GATES_REMAIN`，`releaseReady=false`。
- [BAR 内容核对](C:/Users/dove/Documents/BBIME/research/bbime-app-package-0.1.0.15.json)：包身份／版本一致，自定义 Sym QML 不存在，全部 16 项声明资源与当前文件逐字节一致，包括 ARM 可执行文件和字典。
- [宿主保持记录](C:/Users/dove/Documents/BBIME/research/host-preservation-baseline-0.1.0.15.json)：三个宿主的 74 份受检文件与暂停时预期哈希一致；IntroOP 已有诊断插桩使用其暂停前独立记录，不覆盖旧改动。
- [0.14 设备启动摘录](C:/Users/dove/Documents/BBIME/research/q10-app-startup-0.1.0.14.txt)：只保留启动合成诊断，不包含草稿或个人词库。

可安装包：[BBIME-0.1.0.15.bar](C:/Users/dove/Documents/BBIME/build/releases/0.1.0.15/BBIME-0.1.0.15.bar)。SHA-256：
`6E278638441CF5339482659C92FB044ADAD6C44F4E10F5C652E0D55A14BE90B8`。

源码采用本地分支 `codex/bbime-0.1.0.15-baseline`、基准标签 `bbime-0.1.0.15-baseline` 定位；不推送远端。`build/` 内包与诊断保持 Git 忽略，私有数据备份、连接配置和密钥不入提交。机器可读版本记录见 [baseline-0.1.0.15.json](C:/Users/dove/Documents/BBIME/research/baseline-0.1.0.15.json)。

既有 0.14 设备启动日志确认 scene、SETTINGS、NATIVE_MODULE、SELFTEST、KEYS 全部 PASS，413 个映射音节加载；合成 P95 为约 1.01 ms，不是连续实体输入的性能数据。用户对焦点切换的流畅反馈属于 0.14 局部实测。

本轮仅整理源码与本地包，手机继续保留 0.14。0.15 未安装、未强制关闭应用，也没有把 0.14 的诊断结果改写为 0.15。

布局诊断按“header → modes → editor → composition → footer → candidates”保存，暂停时候选不参与行校验。新增测量完整性、固定高度和底边标志；缺少控件帧时报告 `PENDING_MEASUREMENTS`，不能冒报 PASS。测量值仍需本版实际运行产生。

## 剩余推进时间

以下为有效工时估算，设备与用户等待另计；适用于独立小应用，不是恢复三个宿主的授权。

| 推进项 | 预计时间 | 完成条件 |
| --- | --- | --- |
| 0.15 安装、冷启动与底边测量 | 0.5–1 小时 | 正常退出旧应用后原位更新，版本／包哈希一致；启用、暂停及原生 Sheet 各取得本版布局证据 |
| 焦点、手动暂停、首键和前后台 | 1–2 小时 | 主页面、标题、正文、密码／非编辑区、菜单／Sheet、锁屏恢复按契约运行 |
| 实体 Shift／Alt／Sym 与触屏候选 | 1–2 小时 | 两侧 Shift＋Sym 不移动，下一次单 Shift 正常；Sym 连按无界面；旧触摸拒绝、正常滑动选词及长文不遮挡 |
| 验收收尾与局部复测 | 0.5–1 小时 | 失败归因、必要修正与本版证据对应 |
| **合计** | **3–6 小时** | **以没有新的 SDK 通知、渲染或性能问题为前提，未知平台缺陷另行估算** |

当前可作为本地代码基准，不能标记为全部实机交互验收完成。三个宿主的既有问题和旧修改保留，后续接入需另行恢复工作范围。
