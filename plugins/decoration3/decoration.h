/*
 * Copyright (C) 2020 PandaOS Team.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationButtonGroup>

#include <QPixmap>
#include <QSettings>

namespace Cutefish
{

class Decoration : public KDecoration3::Decoration
{
    Q_OBJECT

public:
    explicit Decoration(QObject *parent = nullptr, const QVariantList &args = {});
    ~Decoration() override = default;

    bool init() override;
    void paint(QPainter *painter, const QRectF &repaintArea) override;

    bool darkMode() const;
    qreal devicePixelRatio() const { return m_devicePixelRatio; }
    QPixmap buttonPixmap(KDecoration3::DecorationButtonType type, bool checked = false) const;

private:
    void updateGeometry();
    void updateButtonsGeometry();
    void updateColors();
    void updateButtonPixmaps();
    void paintCaption(QPainter *painter) const;

    int titleBarHeight() const;
    QColor titleBarBackgroundColor() const;
    QColor titleBarForegroundColor() const;
    QString pixmapPath(KDecoration3::DecorationButtonType type, bool checked) const;

    KDecoration3::DecorationButtonGroup *m_leftButtons = nullptr;
    KDecoration3::DecorationButtonGroup *m_rightButtons = nullptr;

    qreal m_devicePixelRatio = 1.0;
    int m_titleBarHeight = 30;
    int m_frameRadius = 11;
    QColor m_titleBarBgColor = QColor(255, 255, 255);
    QColor m_titleBarFgColor = QColor(56, 56, 56);
    QColor m_unfocusedFgColor = QColor(127, 127, 127);
    QColor m_titleBarBgDarkColor = QColor(44, 44, 45);
    QColor m_titleBarFgDarkColor = QColor(202, 203, 206);
    QColor m_unfocusedFgDarkColor = QColor(112, 112, 112);

    QSettings m_themeSettings;
    QPixmap m_closeBtnPixmap;
    QPixmap m_maximizeBtnPixmap;
    QPixmap m_minimizeBtnPixmap;
    QPixmap m_restoreBtnPixmap;
};

}
