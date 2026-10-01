import bb.cascades 1.4
import QtQuick 1.0 as Quick

Page {
    id: page
    actionBarVisibility: backend.imeEnabled ? ChromeVisibility.Hidden : ChromeVisibility.Visible
    actionBarAutoHideBehavior: ActionBarAutoHideBehavior.Disabled
    keysIgnoreFocusInActionBar: backend.imeEnabled
    onActionMenuVisualStateChanged: {
        if (actionMenuVisualState === ActionMenuVisualState.VisibleFull ||
            actionMenuVisualState === ActionMenuVisualState.AnimatingToVisibleFull)
            backend.disableIme();
    }
    Container {
        id: content
        implicitLayoutAnimationsEnabled: false
        leftPadding: 16
        rightPadding: 16
        topPadding: 8
        bottomPadding: 8
        Container {
            preferredHeight: 64
            minHeight: 64
            maxHeight: 64
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            Label {
                text: "BBIME"
                verticalAlignment: VerticalAlignment.Center
                textStyle.fontSize: FontSize.Medium
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            }
            Button {
                imageSource: "asset:///cancel.png"
                topMargin: 0
                bottomMargin: 0
                accessibility.name: "取消编码"
                preferredWidth: 76
                minWidth: 76
                preferredHeight: 64
                minHeight: 64
                maxHeight: 64
                enabled: backend.imeEnabled && backend.composing
                onClicked: { backend.cancel(); backend.restoreFocus(); }
            }
            Button {
                text: "Sym"
                topMargin: 0
                bottomMargin: 0
                accessibility.name: "符号"
                preferredWidth: 92
                minWidth: 92
                maxWidth: 92
                preferredHeight: 64
                minHeight: 64
                maxHeight: 64
                enabled: backend.imeEnabled
                onClicked: backend.cycleSymbols()
            }
            Button {
                id: imeToggle
                objectName: "imeToggle"
                text: backend.imeEnabled ? (backend.mode === "english" ? "EN" : "中") : ""
                imageSource: backend.imeEnabled ? "" : "asset:///tools.png"
                topMargin: 0
                bottomMargin: 0
                accessibility.name: backend.imeEnabled ?
                    (backend.mode === "english" ? "英文输入，显示菜单并暂停输入" : "中文输入，显示菜单并暂停输入") :
                    "隐藏菜单，启用应用输入法"
                preferredWidth: 76
                minWidth: 76
                maxWidth: 76
                preferredHeight: 64
                minHeight: 64
                maxHeight: 64
                onClicked: backend.toggleIme()
                attachedObjects: [
                    LayoutUpdateHandler {
                        onLayoutFrameChanged: backend.recordLayout("ime_toggle", layoutFrame.x, layoutFrame.y, layoutFrame.width, layoutFrame.height)
                    }
                ]
            }
            attachedObjects: [
                LayoutUpdateHandler {
                    onLayoutFrameChanged: backend.recordLayout("header", layoutFrame.x, layoutFrame.y, layoutFrame.width, layoutFrame.height)
                }
            ]
        }
        SegmentedControl {
            id: modes
            property bool initialized: false
            enabled: backend.imeEnabled
            topMargin: 4
            bottomMargin: 4
            Option { text: "自然码"; value: "natural"; selected: true }
            Option { text: "全拼"; value: "full" }
            Option { text: "English"; value: "english" }
            onCreationCompleted: {
                selectedValue = backend.mode;
                initialized = true;
            }
            onSelectedValueChanged: {
                if (initialized) backend.mode = selectedValue;
            }
            attachedObjects: [
                LayoutUpdateHandler {
                    onLayoutFrameChanged: backend.recordLayout("modes", layoutFrame.x, layoutFrame.y, layoutFrame.width, layoutFrame.height)
                }
            ]
        }
        TextArea {
            id: editor
            objectName: "editor"
            inputMode: TextAreaInputMode.Custom
            inputRoute.primaryKeyTarget: true
            textFormat: TextFormat.Plain
            hintText: "测试文本"
            minHeight: 88
            topMargin: 0
            bottomMargin: 0
            maximumLength: 16384
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            keyListeners: [
                KeyListener { onKeyEvent: backend.handleKey(event) }
            ]
            onCreationCompleted: {
                backend.attachEditor(editor);
                editor.requestFocus();
            }
            attachedObjects: [
                LayoutUpdateHandler {
                    onLayoutFrameChanged: backend.recordLayout("editor", layoutFrame.x, layoutFrame.y, layoutFrame.width, layoutFrame.height)
                }
            ]
        }
        Container {
            preferredHeight: 68
            minHeight: 68
            maxHeight: 68
            Label {
                text: backend.composition.length ? backend.composition : " "
                topMargin: 0
                bottomMargin: 0
                textStyle.color: Color.create("#68d7ab")
                textStyle.fontSize: FontSize.Small
            }
            Label {
                text: backend.pinyin.length ? backend.pinyin : " "
                topMargin: 0
                bottomMargin: 0
                textStyle.color: Color.create("#b8c9d7")
                textStyle.fontSize: FontSize.XSmall
            }
            attachedObjects: [
                LayoutUpdateHandler {
                    onLayoutFrameChanged: backend.recordLayout("composition", layoutFrame.x, layoutFrame.y, layoutFrame.width, layoutFrame.height)
                }
            ]
        }
        ListView {
            id: candidates
            enabled: backend.imeEnabled
            objectName: "candidateStrip"
            property int highlightedIndex: backend.selectedIndex
            property real viewportWidth: 688
            preferredHeight: 72
            minHeight: 72
            maxHeight: 72
            topMargin: 0
            bottomMargin: 0
            horizontalAlignment: HorizontalAlignment.Fill
            dataModel: backend.candidateModel
            layout: StackListLayout {
                orientation: LayoutOrientation.LeftToRight
                headerMode: ListHeaderMode.None
            }
            listItemComponents: [
                ListItemComponent {
                    type: ""
                    Container {
                        id: candidateItem
                        property int candidateIndex: ListItem.indexPath.length ? ListItem.indexPath[0] : -1
                        property bool highlighted: candidateIndex === ListItem.view.highlightedIndex
                        preferredWidth: Math.min(ListItem.view.viewportWidth, Math.max(88, ListItemData.text.length * 40 + 32))
                        preferredHeight: 72
                        minHeight: 72
                        maxHeight: 72
                        leftPadding: 14
                        rightPadding: 14
                        rightMargin: 4
                        layout: DockLayout {}
                        background: highlighted ? Color.create("#25483c") : Color.create("#23272b")
                        accessibility.name: ListItemData.text
                        Label {
                            text: ListItemData.text
                            textFormat: TextFormat.Plain
                            multiline: false
                            horizontalAlignment: HorizontalAlignment.Fill
                            verticalAlignment: VerticalAlignment.Center
                            textStyle.fontSize: FontSize.Small
                            textStyle.color: candidateItem.highlighted ? Color.create("#80ebba") : Color.create("#f4f4f4")
                            textFit.mode: LabelTextFitMode.FitToBounds
                            textFit.minFontSizeValue: 4
                            textFit.maxFontSizeValue: 8
                        }
                    }
                }
            ]
            onTriggered: backend.chooseCandidate(indexPath[0])
            function followHighlight(animated) {
                if (highlightedIndex >= 0)
                    scrollToItem([highlightedIndex], animated ? ScrollAnimation.Smooth : ScrollAnimation.None);
            }
            attachedObjects: [
                LayoutUpdateHandler {
                    onLayoutFrameChanged: {
                        candidates.viewportWidth = layoutFrame.width;
                        backend.recordLayout("candidates", layoutFrame.x, layoutFrame.y, layoutFrame.width, layoutFrame.height);
                    }
                }
            ]
        }
        Container {
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            preferredHeight: 38
            minHeight: 38
            maxHeight: 38
            Label {
                text: backend.status.length ? backend.status : backend.pageLabel
                textStyle.fontSize: FontSize.XSmall
                textStyle.color: Color.create("#e2c46a")
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            }
            Label {
                text: backend.latency
                textStyle.fontSize: FontSize.XSmall
            }
            attachedObjects: [
                LayoutUpdateHandler {
                    onLayoutFrameChanged: backend.recordLayout("footer", layoutFrame.x, layoutFrame.y, layoutFrame.width, layoutFrame.height)
                }
            ]
        }
        attachedObjects: [
            LayoutUpdateHandler {
                onLayoutFrameChanged: backend.recordLayout("content", layoutFrame.x, layoutFrame.y, layoutFrame.width, layoutFrame.height)
            }
        ]
    }
    actions: [
        ActionItem {
            title: "原生模块"
            imageSource: "asset:///diagnostics.png"
            ActionBar.placement: ActionBarPlacement.InOverflow
            enabled: backend.nativeModule.ready
            onTriggered: {
                if (backend.startNativeModule()) moduleSheet.open();
            }
        },
        ActionItem {
            title: "设置"
            imageSource: "asset:///tools.png"
            ActionBar.placement: ActionBarPlacement.InOverflow
            onTriggered: { backend.disableIme(); settingsSheet.open(); }
        },
        ActionItem {
            title: "符号"
            enabled: backend.imeEnabled
            ActionBar.placement: ActionBarPlacement.InOverflow
            onTriggered: backend.cycleSymbols()
        },
        ActionItem {
            title: "复制"
            imageSource: "asset:///copy.png"
            ActionBar.placement: ActionBarPlacement.InOverflow
            onTriggered: backend.copy()
        },
        ActionItem {
            title: "清空"
            imageSource: "asset:///clear.png"
            ActionBar.placement: ActionBarPlacement.InOverflow
            onTriggered: backend.clear()
        },
        ActionItem {
            title: "自检"
            imageSource: "asset:///diagnostics.png"
            ActionBar.placement: ActionBarPlacement.InOverflow
            onTriggered: { backend.runDiagnostics(); backend.restoreFocus(); }
        }
    ]
    attachedObjects: [
        Sheet {
            id: moduleSheet
            content: NativeModulePage {
                ime: backend.nativeModule
                onFinished: moduleSheet.close()
            }
            onClosed: backend.stopNativeModule()
        },
        Quick.Connections {
            target: backend
            onChanged: {
                if (modes.selectedValue !== backend.mode)
                    modes.selectedValue = backend.mode;
            }
            onCandidatesChanged: candidates.scrollToPosition(ScrollPosition.Beginning, ScrollAnimation.None)
            onHighlightChanged: candidates.followHighlight(true)
            onSymbolsChanged: {
                if (backend.symbolsVisible) {
                    if (!symbolDialog.opened) symbolDialog.open();
                } else if (symbolDialog.opened) symbolDialog.close();
                if (symbolGroups.selectedIndex !== backend.symbolGroup)
                    symbolGroups.selectedIndex = backend.symbolGroup;
            }
            onSettingsChanged: {
                settingsStartMode.selectedValue = backend.defaultMode;
                settingsShiftCandidates.checked = backend.shiftCandidates;
                settingsShiftCursor.checked = backend.shiftCursor;
                settingsPunctuation.checked = backend.chinesePunctuation;
                settingsLearning.checked = backend.learnSelections;
            }
        },
        Sheet {
            id: settingsSheet
            onClosed: backend.restoreFocus()
            content: Page {
                titleBar: TitleBar {
                    title: "模块设置"
                    dismissAction: ActionItem {
                        title: "关闭"
                        imageSource: "asset:///cancel.png"
                        onTriggered: settingsSheet.close()
                    }
                }
                ScrollView {
                    horizontalAlignment: HorizontalAlignment.Fill
                    verticalAlignment: VerticalAlignment.Fill
                    Container {
                        leftPadding: 16
                        rightPadding: 16
                        topPadding: 8
                        bottomPadding: 16
                        Label { text: "启动输入方案"; textStyle.fontSize: FontSize.Small }
                        SegmentedControl {
                            id: settingsStartMode
                            Option { text: "自然码"; value: "natural"; selected: true }
                            Option { text: "全拼"; value: "full" }
                            Option { text: "English"; value: "english" }
                            onCreationCompleted: selectedValue = backend.defaultMode
                            onSelectedValueChanged: {
                                if (selectedValue !== backend.defaultMode)
                                    backend.defaultMode = selectedValue;
                            }
                        }
                        Container {
                            preferredHeight: 64
                            minHeight: 64
                            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                            Label {
                                text: "Shift 切换候选"
                                verticalAlignment: VerticalAlignment.Center
                                textStyle.fontSize: FontSize.Small
                                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                            }
                            ToggleButton {
                                id: settingsShiftCandidates
                                checked: backend.shiftCandidates
                                onCheckedChanged: {
                                    if (checked !== backend.shiftCandidates)
                                        backend.shiftCandidates = checked;
                                }
                            }
                        }
                        Container {
                            preferredHeight: 64
                            minHeight: 64
                            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                            Label {
                                text: "Shift 移动光标"
                                verticalAlignment: VerticalAlignment.Center
                                textStyle.fontSize: FontSize.Small
                                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                            }
                            ToggleButton {
                                id: settingsShiftCursor
                                checked: backend.shiftCursor
                                onCheckedChanged: {
                                    if (checked !== backend.shiftCursor)
                                        backend.shiftCursor = checked;
                                }
                            }
                        }
                        Container {
                            preferredHeight: 64
                            minHeight: 64
                            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                            Label {
                                text: "中文标点"
                                verticalAlignment: VerticalAlignment.Center
                                textStyle.fontSize: FontSize.Small
                                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                            }
                            ToggleButton {
                                id: settingsPunctuation
                                checked: backend.chinesePunctuation
                                onCheckedChanged: {
                                    if (checked !== backend.chinesePunctuation)
                                        backend.chinesePunctuation = checked;
                                }
                            }
                        }
                        Container {
                            preferredHeight: 64
                            minHeight: 64
                            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                            Label {
                                text: "本应用词频学习"
                                verticalAlignment: VerticalAlignment.Center
                                textStyle.fontSize: FontSize.Small
                                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                            }
                            ToggleButton {
                                id: settingsLearning
                                checked: backend.learnSelections
                                onCheckedChanged: {
                                    if (checked !== backend.learnSelections)
                                        backend.learnSelections = checked;
                                }
                            }
                        }
                        Button {
                            text: "发布共享设置"
                            imageSource: "asset:///tools.png"
                            horizontalAlignment: HorizontalAlignment.Fill
                            onClicked: backend.publishSettings()
                        }
                        Button {
                            text: "读取共享设置"
                            imageSource: "asset:///diagnostics.png"
                            horizontalAlignment: HorizontalAlignment.Fill
                            onClicked: backend.importSettings()
                        }
                        Label {
                            text: backend.settingsStatus
                            multiline: true
                            textStyle.fontSize: FontSize.XSmall
                        }
                    }
                }
            }
        },
        Dialog {
            id: symbolDialog
            onOpened: symbolList.requestFocus()
            onClosed: { backend.closeSymbols(); backend.restoreFocus(); }
            Container {
                horizontalAlignment: HorizontalAlignment.Fill
                verticalAlignment: VerticalAlignment.Fill
                background: Color.create("#99000000")
                layout: DockLayout {}
                inputRoute.primaryKeyTarget: true
                keyListeners: [
                    KeyListener { onKeyEvent: backend.handleKey(event) }
                ]
                Container {
                    verticalAlignment: VerticalAlignment.Bottom
                    horizontalAlignment: HorizontalAlignment.Fill
                    preferredHeight: 420
                    maxHeight: 420
                    leftPadding: 16
                    rightPadding: 16
                    topPadding: 8
                    bottomPadding: 8
                    background: Color.create("#23272b")
                    Container {
                        preferredHeight: 64
                        minHeight: 64
                        maxHeight: 64
                        layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                        Label {
                            text: "符号"
                            verticalAlignment: VerticalAlignment.Center
                            textStyle.fontSize: FontSize.Medium
                            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                        }
                        Button {
                            imageSource: "asset:///cancel.png"
                            accessibility.name: "关闭符号"
                            preferredWidth: 76
                            minWidth: 76
                            maxWidth: 76
                            preferredHeight: 64
                            minHeight: 64
                            maxHeight: 64
                            onClicked: backend.closeSymbols()
                        }
                    }
                    SegmentedControl {
                        id: symbolGroups
                        Option { text: "中文"; selected: true }
                        Option { text: "English" }
                        Option { text: "数学" }
                        onSelectedIndexChanged: backend.symbolGroup = selectedIndex
                    }
                    ListView {
                        id: symbolList
                        objectName: "symbolGrid"
                        dataModel: backend.symbolModel
                        layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                        layout: GridListLayout {
                            columnCount: 10
                            cellAspectRatio: 0.9
                            horizontalCellSpacing: 4
                            verticalCellSpacing: 4
                            headerMode: ListHeaderMode.None
                        }
                        listItemComponents: [
                            ListItemComponent {
                                type: ""
                                Container {
                                    layout: DockLayout {}
                                    opacity: ListItemData.symbolIndex >= 0 ? 1 : 0
                                    background: Color.create("#343b42")
                                    accessibility.name: ListItemData.text
                                    Label {
                                        text: ListItemData.text
                                        textFormat: TextFormat.Plain
                                        textStyle.fontSize: FontSize.Small
                                        horizontalAlignment: HorizontalAlignment.Center
                                        verticalAlignment: VerticalAlignment.Center
                                        bottomMargin: 20
                                    }
                                    Label {
                                        text: ListItemData.key
                                        textStyle.fontSize: FontSize.PointValue
                                        textStyle.fontSizeValue: 4
                                        textStyle.color: Color.create("#aab4bd")
                                        horizontalAlignment: HorizontalAlignment.Right
                                        verticalAlignment: VerticalAlignment.Bottom
                                        rightMargin: 8
                                    }
                                }
                            }
                        ]
                        onTriggered: {
                            var item = dataModel.data(indexPath);
                            if (item && item.symbolIndex >= 0)
                                backend.chooseSymbol(item.symbolIndex);
                        }
                    }
                }
            }
        }
    ]
}
