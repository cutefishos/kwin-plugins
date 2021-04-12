/*
 *   Copyright © 2021 Reven Martin <revenmartin@gmail.com>
 *   Copyright © 2015 Robert Metsäranta <therealestrob@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; see the file COPYING.  if not, write to
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA 02110-1301, USA.
 */

#include "roundedwindow.h"

// Qt
#include <QFile>
#include <QPainter>

// KWin
#include <kwinglplatform.h>
#include <kwinglutils.h>

KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(RoundedWindowFactory,
                                      RoundedWindow,
                                      "roundedwindow.json",
                                      return RoundedWindow::supported();,
                                      return RoundedWindow::enabledByDefault();)

RoundedWindow::RoundedWindow() 
    : KWin::Effect()
    , m_shader(nullptr)
    , m_frameRadius(12)
    , m_corner(m_frameRadius, m_frameRadius)
{
    QString versionStr = "110";
#ifdef KWIN_HAVE_OPENGLES
    const qint64 coreVersionNumber = kVersionNumber(3, 0);
#else
    const qint64 version = KWin::kVersionNumber(1, 40);
#endif
    if (KWin::GLPlatform::instance()->glslVersion() >= version)
        versionStr = "140";

    QFile file(QString(":/shaders.frag.%1").arg(versionStr));
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "RoundedWindow: cannot open " + file.fileName();
    }

    m_shader = KWin::ShaderManager::instance()->generateCustomShader(KWin::ShaderTrait::MapTexture, QByteArray(), file.readAll());
    file.close();

    if (m_shader->isValid()) {
        const int sampler = m_shader->uniformLocation("sampler");
        const int corner = m_shader->uniformLocation("corner");
        KWin::ShaderManager::instance()->pushShader(m_shader);
        m_shader->setUniform(corner, 1);
        m_shader->setUniform(sampler, 0);
        KWin::ShaderManager::instance()->popShader();

        for (int i = 0; i < NTex; ++i) {
            m_tex[i] = 0;
            m_rect[i] = 0;
        }

        genMasks();
        genRect();
    } else {
        qDebug() << "RoundedWindow: no valid shaders found!";
        deleteLater();
    }
}

RoundedWindow::~RoundedWindow()
{
}

bool RoundedWindow::supported()
{
    return KWin::effects->isOpenGLCompositing() && KWin::GLRenderTarget::supported();
}

bool RoundedWindow::enabledByDefault()
{
    return supported();
}

bool RoundedWindow::hasShadow(KWin::WindowQuadList &qds)
{
    for (int i = 0; i < qds.count(); ++i)
        if (qds.at(i).type() == KWin::WindowQuadShadow)
            return true;

    return false;
}

void RoundedWindow::paintWindow(KWin::EffectWindow *w, int mask, QRegion region, KWin::WindowPaintData &data)
{
    if (!m_shader->isValid()
            || !w->isPaintingEnabled()
            || KWin::effects->hasActiveFullScreenEffect()
            || w->isDesktop()
            || w->isMenu()
            || w->isDock()
            || w->isPopupWindow()
            || w->isPopupMenu()
            || data.quads.isTransformed()
            || (mask & (PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS))
            || !hasShadow(data.quads)) {
        KWin::effects->paintWindow(w, mask, region, data);
        return;
    }

    //map the corners
    const QRect geo(w->geometry());
    const QRect rect[NTex] = {
        QRect(geo.topLeft(), m_corner),
        QRect(geo.topRight()-QPoint(m_frameRadius - 1, 0), m_corner),
        QRect(geo.bottomRight()-QPoint(m_frameRadius - 1, m_frameRadius - 1), m_corner),
        QRect(geo.bottomLeft()-QPoint(0, m_frameRadius - 1), m_corner)
    };

    const KWin::WindowQuadList qds(data.quads);

    //paint the shadow
    data.quads = qds.select(KWin::WindowQuadShadow);
    KWin::effects->paintWindow(w, mask, region, data);

    //copy the corner regions
    KWin::GLTexture tex[NTex];
    const QRect s(KWin::effects->virtualScreenGeometry());
    for (int i = 0; i < NTex; ++i) {
        tex[i] = KWin::GLTexture(GL_RGBA8, rect[i].size());
        tex[i].bind();
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rect[i].x(), s.height() - rect[i].y() - rect[i].height(), rect[i].width(), rect[i].height());
        tex[i].unbind();
    }

    //paint the actual window
    data.quads = qds.filterOut(KWin::WindowQuadShadow);
    KWin::effects->paintWindow(w, mask, region, data);

    //'shape' the corners
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const int mvpMatrixLocation = m_shader->uniformLocation("modelViewProjectionMatrix");
    KWin::ShaderManager *sm = KWin::ShaderManager::instance();
    sm->pushShader(m_shader);
    for (int i = 0; i < NTex; ++i) {
        QMatrix4x4 mvp = data.screenProjectionMatrix();
        mvp.translate(rect[i].x(), rect[i].y());
        m_shader->setUniform(mvpMatrixLocation, mvp);
        glActiveTexture(GL_TEXTURE1);
        m_tex[3-i]->bind();
        glActiveTexture(GL_TEXTURE0);
        tex[i].bind();
        tex[i].render(region, rect[i]);
        tex[i].unbind();
        m_tex[3 - i]->unbind();
    }
    sm->popShader();
    data.quads = qds;

    glDisable(GL_BLEND);
}

void RoundedWindow::genMasks()
{
    for (int i = 0; i < NTex; ++i)
        if (m_tex[i])
            delete m_tex[i];

    QImage img(m_frameRadius * 2, m_frameRadius * 2, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.fillRect(img.rect(), Qt::white);
    p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.setRenderHint(QPainter::Antialiasing);
    p.drawEllipse(img.rect());
    p.end();

    m_tex[TopLeft] = new KWin::GLTexture(img.copy(0, 0, m_frameRadius, m_frameRadius));
    m_tex[TopRight] = new KWin::GLTexture(img.copy(m_frameRadius, 0, m_frameRadius, m_frameRadius));
    m_tex[BottomRight] = new KWin::GLTexture(img.copy(m_frameRadius, m_frameRadius, m_frameRadius, m_frameRadius));
    m_tex[BottomLeft] = new KWin::GLTexture(img.copy(0, m_frameRadius, m_frameRadius, m_frameRadius));
}

void RoundedWindow::genRect()
{
    for (int i = 0; i < NTex; ++i)
        if (m_rect[i])
            delete m_rect[i];

    int m_rSize = m_frameRadius + 1;
    QImage img(m_rSize * 2, m_rSize * 2, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    QRect r(img.rect());
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 0xff));
    p.setRenderHint(QPainter::Antialiasing);
    p.drawEllipse(r);
    p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    p.setBrush(Qt::black);
    r.adjust(1, 1, -1, -1);
    p.drawEllipse(r);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setBrush(QColor(255, 255, 255, 63));
    p.drawEllipse(r);
    p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    p.setBrush(Qt::black);
    r.adjust(0, 1, 0, 0);
    p.drawEllipse(r);
    p.end();

    m_rect[TopLeft] = new KWin::GLTexture(img.copy(0, 0, m_rSize, m_rSize));
    m_rect[TopRight] = new KWin::GLTexture(img.copy(m_rSize, 0, m_rSize, m_rSize));
    m_rect[BottomRight] = new KWin::GLTexture(img.copy(m_rSize, m_rSize, m_rSize, m_rSize));
    m_rect[BottomLeft] = new KWin::GLTexture(img.copy(0, m_rSize, m_rSize, m_rSize));
}

#include "roundedwindow.moc"
