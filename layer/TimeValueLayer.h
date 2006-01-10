/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005
    
    This is experimental software.  Not for distribution.
*/

#ifndef _TIME_VALUE_VIEW_H_
#define _TIME_VALUE_VIEW_H_

#include "base/Layer.h"
#include "model/SparseTimeValueModel.h"

#include <QObject>
#include <QColor>

class View;
class QPainter;

class TimeValueLayer : public Layer
{
    Q_OBJECT

public:
    TimeValueLayer(View *w);

    virtual void paint(QPainter &paint, QRect rect) const;

    virtual QRect getFeatureDescriptionRect(QPainter &, QPoint) const;
    virtual void paintLocalFeatureDescription(QPainter &, QRect, QPoint) const;

    virtual const Model *getModel() const { return m_model; }
    void setModel(SparseTimeValueModel *model);

    virtual PropertyList getProperties() const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
					   int *min, int *max) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual void setProperty(const PropertyName &, int value);

    void setBaseColour(QColor);
    QColor getBaseColour() const { return m_colour; }

    enum PlotStyle { PlotPoints, PlotStems, PlotLines };

    void setPlotStyle(PlotStyle style);
    PlotStyle getPlotStyle() const { return m_plotStyle; }

    virtual QString getPropertyContainerIconName() const { return "values"; }

    virtual bool isLayerScrollable() const;

    virtual int getCompletion() const { return m_model->getCompletion(); }

protected:
    SparseTimeValueModel::PointList getLocalPoints(int) const;

    SparseTimeValueModel *m_model;
    QColor m_colour;
    PlotStyle m_plotStyle;
};

#endif

