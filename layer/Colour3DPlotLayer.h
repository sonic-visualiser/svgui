/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _COLOUR_3D_PLOT_H_
#define _COLOUR_3D_PLOT_H_

#include "SliceableLayer.h"

#include "data/model/DenseThreeDimensionalModel.h"

class View;
class QPainter;
class QImage;

/**
 * This is a view that displays dense 3-D data (time, some sort of
 * binned y-axis range, value) as a colour plot with value mapped to
 * colour range.  Its source is a DenseThreeDimensionalModel.
 *
 * This was the original implementation for the spectrogram view, but
 * it was replaced with a more efficient implementation that derived
 * the spectrogram itself from a DenseTimeValueModel instead of using
 * a three-dimensional model.  This class is retained in case it
 * becomes useful, but it will probably need some cleaning up if it's
 * ever actually used.
 */

class Colour3DPlotLayer : public SliceableLayer
{
    Q_OBJECT

public:
    Colour3DPlotLayer();
    ~Colour3DPlotLayer();

    virtual const ZoomConstraint *getZoomConstraint() const {
        return m_model ? m_model->getZoomConstraint() : 0;
    }
    virtual const Model *getModel() const { return m_model; }
    virtual void paint(View *v, QPainter &paint, QRect rect) const;

    virtual int getVerticalScaleWidth(View *v, QPainter &) const;
    virtual void paintVerticalScale(View *v, QPainter &paint, QRect rect) const;

    virtual QString getFeatureDescription(View *v, QPoint &) const;

    virtual bool snapToFeatureFrame(View *v, int &frame, 
				    size_t &resolution,
				    SnapType snap) const;

    virtual bool isLayerScrollable(const View *v) const;

    virtual ColourSignificance getLayerColourSignificance() const {
        return ColourHasMeaningfulValue;
    }

    void setModel(const DenseThreeDimensionalModel *model);

    virtual int getCompletion(View *) const { return m_model->getCompletion(); }

    virtual PropertyList getProperties() const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual QString getPropertyIconName(const PropertyName &) const;
    virtual QString getPropertyGroupName(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual void setProperty(const PropertyName &, int value);
    virtual void setProperties(const QXmlAttributes &);
    
    enum ColourScale { LinearScale, LogScale, PlusMinusOneScale };

    void setColourScale(ColourScale);
    ColourScale getColourScale() const { return m_colourScale; }

    void setColourMap(int map);
    int getColourMap() const;

    void setNormalizeColumns(bool n);
    bool getNormalizeColumns() const;

    void setNormalizeVisibleArea(bool n);
    bool getNormalizeVisibleArea() const;

    void setInvertVertical(bool i);
    bool getInvertVertical() const;

    void setOpaque(bool i);
    bool getOpaque() const;

    virtual bool getValueExtents(float &min, float &max,
                                 bool &logarithmic, QString &unit) const;

    virtual bool getDisplayExtents(float &min, float &max) const;
    virtual bool setDisplayExtents(float min, float max);

    virtual int getVerticalZoomSteps(int &defaultStep) const;
    virtual int getCurrentVerticalZoomStep() const;
    virtual void setVerticalZoomStep(int);
    virtual RangeMapper *getNewVerticalZoomRangeMapper() const;

    virtual const Model *getSliceableModel() const { return m_model; }

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const;

protected slots:
    void cacheInvalid();
    void cacheInvalid(size_t startFrame, size_t endFrame);
    void modelChanged();
    void modelChanged(size_t, size_t);

protected:
    const DenseThreeDimensionalModel *m_model; // I do not own this
    
    mutable QImage *m_cache;
    mutable QImage *m_peaksCache;
    mutable size_t m_cacheValidStart;
    mutable size_t m_cacheValidEnd;

    ColourScale m_colourScale;
    bool        m_colourScaleSet;
    int         m_colourMap;
    bool        m_normalizeColumns;
    bool        m_normalizeVisibleArea;
    bool        m_invertVertical;
    bool        m_opaque;
    size_t      m_peakResolution;

    int         m_miny;
    int         m_maxy;
    
    DenseThreeDimensionalModel::Column getColumn(size_t col) const;

    int getColourScaleWidth(QPainter &) const;
    void fillCache(size_t firstBin, size_t lastBin) const;
    void paintDense(View *v, QPainter &paint, QRect rect) const;
};

#endif
