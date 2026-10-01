import bb.cascades 1.4

// Presentation only: enabled state and language always come from the host.
Container {
    id: toggle
    property bool inputEnabled: false
    property string inputMode: "natural"
    property string text: inputEnabled ? (inputMode === "english" ? "EN" : "中") : ""
    property url imageSource: inputEnabled ? "" : "asset:///ime-menu.png"
    property bool touchPressed: false
    signal toggleRequested()
    signal clicked()
    objectName: "imeToggle"
    focusPolicy: FocusPolicy.None
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
    layout: DockLayout {}
    background: Color.create(touchPressed ? "#435058" : "#30383e")
    onEnabledChanged: { if (!enabled) touchPressed = false; }
    onTouch: {
        if (!enabled) return;
        if (event.isDown()) touchPressed = true;
        else if (event.isCancel()) touchPressed = false;
        else if (event.isMove() && (event.localX < 0 || event.localX >= 76 || event.localY < 0 || event.localY >= 64))
            touchPressed = false;
        else if (event.isUp()) {
            var activate = touchPressed && event.localX >= 0 && event.localX < 76 && event.localY >= 0 && event.localY < 64;
            touchPressed = false;
            if (activate) clicked();
        }
    }
    onClicked: { if (enabled) toggleRequested(); }
    ImageView {
        imageSource: toggle.imageSource
        visible: !toggle.inputEnabled
        preferredWidth: 36
        minWidth: 36
        maxWidth: 36
        preferredHeight: 36
        minHeight: 36
        maxHeight: 36
        scalingMethod: ScalingMethod.AspectFit
        horizontalAlignment: HorizontalAlignment.Center
        verticalAlignment: VerticalAlignment.Center
    }
    Label {
        text: toggle.text
        visible: toggle.inputEnabled
        textFormat: TextFormat.Plain
        textStyle.fontSize: FontSize.Small
        textStyle.color: Color.White
        horizontalAlignment: HorizontalAlignment.Center
        verticalAlignment: VerticalAlignment.Center
    }
}
