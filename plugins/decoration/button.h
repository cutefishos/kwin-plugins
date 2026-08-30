#pragma once

#include <KDecoration3/DecorationButton>

class Button : public KDecoration3::DecorationButton
{
public:
    explicit Button(KDecoration3::DecorationButtonType type,
                    KDecoration3::Decoration *decoration,
                    QObject *parent = nullptr);

    static KDecoration3::DecorationButton *create(KDecoration3::DecorationButtonType type,
                                                    KDecoration3::Decoration *decoration,
                                                    QObject *parent);

protected:
    void paint(QPainter *painter, const QRectF &repaintArea) override;
};
