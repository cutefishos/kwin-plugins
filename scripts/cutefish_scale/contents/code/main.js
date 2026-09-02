/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Reven Martin <revenmartin@gmail.com>
    SPDX-FileCopyrightText: 2018, 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

function isCutefishShell(window) {
    return window.windowClass
        .toLowerCase()
        .split(/\s+/)
        .indexOf("cutefish-shell") !== -1;
}

var scaleEffect = {
    loadConfig: function () {
        var defaultDuration = 250;
        var duration = effect.readConfig("Duration", defaultDuration) || defaultDuration;
        scaleEffect.duration = animationTime(duration);
        scaleEffect.inScale = 0.96;
        scaleEffect.inOpacity = 1.0;
        scaleEffect.outScale = 0.96;
        scaleEffect.outOpacity = 0.0;
    },
    isScaleWindow: function (window) {
        if (window.hasDecoration) {
            return true;
        }

        // Don't animate combobox popups, tooltips, popup menus, etc.
        if (window.popupWindow) {
            return false;
        }

        // Don't animate the outline and the screenlocker, it looks bad.
        if (window.lockScreen || window.outline) {
            return false;
        }

        // Override-redirect windows are usually used for user interface
        // concepts that are not expected to be animated by this effect.
        if (!window.managed) {
            return false;
        }

        return window.normalWindow || window.dialog;
    },
    setupForcedRoles: function (window) {
        window.setData(Effect.WindowForceBackgroundContrastRole, true);
        window.setData(Effect.WindowForceBlurRole, true);
    },
    cleanupForcedRoles: function (window) {
        window.setData(Effect.WindowForceBackgroundContrastRole, null);
        window.setData(Effect.WindowForceBlurRole, null);
    },
    slotWindowAdded: function (window) {
        if (isCutefishShell(window) && window.caption === "Launcher") {
            return;
        }
        if (effects.hasActiveFullScreenEffect) {
            return;
        }
        if (!scaleEffect.isScaleWindow(window)) {
            return;
        }
        if (!window.visible) {
            return;
        }
        if (effect.isGrabbed(window, Effect.WindowAddedGrabRole)) {
            return;
        }
        scaleEffect.setupForcedRoles(window);
        window.scaleInAnimation = animate({
            window: window,
            curve: QEasingCurve.InOutSine,
            duration: scaleEffect.duration,
            animations: [
                {
                    type: Effect.Scale,
                    from: scaleEffect.inScale
                },
                {
                    type: Effect.Opacity,
                    from: scaleEffect.inOpacity
                }
            ]
        });
    },
    slotWindowClosed: function (window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }
        if (!scaleEffect.isScaleWindow(window)) {
            return;
        }
        if (!window.visible || window.skipsCloseAnimation) {
            return;
        }
        if (effect.isGrabbed(window, Effect.WindowClosedGrabRole)) {
            return;
        }
        if (window.scaleInAnimation) {
            cancel(window.scaleInAnimation);
            delete window.scaleInAnimation;
        }
        scaleEffect.setupForcedRoles(window);
        window.scaleOutAnimation = animate({
            window: window,
            curve: QEasingCurve.InOutSine,
            duration: scaleEffect.duration,
            animations: [
                {
                    type: Effect.Scale,
                    to: scaleEffect.outScale
                },
                {
                    type: Effect.Opacity,
                    to: scaleEffect.outOpacity
                }
            ]
        });
    },
    slotWindowDataChanged: function (window, role) {
        if (role == Effect.WindowAddedGrabRole) {
            if (window.scaleInAnimation && effect.isGrabbed(window, role)) {
                cancel(window.scaleInAnimation);
                delete window.scaleInAnimation;
                scaleEffect.cleanupForcedRoles(window);
            }
        } else if (role == Effect.WindowClosedGrabRole) {
            if (window.scaleOutAnimation && effect.isGrabbed(window, role)) {
                cancel(window.scaleOutAnimation);
                delete window.scaleOutAnimation;
                scaleEffect.cleanupForcedRoles(window);
            }
        }
    },
    init: function () {
        scaleEffect.loadConfig();

        effect.configChanged.connect(scaleEffect.loadConfig);
        effect.animationEnded.connect(scaleEffect.cleanupForcedRoles);
        effects.windowAdded.connect(scaleEffect.slotWindowAdded);
        effects.windowClosed.connect(scaleEffect.slotWindowClosed);
        effects.windowDataChanged.connect(scaleEffect.slotWindowDataChanged);
    }
};

scaleEffect.init();
