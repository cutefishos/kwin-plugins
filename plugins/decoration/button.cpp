/*
 * Copyright (C) 2020 PandaOS Team.
 *
 * Author:     rekols <rekols@foxmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "button.h"
#include "decoration.h"

#include <KDecoration2/DecoratedClient>
#include <KDecoration2/Decoration>

#include <QPainter>
#include <QPainterPath>

Button::Button(KDecoration2::DecorationButtonType type, const QPointer<KDecoration2::Decoration> &decoration, QObject *parent)
    : KDecoration2::DecorationButton(type, decoration, parent)
{
    auto c = decoration->client().toStrongRef().data();

    switch (type) {
    case KDecoration2::DecorationButtonType::Menu:
        break;
    case KDecoration2::DecorationButtonType::Minimize:
        setVisible(c->isMinimizeable());
        connect(c, &KDecoration2::DecoratedClient::minimizeableChanged, this, &Button::setVisible);
        break;
    case KDecoration2::DecorationButtonType::Maximize:
        setVisible(c->isMaximizeable());
        connect(c, &KDecoration2::DecoratedClient::maximizeableChanged, this, &Button::setVisible);
        break;
    case KDecoration2::DecorationButtonType::Close:
        setVisible(c->isCloseable());
        connect(c, &KDecoration2::DecoratedClient::closeableChanged, this, &Button::setVisible);
        break;
    default:
        setVisible(false);
        break;
    }
}

KDecoration2::DecorationButton *Button::create(KDecoration2::DecorationButtonType type, KDecoration2::Decoration *decoration, QObject *parent)
{
    return new Button(type, decoration, parent);
}

void Button::paint(QPainter *painter, const QRect &repaintRegion)
{
    Q_UNUSED(repaintRegion)

    Cutefish::Decoration *decoration = qobject_cast<Cutefish::Decoration *>(this->decoration());

    if (!decoration)
        return;

    auto c = decoration->client().toStrongRef().data();
    const bool isDarkMode = decoration->darkMode();
    const QRect &rect = geometry().toRect();

    painter->save();
    painter->setRenderHints(QPainter::Antialiasing);

    QRect btnRect(0, 0, 28 * decoration->devicePixelRatio(),
                        28 * decoration->devicePixelRatio());
    btnRect.moveCenter(rect.center());

    if (isHovered() || isPressed()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(isDarkMode ? isPressed() ? QColor(255, 255, 255, 255 * 0.1) : QColor(255, 255, 255, 255 * 0.15)
                                     : isPressed() ? QColor(0, 0, 0, 255 * 0.15) : QColor(0, 0, 0, 255 * 0.1));
        painter->drawRoundedRect(btnRect, btnRect.height() / 2, btnRect.height() / 2);
    }

    switch (type()) {
    case KDecoration2::DecorationButtonType::Menu: {
        c->icon().paint(painter, rect);
        break;
    }
    case KDecoration2::DecorationButtonType::ApplicationMenu: {
        break;
    }
    case KDecoration2::DecorationButtonType::Minimize: {
        painter->drawPixmap(btnRect, decoration->minimizeBtnPixmap());
        break;
    }
    case KDecoration2::DecorationButtonType::Maximize: {
        if (isChecked())
            painter->drawPixmap(btnRect, decoration->restoreBtnPixmap());
        else
            painter->drawPixmap(btnRect, decoration->maximizeBtnPixmap());
        break;
    }
    case KDecoration2::DecorationButtonType::Close: {
        painter->drawPixmap(btnRect, decoration->closeBtnPixmap());
        break;
    }
    default:
        break;
    }

    painter->restore();
}
