/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
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

    virtual int getNearestFeatureFrame(int frame, 
				       size_t &resolution,
				       bool snapRight = true) const;

    virtual void drawStart(QMouseEvent *);
    virtual void drawDrag(QMouseEvent *);
    virtual void drawEnd(QMouseEvent *);

    virtual void editStart(QMouseEvent *);
    virtual void editDrag(QMouseEvent *);
    virtual void editEnd(QMouseEvent *);

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

    enum PlotStyle { PlotPoints, PlotStems, PlotConnectedPoints, PlotLines, PlotCurve };

    void setPlotStyle(PlotStyle style);
    PlotStyle getPlotStyle() const { return m_plotStyle; }

    virtual bool isLayerScrollable() const;

    virtual int getCompletion() const { return m_model->getCompletion(); }

    virtual QString toXmlString(QString indent = "",
				QString extraAttributes = "") const;

    void setProperties(const QXmlAttributes &attributes);

protected:
    int getYForValue(float value) const;
    float getValueForY(int y) const;

    SparseTimeValueModel::PointList getLocalPoints(int) const;

    SparseTimeValueModel *m_model;
    bool m_editing;
    SparseTimeValueModel::Point m_editingPoint;
    SparseTimeValueModel::EditCommand *m_editingCommand;
    QColor m_colour;
    PlotStyle m_plotStyle;
};

#endif

