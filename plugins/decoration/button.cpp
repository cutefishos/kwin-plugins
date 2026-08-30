#include "button.h"
#include "decoration.h"

#include <KDecoration3/DecoratedWindow>

#include <QPainter>

Button::Button(KDecoration3::DecorationButtonType type,
               KDecoration3::Decoration *decoration,
               QObject *parent)
    : KDecoration3::DecorationButton(type, decoration, parent)
{
    auto *window = decoration ? decoration->window() : nullptr;
    if (!window) {
        setVisible(false);
        return;
    }

    switch (type) {
    case KDecoration3::DecorationButtonType::Minimize:
        setVisible(window->isMinimizeable());
        connect(window, &KDecoration3::DecoratedWindow::minimizeableChanged, this, &Button::setVisible);
        break;
    case KDecoration3::DecorationButtonType::Maximize:
        setVisible(window->isMaximizeable());
        connect(window, &KDecoration3::DecoratedWindow::maximizeableChanged, this, &Button::setVisible);
        break;
    case KDecoration3::DecorationButtonType::Close:
        setVisible(window->isCloseable());
        connect(window, &KDecoration3::DecoratedWindow::closeableChanged, this, &Button::setVisible);
        break;
    case KDecoration3::DecorationButtonType::Menu:
        break;
    default:
        setVisible(false);
        break;
    }
}

KDecoration3::DecorationButton *Button::create(KDecoration3::DecorationButtonType type,
                                                 KDecoration3::Decoration *decoration,
                                                 QObject *parent)
{
    return new Button(type, decoration, parent);
}

void Button::paint(QPainter *painter, const QRectF &repaintArea)
{
    Q_UNUSED(repaintArea)

    auto *decoration = qobject_cast<Cutefish::Decoration *>(this->decoration());
    if (!decoration) {
        return;
    }

    const QRectF buttonRect = geometry();
    const qreal scale = decoration->devicePixelRatio();
    const QRectF hoverRect = buttonRect.adjusted(2 * scale, 2 * scale, -2 * scale, -2 * scale);
    const QRectF imageRect = QRectF(0, 0, 24 * scale, 24 * scale);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    if (isHovered() || isPressed()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(decoration->darkMode()
                              ? (isPressed() ? QColor(255, 255, 255, 26) : QColor(255, 255, 255, 38))
                              : (isPressed() ? QColor(0, 0, 0, 38) : QColor(0, 0, 0, 26)));
        painter->drawRoundedRect(hoverRect, hoverRect.height() / 2, hoverRect.height() / 2);
    }

    if (type() == KDecoration3::DecorationButtonType::Menu) {
        if (auto *window = decoration->window()) {
            window->icon().paint(painter, buttonRect.toRect());
        }
    } else {
        const QPixmap pixmap = decoration->buttonPixmap(type(), isChecked());
        QRectF centered = imageRect;
        centered.moveCenter(buttonRect.center());
        if (!pixmap.isNull()) {
            painter->drawPixmap(centered, pixmap, pixmap.rect());
        }
    }
    painter->restore();
}
