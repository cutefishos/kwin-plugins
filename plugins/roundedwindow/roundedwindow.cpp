#include "roundedwindow.h"

#include <core/output.h>
#include <core/pixelgrid.h>
#include <core/renderviewport.h>
#include <effect/effecthandler.h>
#include <opengl/glshader.h>
#include <opengl/glshadermanager.h>
#include <opengl/openglcontext.h>
#include <utils/version.h>

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
              // Size of the offscreen texture, in device pixels.
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
              // Convert a point expressed relative to the window frame back to
              // the offscreen texture coordinate system. KWin's normalized
              // GLTexture matrix flips Y.
              "vec2 framePixelToTex(vec2 framePixel)\n"
              "{\n"
              "    vec2 texturePixel = frameRect.xy + framePixel;\n"
              "    return vec2(texturePixel.x / textureSize.x,\n"
              "                1.0 - texturePixel.y / textureSize.y);\n"
              "}\n"
              "\n";

    source += "vec4 sampleFramePixel(vec2 framePixel)\n"
              "{\n"
              "    return " + textureLookup + "(sampler, framePixelToTex(framePixel));\n"
              "}\n"
              "\n"
              // The decoration shadow is already part of KWin's offscreen
              // texture. Once the opaque frame has been rendered, however, the
              // shadow underneath the frame is no longer recoverable by merely
              // lowering the frame alpha. Reconstruct each clipped corner from
              // the two adjacent native-shadow edges.
              "vec4 nativeCornerShadow(vec2 point, bool right, bool bottom)\n"
              "{\n"
              "    const float margin = 3.0;\n"
              "    vec2 size = frameRect.zw;\n"
              "    float leftMargin = frameRect.x;\n"
              "    float topMargin = frameRect.y;\n"
              "    float rightMargin = textureSize.x - frameRect.x - size.x;\n"
              "    float bottomMargin = textureSize.y - frameRect.y - size.y;\n"
              "    bool hasVerticalShadow = right ? rightMargin >= margin : leftMargin >= margin;\n"
              "    bool hasHorizontalShadow = bottom ? bottomMargin >= margin : topMargin >= margin;\n"
              "    if (!hasVerticalShadow || !hasHorizontalShadow) {\n"
              // CSD windows can have no expanded shadow texture. Transparent
              // black keeps the output valid for premultiplied-alpha blending.
              "        return vec4(0.0);\n"
              "    }\n"
              "\n"
              "    vec2 a;\n"
              "    vec2 b;\n"
              "    if (!right && !bottom) {\n"
              "        a = vec2(-margin, point.y + point.x + margin);\n"
              "        b = vec2(point.x + point.y + margin, -margin);\n"
              "    } else if (right && !bottom) {\n"
              "        a = vec2(size.x + margin, point.y + (size.x - point.x) + margin);\n"
              "        b = vec2(point.x - point.y - margin, -margin);\n"
              "    } else if (!right && bottom) {\n"
              "        a = vec2(-margin, point.y - point.x - margin);\n"
              "        b = vec2(point.x + (size.y - point.y) + margin, size.y + margin);\n"
              "    } else {\n"
              "        a = vec2(size.x + margin, point.y - (size.x - point.x) - margin);\n"
              "        b = vec2(point.x - (size.y - point.y) - margin, size.y + margin);\n"
              "    }\n"
              "\n"
              "    vec4 aColor = sampleFramePixel(a);\n"
              "    vec4 bColor = sampleFramePixel(b);\n"
              "    float segment = max(distance(a, b), 0.001);\n"
              "    return mix(aColor, bColor, clamp(distance(a, point) / segment, 0.0, 1.0));\n"
              "}\n"
              "\n"
              "float cornerCoverage(vec2 point, vec2 center, float r)\n"
              "{\n"
              // Pixel centres are half a pixel away from the mathematical edge.
              // This half-pixel coverage convention matches KWin rounded-corner
              // effects and avoids a dark or bright one-pixel halo.
              "    return clamp(r - distance(point, center) + 0.5, 0.0, 1.0);\n"
              "}\n"
              "\n"
              "void main(void)\n"
              "{\n"
              "    vec4 texel = " + textureLookup + "(sampler, texcoord0);\n"
              "    vec4 result = texel;\n"
              "\n"
              // KWin's GLTexture normalized-coordinate matrix flips the Y axis.
              "    vec2 texturePoint = vec2(texcoord0.x, 1.0 - texcoord0.y) * textureSize;\n"
              "    vec2 local = texturePoint - frameRect.xy;\n"
              "    vec2 size = frameRect.zw;\n"
              "    float r = min(radius, 0.5 * min(size.x, size.y));\n"
              "\n"
              "    if (r > 0.0 && local.x >= 0.0 && local.y >= 0.0\n"
              "            && local.x <= size.x && local.y <= size.y) {\n"
              // Only touch the four radius-by-radius corner squares. Straight
              // edges must remain byte-for-byte identical to the captured
              // texture; applying a rounded-rectangle SDF to the whole frame can
              // introduce a translucent fringe along those edges.
              "        bool corner = false;\n"
              "        bool right = false;\n"
              "        bool bottom = false;\n"
              "        vec2 center = vec2(r, r);\n"
              "\n"
              "        if (local.x < r && local.y < r) {\n"
              "            corner = true;\n"
              "        } else if (local.x > size.x - r && local.y < r) {\n"
              "            corner = true;\n"
              "            right = true;\n"
              "            center = vec2(size.x - r, r);\n"
              "        } else if (local.x < r && local.y > size.y - r) {\n"
              "            corner = true;\n"
              "            bottom = true;\n"
              "            center = vec2(r, size.y - r);\n"
              "        } else if (local.x > size.x - r && local.y > size.y - r) {\n"
              "            corner = true;\n"
              "            right = true;\n"
              "            bottom = true;\n"
              "            center = vec2(size.x - r, size.y - r);\n"
              "        }\n"
              "\n"
              "        if (corner) {\n"
              "            float coverage = cornerCoverage(local, center, r);\n"
              "            if (coverage < 1.0) {\n"
              "                vec4 shadow = nativeCornerShadow(local, right, bottom);\n"
              "                result = mix(shadow, texel, coverage);\n"
              "            }\n"
              "        }\n"
              "    }\n"
              "\n"
              // Apply KWin's paint modulation after the corner reconstruction so
              // both the frame and sampled native shadow fade together.
              "    result *= modulation;\n"
              "    result.rgb = mix(vec3(dot(result.rgb, vec3(0.2126, 0.7152, 0.0722))), result.rgb, saturation);\n"
              "    " + output + " = result;\n"
              "}\n";

    return source;
}

