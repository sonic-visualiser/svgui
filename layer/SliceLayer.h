/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SLICE_LAYER_H
#define SV_SLICE_LAYER_H

#include "SingleColourLayer.h"

#include "base/Window.h"

#include "data/model/DenseThreeDimensionalModel.h"

#include <QColor>

class SliceLayer : public SingleColourLayer
{
    Q_OBJECT

public:
    SliceLayer();
    ~SliceLayer();
    
    virtual const Model *getModel() const { return 0; }

    void setSliceableModel(const Model *model);    

    virtual void paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const;

    virtual QString getFeatureDescription(LayerGeometryProvider *v, QPoint &) const;

    virtual int getVerticalScaleWidth(LayerGeometryProvider *v, bool, QPainter &) const;
    virtual void paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect rect) const;

    virtual ColourSignificance getLayerColourSignificance() const {
        return ColourAndBackgroundSignificant;
    }

    virtual bool hasLightBackground() const;

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual QString getPropertyIconName(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual QString getPropertyGroupName(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
                                          int value) const;
    virtual RangeMapper *getNewPropertyRangeMapper(const PropertyName &) const;
    virtual void setProperty(const PropertyName &, int value);
    virtual void setProperties(const QXmlAttributes &);

    virtual bool getValueExtents(double &min, double &max,
                                 bool &logarithmic, QString &unit) const;

    virtual bool getDisplayExtents(double &min, double &max) const;
    virtual bool setDisplayExtents(double min, double max);

    virtual int getVerticalZoomSteps(int &defaultStep) const;
    virtual int getCurrentVerticalZoomStep() const;
    virtual void setVerticalZoomStep(int);
    virtual RangeMapper *getNewVerticalZoomRangeMapper() const;

    virtual bool hasTimeXAxis() const { return false; }

    virtual bool isLayerScrollable(const LayerGeometryProvider *) const { return false; }

    enum EnergyScale { LinearScale, MeterScale, dBScale, AbsoluteScale };

    enum SamplingMode { NearestSample, SampleMean, SamplePeak };

    enum PlotStyle { PlotLines, PlotSteps, PlotBlocks, PlotFilledBlocks };

    enum BinScale { LinearBins, LogBins, InvertedLogBins };

    bool usesSolidColour() const { return m_plotStyle == PlotFilledBlocks; }
    
    void setFillColourMap(int);
    int getFillColourMap() const { return m_colourMap; }

    void setEnergyScale(EnergyScale);
    EnergyScale getEnergyScale() const { return m_energyScale; }

    void setSamplingMode(SamplingMode);
    SamplingMode getSamplingMode() const { return m_samplingMode; }

    void setPlotStyle(PlotStyle style);
    PlotStyle getPlotStyle() const { return m_plotStyle; }

    void setBinScale(BinScale scale);
    BinScale getBinScale() const { return m_binScale; }

    void setThreshold(float);
    float getThreshold() const { return m_threshold; }

    void setGain(float gain);
    float getGain() const;

    void setNormalize(bool n);
    bool getNormalize() const;

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const;

public slots:
    void sliceableModelReplaced(const Model *, const Model *);
    void modelAboutToBeDeleted(Model *);

protected:
    virtual double getXForBin(const LayerGeometryProvider *, double bin) const;
    virtual double getBinForX(const LayerGeometryProvider *, double x) const;

    virtual double getYForValue(const LayerGeometryProvider *v, double value, double &norm) const;
    virtual double getValueForY(const LayerGeometryProvider *v, double y) const;
    
    virtual QString getFeatureDescriptionAux(LayerGeometryProvider *v, QPoint &,
                                             bool includeBinDescription,
                                             int &minbin, int &maxbin,
                                             int &range) const;

    // This curve may, of course, be flat -- the spectrum uses it for
    // normalizing the fft results by the fft size (with 1/(fftsize/2)
    // in each bin).
    typedef std::vector<float> BiasCurve;
    virtual void getBiasCurve(BiasCurve &) const { return; }

    virtual float getThresholdDb() const;

    virtual int getDefaultColourHint(bool dark, bool &impose);

    const DenseThreeDimensionalModel *m_sliceableModel;
    int                               m_colourMap;
    EnergyScale                       m_energyScale;
    SamplingMode                      m_samplingMode;
    PlotStyle                         m_plotStyle;
    BinScale                          m_binScale;
    bool                              m_normalize;
    float                             m_threshold;
    float                             m_initialThreshold;
    float                             m_gain;
    int                               m_minbin;
    int                               m_maxbin;
    mutable std::vector<int>          m_scalePoints;
    mutable int                       m_scalePaintHeight;
    mutable std::map<int, int>        m_xorigins; // LayerGeometryProvider id -> x
    mutable std::map<int, int>        m_yorigins; // LayerGeometryProvider id -> y
    mutable std::map<int, int>        m_heights;  // LayerGeometryProvider id -> h
    mutable sv_frame_t                m_currentf0;
    mutable sv_frame_t                m_currentf1;
    mutable std::vector<float>        m_values;
};

#endif
