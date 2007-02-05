
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

#ifndef _SLICE_LAYER_H_
#define _SLICE_LAYER_H_

#include "Layer.h"

#include "base/Window.h"

#include "data/model/DenseThreeDimensionalModel.h"

#include <QColor>

class SliceLayer : public Layer
{
    Q_OBJECT

public:
    SliceLayer();
    ~SliceLayer();
    
//    virtual void setModel(const Model *model);
//    virtual const Model *getModel() const { return m_model; }
    virtual const Model *getModel() const { return 0; }

    void setSliceableModel(const Model *model);    

    virtual void paint(View *v, QPainter &paint, QRect rect) const;

    virtual QString getFeatureDescription(View *v, QPoint &) const;

    virtual int getVerticalScaleWidth(View *v, QPainter &) const;
    virtual void paintVerticalScale(View *v, QPainter &paint, QRect rect) const;

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual QString getPropertyGroupName(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
					   int *min, int *max) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual RangeMapper *getNewPropertyRangeMapper(const PropertyName &) const;
    virtual void setProperty(const PropertyName &, int value);
    virtual void setProperties(const QXmlAttributes &);

    virtual bool getValueExtents(float &min, float &max,
                                 bool &logarithmic, QString &unit) const;

    virtual bool isLayerScrollable(const View *v) const { return false; }

    enum EnergyScale { LinearScale, MeterScale, dBScale };

    enum SamplingMode { NearestSample, SampleMean, SamplePeak };

    enum PlotStyle { PlotLines, PlotSteps, PlotBlocks, PlotFilledBlocks };

    enum BinScale { LinearBins, LogBins, InvertedLogBins };

    void setBaseColour(QColor);
    QColor getBaseColour() const { return m_colour; }

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

    void setGain(float gain);
    float getGain() const;

    void setNormalize(bool n);
    bool getNormalize() const;

    virtual QString toXmlString(QString indent = "",
				QString extraAttributes = "") const;

public slots:
    void sliceableModelReplaced(const Model *, const Model *);
    void modelAboutToBeDeleted(Model *);

protected:
    float getXForBin(int bin, int totalBins, float w) const;
    int getBinForX(float x, int totalBins, float w) const;
    
    virtual QString getFeatureDescription(View *v, QPoint &,
                                          bool includeBinDescription,
                                          int &minbin, int &maxbin,
                                          int &range) const;

    const DenseThreeDimensionalModel *m_sliceableModel;
    QColor                            m_colour;
    int                               m_colourMap;
    EnergyScale                       m_energyScale;
    SamplingMode                      m_samplingMode;
    PlotStyle                         m_plotStyle;
    BinScale                          m_binScale;
    bool                              m_normalize;
    bool                              m_bias;
    float                             m_gain;
    mutable std::vector<int>          m_scalePoints;
    mutable std::map<View *, int>     m_xorigins;
    mutable size_t                    m_currentf0;
    mutable size_t                    m_currentf1;
    mutable std::vector<float>        m_values;
};

#endif
