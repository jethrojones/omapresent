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

    WebEngineScript {
        id: hostBridge
        name: "omapresent-host-bridge"
        sourceCode: backend.bridgeScript()
        // Deferred, so the renderer's module has already defined
        // window.omapresent by the time onState is assigned to it.
        injectionPoint: WebEngineScript.Deferred
        worldId: WebEngineScript.MainWorld
    }

    WebEngineView {
        id: view
        anchors.fill: parent
        url: "qrc:/renderer/render.html"
        // Paint the theme, not Chromium's white, while the page loads.
        backgroundColor: backend.themeBackground
        webChannel: hostChannel
        userScripts.collection: [hostBridge]

        onLoadingChanged: function(loadRequest) {
            if (loadRequest.status === WebEngineLoadingInfo.LoadSucceededStatus) {
                pane.ready = true;
                view.runJavaScript(backend.previewRenderScript());
            } else if (loadRequest.status === WebEngineLoadingInfo.LoadStartedStatus) {
                pane.ready = false;
            }
        }
    }
}
