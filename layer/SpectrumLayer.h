
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

#ifndef _SPECTRUM_LAYER_H_
#define _SPECTRUM_LAYER_H_

#include "Layer.h"

#include "data/model/DenseTimeValueModel.h"

#include <QColor>

class FFTModel;

class SpectrumLayer : public Layer
{
    Q_OBJECT

public:
    SpectrumLayer();
    ~SpectrumLayer();
    
    void setModel(DenseTimeValueModel *model);
    virtual const Model *getModel() const { return m_model; }
    virtual void paint(View *v, QPainter &paint, QRect rect) const;

    virtual void setProperties(const QXmlAttributes &);

    virtual bool getValueExtents(float &min, float &max,
                                 bool &logarithmic, QString &unit) const;

    virtual bool isLayerScrollable(const View *v) const { return false; }

    virtual QString getPropertyLabel(const PropertyName &) const { return ""; }

protected:
    DenseTimeValueModel *m_model;
    FFTModel *m_fft;
    QColor m_colour;
};

#endif
