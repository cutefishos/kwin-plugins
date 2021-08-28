/*
 *   Copyright © 2021 Reion Wong <reionwong@gmail.com>
 *   Copyright © 2021 Reven Martin <revenmartin@gmail.com>
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
#include <QPainterPath>
#include <QRegion>
#include <QDebug>

Q_DECLARE_METATYPE(QPainterPath)

typedef void (* SetDepth)(void *, int);
static SetDepth setDepthfunc = nullptr;

// From ubreffect
static KWin::GLShader *getShader()
{
    // copy from kwinglutils.cpp
    QByteArray source;
    QTextStream stream(&source);

    KWin::GLPlatform * const gl = KWin::GLPlatform::instance();
    QByteArray varying, output, textureLookup;

    if (!gl->isGLES()) {
        const bool glsl_140 = gl->glslVersion() >= KWin::kVersionNumber(1, 40);

        if (glsl_140)
            stream << "#version 140\n\n";

        varying       = glsl_140 ? QByteArrayLiteral("in")         : QByteArrayLiteral("varying");
        textureLookup = glsl_140 ? QByteArrayLiteral("texture")    : QByteArrayLiteral("texture2D");
        output        = glsl_140 ? QByteArrayLiteral("fragColor")  : QByteArrayLiteral("gl_FragColor");
    } else {
        const bool glsl_es_300 = KWin::GLPlatform::instance()->glslVersion() >= KWin::kVersionNumber(3, 0);

        if (glsl_es_300)
            stream << "#version 300 es\n\n";

        // From the GLSL ES specification:
        //
        //     "The fragment language has no default precision qualifier for floating point types."
        stream << "precision highp float;\n\n";

        varying       = glsl_es_300 ? QByteArrayLiteral("in")         : QByteArrayLiteral("varying");
        textureLookup = glsl_es_300 ? QByteArrayLiteral("texture")    : QByteArrayLiteral("texture2D");
        output        = glsl_es_300 ? QByteArrayLiteral("fragColor")  : QByteArrayLiteral("gl_FragColor");
    }

    KWin::ShaderTraits traits;

    traits |= KWin::ShaderTrait::MapTexture;
    traits |= KWin::ShaderTrait::Modulate;
    traits |= KWin::ShaderTrait::AdjustSaturation;

    if (traits & KWin::ShaderTrait::MapTexture) {
        stream << "uniform sampler2D sampler;\n";

        // custom texture
        stream << "uniform sampler2D topleft;\n";
        stream << "uniform sampler2D topright;\n";
        stream << "uniform sampler2D bottomleft;\n";
        stream << "uniform sampler2D bottomright;\n";

        // scale
        stream << "uniform vec2 scale;\n";
        stream << "uniform vec2 scale1;\n";
        stream << "uniform vec2 scale2;\n";
        stream << "uniform vec2 scale3;\n";

        if (traits & KWin::ShaderTrait::Modulate)
            stream << "uniform vec4 modulation;\n";
        if (traits & KWin::ShaderTrait::AdjustSaturation)
            stream << "uniform float saturation;\n";

        stream << "\n" << varying << " vec2 texcoord0;\n";

    } else if (traits & KWin::ShaderTrait::UniformColor)
        stream << "uniform vec4 geometryColor;\n";

    if (traits & KWin::ShaderTrait::ClampTexture) {
        stream << "uniform vec4 textureClamp;\n";
    }

    if (output != QByteArrayLiteral("gl_FragColor"))
        stream << "\nout vec4 " << output << ";\n";

    stream << "\nvoid main(void)\n{\n";
    if (traits & KWin::ShaderTrait::MapTexture) {
        stream << "vec2 texcoordC = texcoord0;\n";


        stream << "    " << "vec4 var;\n";
        stream << "if (texcoordC.x < 0.5) {\n"
                  "    if (texcoordC.y < 0.5) {\n"
                  "        vec2 cornerCoord = vec2(texcoordC.x * scale.x, texcoordC.y * scale.y);\n"
                  "        var = " << textureLookup << "(topleft, cornerCoord);\n"
                  "    } else {\n"
                  "        vec2 cornerCoordBL = vec2(texcoordC.x * scale2.x, (1.0 - texcoordC.y) * scale2.y);\n"
                  "        var = " << textureLookup << "(bottomleft, cornerCoordBL);\n"
                  "    }\n"
                  "} else {\n"
                  "    if (texcoordC.y < 0.5) {\n"
                  "        vec2 cornerCoordTR = vec2((1.0 - texcoordC.x) * scale1.x, texcoordC.y * scale1.y);\n"
                  "        var = " << textureLookup << "(topright, cornerCoordTR);\n"
                  "    } else {\n"
                  "        vec2 cornerCoordBR = vec2((1.0 - texcoordC.x) * scale3.x, (1.0 - texcoordC.y) * scale3.y);\n"
                  "        var = " << textureLookup << "(bottomright, cornerCoordBR);\n"
                  "    }\n"
                  "}\n";

        stream << "    vec4 texel = " << textureLookup << "(sampler, texcoordC);\n";
        if (traits & KWin::ShaderTrait::Modulate)
            stream << "    texel *= modulation;\n";
        if (traits & KWin::ShaderTrait::AdjustSaturation)
            stream << "    texel.rgb = mix(vec3(dot(texel.rgb, vec3(0.2126, 0.7152, 0.0722))), texel.rgb, saturation);\n";

        stream << "    " << output << " = texel * var;\n";
    } else if (traits & KWin::ShaderTrait::UniformColor)
        stream << "    " << output << " = geometryColor;\n";

    stream << "}";
    stream.flush();

    auto shader = KWin::ShaderManager::instance()->generateCustomShader(traits, QByteArray(), source);
    //shaders.insert(direction, shader);
    return shader;
}

static KWin::GLTexture *getTexture(int borderRadius)
{
    QPixmap pix(QSize(borderRadius, borderRadius));
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.moveTo(borderRadius, 0);
    path.arcTo(0, 0, 2 * borderRadius, 2 * borderRadius, 90, 90);
    path.lineTo(borderRadius, borderRadius);
    path.lineTo(borderRadius, 0);
    painter.fillPath(path, Qt::white);

    auto texture = new KWin::GLTexture(pix);
    texture->setFilter(GL_LINEAR);
    texture->setWrapMode(GL_CLAMP_TO_BORDER);

    return texture;
}

RoundedWindow::RoundedWindow(QObject *, const QVariantList &)
    : KWin::Effect()
    , m_frameRadius(12)
    , m_corner(m_frameRadius, m_frameRadius)
{   
    setDepthfunc = (SetDepth) QLibrary::resolve("kwin.so." + qApp->applicationVersion(), "_ZN4KWin8Toplevel8setDepthEi");

    m_newShader = getShader();
    m_texure = getTexture(m_frameRadius);
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

void RoundedWindow::drawWindow(KWin::EffectWindow *w, int mask, const QRegion &_region, KWin::WindowPaintData &data)
{
    QRegion region = _region;

    if (!w->isPaintingEnabled() || ((mask & PAINT_WINDOW_LANCZOS))) {
        return KWin::Effect::drawWindow(w, mask, region, data);
    }

    if (!m_newShader->isValid()
            || KWin::effects->hasActiveFullScreenEffect()
            || w->isDesktop()
            || w->isMenu()
            || w->isDock()
            || w->isPopupWindow()
            || w->isPopupMenu()
            || w->isFullScreen()
            || !hasShadow(data.quads)) {
        return KWin::Effect::drawWindow(w, mask, region, data);
    }

    KWin::WindowPaintData paintData = data;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };

    auto textureTopLeft = m_texure;
    glActiveTexture(GL_TEXTURE1);
    textureTopLeft->bind();
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glActiveTexture(GL_TEXTURE0);

    auto textureTopRight = m_texure;
    glActiveTexture(GL_TEXTURE2);
    textureTopRight->bind();
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glActiveTexture(GL_TEXTURE0);

    auto textureBottomLeft = m_texure;
    glActiveTexture(GL_TEXTURE3);
    textureBottomLeft->bind();
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glActiveTexture(GL_TEXTURE0);

    auto textureBottomRight = m_texure;
    glActiveTexture(GL_TEXTURE4);
    textureBottomRight->bind();
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glActiveTexture(GL_TEXTURE0);

    paintData.shader = m_newShader;
    KWin::ShaderManager::instance()->pushShader(m_newShader);

    m_newShader->setUniform("topleft", 1);
    m_newShader->setUniform("scale", QVector2D(w->width() * 1.0 / textureTopLeft->width(),
                                               w->height() * 1.0 / textureTopLeft->height()));

    m_newShader->setUniform("topright", 2);
    m_newShader->setUniform("scale1", QVector2D(w->width() * 1.0 / textureTopRight->width(),
                                                w->height() * 1.0 / textureTopRight->height()));

    m_newShader->setUniform("bottomleft", 3);
    m_newShader->setUniform("scale2", QVector2D(w->width() * 1.0 / textureBottomLeft->width(),
                                                w->height() * 1.0 / textureBottomLeft->height()));

    m_newShader->setUniform("bottomright", 4);
    m_newShader->setUniform("scale3", QVector2D(w->width() * 1.0 / textureBottomRight->width(),
                                                w->height() * 1.0 / textureBottomRight->height()));

    // 设置 alpha 通道混合
    if (!w->hasAlpha()) {
        if (setDepthfunc) {
            setDepthfunc(w->parent(), 32);
        }
    }

    KWin::Effect::drawWindow(w, mask, region, paintData);
    KWin::ShaderManager::instance()->popShader();

    glActiveTexture(GL_TEXTURE1);
    textureTopLeft->unbind();
    glActiveTexture(GL_TEXTURE0);

    glActiveTexture(GL_TEXTURE2);
    textureTopRight->unbind();
    glActiveTexture(GL_TEXTURE0);

    glActiveTexture(GL_TEXTURE3);
    textureBottomLeft->unbind();
    glActiveTexture(GL_TEXTURE0);

    glActiveTexture(GL_TEXTURE4);
    textureBottomRight->unbind();
    glActiveTexture(GL_TEXTURE0);

    glDisable(GL_BLEND);
}
