import bb.cascades 1.4
import QtQuick 1.0 as Quick

ListView {
    id: strip
    property variant ime
    property real viewportWidth: 688
    property int highlightedIndex: ime ? ime.selectedIndex : -1
    property int pendingIndex: -1
    property variant pendingTicket: null
    focusPolicy: FocusPolicy.None
    visible: ime && ime.enabled
    enabled: ime && ime.enabled
    dataModel: ime ? ime.candidateModel : null
    preferredHeight: 72
    minHeight: 72
    maxHeight: 72
    topMargin: 0
    bottomMargin: 0
    horizontalAlignment: HorizontalAlignment.Fill
    verticalAlignment: VerticalAlignment.Bottom
    layout: StackListLayout {
        orientation: LayoutOrientation.LeftToRight
        headerMode: ListHeaderMode.None
    }
    listItemComponents: [
        ListItemComponent {
            type: ""
            Container {
                id: item
                property int candidateIndex: ListItem.indexPath.length ? ListItem.indexPath[0] : -1
                property bool highlighted: ListItemData.index === ListItem.view.highlightedIndex
                focusPolicy: FocusPolicy.None
                onTouch: {
                    if (event.isDown()) {
                        ListItem.view.pendingIndex = candidateIndex;
                        ListItem.view.pendingTicket = {
                            session: ListItemData.session,
                            revision: ListItemData.revision,
                            document: ListItemData.document,
                            index: ListItemData.index
                        };
                    } else if (event.isCancel()) {
                        ListItem.view.pendingIndex = -1;
                        ListItem.view.pendingTicket = null;
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
        var touchedIndex = pendingIndex;
        var candidate = pendingTicket;
        pendingIndex = -1;
        pendingTicket = null;
        if (touchedIndex >= 0) {
            if (touchedIndex === indexPath[0] && candidate)
                ime.chooseCandidate(candidate.session, candidate.revision, candidate.document, candidate.index);
            return;
        }
        candidate = dataModel.data(indexPath);
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
