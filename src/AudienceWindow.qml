import QtQuick
import QtWebChannel
import QtWebEngine

// The audience window (spec §5.1): headings, media and lists, themed, filling
// its output. A separate top-level window on purpose — Hyprland treats it as
// its own thing, and sharing just this window in a call or in OBS gives a clean
// full-frame capture with none of the presenter's notes in it.
//
// It renders nothing itself. Everything on screen came from the shared renderer
// bundle, driven by Presentation through docs/renderer-contract.md.
Window {
    id: audience
    objectName: "audienceWindow"
    visible: true
    color: background
    title: presentation.deckTitle.length > 0
        ? presentation.deckTitle + " - Omapresent"
        : "Omapresent"

    readonly property color background: paletteColor("background", "#101010")
    readonly property color foreground: paletteColor("foreground", "#eeeeee")
    readonly property color muted: paletteColor("muted", "#909090")

    function paletteColor(key, fallback) {
        var value = presentation.palette[key];
        return value !== undefined && value !== "" ? value : fallback;
    }

    onActiveChanged: if (active) keys.forceActiveFocus()
    Component.onDestruction: presentation.viewGone("audience")

    WebChannel {
        id: hostChannel
        Component.onCompleted: registerObjects({"omapresentHost": presentation.audienceHost})
    }

    WebEngineView {
        id: view
        anchors.fill: parent
        url: presentation.rendererUrl
        backgroundColor: audience.background
        webChannel: hostChannel
        // The page never needs the keyboard: Space reaches the player through
        // playPause() on the contract. Clicking a video must not take focus
        // away from the key handler.
        activeFocusOnPress: false

        onLoadingChanged: function(loadRequest) {
            if (loadRequest.status === WebEngineLoadingInfo.LoadSucceededStatus) {
                // The load event has already run render.js, so window.omapresent
                // exists and the bridge has something to hang onState on.
                view.runJavaScript(presentation.bridgeScript);
                presentation.viewReady("audience");
            } else if (loadRequest.status === WebEngineLoadingInfo.LoadStartedStatus) {
                presentation.viewGone("audience");
            }
        }
    }

    Connections {
        target: presentation
        function onRunInView(role, script) {
            if (role === "audience")
                view.runJavaScript(script);
        }
    }

    // Every key goes to the same place from either window, so the two behave
    // identically wherever the focus happens to be (spec §5.2).
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

    // Single-monitor mode: `N` brings the speaker notes up over the slide,
    // because there is no second screen to put them on (spec §5.1).
    Rectangle {
        anchors.fill: parent
        visible: presentation.notesOverlay
        color: Qt.rgba(audience.background.r, audience.background.g, audience.background.b, 0.94)

        Flickable {
            anchors.fill: parent
            anchors.margins: Math.round(audience.height * 0.06)
            contentWidth: width
            contentHeight: notesText.contentHeight
            clip: true

            Text {
                id: notesText
                width: parent.width
                text: presentation.notesHtml
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                color: audience.foreground
                font.pixelSize: Math.round(audience.height * 0.032)
                linkColor: audience.paletteColor("accent", "#7aa2f7")
            }
        }

        Text {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 16
            text: "N to close"
            color: audience.muted
            font.pixelSize: Math.round(audience.height * 0.018)
        }
    }

    // On one screen this window is the only one there is, so `Ctrl+?` has
    // nowhere else to put the reference.
    Rectangle {
        anchors.fill: parent
        visible: presentation.shortcutsVisible && presentation.singleOutput
        color: Qt.rgba(audience.background.r, audience.background.g, audience.background.b, 0.96)

        Column {
            anchors.centerIn: parent
            spacing: 6

            Repeater {
                model: presentation.shortcutReference()

                Row {
                    spacing: 18

                    Text {
                        width: Math.round(audience.width * 0.14)
                        text: modelData.split("\t")[0]
                        color: audience.paletteColor("accent", "#7aa2f7")
                        font.pixelSize: Math.round(audience.height * 0.024)
                        horizontalAlignment: Text.AlignRight
                    }

                    Text {
                        text: modelData.split("\t")[1]
                        color: audience.foreground
                        font.pixelSize: Math.round(audience.height * 0.024)
                    }
                }
            }
        }
    }
}
