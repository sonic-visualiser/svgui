
/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "SpectrumLayer.h"

#include "data/model/FFTModel.h"
#include "view/View.h"

#include <QPainter>
#include <QPainterPath>

SpectrumLayer::SpectrumLayer() :
    m_model(0),
    m_fft(0),
    m_colour(Qt::blue)
{
}

SpectrumLayer::~SpectrumLayer()
{
    delete m_fft;
}

void
SpectrumLayer::setModel(DenseTimeValueModel *model)
{
    m_model = model;
    delete m_fft;
    m_fft = new FFTModel(m_model,
                         -1,
                         HanningWindow,
                         1024,
                         256,
                         1024,
                         true);
    m_fft->resume();
}

void
SpectrumLayer::paint(View *v, QPainter &paint, QRect rect) const
{
    if (!m_fft) return;

    int fftSize = 1024; //!!! ...
    int windowIncrement = 256;
    int windowSize = 1024;

    size_t f = v->getCentreFrame();

    int w = (v->width() * 2) / 3;
    int xorigin = (v->width() / 2) - (w / 2);
    
    int h = (v->height() * 2) / 3;
    int yorigin = (v->height() / 2) + (h / 2);

    size_t column = f / windowIncrement;

    paint.save();
    paint.setPen(m_colour);
    paint.setRenderHint(QPainter::Antialiasing, false);
    
    QPainterPath path;
    float thresh = -80.f;

    for (size_t bin = 0; bin < m_fft->getHeight(); ++bin) {

        float mag = m_fft->getMagnitudeAt(column, bin);
        float db = thresh;
        if (mag > 0.f) db = 10.f * log10f(mag);
        if (db < thresh) db = thresh;
        float val = (db - thresh) / -thresh;
        float x = xorigin + (float(w) * bin) / m_fft->getHeight();
        float y = yorigin - (float(h) * val);

        if (bin == 0) {
            path.moveTo(x, y);
        } else {
            path.lineTo(x, y);
        }
    }

    paint.drawPath(path);
//    paint.setRenderHint(QPainter::Antialiasing, false);
    paint.restore();

}

void
SpectrumLayer::setProperties(const QXmlAttributes &attr)
{
}

bool
SpectrumLayer::getValueExtents(float &min, float &max, bool &logarithmic,
                               QString &units) const
{
    return false;
}