RoundedWindow::RoundedWindow()
    : KWin::OffscreenEffect()
{
    auto watchWindow = [this](KWin::EffectWindow *window) {
        connect(window, &KWin::EffectWindow::windowMaximizedStateAboutToChange, this,
                [this](KWin::EffectWindow *w, bool horizontal, bool vertical) {
                    // Drop the current offscreen texture before either direction
                    // of the maximize transition. In particular, a restore must
                    // not reuse the full-screen texture captured while maximized.
                    unredirect(w);

                    if (horizontal && vertical) {
                        m_maximizingWindows.insert(w);
                        m_restoringWindows.remove(w);
                    } else if (isMaximized(w)) {
                        m_restoringWindows.insert(w);
                        m_maximizingWindows.remove(w);
                    } else {
                        m_maximizingWindows.remove(w);
                        m_restoringWindows.remove(w);
                    }

                    w->addRepaintFull();
                });
    };

    connect(KWin::effects, &KWin::EffectsHandler::windowAdded, this, watchWindow);
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        watchWindow(window);
    }

    connect(KWin::effects, &KWin::EffectsHandler::windowClosed, this,
            [this](KWin::EffectWindow *window) {
                // A closed EffectWindow becomes "deleted" before the close
                // animation has finished. Remember that it was rounded so the
                // scale/fade animation keeps drawing the already-rounded FBO
                // instead of switching to a square window for its last frames.
                if (m_roundedWindows.contains(window)) {
                    m_closingWindows.insert(window);
                }
                m_maximizingWindows.remove(window);
                m_restoringWindows.remove(window);
            });

    connect(KWin::effects, &KWin::EffectsHandler::windowDeleted, this,
            [this](KWin::EffectWindow *window) {
                m_closingWindows.remove(window);
                m_maximizingWindows.remove(window);
                m_restoringWindows.remove(window);
                m_roundedWindows.remove(window);
            });
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

bool RoundedWindow::shouldRound(KWin::EffectWindow *window) const
{
    if (!window || !window->isManaged()) {
        return false;
    }

    // Deleted windows can remain visible while KWin runs the close animation.
    // Only keep rounding if this exact window was already rounded before close.
    if (window->isDeleted()) {
        return m_closingWindows.contains(window);
    }

    // A fully maximized window is square, but as soon as a restore starts we
    // need the rounded path available from the first transformed frame.
    if (window->isFullScreen()
            || (isMaximized(window) && !m_restoringWindows.contains(window))) {
        return false;
    }

    if (window->isDesktop() || window->isDock() || window->isMenu() || window->isDropdownMenu()
            || window->isPopupMenu() || window->isPopupWindow() || window->isTooltip()
            || window->isNotification() || window->isCriticalNotification()
            || window->isOnScreenDisplay() || window->isDNDIcon() || window->isSplash()) {
        return false;
    }

    const bool allowListed = s_allowList.contains(window->windowClass());
    if (allowListed) {
        return true;
    }

    if (!window->hasDecoration()) {
        return false;
    }

    return window->isNormalWindow() || window->isDialog() || window->isUtility();
}

