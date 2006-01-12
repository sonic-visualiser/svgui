/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#ifndef _TIME_RULER_H_
#define _TIME_RULER_H_

#include "base/Layer.h"

#include <QRect>
#include <QColor>

class View;
class Model;
class QPainter;

class TimeRulerLayer : public Layer
{
    Q_OBJECT

public:
    TimeRulerLayer(View *w);

    virtual void paint(QPainter &paint, QRect rect) const;

    void setModel(Model *);
    virtual const Model *getModel() const { return m_model; }

    void setBaseColour(QColor);
    QColor getBaseColour() const { return m_colour; }

    enum LabelHeight { LabelTop, LabelMiddle, LabelBottom };
    void setLabelHeight(LabelHeight h) { m_labelHeight = h; }
    LabelHeight getLabelHeight() const { return m_labelHeight; }

    virtual PropertyList getProperties() const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
					   int *min, int *max) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual void setProperty(const PropertyName &, int value);

    virtual QString getPropertyContainerIconName() const { return "timeruler"; }

protected:
    Model *m_model;
    QColor m_colour;
    LabelHeight m_labelHeight;
};

#endif
