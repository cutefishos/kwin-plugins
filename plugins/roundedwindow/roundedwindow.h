#pragma once

#include <effect/offscreeneffect.h>

#include <QSet>

#include <memory>

namespace KWin
{
class GLShader;
}

/**
 * Rounds the corners of ordinary windows.
 *
 * The window is redirected into an offscreen texture, which is then painted with
 * a shader that masks out the four corners with a rounded rectangle. The offscreen
 * texture always has an alpha channel, so windows that do not have one themselves
 * can be given a rounded shape as well.
 */
class RoundedWindow : public KWin::OffscreenEffect
{
    Q_OBJECT

public:
    RoundedWindow();
    ~RoundedWindow() override;

    static bool supported();
    static bool enabledByDefault();

    int requestedEffectChainPosition() const override { return 99; }

    void prePaintWindow(KWin::EffectWindow *window,
                        KWin::WindowPrePaintData &data,
                        std::chrono::milliseconds presentTime) override;

    void drawWindow(const KWin::RenderTarget &renderTarget,
                    const KWin::RenderViewport &viewport,
                    KWin::EffectWindow *window,
                    int mask,
                    const QRegion &region,
                    KWin::WindowPaintData &data) override;

private:
    bool shouldRound(KWin::EffectWindow *window) const;
    bool isMaximized(KWin::EffectWindow *window) const;
    KWin::GLShader *shader();

    std::unique_ptr<KWin::GLShader> m_shader;
    QSet<KWin::EffectWindow *> m_roundedWindows;
    QSet<KWin::EffectWindow *> m_closingWindows;
    QSet<KWin::EffectWindow *> m_maximizingWindows;
    QSet<KWin::EffectWindow *> m_restoringWindows;
    qreal m_frameRadius = 11;
};
