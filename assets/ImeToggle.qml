import bb.cascades 1.4

// Presentation only: enabled state and language always come from the host.
Button {
    property bool inputEnabled: false
    property string inputMode: "natural"
    signal toggleRequested()
    objectName: "imeToggle"
    focusPolicy: FocusPolicy.None
    text: inputEnabled ? (inputMode === "english" ? "EN" : "中") : ""
    imageSource: inputEnabled ? "" : "asset:///ime-menu.png"
    accessibility.name: inputEnabled ?
        (inputMode === "english" ? "英文输入，显示菜单并暂停输入" : "自然码输入，显示菜单并暂停输入") :
        "隐藏菜单，恢复应用输入法"
    preferredWidth: 76
    minWidth: 76
    maxWidth: 76
    preferredHeight: 64
    minHeight: 64
    maxHeight: 64
    topMargin: 0
    bottomMargin: 0
    horizontalAlignment: HorizontalAlignment.Right
    onClicked: toggleRequested()
}