bool RoundedWindow::isMaximized(KWin::EffectWindow *window) const
{
    const QRectF frame = window->frameGeometry();
    const QRectF maximizedArea = KWin::effects->clientArea(KWin::MaximizeArea, window);

    // Wayland geometry can differ from the maximize area by a fractional pixel
    // while output scaling and decoration geometry settle. Exact QRectF equality
    // makes the effect oscillate on/off at the end of maximize/restore.
    constexpr qreal tolerance = 1.0;
    return qAbs(frame.x() - maximizedArea.x()) <= tolerance
        && qAbs(frame.y() - maximizedArea.y()) <= tolerance
        && qAbs(frame.width() - maximizedArea.width()) <= tolerance
        && qAbs(frame.height() - maximizedArea.height()) <= tolerance;
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
    const bool round = shouldRound(window);
    if (round) {
        // The corners are cut out of the window, so it can no longer be treated
        // as opaque. This also applies while open/close/restore animations
        // transform the rounded offscreen texture.
        data.setTranslucent();
        if (!window->isDeleted()) {
            m_roundedWindows.insert(window);
        }
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
    const bool transformed = mask & KWin::Effect::PAINT_WINDOW_TRANSFORMED;

    // Transformation itself is not a reason to drop rounded corners: opening,
    // closing, minimizing and restoring all legitimately transform the window.
    // The problematic case is specifically the transition *into* maximized
    // state, where KWin cross-fades the previous buffer. Keep that direction on
    // the normal path so our offscreen texture cannot be captured recursively.
    if (transformed && m_maximizingWindows.contains(window)) {
        unredirect(window);
        KWin::OffscreenEffect::drawWindow(renderTarget, viewport, window, mask, region, data);
        return;
    }

    // Clear transition bookkeeping only after the target geometry has settled.
    // This avoids losing the state if KWin happens to paint one ordinary frame
    // between the about-to-change signal and the scripted animation.
    if (!transformed) {
        if (m_maximizingWindows.contains(window) && isMaximized(window)) {
            m_maximizingWindows.remove(window);
        }
        if (m_restoringWindows.contains(window) && !isMaximized(window)) {
            m_restoringWindows.remove(window);
        }
    }

    // Keep KWin's offscreen state tied to the current paint pass. Persisting our
    // own redirect state across maximize/restore lets an old FBO survive a geometry
    // transition and is especially fragile when the Maximize effect cross-fades a
    // previous buffer. This is the same model used by maintained KWin rounded-corner
    // effects: redirect only while the window currently needs the effect.
    if (!shouldRound(window)) {
        if (!window->isDeleted()) {
            m_roundedWindows.remove(window);
        }
        unredirect(window);
        KWin::OffscreenEffect::drawWindow(renderTarget, viewport, window, mask, region, data);
        return;
    }

    if (!window->isDeleted()) {
        m_roundedWindows.insert(window);
    }

    KWin::GLShader *s = shader();
    if (!s) {
        unredirect(window);
        KWin::OffscreenEffect::drawWindow(renderTarget, viewport, window, mask, region, data);
        return;
    }

    redirect(window);
    setShader(window, s);

    // OffscreenEffect allocates its texture in device pixels. Keep every shader
    // geometry in that same coordinate space; using logical sizes here causes the
    // mask and shadow boundary to drift at fractional scale.
    const qreal scale = window->screen() ? window->screen()->scale() : viewport.scale();
    const QRectF expanded = KWin::snapToPixels(window->expandedGeometry(), scale);
    const QRectF frame = KWin::snapToPixels(window->frameGeometry(), scale);

    const QSizeF textureSize(expanded.width() * scale, expanded.height() * scale);
    const QPointF frameOffset((frame.x() - expanded.x()) * scale,
                              (frame.y() - expanded.y()) * scale);
    const QSizeF frameSize(frame.width() * scale, frame.height() * scale);

    KWin::ShaderBinder binder(s);
    s->setUniform("textureSize", QVector2D(textureSize.width(), textureSize.height()));
    s->setUniform("frameRect", QVector4D(frameOffset.x(),
                                         frameOffset.y(),
                                         frameSize.width(),
                                         frameSize.height()));
    s->setUniform("radius", float(m_frameRadius * scale));

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
