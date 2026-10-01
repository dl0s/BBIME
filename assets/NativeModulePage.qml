import bb.cascades 1.4
import QtQuick 1.0 as Quick

Page {
    id: page
    property variant ime
    signal finished()
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
                focusPolicy: FocusPolicy.None
                enabled: ime.ready
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                Option { text: "自然码"; value: "natural"; selected: true }
                Option { text: "全拼"; value: "full" }
                Option { text: "English"; value: "english" }
                onSelectedValueChanged: ime.mode = selectedValue
            }
            Button {
                focusPolicy: FocusPolicy.None
                text: ime.enabled ? (ime.mode === "english" ? "EN" : "中") : ""
                imageSource: ime.enabled ? "" : "asset:///tools.png"
                enabled: ime.ready
                preferredWidth: 76
                minWidth: 76
                maxWidth: 76
                accessibility.name: ime.enabled ? "暂停输入" : "启用输入"
                onClicked: ime.enabled = !ime.enabled
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
        }
        TextField {
            id: title
            objectName: "moduleTitle"
            textFormat: TextFormat.Plain
            hintText: "标题"
            maximumLength: 96
            onCreationCompleted: ime.registerEditor(title, "text", false)
            keyListeners: [ KeyListener { onKeyEvent: ime.handleKey(title, event) } ]
        }
        TextArea {
            id: body
            objectName: "moduleBody"
            textFormat: TextFormat.Plain
            hintText: "正文"
            maximumLength: 16384
            minHeight: 88
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            onCreationCompleted: ime.registerEditor(body, "text", false)
            keyListeners: [ KeyListener { onKeyEvent: ime.handleKey(body, event) } ]
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
