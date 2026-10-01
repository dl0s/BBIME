import bb.cascades 1.4
import QtQuick 1.0 as Quick

Dialog {
    id: panel
    property variant ime
    onOpened: grid.requestFocus()
    onClosed: { ime.closeSymbols(); ime.restoreFocus(); }
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
                onSelectedIndexChanged: panel.ime.symbolGroup = selectedIndex
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
                onSymbolsChanged: {
                    if (panel.ime.symbolsVisible) {
                        if (!panel.opened) panel.open();
                    } else if (panel.opened) panel.close();
                    if (groups.selectedIndex !== panel.ime.symbolGroup)
                        groups.selectedIndex = panel.ime.symbolGroup;
                }
            }
        ]
    }
}
