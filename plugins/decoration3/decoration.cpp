/*
 * Copyright (C) 2020 PandaOS Team.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "decoration.h"
#include "button.h"

#include <KPluginFactory>
#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/DecorationSettings>

#include <QFontMetricsF>
#include <QPainter>

K_PLUGIN_FACTORY_WITH_JSON(CutefishDecorationFactory, "cutefishos.json",
                           registerPlugin<Cutefish::Decoration>();)

namespace Cutefish
{

Decoration::Decoration(QObject *parent, const QVariantList &args)
    : KDecoration3::Decoration(parent, args)
    , m_themeSettings(QSettings::UserScope, QStringLiteral("cutefishos"), QStringLiteral("theme"))
{
}

bool Decoration::init()
{
    if (!window()) {
        return false;
    }

    m_devicePixelRatio = m_themeSettings.value(QStringLiteral("PixelRatio"), 1.0).toReal();
    if (m_devicePixelRatio <= 0) {
        m_devicePixelRatio = 1.0;
    }
    m_frameRadius = qRound(11 * m_devicePixelRatio);

    updateColors();
    updateButtonPixmaps();

    m_leftButtons = new KDecoration3::DecorationButtonGroup(
        KDecoration3::DecorationButtonGroup::Position::Left, this, &Button::create);
    m_rightButtons = new KDecoration3::DecorationButtonGroup(
        KDecoration3::DecorationButtonGroup::Position::Right, this, &Button::create);

    auto window = this->window();
    connect(window, &KDecoration3::DecoratedWindow::captionChanged, this, [this] { update(); });
    connect(window, &KDecoration3::DecoratedWindow::activeChanged, this, [this] { update(); });
    connect(window, &KDecoration3::DecoratedWindow::maximizedChanged, this, [this] {
        updateGeometry();
        updateButtonsGeometry();
        update();
    });
    connect(window, &KDecoration3::DecoratedWindow::widthChanged, this, [this] {
        updateGeometry();
        updateButtonsGeometry();
    });
    connect(window, &KDecoration3::DecoratedWindow::heightChanged, this, [this] { updateGeometry(); });

    if (settings()) {
        connect(settings().get(), &KDecoration3::DecorationSettings::fontChanged, this, [this] {
            updateGeometry();
            update();
        });
        connect(settings().get(), &KDecoration3::DecorationSettings::spacingChanged, this, [this] {
            updateButtonsGeometry();
            update();
        });
        connect(settings().get(), &KDecoration3::DecorationSettings::reconfigured, this, [this] {
            updateGeometry();
            updateButtonsGeometry();
            update();
        });
    }

    updateGeometry();
    updateButtonsGeometry();
    return true;
}

void Decoration::paint(QPainter *painter, const QRectF &repaintArea)
{
    Q_UNUSED(repaintArea)

    if (!window()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(titleBarBackgroundColor());

    if (!window()->isShaded() && !window()->isMaximized()) {
        painter->drawRoundedRect(rect(), m_frameRadius, m_frameRadius);
    } else {
        painter->drawRect(rect());
    }
    painter->restore();

    if (m_leftButtons) {
        m_leftButtons->paint(painter, repaintArea);
    }
    if (m_rightButtons) {
        m_rightButtons->paint(painter, repaintArea);
    }
    paintCaption(painter);
}

void Decoration::updateGeometry()
{
    if (!window()) {
        return;
    }

    const qreal height = titleBarHeight();
    setBorders(QMarginsF(0, height, 0, 0));
    setResizeOnlyBorders(QMarginsF(5, 5, 5, 5));
    setTitleBar(QRectF(0, 0, window()->width(), height));
}

void Decoration::updateButtonsGeometry()
{
    if (!window() || !m_leftButtons || !m_rightButtons) {
        return;
    }

    const QSizeF buttonSize(titleBarHeight(), titleBarHeight());
    const auto buttons = m_leftButtons->buttons() + m_rightButtons->buttons();
    for (auto *button : buttons) {
        button->setGeometry(QRectF(QPointF(0, 0), buttonSize));
    }

    constexpr qreal spacing = 8.0;
    m_leftButtons->setSpacing(spacing);
    m_leftButtons->setPos(QPointF(0, 0));
    m_rightButtons->setSpacing(spacing);
    m_rightButtons->setPos(QPointF(size().width() - m_rightButtons->geometry().width() - 2, 0));
}

void Decoration::updateColors()
{
    // The theme settings are intentionally read once per decoration. KWin creates a new
    // decoration when the window/theme is reconfigured, avoiding a dependency on KDE5 APIs.
}

void Decoration::updateButtonPixmaps()
{
    const auto load = [](const QString &path) {
        return QPixmap(path);
    };
    m_closeBtnPixmap = load(pixmapPath(KDecoration3::DecorationButtonType::Close, false));
    m_maximizeBtnPixmap = load(pixmapPath(KDecoration3::DecorationButtonType::Maximize, false));
    m_minimizeBtnPixmap = load(pixmapPath(KDecoration3::DecorationButtonType::Minimize, false));
    m_restoreBtnPixmap = load(pixmapPath(KDecoration3::DecorationButtonType::Maximize, true));
}

QPixmap Decoration::buttonPixmap(KDecoration3::DecorationButtonType type, bool checked) const
{
    switch (type) {
    case KDecoration3::DecorationButtonType::Close:
        return m_closeBtnPixmap;
    case KDecoration3::DecorationButtonType::Maximize:
        return checked ? m_restoreBtnPixmap : m_maximizeBtnPixmap;
    case KDecoration3::DecorationButtonType::Minimize:
        return m_minimizeBtnPixmap;
    default:
        return {};
    }
}

QString Decoration::pixmapPath(KDecoration3::DecorationButtonType type, bool checked) const
{
    const QString mode = darkMode() ? QStringLiteral("dark") : QStringLiteral("light");
    QString name;
    switch (type) {
    case KDecoration3::DecorationButtonType::Close:
        name = QStringLiteral("close_normal.svg");
        break;
    case KDecoration3::DecorationButtonType::Maximize:
        name = checked ? QStringLiteral("restore_normal.svg") : QStringLiteral("maximize_normal.svg");
        break;
    case KDecoration3::DecorationButtonType::Minimize:
        name = QStringLiteral("minimize_normal.svg");
        break;
    default:
        return {};
    }
    return QStringLiteral(":/images/%1/%2").arg(mode, name);
}

int Decoration::titleBarHeight() const
{
    return qMax(1, qRound(m_titleBarHeight * m_devicePixelRatio));
}

bool Decoration::darkMode() const
{
    return m_themeSettings.value(QStringLiteral("DarkMode"), false).toBool();
}

QColor Decoration::titleBarBackgroundColor() const
{
    return darkMode() ? m_titleBarBgDarkColor : m_titleBarBgColor;
}

QColor Decoration::titleBarForegroundColor() const
{
    if (window() && window()->isActive()) {
        return darkMode() ? m_titleBarFgDarkColor : m_titleBarFgColor;
    }
    return darkMode() ? m_unfocusedFgDarkColor : m_unfocusedFgColor;
}

void Decoration::paintCaption(QPainter *painter) const
{
    if (!window()) {
        return;
    }

    QFont font = settings() ? settings()->font() : QFont();
    QFontMetricsF metrics(font);
    const QRectF titleRect(0, 0, size().width(), titleBarHeight());
    const qreal left = m_leftButtons ? m_leftButtons->geometry().width() + 20 : 20;
    const qreal right = m_rightButtons ? m_rightButtons->geometry().width() + 20 : 20;
    const QRectF available = titleRect.adjusted(left, 0, -right, 0);
    const qreal textWidth = metrics.horizontalAdvance(window()->caption());
    const QRectF natural((size().width() - textWidth) / 2, 0, textWidth, titleBarHeight());

    Qt::Alignment alignment = Qt::AlignCenter;
    QRectF captionRect = titleRect;
    if (natural.left() < available.left()) {
        captionRect = available;
        alignment = Qt::AlignLeft | Qt::AlignVCenter;
    } else if (natural.right() > available.right()) {
        captionRect = available;
        alignment = Qt::AlignRight | Qt::AlignVCenter;
    }

    painter->save();
    painter->setFont(font);
    painter->setPen(titleBarForegroundColor());
    painter->drawText(captionRect, alignment, metrics.elidedText(window()->caption(), Qt::ElideMiddle, qRound(captionRect.width())));
    painter->restore();
}

}

#include "decoration.moc"
