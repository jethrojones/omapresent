import QtQuick
import QtQuick.Layouts
import QtWebChannel
import QtWebEngine

// The presenter window (spec §5.1): the current slide scaled down, the next
// slide, the speaker notes as formatted Markdown, an elapsed timer that resets
// on click, a wall clock, the slide number and the deck's recall bindings.
//
// It only exists when there are two outputs. On one screen there is nothing to
// put it on, so the audience window carries the notes overlay instead.
Window {
    id: presenter
    objectName: "presenterWindow"
    visible: true
    color: background
    title: "Presenter - Omapresent"

    readonly property color background: paletteColor("dark_background", paletteColor("background", "#101010"))
    readonly property color foreground: paletteColor("foreground", "#eeeeee")
    readonly property color muted: paletteColor("muted", "#909090")
    readonly property color accent: paletteColor("accent", "#7aa2f7")
    readonly property int baseFontSize: Math.max(12, Math.round(height * 0.018))

    function paletteColor(key, fallback) {
        var value = presentation.palette[key];
        return value !== undefined && value !== "" ? value : fallback;
    }

    function twoDigits(value) {
        return value < 10 ? "0" + value : "" + value;
    }

    function elapsedText() {
        var total = presentation.elapsedSeconds;
        var hours = Math.floor(total / 3600);
        var minutes = Math.floor((total % 3600) / 60);
        var seconds = total % 60;
        return (hours > 0 ? hours + ":" + twoDigits(minutes) : "" + minutes)
            + ":" + twoDigits(seconds);
    }

    onActiveChanged: if (active) keys.forceActiveFocus()
    Component.onDestruction: {
        presentation.viewGone("presenter");
        presentation.viewGone("preview");
    }

    WebChannel {
        id: hostChannel
        Component.onCompleted: registerObjects({"omapresentHost": presentation.presenterHost})
    }

    Connections {
        target: presentation
        function onRunInView(role, script) {
            if (role === "presenter")
                currentView.runJavaScript(script);
            else if (role === "preview")
                previewView.runJavaScript(script);
        }
    }

    Timer {
        id: wallClock
        interval: 1000
        repeat: true
        running: true
        triggeredOnStart: true
        property string text: ""
        onTriggered: text = Qt.formatTime(new Date(), "h:mm AP")
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // --- What the audience is looking at right now --------------------
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 3
            spacing: 6

            Text {
                text: presentation.heading.length > 0 ? presentation.heading : "Current"
                color: presenter.muted
                font.pixelSize: presenter.baseFontSize
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: presenter.paletteColor("background", "#101010")
                border.color: presenter.muted
                border.width: 1

                WebEngineView {
                    id: currentView
                    anchors.fill: parent
                    anchors.margins: 1
                    url: presentation.rendererUrl
                    backgroundColor: presenter.paletteColor("background", "#101010")
                    webChannel: hostChannel
                    activeFocusOnPress: false

                    onLoadingChanged: function(loadRequest) {
                        if (loadRequest.status === WebEngineLoadingInfo.LoadSucceededStatus) {
                            // render.js has run by now, so onState has something
                            // to attach to.
                            currentView.runJavaScript(presentation.bridgeScript);
                            presentation.viewReady("presenter");
                        } else if (loadRequest.status === WebEngineLoadingInfo.LoadStartedStatus) {
                            presentation.viewGone("presenter");
                        }
                    }
                }

                // What the audience screen is doing when it is not showing the
                // slide, so the speaker is never guessing (spec §5.2 B / W).
                Rectangle {
                    anchors.fill: parent
                    visible: presentation.blank.length > 0
                    color: Qt.rgba(0, 0, 0, 0.72)

                    Text {
                        anchors.centerIn: parent
                        text: "Audience screen is " + presentation.blank
                        color: "#ffffff"
                        font.pixelSize: presenter.baseFontSize * 1.6
                    }
                }
            }
        }

        // --- Clock, next slide, notes, bindings ---------------------------
        ColumnLayout {
            Layout.fillHeight: true
            Layout.preferredWidth: 2
            Layout.fillWidth: true
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                // Click the elapsed timer to start it again (spec §5.1).
                Text {
                    text: presenter.elapsedText()
                    color: presenter.foreground
                    font.pixelSize: presenter.baseFontSize * 2.2

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: presentation.resetTimer()
                    }
                }

                Text {
                    text: wallClock.text
                    color: presenter.muted
                    font.pixelSize: presenter.baseFontSize * 1.4
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: (presentation.slideIndex + 1) + " / " + presentation.slideCount
                    color: presenter.foreground
                    font.pixelSize: presenter.baseFontSize * 1.4
                }
            }

            Text {
                text: presentation.slideIndex + 1 < presentation.slideCount
                    ? "Next" : "Next - end of deck"
                color: presenter.muted
                font.pixelSize: presenter.baseFontSize
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.round(presenter.height * 0.24)
                color: presenter.paletteColor("background", "#101010")
                border.color: presenter.muted
                border.width: 1

                WebEngineView {
                    id: previewView
                    anchors.fill: parent
                    anchors.margins: 1
                    url: presentation.rendererUrl
                    backgroundColor: presenter.paletteColor("background", "#101010")
                    activeFocusOnPress: false
                    // No host bridge: the preview never reports state, it only
                    // ever gets told which slide to show.

                    onLoadingChanged: function(loadRequest) {
                        if (loadRequest.status === WebEngineLoadingInfo.LoadSucceededStatus)
                            presentation.viewReady("preview");
                        else if (loadRequest.status === WebEngineLoadingInfo.LoadStartedStatus)
                            presentation.viewGone("preview");
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: presentation.slideIndex + 1 >= presentation.slideCount
                    text: "End of deck"
                    color: presenter.muted
                    font.pixelSize: presenter.baseFontSize
                }
            }

            Text {
                text: "Notes"
                color: presenter.muted
                font.pixelSize: presenter.baseFontSize
            }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: notesText.contentHeight
                clip: true

                Text {
                    id: notesText
                    width: parent.width
                    // Already formatted Markdown from the renderer; the
                    // presenter never sees the raw source (spec §5.1).
                    text: presentation.notesHtml
                    textFormat: Text.RichText
                    wrapMode: Text.Wrap
                    color: presenter.foreground
                    linkColor: presenter.accent
                    font.pixelSize: presenter.baseFontSize * 1.25
                }
            }

            // The deck's recall bindings, so the speaker can see what is
            // poppable without remembering it (spec §4.9).
            Flow {
                Layout.fillWidth: true
                spacing: 8
                visible: presentation.recallKeys.length > 0

                Repeater {
                    model: presentation.recallKeys

                    Rectangle {
                        width: keyLabel.implicitWidth + 16
                        height: keyLabel.implicitHeight + 8
                        radius: 3
                        color: presentation.recall === modelData
                            ? presenter.accent
                            : presenter.paletteColor("selection", "#303030")

                        Text {
                            id: keyLabel
                            anchors.centerIn: parent
                            text: modelData
                            color: presentation.recall === modelData
                                ? presenter.paletteColor("background", "#101010")
                                : presenter.foreground
                            font.pixelSize: presenter.baseFontSize
                        }
                    }
                }
            }

            // Whatever is half-typed or switched on, in one line.
            Text {
                Layout.fillWidth: true
                color: presenter.accent
                font.pixelSize: presenter.baseFontSize * 1.2
                text: {
                    var parts = [];
                    if (presentation.jumpBuffer.length > 0)
                        parts.push("go to slide " + presentation.jumpBuffer + "...");
                    if (presentation.overview)
                        parts.push("overview");
                    if (presentation.recall.length > 0)
                        parts.push("recall " + presentation.recall);
                    return parts.join("   ");
                }
            }
        }
    }

    Item {
        id: keys
        anchors.fill: parent
        focus: true

        Keys.onPressed: function(event) {
            event.accepted = presentation.handleKey(event.key, event.modifiers, event.text);
        }

        WheelHandler {
            onWheel: function(event) { presentation.scrollBy(-event.angleDelta.y); }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: presentation.shortcutsVisible
        color: Qt.rgba(presenter.background.r, presenter.background.g, presenter.background.b, 0.96)

        Column {
            anchors.centerIn: parent
            spacing: 6

            Repeater {
                model: presentation.shortcutReference()

                Row {
                    spacing: 18

                    Text {
                        width: Math.round(presenter.width * 0.18)
                        text: modelData.split("\t")[0]
                        color: presenter.accent
                        font.pixelSize: presenter.baseFontSize * 1.3
                        horizontalAlignment: Text.AlignRight
                    }

                    Text {
                        text: modelData.split("\t")[1]
                        color: presenter.foreground
                        font.pixelSize: presenter.baseFontSize * 1.3
                    }
                }
            }
        }
    }
}
