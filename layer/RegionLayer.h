/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2008 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_REGION_LAYER_H
#define SV_REGION_LAYER_H

#include "SingleColourLayer.h"
#include "VerticalScaleLayer.h"
#include "ColourScaleLayer.h"

#include "data/model/RegionModel.h"

#include <QObject>
#include <QColor>

#include <map>

class View;
class QPainter;

class RegionLayer : public SingleColourLayer,
                    public VerticalScaleLayer,
                    public ColourScaleLayer
{
    Q_OBJECT

public:
    RegionLayer();

    virtual void paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const;

    virtual int getVerticalScaleWidth(LayerGeometryProvider *v, bool, QPainter &) const;
    virtual void paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect rect) const;

    virtual QString getFeatureDescription(LayerGeometryProvider *v, QPoint &) const;
    virtual QString getLabelPreceding(sv_frame_t) const;

    virtual bool snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
                                    int &resolution,
                                    SnapType snap) const;
    virtual bool snapToSimilarFeature(LayerGeometryProvider *v, sv_frame_t &frame,
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

    virtual bool editOpen(LayerGeometryProvider *v, QMouseEvent *);

    virtual void moveSelection(Selection s, sv_frame_t newStartFrame);
    virtual void resizeSelection(Selection s, Selection newSize);
    virtual void deleteSelection(Selection s);

    virtual void copy(LayerGeometryProvider *v, Selection s, Clipboard &to);
    virtual bool paste(LayerGeometryProvider *v, const Clipboard &from, sv_frame_t frameOffset,
                       bool interactive);

    virtual const Model *getModel() const { return m_model; }
    void setModel(RegionModel *model);

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual QString getPropertyGroupName(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
                                          int value) const;
    virtual void setProperty(const PropertyName &, int value);

    void setFillColourMap(int);
    int getFillColourMap() const { return m_colourMap; }

    enum VerticalScale {
        AutoAlignScale,
        EqualSpaced,
        LinearScale,
        LogScale,
    };

    void setVerticalScale(VerticalScale scale);
    VerticalScale getVerticalScale() const { return m_verticalScale; }

    enum PlotStyle {
        PlotLines,
        PlotSegmentation
    };

    void setPlotStyle(PlotStyle style);
    PlotStyle getPlotStyle() const { return m_plotStyle; }

    virtual bool isLayerScrollable(const LayerGeometryProvider *v) const;

    virtual bool isLayerEditable() const { return true; }

    virtual int getCompletion(LayerGeometryProvider *) const { return m_model->getCompletion(); }

    virtual bool getValueExtents(double &min, double &max,
                                 bool &log, QString &unit) const;

    virtual bool getDisplayExtents(double &min, double &max) const;

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const;

    void setProperties(const QXmlAttributes &attributes);

    /// VerticalScaleLayer and ColourScaleLayer methods
    int getYForValue(LayerGeometryProvider *v, double value) const;
    double getValueForY(LayerGeometryProvider *v, int y) const;
    virtual QString getScaleUnits() const;
    QColor getColourForValue(LayerGeometryProvider *v, double value) const;

protected slots:
    void recalcSpacing();

protected:
    double getValueForY(LayerGeometryProvider *v, int y, int avoid) const;
    void getScaleExtents(LayerGeometryProvider *, double &min, double &max, bool &log) const;

    virtual int getDefaultColourHint(bool dark, bool &impose);

    RegionModel::PointList getLocalPoints(LayerGeometryProvider *v, int x) const;

    bool getPointToDrag(LayerGeometryProvider *v, int x, int y, RegionModel::Point &) const;

    RegionModel *m_model;
    bool m_editing;
    int m_dragPointX;
    int m_dragPointY;
    int m_dragStartX;
    int m_dragStartY;
    RegionModel::Point m_originalPoint;
    RegionModel::Point m_editingPoint;
    RegionModel::EditCommand *m_editingCommand;
    VerticalScale m_verticalScale;
    int m_colourMap;
    bool m_colourInverted;
    PlotStyle m_plotStyle;

    typedef std::map<double, int> SpacingMap;

    // region value -> ordering
    SpacingMap m_spacingMap;

    // region value -> number of regions with this value
    SpacingMap m_distributionMap;

    int spacingIndexToY(LayerGeometryProvider *v, int i) const;
    double yToSpacingIndex(LayerGeometryProvider *v, int y) const;

    void finish(RegionModel::EditCommand *command) {
        Command *c = command->finish();
        if (c) CommandHistory::getInstance()->addCommand(c, false);
    }
};

#endif

