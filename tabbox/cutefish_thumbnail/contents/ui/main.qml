/*
    SPDX-FileCopyrightText: 2020 Chris Holland <zrenfire@gmail.com>
    SPDX-FileCopyrightText: 2021 CutefishOS Team

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

import org.kde.kwin 3.0 as KWin

import FishUI 1.0 as FishUI

// https://techbase.kde.org/Development/Tutorials/KWin/WindowSwitcher

KWin.TabBoxSwitcher {
    id: tabBox

    Window {
        id: dialog

        visible: tabBox.visible
        flags: Qt.BypassWindowManagerHint | Qt.FramelessWindowHint
        color: "transparent"

        readonly property int maxWidth: tabBox.screenGeometry.width * 0.95
        readonly property int maxHeight: tabBox.screenGeometry.height * 0.7
        readonly property int maxGridColumnsByWidth: Math.max(1, Math.floor(maxWidth / thumbnailGridView.cellWidth))

        property int gridColumns: maxGridColumnsByWidth
        readonly property int gridRows: Math.ceil(thumbnailGridView.count / gridColumns)
        readonly property int optimalWidth: thumbnailGridView.cellWidth * gridColumns
        readonly property int optimalHeight: thumbnailGridView.cellHeight * gridRows

        width: Math.min(Math.max(thumbnailGridView.cellWidth, optimalWidth), maxWidth)
        height: Math.min(Math.max(thumbnailGridView.cellHeight, optimalHeight), maxHeight)

        x: tabBox.screenGeometry.x + (tabBox.screenGeometry.width - dialog.width) / 2
        y: tabBox.screenGeometry.y + (tabBox.screenGeometry.height - dialog.height) / 2

        FishUI.WindowHelper {
            id: windowHelper
        }

        FishUI.WindowBlur {
            view: dialog
            geometry: Qt.rect(dialog.x, dialog.y, dialog.width, dialog.height)
            windowRadius: _background.radius
            enabled: windowHelper.compositing
        }

        FishUI.WindowShadow {
            view: dialog
            geometry: Qt.rect(dialog.x, dialog.y, dialog.width, dialog.height)
            radius: _background.radius
        }

        Rectangle {
            id: _background
            anchors.fill: parent
            radius: windowHelper.compositing ? 14 : 0
            color: FishUI.Theme.backgroundColor
            opacity: windowHelper.compositing ? (FishUI.Theme.darkMode ? 0.3 : 0.4) : 1.0

            border.color: FishUI.Theme.darkMode ? "#686868" : "#D9D9D9"
            border.width: windowHelper.compositing ? 0 : 1
        }

        onVisibleChanged: {
            if (visible) {
                dialogMainItem.calculateColumnCount();
            } else {
                thumbnailGridView.highCount = 0;
            }
        }

        Item {
            id: dialogMainItem
            anchors.fill: parent

            readonly property real screenFactor: tabBox.screenGeometry.width / tabBox.screenGeometry.height

            clip: true

            // Simple greedy algorithm
            function calculateColumnCount() {
                if (thumbnailGridView.count === 0) {
                    dialog.gridColumns = 1;
                    return;
                }

                // respect screenGeometry
                var c = Math.min(thumbnailGridView.count, dialog.maxGridColumnsByWidth);

                var residue = thumbnailGridView.count % c;
                if (residue == 0) {
                    dialog.gridColumns = c;
                    return;
                }

                // start greedy recursion
                dialog.gridColumns = columnCountRecursion(c, c, c - residue);
            }

            // Step for the greedy algorithm
            function columnCountRecursion(prevC, prevBestC, prevDiff) {
                var c = prevC - 1;

                // don't increase vertical extent more than horizontal
                // and don't exceed maxHeight
                if (c < 1 || prevC * prevC <= thumbnailGridView.count + prevDiff ||
                        dialog.maxHeight < Math.ceil(thumbnailGridView.count / c) * thumbnailGridView.cellHeight) {
                    return prevBestC;
                }
                var residue = thumbnailGridView.count % c;
                // halts algorithm at some point
                if (residue == 0) {
                    return c;
                }
                // empty slots
                var diff = c - residue;

                // compare it to the previous count of empty slots
                if (diff < prevDiff) {
                    return columnCountRecursion(c, c, diff);
                }
                // when it's the same or worse keep the previous best (greedy)
                return columnCountRecursion(c, prevBestC, diff);
            }

            GridView {
                id: thumbnailGridView
                anchors.fill: parent

                model: tabBox.model
                currentIndex: tabBox.currentIndex

                clip: true
                keyNavigationWraps: true
                highlightMoveDuration: 0

                readonly property int iconSize: 48
                readonly property int captionRowHeight: 22
                readonly property int thumbnailWidth: 300
                readonly property int thumbnailHeight: thumbnailWidth * (1.0 / dialogMainItem.screenFactor)
                cellWidth: thumbnailWidth
                cellHeight: captionRowHeight + thumbnailHeight

                // allow expansion on increasing count
                property int highCount: 0
                onCountChanged: {
                    if (highCount != count) {
                        dialogMainItem.calculateColumnCount();
                        highCount = count;
                    }
                }

                delegate: MouseArea {
                    id: thumbnailGridItem

                    width: thumbnailGridView.cellWidth
                    height: thumbnailGridView.cellHeight
                    hoverEnabled: true

                    readonly property bool isCurrent: thumbnailGridView.currentIndex === index

                    onClicked: {
                        thumbnailGridView.currentIndex = index;
                        tabBox.currentIndex = index;
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 4

                        // KWin.WindowThumbnail needs a container, otherwise it is
                        // drawn with the size of the parent layout.
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            KWin.WindowThumbnail {
                                anchors.fill: parent
                                wId: windowId
                            }

                            FishUI.IconItem {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.verticalCenter: parent.bottom
                                anchors.verticalCenterOffset: -Math.round(thumbnailGridView.iconSize / 4)
                                width: thumbnailGridView.iconSize
                                height: thumbnailGridView.iconSize
                                source: model.icon
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.preferredHeight: thumbnailGridView.captionRowHeight
                            text: model.caption
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.bold: thumbnailGridItem.isCurrent
                            color: thumbnailGridItem.isCurrent ? FishUI.Theme.highlightedTextColor
                                                               : FishUI.Theme.textColor
                        }
                    }
                } // GridView.delegate

                highlight: Item {
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: FishUI.Units.largeSpacing
                        radius: _background.radius
                        color: FishUI.Theme.highlightColor
                        opacity: 0.7
                    }
                }

                onCurrentIndexChanged: tabBox.currentIndex = thumbnailGridView.currentIndex

                Connections {
                    target: tabBox
                    function onCurrentIndexChanged() {
                        thumbnailGridView.currentIndex = tabBox.currentIndex;
                    }
                }
            } // GridView

            Keys.onPressed: function (event) {
                if (event.key == Qt.Key_Left) {
                    thumbnailGridView.moveCurrentIndexLeft();
                } else if (event.key == Qt.Key_Right) {
                    thumbnailGridView.moveCurrentIndexRight();
                } else if (event.key == Qt.Key_Up) {
                    thumbnailGridView.moveCurrentIndexUp();
                } else if (event.key == Qt.Key_Down) {
                    thumbnailGridView.moveCurrentIndexDown();
                } else {
                    return;
                }

                tabBox.currentIndex = thumbnailGridView.currentIndex;
            }
        } // dialogMainItem
    } // Window
}
