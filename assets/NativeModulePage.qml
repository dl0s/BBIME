import bb.cascades 1.4
import QtQuick 1.0 as Quick

Page {
    id: page
    property variant ime
    signal finished()
    actionBarVisibility: ime.enabled ? ChromeVisibility.Hidden : ChromeVisibility.Visible
    actionBarAutoHideBehavior: ActionBarAutoHideBehavior.Disabled
    keysIgnoreFocusInActionBar: ime.enabled
    onActionMenuVisualStateChanged: {
        if (actionMenuVisualState === ActionMenuVisualState.VisibleFull ||
            actionMenuVisualState === ActionMenuVisualState.AnimatingToVisibleFull)
            ime.suspend();
    }
    titleBar: TitleBar {
        title: "原生模块"
        dismissAction: ActionItem {
            title: "关闭"
            imageSource: "asset:///cancel.png"
            onTriggered: { ime.suspend(); page.finished(); }
        }
    }
    Container {
        leftPadding: 16
        rightPadding: 16
        topPadding: 8
        bottomPadding: 8
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
            Button {
                focusPolicy: FocusPolicy.None
                text: "Sym"
                enabled: ime.enabled
                preferredWidth: 76
                minWidth: 76
                maxWidth: 76
                onClicked: ime.cycleSymbols()
            }
            ImeToggle {
                inputEnabled: ime.enabled
                inputMode: ime.mode
                enabled: ime.ready || ime.enabled
                onToggleRequested: {
                    ime.enabled = !ime.enabled;
                    if (ime.enabled) ime.restoreFocus();
                }
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
            onCreationCompleted: ime.registerEditor(title, "text", false)
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
            onCreationCompleted: ime.registerEditor(body, "text", false)
            keyListeners: [ KeyListener { onKeyEvent: backend.handleNativeKey(body, event) } ]
        }
        TextField {
            id: password
            objectName: "modulePassword"
            inputMode: TextFieldInputMode.Password
            hintText: "密码"
            maximumLength: 64
        }
        Label {
            text: ime.composition.length ? ime.composition : " "
            textStyle.fontSize: FontSize.Small
            textStyle.color: Color.create("#68d7ab")
        }
        CandidateStrip { ime: page.ime }
    }
    attachedObjects: [
        NativeSymbolPanel { ime: page.ime },
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
