import bb.cascades 1.4
import QtQuick 1.0 as Quick

Page {
    id: page
    property variant ime
    property bool sheetOpened: false
    function syncNativeScope() {
        backend.setNativeScopeActive(sheetOpened &&
            actionMenuVisualState === ActionMenuVisualState.Hidden);
    }
    signal finished()
    actionBarVisibility: ime.enabled ? ChromeVisibility.Hidden : ChromeVisibility.Visible
    actionBarAutoHideBehavior: ActionBarAutoHideBehavior.Disabled
    keysIgnoreFocusInActionBar: ime.enabled
    onCreationCompleted: syncNativeScope()
    onActionMenuVisualStateChanged: syncNativeScope()
    titleBar: TitleBar {
        title: "原生模块"
        dismissAction: ActionItem {
            title: "关闭"
            imageSource: "asset:///cancel.png"
            onTriggered: {
                backend.setNativeScopeActive(false);
                page.finished();
            }
        }
    }
    Container {
        id: nativeContent
        layout: StackLayout { orientation: LayoutOrientation.TopToBottom }
        leftPadding: 16
        rightPadding: 16
        topPadding: 8
        bottomPadding: 0
        Container {
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            SegmentedControl {
                id: modes
                property bool initialized: false
                focusPolicy: FocusPolicy.None
                enabled: ime.enabled
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                Option { text: "自然码"; value: "natural"; selected: true }
                Option { text: "English"; value: "english" }
                onCreationCompleted: {
                    selectedValue = ime.mode;
                    initialized = true;
                }
                onSelectedValueChanged: {
                    if (initialized && ime.enabled) ime.mode = selectedValue;
                }
            }
            ImeToggle {
                inputEnabled: ime.enabled
                inputMode: ime.mode
                enabled: ime.ready || ime.enabled
                onToggleRequested: backend.toggleNativeIme()
            }
        }
        TextField {
            id: title
            objectName: "moduleTitle"
            textFormat: TextFormat.Plain
            hintText: "标题"
            maximumLength: 96
            inputMode: TextFieldInputMode.Custom
            builtInShortcutsEnabled: false
            input.flags: TextInputFlag.VirtualKeyboardOff
            onCreationCompleted: backend.registerNativeEditor(title)
            onFocusedChanged: backend.nativeEditorFocusChanged(title, focused)
            keyListeners: [ KeyListener { onKeyEvent: backend.handleNativeKey(title, event) } ]
        }
        TextArea {
            id: body
            objectName: "moduleBody"
            textFormat: TextFormat.Plain
            hintText: "正文"
            maximumLength: 16384
            inputMode: TextAreaInputMode.Custom
            builtInShortcutsEnabled: false
            input.flags: TextInputFlag.VirtualKeyboardOff
            minHeight: 88
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            onCreationCompleted: backend.registerNativeEditor(body)
            onFocusedChanged: backend.nativeEditorFocusChanged(body, focused)
            keyListeners: [ KeyListener { onKeyEvent: backend.handleNativeKey(body, event) } ]
        }
        TextField {
            id: password
            objectName: "modulePassword"
            inputMode: TextFieldInputMode.Password
            hintText: "密码"
            maximumLength: 64
            onFocusedChanged: backend.nativeEditorFocusChanged(password, focused)
        }
        Label {
            text: ime.composition.length ? ime.composition : " "
            textStyle.fontSize: FontSize.Small
            textStyle.color: Color.create("#68d7ab")
        }
        CandidateStrip { ime: page.ime }
    }
    attachedObjects: [
        Quick.Connections {
            target: page.ime
            onChanged: {
                if (modes.selectedValue !== page.ime.mode)
                    modes.selectedValue = page.ime.mode;
            }
            onSubmitRequested: {
                if (editor === title) body.requestFocus();
            }
        }
    ]
}
