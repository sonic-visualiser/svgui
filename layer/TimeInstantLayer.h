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

#ifndef _TIME_INSTANT_LAYER_H_
#define _TIME_INSTANT_LAYER_H_

#include "base/Layer.h"
#include "model/SparseOneDimensionalModel.h"

#include <QObject>
#include <QColor>

class View;
class QPainter;

class TimeInstantLayer : public Layer
{
    Q_OBJECT

public:
    TimeInstantLayer();

    virtual void paint(View *v, QPainter &paint, QRect rect) const;

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

    virtual void editOpen(View *, QMouseEvent *);

    virtual void moveSelection(Selection s, size_t newStartFrame);
    virtual void resizeSelection(Selection s, Selection newSize);
    virtual void deleteSelection(Selection s);

    virtual void copy(Selection s, Clipboard &to);
    virtual void paste(const Clipboard &from, int frameOffset);

    virtual const Model *getModel() const { return m_model; }
    void setModel(SparseOneDimensionalModel *model);

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
	PlotInstants,
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
    SparseOneDimensionalModel::PointList getLocalPoints(View *v, int) const;

    SparseOneDimensionalModel *m_model;
    bool m_editing;
    SparseOneDimensionalModel::Point m_editingPoint;
    SparseOneDimensionalModel::EditCommand *m_editingCommand;
    QColor m_colour;
    PlotStyle m_plotStyle;
};

#endif

