import bb.cascades 1.4
import QtQuick 1.0 as Quick

ListView {
    id: strip
    property variant ime
    property real viewportWidth: 688
    property int highlightedIndex: ime ? ime.selectedIndex : -1
    focusPolicy: FocusPolicy.None
    enabled: ime && ime.enabled
    dataModel: ime ? ime.candidateModel : null
    preferredHeight: 72
    minHeight: 72
    maxHeight: 72
    horizontalAlignment: HorizontalAlignment.Fill
    layout: StackListLayout {
        orientation: LayoutOrientation.LeftToRight
        headerMode: ListHeaderMode.None
    }
    listItemComponents: [
        ListItemComponent {
            type: ""
            Container {
                id: item
                property bool highlighted: ListItemData.index === ListItem.view.highlightedIndex
                focusPolicy: FocusPolicy.None
                preferredWidth: Math.min(ListItem.view.viewportWidth, Math.max(88, ListItemData.text.length * 40 + 32))
                preferredHeight: 72
                minHeight: 72
                maxHeight: 72
                leftPadding: 14
                rightPadding: 14
                rightMargin: 4
                layout: DockLayout {}
                background: highlighted ? Color.create("#25483c") : Color.create("#23272b")
                Label {
                    text: ListItemData.text
                    textFormat: TextFormat.Plain
                    multiline: false
                    horizontalAlignment: HorizontalAlignment.Fill
                    verticalAlignment: VerticalAlignment.Center
                    textStyle.fontSize: FontSize.Small
                    textStyle.color: item.highlighted ? Color.create("#80ebba") : Color.create("#f4f4f4")
                    textFit.mode: LabelTextFitMode.FitToBounds
                    textFit.minFontSizeValue: 4
                    textFit.maxFontSizeValue: 8
                }
            }
        }
    ]
    onTriggered: {
        var candidate = dataModel.data(indexPath);
        if (candidate)
            ime.chooseCandidate(candidate.session, candidate.revision, candidate.document, candidate.index);
    }
    attachedObjects: [
        LayoutUpdateHandler {
            onLayoutFrameChanged: strip.viewportWidth = layoutFrame.width
        },
        Quick.Connections {
            target: strip.ime
            onCandidatesChanged: strip.scrollToPosition(ScrollPosition.Beginning, ScrollAnimation.None)
            onHighlightChanged: {
                if (strip.highlightedIndex >= 0)
                    strip.scrollToItem([strip.highlightedIndex], ScrollAnimation.Smooth);
            }
        }
    ]
}
