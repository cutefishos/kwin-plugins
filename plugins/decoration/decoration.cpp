#include "decoration.h"
#include "button.h"

#include <KPluginFactory>
#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/DecorationSettings>
#include <KDecoration3/DecorationShadow>

#include <QFontMetricsF>
#include <QImageReader>
#include <QPainter>
#include <QRadialGradient>

#include <cmath>

K_PLUGIN_FACTORY_WITH_JSON(CutefishDecorationFactory, "cutefishos.json",
                           registerPlugin<Cutefish::Decoration>();)

namespace Cutefish
{

static int s_decorationCount = 0;
static int s_shadowRadius = -1;
static std::shared_ptr<KDecoration3::DecorationShadow> s_shadow;

Decoration::Decoration(QObject *parent, const QVariantList &args)
    : KDecoration3::Decoration(parent, args)
    , m_themeSettings(QSettings::UserScope, QStringLiteral("cutefishos"), QStringLiteral("theme"))
{
    ++s_decorationCount;
}

Decoration::~Decoration()
{
    if (--s_decorationCount == 0) {
        s_shadow.reset();
        s_shadowRadius = -1;
    }
}

bool Decoration::init()
{
    if (!window()) {
        return false;
    }

    m_themeSettingsFile = m_themeSettings.fileName();
    reloadTheme();

    m_leftButtons = new KDecoration3::DecorationButtonGroup(
        KDecoration3::DecorationButtonGroup::Position::Left, this, &Button::create);
    m_rightButtons = new KDecoration3::DecorationButtonGroup(
        KDecoration3::DecorationButtonGroup::Position::Right, this, &Button::create);

    auto window = this->window();
    connect(window, &KDecoration3::DecoratedWindow::captionChanged, this, [this] { update(); });
    connect(window, &KDecoration3::DecoratedWindow::activeChanged, this, [this] { update(); });
    connect(window, &KDecoration3::DecoratedWindow::shadedChanged, this, [this] {
        updateGeometry();
        updateButtonsGeometry();
        update();
    });
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
        connect(settings().get(), &KDecoration3::DecorationSettings::decorationButtonsLeftChanged, this,
                [this] { updateButtonsGeometry(); });
        connect(settings().get(), &KDecoration3::DecorationSettings::decorationButtonsRightChanged, this,
                [this] { updateButtonsGeometry(); });
        connect(settings().get(), &KDecoration3::DecorationSettings::reconfigured, this, [this] {
            reloadTheme();
            updateGeometry();
            updateButtonsGeometry();
            update();
        });
    }

    // Follow the CutefishOS theme settings (dark mode, scaling) while the window is open.
    if (!m_themeSettingsFile.isEmpty()) {
        m_themeWatcher.addPath(m_themeSettingsFile);
        connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged, this, [this] {
            reloadTheme();
            updateGeometry();
            updateButtonsGeometry();
            update();

            // Editors replace the file instead of writing in place, which drops the watch.
            if (!m_themeWatcher.files().contains(m_themeSettingsFile)) {
                m_themeWatcher.addPath(m_themeSettingsFile);
            }
        });
    }

    updateGeometry();
    updateButtonsGeometry();
    return true;
}

void Decoration::paint(QPainter *painter, const QRectF &repaintArea)
{
    if (!window()) {
        return;
    }

    if (!window()->isShaded()) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(titleBarBackgroundColor());

        const bool rounded = !window()->isMaximized()
            && (!settings() || settings()->isAlphaChannelSupported());
        if (rounded) {
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

void Decoration::updateShadow()
{
    if (s_shadow && s_shadowRadius == m_frameRadius) {
        setShadow(s_shadow);
        return;
    }

    const int shadowSize = 90;
    const int shadowStrength = 35;
    const QColor shadowColor = Qt::black;
    const int shadowOverlap = m_frameRadius;
    const int shadowOffset = shadowOverlap / 2;

    QImage image(2 * shadowSize, 2 * shadowSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    // Gaussian delta function used to fade the shadow out.
    auto alpha = [](qreal x) { return std::exp(-x * x / 0.15); };
    auto gradientStopColor = [](QColor color, int alpha) {
        color.setAlpha(alpha);
        return color;
    };

    QRadialGradient radialGradient(shadowSize, shadowSize, shadowSize);
    for (int i = 0; i < 10; ++i) {
        const qreal x(qreal(i) / 9);
        radialGradient.setColorAt(x, gradientStopColor(shadowColor, alpha(x) * shadowStrength));
    }
    radialGradient.setColorAt(1, gradientStopColor(shadowColor, 0));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(image.rect(), radialGradient);

    const QRectF innerRect(shadowSize - shadowOverlap,
                           shadowSize - shadowOffset - shadowOverlap,
                           2 * shadowOverlap,
                           shadowOffset + 2 * shadowOverlap);

    // Contrast pixel around the window.
    painter.setPen(gradientStopColor(shadowColor, shadowStrength * 0.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(innerRect, -0.5 + m_frameRadius, -0.5 + m_frameRadius);

    // Mask out the area covered by the window itself.
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.drawRoundedRect(innerRect, 0.5 + m_frameRadius, 0.5 + m_frameRadius);
    painter.end();

    s_shadow = std::make_shared<KDecoration3::DecorationShadow>();
    s_shadow->setPadding(QMarginsF(shadowSize - shadowOverlap,
                                   shadowSize - shadowOffset - shadowOverlap,
                                   shadowSize - shadowOverlap,
                                   shadowSize - shadowOverlap));
    s_shadow->setInnerShadowRect(QRectF(shadowSize, shadowSize, 1, 1));
    s_shadow->setShadow(image);
    s_shadowRadius = m_frameRadius;

    setShadow(s_shadow);
}

void Decoration::reloadTheme()
{
    m_themeSettings.sync();

    m_devicePixelRatio = m_themeSettings.value(QStringLiteral("PixelRatio"), 1.0).toReal();
    if (m_devicePixelRatio <= 0) {
        m_devicePixelRatio = 1.0;
    }
    m_darkMode = m_themeSettings.value(QStringLiteral("DarkMode"), false).toBool();
    m_frameRadius = qRound(11 * m_devicePixelRatio);

    updateButtonPixmaps();
    updateShadow();
}

void Decoration::updateButtonPixmaps()
{
    m_closeBtnPixmap = loadPixmap(pixmapPath(KDecoration3::DecorationButtonType::Close, false));
    m_maximizeBtnPixmap = loadPixmap(pixmapPath(KDecoration3::DecorationButtonType::Maximize, false));
    m_minimizeBtnPixmap = loadPixmap(pixmapPath(KDecoration3::DecorationButtonType::Minimize, false));
    m_restoreBtnPixmap = loadPixmap(pixmapPath(KDecoration3::DecorationButtonType::Maximize, true));
}

QPixmap Decoration::loadPixmap(const QString &path) const
{
    QImageReader reader(path);
    if (!reader.canRead()) {
        return {};
    }

    // The icons are SVGs, render them at the size the buttons are painted with.
    reader.setScaledSize(QSize(24, 24) * m_devicePixelRatio);
    return QPixmap::fromImage(reader.read());
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
