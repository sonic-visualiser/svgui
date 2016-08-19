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

#ifndef SPECTROGRAM_LAYER_H
#define SPECTROGRAM_LAYER_H

#include "SliceableLayer.h"
#include "base/Window.h"
#include "base/MagnitudeRange.h"
#include "base/RealTime.h"
#include "base/Thread.h"
#include "base/PropertyContainer.h"
#include "data/model/PowerOfSqrtTwoZoomConstraint.h"
#include "data/model/DenseTimeValueModel.h"
#include "data/model/FFTModel.h"

#include "VerticalBinLayer.h"
#include "ColourScale.h"
#include "Colour3DPlotRenderer.h"

#include <QMutex>
#include <QWaitCondition>
#include <QImage>
#include <QPixmap>

class View;
class QPainter;
class QImage;
class QPixmap;
class QTimer;
class FFTModel;
class Dense3DModelPeakCache;

/**
 * SpectrogramLayer represents waveform data (obtained from a
 * DenseTimeValueModel) in spectrogram form.
 */

class SpectrogramLayer : public VerticalBinLayer,
			 public PowerOfSqrtTwoZoomConstraint
{
    Q_OBJECT

public:
    enum Configuration { FullRangeDb, MelodicRange, MelodicPeaks };
    
    SpectrogramLayer(Configuration = FullRangeDb);
    ~SpectrogramLayer();

    virtual const ZoomConstraint *getZoomConstraint() const { return this; }
    virtual const Model *getModel() const { return m_model; }
    virtual void paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const;
    virtual void setSynchronousPainting(bool synchronous);

    virtual int getVerticalScaleWidth(LayerGeometryProvider *v, bool detailed, QPainter &) const;
    virtual void paintVerticalScale(LayerGeometryProvider *v, bool detailed, QPainter &paint, QRect rect) const;

    virtual bool getCrosshairExtents(LayerGeometryProvider *, QPainter &, QPoint cursorPos,
                                     std::vector<QRect> &extents) const;
    virtual void paintCrosshairs(LayerGeometryProvider *, QPainter &, QPoint) const;

    virtual QString getFeatureDescription(LayerGeometryProvider *v, QPoint &) const;

    virtual bool snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
				    int &resolution,
				    SnapType snap) const;

    virtual void measureDoubleClick(LayerGeometryProvider *, QMouseEvent *);

    virtual bool hasLightBackground() const;

    void setModel(const DenseTimeValueModel *model);

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual QString getPropertyIconName(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual QString getPropertyGroupName(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual QString getPropertyValueIconName(const PropertyName &,
                                             int value) const;
    virtual RangeMapper *getNewPropertyRangeMapper(const PropertyName &) const;
    virtual void setProperty(const PropertyName &, int value);

    /**
     * Specify the channel to use from the source model.
     * A value of -1 means to mix all available channels.
     * The default is channel 0.
     */
    void setChannel(int);
    int getChannel() const;

    void setWindowSize(int);
    int getWindowSize() const;
    
    void setWindowHopLevel(int level);
    int getWindowHopLevel() const;

    void setWindowType(WindowType type);
    WindowType getWindowType() const;

    /**
     * Set the gain multiplier for sample values in this view.
     * The default is 1.0.
     */
    void setGain(float gain);
    float getGain() const;

    /**
     * Set the threshold for sample values to qualify for being shown
     * in the FFT, in voltage units.
     *
     * The default is 10^-8 (-80dB).
     */
    void setThreshold(float threshold);
    float getThreshold() const;

    void setMinFrequency(int);
    int getMinFrequency() const;

    void setMaxFrequency(int); // 0 -> no maximum
    int getMaxFrequency() const;

    /**
     * Specify the scale for sample levels.  See ColourScale and
     * WaveformLayer for comparison and details of meter and dB
     * scaling.  The default is LogColourScale.
     */
    void setColourScale(ColourScaleType);
    ColourScaleType getColourScale() const;

    /**
     * Specify multiple factor for colour scale. This is 2.0 for
     * log-power spectrogram and 1.0 otherwise.
     */
    void setColourScaleMultiple(double);
    double getColourScaleMultiple() const;
    
    /**
     * Specify the scale for the y axis.
     */
    void setBinScale(BinScale);
    BinScale getBinScale() const;

    /**
     * Specify the processing of frequency bins for the y axis.
     */
    void setBinDisplay(BinDisplay);
    BinDisplay getBinDisplay() const;

    /**
     * Specify the normalization mode for individual columns.
     */
    void setNormalization(ColumnNormalization);
    ColumnNormalization getNormalization() const;

    /**
     * Specify whether to normalize the visible area.
     */
    void setNormalizeVisibleArea(bool);
    bool getNormalizeVisibleArea() const;

    /**
     * Specify the colour map. See ColourMapper for the colour map
     * values.
     */
    void setColourMap(int map);
    int getColourMap() const;

    /**
     * Specify the colourmap rotation for the colour scale.
     */
    void setColourRotation(int);
    int getColourRotation() const;

    virtual VerticalPosition getPreferredFrameCountPosition() const {
	return PositionTop;
    }

    virtual bool isLayerOpaque() const { return true; }

    virtual ColourSignificance getLayerColourSignificance() const {
        return ColourHasMeaningfulValue;
    }

    double getYForFrequency(const LayerGeometryProvider *v, double frequency) const;
    double getFrequencyForY(const LayerGeometryProvider *v, int y) const;

    //!!! VerticalBinLayer methods. Note overlap with get*BinRange()
    double getYForBin(const LayerGeometryProvider *, double bin) const;
    double getBinForY(const LayerGeometryProvider *, double y) const;
    
    virtual int getCompletion(LayerGeometryProvider *v) const;
    virtual QString getError(LayerGeometryProvider *v) const;

    virtual bool getValueExtents(double &min, double &max,
                                 bool &logarithmic, QString &unit) const;

    virtual bool getDisplayExtents(double &min, double &max) const;

    virtual bool setDisplayExtents(double min, double max);

    virtual bool getYScaleValue(const LayerGeometryProvider *, int, double &, QString &) const;

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const;

    void setProperties(const QXmlAttributes &attributes);

    virtual void setLayerDormant(const LayerGeometryProvider *v, bool dormant);

    virtual bool isLayerScrollable(const LayerGeometryProvider *) const { return false; }

    virtual int getVerticalZoomSteps(int &defaultStep) const;
    virtual int getCurrentVerticalZoomStep() const;
    virtual void setVerticalZoomStep(int);
    virtual RangeMapper *getNewVerticalZoomRangeMapper() const;

    virtual const Model *getSliceableModel() const;

protected slots:
    void cacheInvalid();
    void cacheInvalid(sv_frame_t startFrame, sv_frame_t endFrame);
    
    void preferenceChanged(PropertyContainer::PropertyName name);

protected:
    const DenseTimeValueModel *m_model; // I do not own this

    int                 m_channel;
    int                 m_windowSize;
    WindowType          m_windowType;
    int                 m_windowHopLevel;
    float               m_gain;
    float               m_initialGain;
    float               m_threshold;
    float               m_initialThreshold;
    int                 m_colourRotation;
    int                 m_initialRotation;
    int                 m_minFrequency;
    int                 m_maxFrequency;
    int                 m_initialMaxFrequency;
    ColourScaleType     m_colourScale;
    double              m_colourScaleMultiple;
    int                 m_colourMap;
    QColor              m_crosshairColour;
    BinScale            m_binScale;
    BinDisplay          m_binDisplay;
    ColumnNormalization m_normalization; // of individual columns
    bool                m_normalizeVisibleArea;
    int                 m_lastEmittedZoomStep;
    bool                m_synchronous;

    mutable bool        m_haveDetailedScale;

    static std::pair<ColourScaleType, double> convertToColourScale(int value);
    static int convertFromColourScale(ColourScaleType type, double multiple);
    static std::pair<ColumnNormalization, bool> convertToColumnNorm(int value);
    static int convertFromColumnNorm(ColumnNormalization norm, bool visible);

    bool m_exiting;

    int getColourScaleWidth(QPainter &) const;

    void illuminateLocalFeatures(LayerGeometryProvider *v, QPainter &painter) const;

    double getEffectiveMinFrequency() const;
    double getEffectiveMaxFrequency() const;

    bool getXBinRange(LayerGeometryProvider *v, int x, double &windowMin, double &windowMax) const;
    bool getYBinRange(LayerGeometryProvider *v, int y, double &freqBinMin, double &freqBinMax) const;

    bool getYBinSourceRange(LayerGeometryProvider *v, int y, double &freqMin, double &freqMax) const;
    bool getAdjustedYBinSourceRange(LayerGeometryProvider *v, int x, int y,
				    double &freqMin, double &freqMax,
				    double &adjFreqMin, double &adjFreqMax) const;
    bool getXBinSourceRange(LayerGeometryProvider *v, int x, RealTime &timeMin, RealTime &timeMax) const;
    bool getXYBinSourceRange(LayerGeometryProvider *v, int x, int y, double &min, double &max,
			     double &phaseMin, double &phaseMax) const;

    int getWindowIncrement() const {
        if (m_windowHopLevel == 0) return m_windowSize;
        else if (m_windowHopLevel == 1) return (m_windowSize * 3) / 4;
        else return m_windowSize / (1 << (m_windowHopLevel - 1));
    }

    int getFFTOversampling() const;
    int getFFTSize() const; // m_windowSize * getFFTOversampling()

    mutable FFTModel *m_fftModel; //!!! should not be mutable, see getFFTModel()?
    mutable Dense3DModelPeakCache *m_peakCache;
    const int m_peakCacheDivisor;

    typedef std::map<int, MagnitudeRange> ViewMagMap; // key is view id
    mutable ViewMagMap m_viewMags;
    mutable ViewMagMap m_lastRenderedMags; // when in normalizeVisibleArea mode
    void invalidateMagnitudes();

    typedef std::map<int, Colour3DPlotRenderer *> ViewRendererMap; // key is view id
    mutable ViewRendererMap m_renderers;
    Colour3DPlotRenderer *getRenderer(LayerGeometryProvider *) const;
    void invalidateRenderers();
    
    FFTModel *getFFTModel() const;
    Dense3DModelPeakCache *getPeakCache() const;
    void invalidateFFTModel();

    void paintWithRenderer(LayerGeometryProvider *v, QPainter &paint, QRect rect) const;

    void paintDetailedScale(LayerGeometryProvider *v,
                            QPainter &paint, QRect rect) const;
    void paintDetailedScalePhase(LayerGeometryProvider *v,
                                 QPainter &paint, QRect rect) const;
    
    virtual void updateMeasureRectYCoords(LayerGeometryProvider *v,
                                          const MeasureRect &r) const;
    virtual void setMeasureRectYCoord(LayerGeometryProvider *v,
                                      MeasureRect &r, bool start, int y) const;
};

#endif
