import bb.cascades 1.4
import QtQuick 1.0 as Quick

Page {
    id: page
    property bool sheetActive: false
    function syncMainScope() {
        backend.setMainScopeActive(!sheetActive &&
            actionMenuVisualState === ActionMenuVisualState.Hidden);
    }
    actionBarVisibility: backend.imeEnabled ? ChromeVisibility.Hidden : ChromeVisibility.Visible
    actionBarAutoHideBehavior: ActionBarAutoHideBehavior.Disabled
    keysIgnoreFocusInActionBar: backend.imeEnabled
    onCreationCompleted: syncMainScope()
    onActionMenuVisualStateChanged: syncMainScope()
    Container {
        id: content
        implicitLayoutAnimationsEnabled: false
        layout: StackLayout { orientation: LayoutOrientation.TopToBottom }
        leftPadding: 16
        rightPadding: 16
        topPadding: 8
        bottomPadding: 0
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
                focusPolicy: FocusPolicy.None
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
            ImeToggle {
                id: imeToggle
                inputEnabled: backend.imeEnabled
                inputMode: backend.mode
                onToggleRequested: backend.toggleIme()
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
            focusPolicy: FocusPolicy.None
            enabled: backend.imeEnabled
            topMargin: 4
            bottomMargin: 4
            Option { text: "自然码"; value: "natural"; selected: true }
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
            builtInShortcutsEnabled: false
            input.flags: TextInputFlag.VirtualKeyboardOff
            inputRoute.primaryKeyTarget: backend.imeEnabled && focused
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
            onFocusedChanged: backend.editorFocusChanged(focused)
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
        ListView {
            id: candidates
            focusPolicy: FocusPolicy.None
            visible: backend.imeEnabled
            enabled: backend.imeEnabled
            objectName: "candidateStrip"
            property int highlightedIndex: backend.selectedIndex
            property real viewportWidth: 688
            property int pendingIndex: -1
            property variant pendingGeneration: null
            preferredHeight: 72
            minHeight: 72
            maxHeight: 72
            topMargin: 0
            bottomMargin: 0
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Bottom
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
                        focusPolicy: FocusPolicy.None
                        property int candidateIndex: ListItem.indexPath.length ? ListItem.indexPath[0] : -1
                        property bool highlighted: candidateIndex === ListItem.view.highlightedIndex
                        onTouch: {
                            if (event.isDown()) {
                                ListItem.view.pendingIndex = candidateIndex;
                                ListItem.view.pendingGeneration = ListItemData.generation;
                            } else if (event.isCancel()) {
                                ListItem.view.pendingIndex = -1;
                                ListItem.view.pendingGeneration = null;
                            }
                        }
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
            onTriggered: {
                var touchedIndex = pendingIndex;
                var touchedGeneration = pendingGeneration;
                pendingIndex = -1;
                pendingGeneration = null;
                if (touchedIndex >= 0) {
                    if (touchedIndex === indexPath[0])
                        backend.chooseCandidateAt(touchedIndex, touchedGeneration);
                    return;
                }
                var item = dataModel.data(indexPath);
                if (item) backend.chooseCandidateAt(indexPath[0], item.generation);
            }
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
                page.sheetActive = true;
                page.syncMainScope();
                if (backend.startNativeModule()) moduleSheet.open();
                else {
                    page.sheetActive = false;
                    page.syncMainScope();
                }
            }
        },
        ActionItem {
            title: "设置"
            imageSource: "asset:///tools.png"
            ActionBar.placement: ActionBarPlacement.InOverflow
            onTriggered: {
                page.sheetActive = true;
                page.syncMainScope();
                settingsSheet.open();
            }
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
                id: nativeModulePage
                ime: backend.nativeModule
                onFinished: {
                    backend.setNativeScopeActive(false);
                    nativeModulePage.sheetOpened = false;
                    moduleSheet.close();
                }
            }
            onOpened: {
                nativeModulePage.sheetOpened = true;
                nativeModulePage.syncNativeScope();
            }
            onClosed: {
                backend.setNativeScopeActive(false);
                nativeModulePage.sheetOpened = false;
                backend.stopNativeModule();
                page.sheetActive = false;
                page.syncMainScope();
            }
        },
        Quick.Connections {
            target: backend
            onChanged: {
                if (modes.selectedValue !== backend.mode)
                    modes.selectedValue = backend.mode;
            }
            onCandidatesChanged: candidates.scrollToPosition(ScrollPosition.Beginning, ScrollAnimation.None)
            onHighlightChanged: candidates.followHighlight(true)
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
            onClosed: {
                page.sheetActive = false;
                page.syncMainScope();
            }
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
        }
    ]
}
