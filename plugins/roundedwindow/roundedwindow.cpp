#include "roundedwindow.h"

#include <effect/effecthandler.h>
#include <opengl/glshader.h>
#include <opengl/glshadermanager.h>
#include <opengl/openglcontext.h>
#include <utils/version.h>

#include <QSettings>
#include <QVector2D>
#include <QVector4D>

// Applications that draw their own window frame but still want rounded corners.
static const QStringList s_allowList = {
    "netease-cloud-music netease-cloud-music",
    "com.alibabainc.dingtalk com.alibabainc.dingtalk",
    "tenvideo_universal tenvideo_universal",
    "com.eusoft.ting.en com.eusoft.ting.en",
    "i4toolslinux i4tools",
    "youku-app youku-app",
    "qqmusic qqmusic",
    "mytime mytime",
    "feishu feishu",
    "bytedance-feishu bytedance-feishu",
    "xmind xmind",
    "mtxx mtxx",
    "ynote-desktop ynote-desktop",

    // Open source software
    "code code",
    "motrix motrix"
};

static QByteArray fragmentShaderSource()
{
    const KWin::OpenGlContext *context = KWin::effects->openglContext();
    const bool gles = context && context->isOpenGLES();
    const KWin::Version glslVersion = context ? context->glslVersion() : KWin::Version(1, 40);

    QByteArray source;
    QByteArray varying = QByteArrayLiteral("varying");
    QByteArray textureLookup = QByteArrayLiteral("texture2D");
    QByteArray output = QByteArrayLiteral("gl_FragColor");

    if (!gles) {
        if (glslVersion >= KWin::Version(1, 40)) {
            source += "#version 140\n\n";
            varying = QByteArrayLiteral("in");
            textureLookup = QByteArrayLiteral("texture");
            output = QByteArrayLiteral("fragColor");
        }
    } else {
        if (glslVersion >= KWin::Version(3, 0)) {
            source += "#version 300 es\n\n";
            varying = QByteArrayLiteral("in");
            textureLookup = QByteArrayLiteral("texture");
            output = QByteArrayLiteral("fragColor");
        }
        // The fragment language has no default precision qualifier for floats.
        source += "precision highp float;\n\n";
    }

    source += "uniform sampler2D sampler;\n"
              "uniform vec4 modulation;\n"
              "uniform float saturation;\n"
              "\n"
              // Size of the offscreen texture, in logical pixels.
              "uniform vec2 textureSize;\n"
              // Frame of the window inside the texture (x, y, width, height).
              "uniform vec4 frameRect;\n"
              "uniform float radius;\n"
              "\n";

    source += varying + " vec2 texcoord0;\n";
    if (output != QByteArrayLiteral("gl_FragColor")) {
        source += "out vec4 " + output + ";\n";
    }

    source += "\n"
              // Signed distance to a rounded rectangle centred on the origin.
              "float roundedBoxDistance(vec2 point, vec2 halfSize, float r)\n"
              "{\n"
              "    vec2 q = abs(point) - halfSize + vec2(r);\n"
              "    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;\n"
              "}\n"
              "\n"
              "void main(void)\n"
              "{\n"
              "    vec4 texel = " + textureLookup + "(sampler, texcoord0);\n"
              "    texel *= modulation;\n"
              "    texel.rgb = mix(vec3(dot(texel.rgb, vec3(0.2126, 0.7152, 0.0722))), texel.rgb, saturation);\n"
              "\n"
              "    vec2 point = texcoord0 * textureSize;\n"
              "    vec2 halfSize = frameRect.zw * 0.5;\n"
              "    vec2 centered = point - (frameRect.xy + halfSize);\n"
              "\n"
              // Everything outside of the window frame - the decoration shadow -
              // has to be left alone.
              "    vec2 outside = abs(centered) - halfSize;\n"
              "    if (max(outside.x, outside.y) > 0.0) {\n"
              "        " + output + " = texel;\n"
              "        return;\n"
              "    }\n"
              "\n"
              "    float dist = roundedBoxDistance(centered, halfSize, radius);\n"
              "    float aa = max(length(fwidth(point)) * 0.5, 0.5);\n"
              "    " + output + " = texel * (1.0 - smoothstep(-aa, aa, dist));\n"
              "}\n";

    return source;
}

RoundedWindow::RoundedWindow()
    : KWin::OffscreenEffect()
{
    reconfigure(ReconfigureAll);

    connect(KWin::effects, &KWin::EffectsHandler::windowAdded, this, &RoundedWindow::handleWindowAdded);
    connect(KWin::effects, &KWin::EffectsHandler::windowDeleted, this, &RoundedWindow::handleWindowDeleted);

    const auto windows = KWin::effects->stackingOrder();
    for (KWin::EffectWindow *window : windows) {
        handleWindowAdded(window);
    }
}

RoundedWindow::~RoundedWindow() = default;

bool RoundedWindow::supported()
{
    return KWin::effects->isOpenGLCompositing() && KWin::OffscreenEffect::supported();
}

bool RoundedWindow::enabledByDefault()
{
    return supported();
}

