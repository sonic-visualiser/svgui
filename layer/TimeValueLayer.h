/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#ifndef _TIME_VALUE_LAYER_H_
#define _TIME_VALUE_LAYER_H_

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
    TimeValueLayer();

    virtual void paint(View *v, QPainter &paint, QRect rect) const;

    virtual int getVerticalScaleWidth(View *v, QPainter &) const;
    virtual void paintVerticalScale(View *v, QPainter &paint, QRect rect) const;

    virtual QString getFeatureDescription(View *v, QPoint &) const;

    virtual bool snapToFeatureFrame(View *v, int &frame,
				    size_t &resolution,
				    SnapType snap) const;

    virtual void drawStart(View *v, QMouseEvent *);
    virtual void drawDrag(View *v, QMouseEvent *);
    virtual void drawEnd(View *v, QMouseEvent *);

    virtual void editStart(View *v, QMouseEvent *);
    virtual void editDrag(View *v, QMouseEvent *);
    virtual void editEnd(View *v, QMouseEvent *);

    virtual void moveSelection(Selection s, size_t newStartFrame);
    virtual void resizeSelection(Selection s, Selection newSize);

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

    enum PlotStyle {
	PlotPoints,
	PlotStems,
	PlotConnectedPoints,
	PlotLines,
	PlotCurve,
	PlotSegmentation
    };

    void setPlotStyle(PlotStyle style);
    PlotStyle getPlotStyle() const { return m_plotStyle; }

    virtual bool isLayerScrollable(const View *v) const;

    virtual bool isLayerEditable() const { return true; }

    virtual int getCompletion() const { return m_model->getCompletion(); }

    virtual QString toXmlString(QString indent = "",
				QString extraAttributes = "") const;

    void setProperties(const QXmlAttributes &attributes);

protected:
    int getYForValue(View *, float value) const;
    float getValueForY(View *, int y) const;

    SparseTimeValueModel::PointList getLocalPoints(View *v, int) const;

    SparseTimeValueModel *m_model;
    bool m_editing;
    SparseTimeValueModel::Point m_originalPoint;
    SparseTimeValueModel::Point m_editingPoint;
    SparseTimeValueModel::EditCommand *m_editingCommand;
    QColor m_colour;
    PlotStyle m_plotStyle;
};

#endif

