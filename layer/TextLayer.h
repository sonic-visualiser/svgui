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

#ifndef _TEXT_LAYER_H_
#define _TEXT_LAYER_H_

#include "SingleColourLayer.h"
#include "data/model/TextModel.h"

#include <QObject>
#include <QColor>

class View;
class QPainter;

class TextLayer : public SingleColourLayer
{
    Q_OBJECT

public:
    TextLayer();

    virtual void paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const;

    virtual QString getFeatureDescription(LayerGeometryProvider *v, QPoint &) const;

    virtual bool snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
                                    int &resolution,
                                    SnapType snap) const;

    virtual void drawStart(LayerGeometryProvider *v, QMouseEvent *);
    virtual void drawDrag(LayerGeometryProvider *v, QMouseEvent *);
    virtual void drawEnd(LayerGeometryProvider *v, QMouseEvent *);

    virtual void eraseStart(LayerGeometryProvider *v, QMouseEvent *);
    virtual void eraseDrag(LayerGeometryProvider *v, QMouseEvent *);
    virtual void eraseEnd(LayerGeometryProvider *v, QMouseEvent *);

    virtual void editStart(LayerGeometryProvider *v, QMouseEvent *);
    virtual void editDrag(LayerGeometryProvider *v, QMouseEvent *);
    virtual void editEnd(LayerGeometryProvider *v, QMouseEvent *);

    virtual void moveSelection(Selection s, sv_frame_t newStartFrame);
    virtual void resizeSelection(Selection s, Selection newSize);
    virtual void deleteSelection(Selection s);

    virtual void copy(LayerGeometryProvider *v, Selection s, Clipboard &to);
    virtual bool paste(LayerGeometryProvider *v, const Clipboard &from, sv_frame_t frameOffset,
                       bool interactive);

    virtual bool editOpen(LayerGeometryProvider *, QMouseEvent *); // on double-click

    virtual const Model *getModel() const { return m_model; }
    void setModel(TextModel *model);

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
                                          int value) const;
    virtual void setProperty(const PropertyName &, int value);

    virtual bool isLayerScrollable(const LayerGeometryProvider *v) const;

    virtual bool isLayerEditable() const { return true; }

    virtual int getCompletion(LayerGeometryProvider *) const { return m_model->getCompletion(); }

    virtual bool getValueExtents(double &min, double &max,
                                 bool &logarithmic, QString &unit) const;

    virtual int getVerticalScaleWidth(LayerGeometryProvider *, bool, QPainter &) const { return 0; }

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const;

    void setProperties(const QXmlAttributes &attributes);

protected:
    int getYForHeight(LayerGeometryProvider *v, double height) const;
    double getHeightForY(LayerGeometryProvider *v, int y) const;

    virtual int getDefaultColourHint(bool dark, bool &impose);

    TextModel::PointList getLocalPoints(LayerGeometryProvider *v, int x, int y) const;

    bool getPointToDrag(LayerGeometryProvider *v, int x, int y, TextModel::Point &) const;

    TextModel *m_model;
    bool m_editing;
    QPoint m_editOrigin;
    TextModel::Point m_originalPoint;
    TextModel::Point m_editingPoint;
    TextModel::EditCommand *m_editingCommand;

    void finish(TextModel::EditCommand *command) {
        Command *c = command->finish();
        if (c) CommandHistory::getInstance()->addCommand(c, false);
    }
};

#endif

