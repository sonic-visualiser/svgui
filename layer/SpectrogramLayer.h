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

#ifndef _SPECTROGRAM_LAYER_H_
#define _SPECTROGRAM_LAYER_H_

#include "base/Layer.h"
#include "base/Window.h"
#include "base/RealTime.h"
#include "model/PowerOfSqrtTwoZoomConstraint.h"
#include "model/DenseTimeValueModel.h"

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <fftw3.h>

class View;
class QPainter;
class QImage;
class QPixmap;
class QTimer;
class FFTCacheBase;

/**
 * SpectrogramLayer represents waveform data (obtained from a
 * DenseTimeValueModel) in spectrogram form.
 */

class SpectrogramLayer : public Layer,
			 public PowerOfSqrtTwoZoomConstraint
{
    Q_OBJECT

public:
    enum Configuration { FullRangeDb, MelodicRange, MelodicPeaks };
    
    SpectrogramLayer(Configuration = FullRangeDb);
    ~SpectrogramLayer();

    virtual const ZoomConstraint *getZoomConstraint() const { return this; }
    virtual const Model *getModel() const { return m_model; }
    virtual void paint(View *v, QPainter &paint, QRect rect) const;

    virtual int getVerticalScaleWidth(View *v, QPainter &) const;
    virtual void paintVerticalScale(View *v, QPainter &paint, QRect rect) const;

    virtual bool getCrosshairExtents(View *, QPainter &, QPoint cursorPos,
                                     std::vector<QRect> &extents) const;
    virtual void paintCrosshairs(View *, QPainter &, QPoint) const;

    virtual QString getFeatureDescription(View *v, QPoint &) const;

    virtual bool snapToFeatureFrame(View *v, int &frame,
				    size_t &resolution,
				    SnapType snap) const;

    void setModel(const DenseTimeValueModel *model);

    virtual PropertyList getProperties() const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual QString getPropertyGroupName(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
					   int *min, int *max) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual void setProperty(const PropertyName &, int value);

    /**
     * Specify the channel to use from the source model.
     * A value of -1 means to mix all available channels.
     * The default is channel 0.
     */
    void setChannel(int);
    int getChannel() const;

    void setWindowSize(size_t);
    size_t getWindowSize() const;
    
    void setWindowOverlap(size_t percent);
    size_t getWindowOverlap() const;

    void setWindowType(WindowType type);
    WindowType getWindowType() const;

    /**
     * Set the gain multiplier for sample values in this view prior to
     * FFT calculation.
     *
     * The default is 1.0.
     */
    void setGain(float gain);
    float getGain() const;

    /**
     * Set the threshold for sample values to be shown in the FFT,
     * in voltage units.
     *
     * The default is 0.0.
     */
    void setThreshold(float threshold);
    float getThreshold() const;

    void setMinFrequency(size_t);
    size_t getMinFrequency() const;

    void setMaxFrequency(size_t); // 0 -> no maximum
    size_t getMaxFrequency() const;

    enum ColourScale {
	LinearColourScale,
	MeterColourScale,
	dBColourScale,
	PhaseColourScale
    };

    /**
     * Specify the scale for sample levels.  See WaveformLayer for
     * details of meter and dB scaling.  The default is dBColourScale.
     */
    void setColourScale(ColourScale);
    ColourScale getColourScale() const;

    enum FrequencyScale {
	LinearFrequencyScale,
	LogFrequencyScale
    };
    
    /**
     * Specify the scale for the y axis.
     */
    void setFrequencyScale(FrequencyScale);
    FrequencyScale getFrequencyScale() const;

    enum BinDisplay {
	AllBins,
	PeakBins,
	PeakFrequencies
    };
    
    /**
     * Specify the processing of frequency bins for the y axis.
     */
    void setBinDisplay(BinDisplay);
    BinDisplay getBinDisplay() const;

    void setNormalizeColumns(bool n);
    bool getNormalizeColumns() const;

    enum ColourScheme { DefaultColours, WhiteOnBlack, BlackOnWhite,
			RedOnBlue, YellowOnBlack, BlueOnBlack, Rainbow };

    void setColourScheme(ColourScheme scheme);
    ColourScheme getColourScheme() const;

    /**
     * Specify the colourmap rotation for the colour scale.
     */
    void setColourRotation(int);
    int getColourRotation() const;

    virtual VerticalPosition getPreferredFrameCountPosition() const {
	return PositionTop;
    }

    virtual bool isLayerOpaque() const { return true; }

    float getYForFrequency(View *v, float frequency) const;
    float getFrequencyForY(View *v, int y) const;

    virtual int getCompletion() const;

    virtual bool getValueExtents(float &min, float &max, QString &unit) const;

    virtual QString toXmlString(QString indent = "",
				QString extraAttributes = "") const;

    void setProperties(const QXmlAttributes &attributes);

    virtual void setLayerDormant(const View *v, bool dormant);

protected slots:
    void cacheInvalid();
    void cacheInvalid(size_t startFrame, size_t endFrame);

    void fillTimerTimedOut();

protected:
    const DenseTimeValueModel *m_model; // I do not own this
    
    int                 m_channel;
    size_t              m_windowSize;
    WindowType          m_windowType;
    size_t              m_windowOverlap;
    float               m_gain;
    float               m_threshold;
    int                 m_colourRotation;
    size_t              m_minFrequency;
    size_t              m_maxFrequency;
    ColourScale         m_colourScale;
    ColourScheme        m_colourScheme;
    QColor              m_crosshairColour;
    FrequencyScale      m_frequencyScale;
    BinDisplay          m_binDisplay;
    bool                m_normalizeColumns;

    enum { NO_VALUE = 0 }; // colour index for unused pixels

    class ColourMap
    {
    public:
        QColor getColour(unsigned char index) const {
            return m_colours[index];
        }
    
        void setColour(unsigned char index, QColor colour) {
            m_colours[index] = colour;
        }

    private:
        QColor m_colours[256];
    };

    ColourMap m_colourMap;
    FFTCacheBase *m_cache;
    FFTCacheBase *m_writeCache;
    bool m_cacheInvalid;

    class CacheFillThread : public QThread
    {
    public:
	CacheFillThread(SpectrogramLayer &layer) :
	    m_layer(layer), m_fillExtent(0) { }

	size_t getFillExtent() const { return m_fillExtent; }
	size_t getFillCompletion() const { return m_fillCompletion; }
	virtual void run();

    protected:
	SpectrogramLayer &m_layer;
	size_t m_fillExtent;
	size_t m_fillCompletion;
    };

    void fillCache();

    mutable QPixmap *m_pixmapCache;
    mutable bool m_pixmapCacheInvalid;
    mutable long m_pixmapCacheStartFrame;
    mutable size_t m_pixmapCacheZoomLevel;

    QWaitCondition m_condition;
    mutable QMutex m_mutex;

    CacheFillThread *m_fillThread;
    QTimer *m_updateTimer;
    mutable size_t m_candidateFillStartFrame;
    size_t m_lastFillExtent;
    bool m_exiting;

    void setCacheColourmap();
    void rotateCacheColourmap(int distance);

    void fillCacheColumn(int column,
			 double *inputBuffer,
			 fftw_complex *outputBuffer,
			 fftw_plan plan,
			 size_t windowSize,
			 size_t windowIncrement,
                         float *workbuffer,
			 const Window<double> &windower)
	const;

    static float calculateFrequency(size_t bin,
				    size_t windowSize,
				    size_t windowIncrement,
				    size_t sampleRate,
				    float previousPhase,
				    float currentPhase,
				    bool &steadyState);

    unsigned char getDisplayValue(float input) const;
    float getInputForDisplayValue(unsigned char uc) const;

    int getColourScaleWidth(QPainter &) const;

    float getEffectiveMinFrequency() const;
    float getEffectiveMaxFrequency() const;

    struct LayerRange {
	long   startFrame;
	int    zoomLevel;
	size_t modelStart;
	size_t modelEnd;
    };
    bool getXBinRange(View *v, int x, float &windowMin, float &windowMax) const;
    bool getYBinRange(View *v, int y, float &freqBinMin, float &freqBinMax) const;

    bool getYBinSourceRange(View *v, int y, float &freqMin, float &freqMax) const;
    bool getAdjustedYBinSourceRange(View *v, int x, int y,
				    float &freqMin, float &freqMax,
				    float &adjFreqMin, float &adjFreqMax) const;
    bool getXBinSourceRange(View *v, int x, RealTime &timeMin, RealTime &timeMax) const;
    bool getXYBinSourceRange(View *v, int x, int y, float &min, float &max,
			     float &phaseMin, float &phaseMax) const;

    size_t getWindowIncrement() const {
	return m_windowSize - m_windowSize * m_windowOverlap / 100;
    }
};

#endif
