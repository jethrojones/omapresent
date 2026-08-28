import QtQuick
import QtWebChannel
import QtWebEngine

// The live preview: the same renderer bundle the presentation windows, the PDF
// and the published page use (docs/renderer-contract.md). Loaded through a
// Loader so that Main.qml itself never imports QtWebEngine — the unit suite
// builds Main.qml without a web engine behind it.
Item {
    id: pane
    objectName: "previewPane"

    // True once render.html has loaded and window.omapresent exists.
    property bool ready: false

    function runScript(script) {
        if (ready)
            view.runJavaScript(script);
    }

    WebChannel {
        id: hostChannel
        // The renderer reports its state back through this object; C++ never
        // polls the page for it.
        Component.onCompleted: registerObjects({"omapresentHost": backend.renderHost})
    }

    WebEngineView {
        id: view
        anchors.fill: parent
        url: "qrc:/renderer/render.html"
        // Paint the theme, not Chromium's white, while the page loads.
        backgroundColor: backend.themeBackground
        webChannel: hostChannel

        onLoadingChanged: function(loadRequest) {
            if (loadRequest.status === WebEngineLoadingInfo.LoadSucceededStatus) {
                pane.ready = true;
                // The bridge goes in here rather than as a user script because
                // this page is loaded once and never navigates, and because a
                // load that has succeeded is a load whose deferred module has
                // already defined window.omapresent for onState to hang off.
                view.runJavaScript(backend.bridgeScript());
                view.runJavaScript(backend.previewRenderScript());
            } else if (loadRequest.status === WebEngineLoadingInfo.LoadStartedStatus) {
                pane.ready = false;
            } else if (loadRequest.status === WebEngineLoadingInfo.LoadFailedStatus) {
                console.warn("preview: could not load the renderer:", loadRequest.errorString);
            }
        }

        // A renderer that throws would otherwise fail by going blank. Its
        // diagnostics belong in the app's log like everything else's.
        onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceId) {
            console.warn("renderer:", sourceId + ":" + lineNumber, message);
        }
    }
}
