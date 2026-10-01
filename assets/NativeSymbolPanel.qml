import bb.cascades 1.4
import QtQuick 1.0 as Quick

Dialog {
    id: panel
    property variant ime
    // 0 closed, 1 opening, 2 open, 3 closing. SDK opened changes only
    // after the animation, and ignores close/open requests during it.
    property int animationPhase: 0
    property bool syncingGroup: false
    function reconcile() {
        if (!ime) return;
        if (groups.selectedIndex !== ime.symbolGroup) {
            syncingGroup = true;
            groups.selectedIndex = ime.symbolGroup;
            syncingGroup = false;
        }
        if (ime.symbolsVisible && animationPhase === 0) {
            animationPhase = 1;
            ime.setSymbolPanelActive(true);
            open();
        } else if (!ime.symbolsVisible && animationPhase === 2) {
            animationPhase = 3;
            close();
        }
    }
    onOpened: {
        animationPhase = 2;
        if (ime && ime.symbolsVisible) grid.requestFocus();
        reconcile();
    }
    onClosed: {
        var requestedClose = animationPhase === 3;
        animationPhase = 0;
        if (!ime) return;
        // An unsolicited dismissal cancels this panel. A requested close
        // must not cancel a newer open intent that arrived during animation.
        if (!requestedClose && ime.symbolsVisible) ime.closeSymbols();
        if (!ime.symbolsVisible) {
            ime.setSymbolPanelActive(false);
            ime.restoreFocus();
        }
        reconcile();
    }
    onCreationCompleted: reconcile()
    Container {
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        background: Color.create("#99000000")
        layout: DockLayout {}
        inputRoute.primaryKeyTarget: true
        keyListeners: [ KeyListener { onKeyEvent: panel.ime.handleSymbolKey(event) } ]
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
                    onClicked: panel.ime.closeSymbols()
                }
            }
            SegmentedControl {
                id: groups
                Option { text: "中文"; selected: true }
                Option { text: "English" }
                Option { text: "数学" }
                onSelectedIndexChanged: {
                    if (!panel.syncingGroup && panel.ime && panel.ime.symbolsVisible)
                        panel.ime.symbolGroup = selectedIndex;
                }
            }
            ListView {
                id: grid
                dataModel: panel.ime.symbolModel
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
                            opacity: ListItemData.index >= 0 ? 1 : 0
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
                    if (item && item.index >= 0)
                        panel.ime.chooseSymbol(item.session, item.revision,
                            item.document, item.panel, item.group, item.index);
                }
            }
        }
        attachedObjects: [
            Quick.Connections {
                target: panel.ime
                onSymbolsChanged: panel.reconcile()
            }
        ]
    }
}
