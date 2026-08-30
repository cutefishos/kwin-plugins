"use strict";

// KWin never lets an ordinary window grow past the work area, and since the
// status bar and the dock now reserve work area on Wayland as well, the
// launcher would come up as a screen-sized window trimmed by both struts. It is
// a full-screen overlay, so it is forced back onto the whole screen area here.
//
// It stays an ordinary window on purpose: a real full-screen window would be
// stacked above the dock, and the dock is meant to float on top of the
// launcher. The status bar drops below it on its own while the launcher is up.

function forceFullScreen(window) {
    var screenGeometry = workspace.clientArea(KWin.ScreenArea, window);
    if (window.frameGeometry.x !== screenGeometry.x
            || window.frameGeometry.y !== screenGeometry.y
            || window.frameGeometry.width !== screenGeometry.width
            || window.frameGeometry.height !== screenGeometry.height) {
        window.frameGeometry = screenGeometry;
    }
}

function isLauncher(window) {
    if (window.dialog) {
        return false;
    }

    // The status bar, the dock, the launcher and the desktop are one process
    // since 0.9, so they all share a resource class and only the window title
    // tells them apart. The standalone binary is still matched for older
    // sessions.
    if (window.resourceClass === "cutefish-shell") {
        return window.caption === "Launcher";
    }

    return window.resourceClass === "cutefish-launcher"
            && window.resourceName === "cutefish-launcher";
}

function setupConnection(window) {
    if (!isLauncher(window)) {
        return;
    }

    // Belt and braces for whichever open/close animation effect is enabled:
    // the launcher is a full-screen overlay that animates its own contents, so
    // having the compositor scale or fade the whole surface on top of that
    // looks wrong. The scale and popup effects skip it by window class too.
    window.skipsCloseAnimation = true;

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
