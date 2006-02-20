/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#ifndef _SPECTROGRAM_LAYER_H_
#define _SPECTROGRAM_LAYER_H_

#include "base/Layer.h"
#include "base/Window.h"
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
class RealTime;

/**
 * SpectrogramLayer represents waveform data (obtained from a
 * DenseTimeValueModel) in spectrogram form.
 */

class SpectrogramLayer : public Layer,
			 public PowerOfSqrtTwoZoomConstraint
{
    Q_OBJECT

public:
    enum Configuration { FullRangeDb, MelodicRange };
    
    SpectrogramLayer(View *w, Configuration = FullRangeDb);
    ~SpectrogramLayer();

    virtual const ZoomConstraint *getZoomConstraint() const { return this; }
    virtual const Model *getModel() const { return m_model; }
    virtual void paint(QPainter &paint, QRect rect) const;

    virtual int getVerticalScaleWidth(QPainter &) const;
    virtual void paintVerticalScale(QPainter &paint, QRect rect) const;

    virtual QString getFeatureDescription(QPoint &) const;

    virtual bool snapToFeatureFrame(int &frame,
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

    void setMaxFrequency(size_t); // 0 -> no maximum
    size_t getMaxFrequency() const;

    enum ColourScale { LinearColourScale, MeterColourScale, dBColourScale,
		       PhaseColourScale };

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

    enum FrequencyAdjustment {
	RawFrequency,
	PhaseAdjustedFrequency,
	PhaseAdjustedPeaks
    };
    
    /**
     * Specify the processing of frequency bins for the y axis.
     */
    void setFrequencyAdjustment(FrequencyAdjustment);
    FrequencyAdjustment getFrequencyAdjustment() const;

    enum ColourScheme { DefaultColours, WhiteOnBlack, BlackOnWhite,
			RedOnBlue, YellowOnBlack, RedOnBlack };

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

    virtual int getCompletion() const;

    virtual QString toXmlString(QString indent = "",
				QString extraAttributes = "") const;

    void setProperties(const QXmlAttributes &attributes);

    virtual void setLayerDormant(bool dormant);

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
    int                 m_colourRotation;
    size_t              m_maxFrequency;
    ColourScale         m_colourScale;
    ColourScheme        m_colourScheme;
    FrequencyScale      m_frequencyScale;
    FrequencyAdjustment m_frequencyAdjustment;

    // A QImage would do just as well here, and we originally used
    // one: the problem is that we want to munlock() the memory it
    // uses, and it's much easier to do that if we control it.  This
    // cache is hardwired to an effective 8-bit colour mapped layout.
    class Cache {
    public:
	Cache(size_t width, size_t height);
	~Cache();

	size_t getWidth() const;
	size_t getHeight() const;

	void resize(size_t width, size_t height);
	
	unsigned char getValueAt(size_t x, size_t y) const;
	void setValueAt(size_t x, size_t y, unsigned char value);

	QColor getColour(unsigned char index) const;
	void setColour(unsigned char index, QColor colour);

	void fill(unsigned char value);

    protected:
	size_t m_width;
	size_t m_height;
	unsigned char *m_values;
	QColor m_colours[256];
    };
    
    Cache *m_cache;
    Cache *m_phaseAdjustCache;
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
    size_t m_lastFillExtent;
    bool m_cachedInitialVisibleArea;
    bool m_exiting;

    void setCacheColourmap();
    void rotateCacheColourmap(int distance);

    bool fillCacheColumn(int column,
			 double *inputBuffer,
			 fftw_complex *outputBuffer,
			 fftw_plan plan,
			 size_t windowSize,
			 size_t windowIncrement,
			 const Window<double> &windower,
			 bool resetStoredPhase)
	const;

    bool getYBinRange(int y, float &freqBinMin, float &freqBinMax) const;

    struct LayerRange {
	long   startFrame;
	int    zoomLevel;
	size_t modelStart;
	size_t modelEnd;
    };
    bool getXBinRange(int x, float &windowMin, float &windowMax) const;

    bool getYBinSourceRange(int y, float &freqMin, float &freqMax) const;
    bool getAdjustedYBinSourceRange(int x, int y,
				    float &freqMin, float &freqMax,
				    float &adjFreqMin, float &adjFreqMax) const;
    bool getXBinSourceRange(int x, RealTime &timeMin, RealTime &timeMax) const;
    bool getXYBinSourceRange(int x, int y, float &dbMin, float &dbMax) const;

    size_t getWindowIncrement() const {
	return m_windowSize - m_windowSize * m_windowOverlap / 100;
    }
};

#endif