void RoundedWindow::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    QSettings settings(QSettings::UserScope, QStringLiteral("cutefishos"), QStringLiteral("theme"));
    qreal devicePixelRatio = settings.value(QStringLiteral("PixelRatio"), 1.0).toReal();
    if (devicePixelRatio <= 0) {
        devicePixelRatio = 1.0;
    }
    m_frameRadius = 11 * devicePixelRatio;

    const auto windows = m_redirected;
    for (KWin::EffectWindow *window : windows) {
        window->addRepaintFull();
    }
}

void RoundedWindow::handleWindowAdded(KWin::EffectWindow *window)
{
    if (!window) {
        return;
    }

    connect(window, &KWin::EffectWindow::windowMaximizedStateChanged, this,
            [this, window] { updateWindow(window); });
    connect(window, &KWin::EffectWindow::windowFullScreenChanged, this,
            [this, window] { updateWindow(window); });
    connect(window, &KWin::EffectWindow::windowFrameGeometryChanged, this,
            [this, window] { updateWindow(window); });
    connect(window, &KWin::EffectWindow::windowDecorationChanged, this,
            [this, window] { updateWindow(window); });

    updateWindow(window);
}

void RoundedWindow::handleWindowDeleted(KWin::EffectWindow *window)
{
    m_redirected.remove(window);
}

void RoundedWindow::updateWindow(KWin::EffectWindow *window)
{
    const bool wanted = shouldRound(window);
    const bool redirected = m_redirected.contains(window);

    if (wanted == redirected) {
        return;
    }

    if (wanted) {
        if (KWin::GLShader *s = shader()) {
            redirect(window);
            setShader(window, s);
            m_redirected.insert(window);
        }
    } else {
        unredirect(window);
        m_redirected.remove(window);
    }

    window->addRepaintFull();
}

bool RoundedWindow::shouldRound(KWin::EffectWindow *window) const
{
    if (!window || window->isDeleted() || !window->isOnCurrentDesktop()) {
        return false;
    }

    if (s_allowList.contains(window->windowClass())) {
        return true;
    }

    if (!window->isManaged() || window->isFullScreen() || isMaximized(window)) {
        return false;
    }

    // Client side decorated windows - the CutefishOS applications themselves -
    // paint their own rounded corners and shadow. Cutting into them would only
    // produce artefacts, so this effect is for windows KWin decorates.
    if (!window->hasDecoration()) {
        return false;
    }

    if (window->isDesktop() || window->isDock() || window->isMenu() || window->isDropdownMenu()
            || window->isPopupMenu() || window->isPopupWindow() || window->isTooltip()
            || window->isNotification() || window->isCriticalNotification()
            || window->isOnScreenDisplay() || window->isDNDIcon() || window->isSplash()) {
        return false;
    }

    return window->isNormalWindow() || window->isDialog() || window->isUtility();
}

bool RoundedWindow::isMaximized(KWin::EffectWindow *window) const
{
    const QRectF maximizedArea = KWin::effects->clientArea(KWin::MaximizeArea, window);
    return window->frameGeometry() == maximizedArea;
}

KWin::GLShader *RoundedWindow::shader()
{
    if (!m_shader) {
        m_shader = KWin::ShaderManager::instance()->generateCustomShader(
            KWin::ShaderTrait::MapTexture | KWin::ShaderTrait::Modulate | KWin::ShaderTrait::AdjustSaturation,
            QByteArray(), fragmentShaderSource());

        if (m_shader && !m_shader->isValid()) {
            m_shader.reset();
        }
    }

    return m_shader.get();
}

void RoundedWindow::prePaintWindow(KWin::EffectWindow *window,
                                   KWin::WindowPrePaintData &data,
                                   std::chrono::milliseconds presentTime)
{
    if (m_redirected.contains(window)) {
        // The corners are cut out of the window, so it can no longer be treated
        // as an opaque window - otherwise nothing would be blended with what is
        // behind it and the cut out corners would stay black.
        data.setTranslucent();
    }

    KWin::OffscreenEffect::prePaintWindow(window, data, presentTime);
}

void RoundedWindow::drawWindow(const KWin::RenderTarget &renderTarget,
                               const KWin::RenderViewport &viewport,
                               KWin::EffectWindow *window,
                               int mask,
                               const QRegion &region,
                               KWin::WindowPaintData &data)
{
    if (m_redirected.contains(window)) {
        if (KWin::GLShader *s = shader()) {
            const QRectF expanded = window->expandedGeometry();
            const QRectF frame = window->frameGeometry();

            // The offscreen texture covers the expanded geometry of the window,
            // texcoord0 runs from (0, 0) to (1, 1) over it.
            KWin::ShaderBinder binder(s);
            s->setUniform("textureSize", QVector2D(expanded.width(), expanded.height()));
            s->setUniform("frameRect", QVector4D(frame.x() - expanded.x(),
                                                 frame.y() - expanded.y(),
                                                 frame.width(),
                                                 frame.height()));
            s->setUniform("radius", float(m_frameRadius));
        }
    }

    KWin::OffscreenEffect::drawWindow(renderTarget, viewport, window, mask, region, data);
}

namespace KWin
{
KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(RoundedWindow,
                                      "metadata.json",
                                      return RoundedWindow::supported();,
                                      return RoundedWindow::enabledByDefault();)
}

#include "roundedwindow.moc"
