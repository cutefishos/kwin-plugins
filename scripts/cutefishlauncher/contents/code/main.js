"use strict";

function forceFullScreen(window) {
    var screenGeometry = workspace.clientArea(KWin.ScreenArea, window);
    window.frameGeometry = screenGeometry;
}

function setupConnection(window) {
    if (window.resourceClass != "cutefish-launcher"
            || window.resourceName != "cutefish-launcher" || window.dialog) {
        return;
    }

    forceFullScreen(window);
    window.frameGeometryChanged.connect(function () {
        forceFullScreen(window);
    });
}

workspace.windowAdded.connect(setupConnection);

var windows = workspace.windowList();
for (var i = 0; i < windows.length; i++) {
    setupConnection(windows[i]);
}
