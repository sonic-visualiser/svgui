/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#include "SpectrogramLayer.h"

#include "base/View.h"
#include "base/Profiler.h"
#include "base/AudioLevel.h"
#include "base/Window.h"
#include "base/Pitch.h"

#include "dsp/maths/MathUtilities.h"

#include <QPainter>
#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QTimer>

#include <iostream>

#include <cassert>
#include <cmath>

//#define DEBUG_SPECTROGRAM_REPAINT 1


SpectrogramLayer::SpectrogramLayer(View *w, Configuration config) :
    Layer(w),
    m_model(0),
    m_channel(0),
    m_windowSize(1024),
    m_windowType(HanningWindow),
    m_windowOverlap(50),
    m_gain(1.0),
    m_threshold(0.0),
    m_colourRotation(0),
    m_minFrequency(0),
    m_maxFrequency(8000),
    m_colourScale(dBColourScale),
    m_colourScheme(DefaultColours),
    m_frequencyScale(LinearFrequencyScale),
    m_binDisplay(AllBins),
    m_normalizeColumns(false),
    m_cache(0),
    m_cacheInvalid(true),
    m_pixmapCache(0),
    m_pixmapCacheInvalid(true),
    m_fillThread(0),
    m_updateTimer(0),
    m_lastFillExtent(0),
    m_exiting(false)
{
    if (config == MelodicRange) {
	setWindowSize(8192);
	setWindowOverlap(90);
	setWindowType(ParzenWindow);
	setMaxFrequency(1000);
	setColourScale(LinearColourScale);
    } else if (config == MelodicPeaks) {
	setWindowSize(4096);
	setWindowOverlap(90);
	setWindowType(BlackmanWindow);
	setMaxFrequency(2000);
	setMinFrequency(40);
	setFrequencyScale(LogFrequencyScale);
	setColourScale(MeterColourScale);
	setBinDisplay(PeakFrequencies);
	setNormalizeColumns(true);
    }

    if (m_view) m_view->setLightBackground(false);
    m_view->addLayer(this);
}

SpectrogramLayer::~SpectrogramLayer()
{
    delete m_updateTimer;
    m_updateTimer = 0;

    m_exiting = true;
    m_condition.wakeAll();
    if (m_fillThread) m_fillThread->wait();
    delete m_fillThread;
    
    delete m_cache;
}

void
SpectrogramLayer::setModel(const DenseTimeValueModel *model)
{
    std::cerr << "SpectrogramLayer(" << this << "): setModel(" << model << ")" << std::endl;

    m_mutex.lock();
    m_cacheInvalid = true;
    m_model = model;
    delete m_cache; //!!! hang on, this isn't safe to do here is it? 
		    // we need some sort of guard against the fill
		    // thread trying to read the defunct model too.
		    // should we use a scavenger?
    m_cache = 0;
    m_mutex.unlock();

    if (!m_model || !m_model->isOK()) return;

    connect(m_model, SIGNAL(modelChanged()), this, SIGNAL(modelChanged()));
    connect(m_model, SIGNAL(modelChanged(size_t, size_t)),
	    this, SIGNAL(modelChanged(size_t, size_t)));

    connect(m_model, SIGNAL(completionChanged()),
	    this, SIGNAL(modelCompletionChanged()));

    connect(m_model, SIGNAL(modelChanged()), this, SLOT(cacheInvalid()));
    connect(m_model, SIGNAL(modelChanged(size_t, size_t)),
	    this, SLOT(cacheInvalid(size_t, size_t)));

    emit modelReplaced();
    fillCache();
}

Layer::PropertyList
SpectrogramLayer::getProperties() const
{
    PropertyList list;
    list.push_back(tr("Colour"));
    list.push_back(tr("Colour Scale"));
    list.push_back(tr("Window Type"));
    list.push_back(tr("Window Size"));
    list.push_back(tr("Window Overlap"));
    list.push_back(tr("Normalize"));
    list.push_back(tr("Bin Display"));
    list.push_back(tr("Threshold"));
    list.push_back(tr("Gain"));
    list.push_back(tr("Colour Rotation"));
    list.push_back(tr("Min Frequency"));
    list.push_back(tr("Max Frequency"));
    list.push_back(tr("Frequency Scale"));
    return list;
}

Layer::PropertyType
SpectrogramLayer::getPropertyType(const PropertyName &name) const
{
    if (name == tr("Gain")) return RangeProperty;
    if (name == tr("Colour Rotation")) return RangeProperty;
    if (name == tr("Normalize")) return ToggleProperty;
    if (name == tr("Threshold")) return RangeProperty;
    return ValueProperty;
}

QString
SpectrogramLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == tr("Window Size") ||
	name == tr("Window Type") ||
	name == tr("Window Overlap")) return tr("Window");
    if (name == tr("Colour") ||
	name == tr("Gain") ||
	name == tr("Threshold") ||
	name == tr("Colour Rotation")) return tr("Colour");
    if (name == tr("Normalize") ||
	name == tr("Bin Display") ||
	name == tr("Colour Scale")) return tr("Scale");
    if (name == tr("Max Frequency") ||
	name == tr("Min Frequency") ||
	name == tr("Frequency Scale") ||
	name == tr("Frequency Adjustment")) return tr("Range");
    return QString();
}

int
SpectrogramLayer::getPropertyRangeAndValue(const PropertyName &name,
					    int *min, int *max) const
{
    int deft = 0;

    int throwaway;
    if (!min) min = &throwaway;
    if (!max) max = &throwaway;

    if (name == tr("Gain")) {

	*min = -50;
	*max = 50;

	deft = lrint(log10(m_gain) * 20.0);
	if (deft < *min) deft = *min;
	if (deft > *max) deft = *max;

    } else if (name == tr("Threshold")) {

	*min = -50;
	*max = 0;

	deft = lrintf(AudioLevel::multiplier_to_dB(m_threshold));
	if (deft < *min) deft = *min;
	if (deft > *max) deft = *max;

    } else if (name == tr("Colour Rotation")) {

	*min = 0;
	*max = 256;

	deft = m_colourRotation;

    } else if (name == tr("Colour Scale")) {

	*min = 0;
	*max = 3;

	deft = (int)m_colourScale;

    } else if (name == tr("Colour")) {

	*min = 0;
	*max = 5;

	deft = (int)m_colourScheme;

    } else if (name == tr("Window Type")) {

	*min = 0;
	*max = 6;

	deft = (int)m_windowType;

    } else if (name == tr("Window Size")) {

	*min = 0;
	*max = 10;
	
	deft = 0;
	int ws = m_windowSize;
	while (ws > 32) { ws >>= 1; deft ++; }

    } else if (name == tr("Window Overlap")) {
	
	*min = 0;
	*max = 4;
	
	deft = m_windowOverlap / 25;
	if (m_windowOverlap == 90) deft = 4;
    
    } else if (name == tr("Min Frequency")) {

	*min = 0;
	*max = 9;

	switch (m_minFrequency) {
	case 0: default: deft = 0; break;
	case 10: deft = 1; break;
	case 20: deft = 2; break;
	case 40: deft = 3; break;
	case 100: deft = 4; break;
	case 250: deft = 5; break;
	case 500: deft = 6; break;
	case 1000: deft = 7; break;
	case 4000: deft = 8; break;
	case 10000: deft = 9; break;
	}
    
    } else if (name == tr("Max Frequency")) {

	*min = 0;
	*max = 9;

	switch (m_maxFrequency) {
	case 500: deft = 0; break;
	case 1000: deft = 1; break;
	case 1500: deft = 2; break;
	case 2000: deft = 3; break;
	case 4000: deft = 4; break;
	case 6000: deft = 5; break;
	case 8000: deft = 6; break;
	case 12000: deft = 7; break;
	case 16000: deft = 8; break;
	default: deft = 9; break;
	}

    } else if (name == tr("Frequency Scale")) {

	*min = 0;
	*max = 1;
	deft = (int)m_frequencyScale;

    } else if (name == tr("Bin Display")) {

	*min = 0;
	*max = 2;
	deft = (int)m_binDisplay;

    } else if (name == tr("Normalize")) {
	
	deft = (m_normalizeColumns ? 1 : 0);

    } else {
	deft = Layer::getPropertyRangeAndValue(name, min, max);
    }

    return deft;
}

QString
SpectrogramLayer::getPropertyValueLabel(const PropertyName &name,
					int value) const
{
    if (name == tr("Colour")) {
	switch (value) {
	default:
	case 0: return tr("Default");
	case 1: return tr("White on Black");
	case 2: return tr("Black on White");
	case 3: return tr("Red on Blue");
	case 4: return tr("Yellow on Black");
	case 5: return tr("Fruit Salad");
	}
    }
    if (name == tr("Colour Scale")) {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Meter");
	case 2: return tr("dB");
	case 3: return tr("Phase");
	}
    }
    if (name == tr("Window Type")) {
	switch ((WindowType)value) {
	default:
	case RectangularWindow: return tr("Rectangle");
	case BartlettWindow: return tr("Bartlett");
	case HammingWindow: return tr("Hamming");
	case HanningWindow: return tr("Hanning");
	case BlackmanWindow: return tr("Blackman");
	case GaussianWindow: return tr("Gaussian");
	case ParzenWindow: return tr("Parzen");
	}
    }
    if (name == tr("Window Size")) {
	return QString("%1").arg(32 << value);
    }
    if (name == tr("Window Overlap")) {
	switch (value) {
	default:
	case 0: return tr("0%");
	case 1: return tr("25%");
	case 2: return tr("50%");
	case 3: return tr("75%");
	case 4: return tr("90%");
	}
    }
    if (name == tr("Min Frequency")) {
	switch (value) {
	default:
	case 0: return tr("No min");
	case 1: return tr("10 Hz");
	case 2: return tr("20 Hz");
	case 3: return tr("40 Hz");
	case 4: return tr("100 Hz");
	case 5: return tr("250 Hz");
	case 6: return tr("500 Hz");
	case 7: return tr("1 KHz");
	case 8: return tr("4 KHz");
	case 9: return tr("10 KHz");
	}
    }
    if (name == tr("Max Frequency")) {
	switch (value) {
	default:
	case 0: return tr("500 Hz");
	case 1: return tr("1 KHz");
	case 2: return tr("1.5 KHz");
	case 3: return tr("2 KHz");
	case 4: return tr("4 KHz");
	case 5: return tr("6 KHz");
	case 6: return tr("8 KHz");
	case 7: return tr("12 KHz");
	case 8: return tr("16 KHz");
	case 9: return tr("No max");
	}
    }
    if (name == tr("Frequency Scale")) {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Log");
	}
    }
    if (name == tr("Bin Display")) {
	switch (value) {
	default:
	case 0: return tr("All Bins");
	case 1: return tr("Peak Bins");
	case 2: return tr("Frequencies");
	}
    }
    return tr("<unknown>");
}

void
SpectrogramLayer::setProperty(const PropertyName &name, int value)
{
    if (name == tr("Gain")) {
	setGain(pow(10, float(value)/20.0));
    } else if (name == tr("Threshold")) {
	if (value == -50) setThreshold(0.0);
	else setThreshold(AudioLevel::dB_to_multiplier(value));
    } else if (name == tr("Colour Rotation")) {
	setColourRotation(value);
    } else if (name == tr("Colour")) {
	if (m_view) m_view->setLightBackground(value == 2);
	switch (value) {
	default:
	case 0:	setColourScheme(DefaultColours); break;
	case 1: setColourScheme(WhiteOnBlack); break;
	case 2: setColourScheme(BlackOnWhite); break;
	case 3: setColourScheme(RedOnBlue); break;
	case 4: setColourScheme(YellowOnBlack); break;
	case 5: setColourScheme(Rainbow); break;
	}
    } else if (name == tr("Window Type")) {
	setWindowType(WindowType(value));
    } else if (name == tr("Window Size")) {
	setWindowSize(32 << value);
    } else if (name == tr("Window Overlap")) {
	if (value == 4) setWindowOverlap(90);
	else setWindowOverlap(25 * value);
    } else if (name == tr("Min Frequency")) {
	switch (value) {
	default:
	case 0: setMinFrequency(0); break;
	case 1: setMinFrequency(10); break;
	case 2: setMinFrequency(20); break;
	case 3: setMinFrequency(40); break;
	case 4: setMinFrequency(100); break;
	case 5: setMinFrequency(250); break;
	case 6: setMinFrequency(500); break;
	case 7: setMinFrequency(1000); break;
	case 8: setMinFrequency(4000); break;
	case 9: setMinFrequency(10000); break;
	}
    } else if (name == tr("Max Frequency")) {
	switch (value) {
	case 0: setMaxFrequency(500); break;
	case 1: setMaxFrequency(1000); break;
	case 2: setMaxFrequency(1500); break;
	case 3: setMaxFrequency(2000); break;
	case 4: setMaxFrequency(4000); break;
	case 5: setMaxFrequency(6000); break;
	case 6: setMaxFrequency(8000); break;
	case 7: setMaxFrequency(12000); break;
	case 8: setMaxFrequency(16000); break;
	default:
	case 9: setMaxFrequency(0); break;
	}
    } else if (name == tr("Colour Scale")) {
	switch (value) {
	default:
	case 0: setColourScale(LinearColourScale); break;
	case 1: setColourScale(MeterColourScale); break;
	case 2: setColourScale(dBColourScale); break;
	case 3: setColourScale(PhaseColourScale); break;
	}
    } else if (name == tr("Frequency Scale")) {
	switch (value) {
	default:
	case 0: setFrequencyScale(LinearFrequencyScale); break;
	case 1: setFrequencyScale(LogFrequencyScale); break;
	}
    } else if (name == tr("Bin Display")) {
	switch (value) {
	default:
	case 0: setBinDisplay(AllBins); break;
	case 1: setBinDisplay(PeakBins); break;
	case 2: setBinDisplay(PeakFrequencies); break;
	}
    } else if (name == "Normalize") {
	setNormalizeColumns(value ? true : false);
    }
}

void
SpectrogramLayer::setChannel(int ch)
{
    if (m_channel == ch) return;

    m_mutex.lock();
    m_cacheInvalid = true;
    m_pixmapCacheInvalid = true;
    
    m_channel = ch;

    m_mutex.unlock();

    emit layerParametersChanged();

    fillCache();
}

int
SpectrogramLayer::getChannel() const
{
    return m_channel;
}

void
SpectrogramLayer::setWindowSize(size_t ws)
{
    if (m_windowSize == ws) return;

    m_mutex.lock();
    m_cacheInvalid = true;
    m_pixmapCacheInvalid = true;
    
    m_windowSize = ws;
    
    m_mutex.unlock();

    emit layerParametersChanged();

    fillCache();
}

size_t
SpectrogramLayer::getWindowSize() const
{
    return m_windowSize;
}

void
SpectrogramLayer::setWindowOverlap(size_t wi)
{
    if (m_windowOverlap == wi) return;

    m_mutex.lock();
    m_cacheInvalid = true;
    m_pixmapCacheInvalid = true;
    
    m_windowOverlap = wi;
    
    m_mutex.unlock();

    emit layerParametersChanged();

    fillCache();
}

size_t
SpectrogramLayer::getWindowOverlap() const
{
    return m_windowOverlap;
}

void
SpectrogramLayer::setWindowType(WindowType w)
{
    if (m_windowType == w) return;

    m_mutex.lock();
    m_cacheInvalid = true;
    m_pixmapCacheInvalid = true;
    
    m_windowType = w;
    
    m_mutex.unlock();

    emit layerParametersChanged();

    fillCache();
}

WindowType
SpectrogramLayer::getWindowType() const
{
    return m_windowType;
}

void
SpectrogramLayer::setGain(float gain)
{
    if (m_gain == gain) return;

    m_mutex.lock();
    m_pixmapCacheInvalid = true;
    
    m_gain = gain;
    
    m_mutex.unlock();

    emit layerParametersChanged();

    fillCache();
}

float
SpectrogramLayer::getGain() const
{
    return m_gain;
}

void
SpectrogramLayer::setThreshold(float threshold)
{
    if (m_threshold == threshold) return;

    m_mutex.lock();
    m_pixmapCacheInvalid = true;
    
    m_threshold = threshold;
    
    m_mutex.unlock();

    emit layerParametersChanged();

    fillCache();
}

float
SpectrogramLayer::getThreshold() const
{
    return m_threshold;
}

void
SpectrogramLayer::setMinFrequency(size_t mf)
{
    if (m_minFrequency == mf) return;

    m_mutex.lock();
    m_pixmapCacheInvalid = true;
    
    m_minFrequency = mf;
    
    m_mutex.unlock();

    emit layerParametersChanged();
}

size_t
SpectrogramLayer::getMinFrequency() const
{
    return m_minFrequency;
}

void
SpectrogramLayer::setMaxFrequency(size_t mf)
{
    if (m_maxFrequency == mf) return;

    m_mutex.lock();
    m_pixmapCacheInvalid = true;
    
    m_maxFrequency = mf;
    
    m_mutex.unlock();

    emit layerParametersChanged();
}

size_t
SpectrogramLayer::getMaxFrequency() const
{
    return m_maxFrequency;
}

void
SpectrogramLayer::setColourRotation(int r)
{
    m_mutex.lock();
    m_pixmapCacheInvalid = true;

    if (r < 0) r = 0;
    if (r > 256) r = 256;
    int distance = r - m_colourRotation;

    if (distance != 0) {
	rotateCacheColourmap(-distance);
	m_colourRotation = r;
    }
    
    m_mutex.unlock();

    emit layerParametersChanged();
}

void
SpectrogramLayer::setColourScale(ColourScale colourScale)
{
    if (m_colourScale == colourScale) return;

    m_mutex.lock();
    m_pixmapCacheInvalid = true;
    
    m_colourScale = colourScale;
    
    m_mutex.unlock();
    fillCache();

    emit layerParametersChanged();
}

SpectrogramLayer::ColourScale
SpectrogramLayer::getColourScale() const
{
    return m_colourScale;
}

void
SpectrogramLayer::setColourScheme(ColourScheme scheme)
{
    if (m_colourScheme == scheme) return;

    m_mutex.lock();
    m_pixmapCacheInvalid = true;
    
    m_colourScheme = scheme;
    setCacheColourmap();

    m_mutex.unlock();

    emit layerParametersChanged();
}

SpectrogramLayer::ColourScheme
SpectrogramLayer::getColourScheme() const
{
    return m_colourScheme;
}

void
SpectrogramLayer::setFrequencyScale(FrequencyScale frequencyScale)
{
    if (m_frequencyScale == frequencyScale) return;

    m_mutex.lock();

    m_pixmapCacheInvalid = true;
    
    m_frequencyScale = frequencyScale;
    
    m_mutex.unlock();

    emit layerParametersChanged();
}

SpectrogramLayer::FrequencyScale
SpectrogramLayer::getFrequencyScale() const
{
    return m_frequencyScale;
}

void
SpectrogramLayer::setBinDisplay(BinDisplay binDisplay)
{
    if (m_binDisplay == binDisplay) return;

    m_mutex.lock();

    m_pixmapCacheInvalid = true;
    
    m_binDisplay = binDisplay;
    
    m_mutex.unlock();

    fillCache();

    emit layerParametersChanged();
}

SpectrogramLayer::BinDisplay
SpectrogramLayer::getBinDisplay() const
{
    return m_binDisplay;
}

void
SpectrogramLayer::setNormalizeColumns(bool n)
{
    if (m_normalizeColumns == n) return;
    m_mutex.lock();

    m_pixmapCacheInvalid = true;
    m_normalizeColumns = n;
    m_mutex.unlock();

    fillCache();
    emit layerParametersChanged();
}

bool
SpectrogramLayer::getNormalizeColumns() const
{
    return m_normalizeColumns;
}

void
SpectrogramLayer::setLayerDormant(bool dormant)
{
    if (dormant == m_dormant) return;

    if (dormant) {

	m_mutex.lock();
	m_dormant = true;

//	delete m_cache;
//	m_cache = 0;
	
	m_cacheInvalid = true;
	m_pixmapCacheInvalid = true;
	delete m_pixmapCache;
	m_pixmapCache = 0;
	
	m_mutex.unlock();

    } else {

	m_dormant = false;
	fillCache();
    }
}

void
SpectrogramLayer::cacheInvalid()
{
    m_cacheInvalid = true;
    m_pixmapCacheInvalid = true;
    fillCache();
}

void
SpectrogramLayer::cacheInvalid(size_t, size_t)
{
    // for now (or forever?)
    cacheInvalid();
}

void
SpectrogramLayer::fillCache()
{
#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << "SpectrogramLayer::fillCache" << std::endl;
#endif
    QMutexLocker locker(&m_mutex);

    m_lastFillExtent = 0;

    delete m_updateTimer;
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, SIGNAL(timeout()), this, SLOT(fillTimerTimedOut()));
    m_updateTimer->start(200);

    if (!m_fillThread) {
	std::cerr << "SpectrogramLayer::fillCache creating thread" << std::endl;
	m_fillThread = new CacheFillThread(*this);
	m_fillThread->start();
    }

    m_condition.wakeAll();
}   

void
SpectrogramLayer::fillTimerTimedOut()
{
    if (m_fillThread && m_model) {
	size_t fillExtent = m_fillThread->getFillExtent();
#ifdef DEBUG_SPECTROGRAM_REPAINT
	std::cerr << "SpectrogramLayer::fillTimerTimedOut: extent " << fillExtent << ", last " << m_lastFillExtent << ", total " << m_model->getEndFrame() << std::endl;
#endif
	if (fillExtent >= m_lastFillExtent) {
	    if (fillExtent >= m_model->getEndFrame() && m_lastFillExtent > 0) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
		std::cerr << "complete!" << std::endl;
#endif
		emit modelChanged();
		m_pixmapCacheInvalid = true;
		delete m_updateTimer;
		m_updateTimer = 0;
		m_lastFillExtent = 0;
	    } else if (fillExtent > m_lastFillExtent) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
		std::cerr << "SpectrogramLayer: emitting modelChanged("
			  << m_lastFillExtent << "," << fillExtent << ")" << std::endl;
#endif
		emit modelChanged(m_lastFillExtent, fillExtent);
		m_pixmapCacheInvalid = true;
		m_lastFillExtent = fillExtent;
	    }
	} else {
	    if (m_view) {
		size_t sf = 0;
		if (m_view->getStartFrame() > 0) sf = m_view->getStartFrame();
#ifdef DEBUG_SPECTROGRAM_REPAINT
		std::cerr << "SpectrogramLayer: going backwards, emitting modelChanged("
			  << sf << "," << m_view->getEndFrame() << ")" << std::endl;
#endif
		emit modelChanged(sf, m_view->getEndFrame());
		m_pixmapCacheInvalid = true;
	    }
	    m_lastFillExtent = fillExtent;
	}
    }
}

void
SpectrogramLayer::setCacheColourmap()
{
    if (m_cacheInvalid || !m_cache) return;

    int formerRotation = m_colourRotation;

    if (m_colourScheme == BlackOnWhite) {
	m_cache->setColour(NO_VALUE, Qt::white);
    } else {
	m_cache->setColour(NO_VALUE, Qt::black);
    }

    for (int pixel = 1; pixel < 256; ++pixel) {

	QColor colour;
	int hue, px;

	switch (m_colourScheme) {

	default:
	case DefaultColours:
	    hue = 256 - pixel;
	    colour = QColor::fromHsv(hue, pixel/2 + 128, pixel);
	    break;

	case WhiteOnBlack:
	    colour = QColor(pixel, pixel, pixel);
	    break;

	case BlackOnWhite:
	    colour = QColor(256-pixel, 256-pixel, 256-pixel);
	    break;

	case RedOnBlue:
	    colour = QColor(pixel > 128 ? (pixel - 128) * 2 : 0, 0,
			    pixel < 128 ? pixel : (256 - pixel));
	    break;

	case YellowOnBlack:
	    px = 256 - pixel;
	    colour = QColor(px < 64 ? 255 - px/2 :
			    px < 128 ? 224 - (px - 64) :
			    px < 192 ? 160 - (px - 128) * 3 / 2 :
			    256 - px,
			    pixel,
			    pixel / 4);
	    break;

	case Rainbow:
	    hue = 250 - pixel;
	    if (hue < 0) hue += 256;
	    colour = QColor::fromHsv(pixel, 255, 255);
	    break;
	}

	m_cache->setColour(pixel, colour);
    }

    m_colourRotation = 0;
    rotateCacheColourmap(m_colourRotation - formerRotation);
    m_colourRotation = formerRotation;
}

void
SpectrogramLayer::rotateCacheColourmap(int distance)
{
    if (!m_cache) return;

    QColor newPixels[256];

    newPixels[NO_VALUE] = m_cache->getColour(NO_VALUE);

    for (int pixel = 1; pixel < 256; ++pixel) {
	int target = pixel + distance;
	while (target < 1) target += 255;
	while (target > 255) target -= 255;
	newPixels[target] = m_cache->getColour(pixel);
    }

    for (int pixel = 0; pixel < 256; ++pixel) {
	m_cache->setColour(pixel, newPixels[pixel]);
    }
}

float
SpectrogramLayer::calculateFrequency(size_t bin,
				     size_t windowSize,
				     size_t windowIncrement,
				     size_t sampleRate,
				     float oldPhase,
				     float newPhase,
				     bool &steadyState)
{
    // At frequency f, phase shift of 2pi (one cycle) happens in 1/f sec.
    // At hopsize h and sample rate sr, one hop happens in h/sr sec.
    // At window size w, for bin b, f is b*sr/w.
    // thus 2pi phase shift happens in w/(b*sr) sec.
    // We need to know what phase shift we expect from h/sr sec.
    // -> 2pi * ((h/sr) / (w/(b*sr)))
    //  = 2pi * ((h * b * sr) / (w * sr))
    //  = 2pi * (h * b) / w.

    float frequency = (float(bin) * sampleRate) / windowSize;

    float expectedPhase =
	oldPhase + (2.0 * M_PI * bin * windowIncrement) / windowSize;

    float phaseError = MathUtilities::princarg(newPhase - expectedPhase);
	    
    if (fabs(phaseError) < (1.1 * (windowIncrement * M_PI) / windowSize)) {

	// The new frequency estimate based on the phase error
	// resulting from assuming the "native" frequency of this bin

	float newFrequency =
	    (sampleRate * (expectedPhase + phaseError - oldPhase)) /
	    (2 * M_PI * windowIncrement);

	steadyState = true;
	return newFrequency;
    }

    steadyState = false;
    return frequency;
}

void
SpectrogramLayer::fillCacheColumn(int column, double *input,
				  fftw_complex *output,
				  fftw_plan plan, 
				  size_t windowSize,
				  size_t increment,
				  const Window<double> &windower) const
{
    //!!! we _do_ need a lock for these references to the model
    // though, don't we?

    int startFrame = increment * column;
    int endFrame = startFrame + windowSize;

    startFrame -= int(windowSize - increment) / 2;
    endFrame   -= int(windowSize - increment) / 2;
    size_t pfx = 0;

    if (startFrame < 0) {
	pfx = size_t(-startFrame);
	for (size_t i = 0; i < pfx; ++i) {
	    input[i] = 0.0;
	}
    }

    size_t got = m_model->getValues(m_channel, startFrame + pfx,
				    endFrame, input + pfx);
    while (got + pfx < windowSize) {
	input[got + pfx] = 0.0;
	++got;
    }

    if (m_channel == -1) {
	int channels = m_model->getChannelCount();
	if (channels > 1) {
	    for (size_t i = 0; i < windowSize; ++i) {
		input[i] /= channels;
	    }
	}
    }

    windower.cut(input);

    for (size_t i = 0; i < windowSize/2; ++i) {
	double temp = input[i];
	input[i] = input[i + windowSize/2];
	input[i + windowSize/2] = temp;
    }
    
    fftw_execute(plan);

    double factor = 0.0;

    // Calculate magnitude and phase from real and imaginary in
    // output[i][0] and output[i][1] respectively, and store the phase
    // straight into cache and the magnitude back into output[i][0]
    // (because we'll need to know the normalization factor,
    // i.e. maximum magnitude in this column, before we can store it)

    for (size_t i = 0; i < windowSize/2; ++i) {

	double mag = sqrt(output[i][0] * output[i][0] +
			  output[i][1] * output[i][1]);
	mag /= windowSize / 2;

	if (mag > factor) factor = mag;

	double phase = atan2(output[i][1], output[i][0]);
	phase = MathUtilities::princarg(phase);

	output[i][0] = mag;
	m_cache->setPhaseAt(column, i, phase);
    }

    m_cache->setNormalizationFactor(column, factor);

    for (size_t i = 0; i < windowSize/2; ++i) {
	m_cache->setMagnitudeAt(column, i, output[i][0]);
    }
}

unsigned char
SpectrogramLayer::getDisplayValue(float input) const
{
    int value;

    switch (m_colourScale) {
	
    default:
    case LinearColourScale:
	value = int
	    (input * (m_normalizeColumns ? 1.0 : 50.0) * 255.0) + 1;
	break;
	
    case MeterColourScale:
	value = AudioLevel::multiplier_to_preview
	    (input * (m_normalizeColumns ? 1.0 : 50.0), 255) + 1;
	break;
	
    case dBColourScale:
	input = 20.0 * log10(input);
	input = (input + 80.0) / 80.0;
	if (input < 0.0) input = 0.0;
	if (input > 1.0) input = 1.0;
	value = int(input * 255.0) + 1;
	break;
	
    case PhaseColourScale:
	value = int((input * 127.0 / M_PI) + 128);
	break;
    }
    
    if (value > UCHAR_MAX) value = UCHAR_MAX;
    if (value < 0) value = 0;
    return value;
}

float
SpectrogramLayer::getInputForDisplayValue(unsigned char uc) const
{
    int value = uc;
    float input;

    switch (m_colourScale) {
	
    default:
    case LinearColourScale:
	input = float(value - 1) / 255.0 / (m_normalizeColumns ? 1 : 50);
	break;
    
    case MeterColourScale:
	input = AudioLevel::preview_to_multiplier(value - 1, 255)
	    / (m_normalizeColumns ? 1.0 : 50.0);
	break;

    case dBColourScale:
	input = float(value - 1) / 255.0;
	input = (input * 80.0) - 80.0;
	input = powf(10.0, input) / 20.0;
	value = int(input);
	break;

    case PhaseColourScale:
	input = float(value - 128) * M_PI / 127.0;
	break;
    }

    return input;
}


SpectrogramLayer::Cache::Cache() :
    m_width(0),
    m_height(0),
    m_magnitude(0),
    m_phase(0),
    m_factor(0)
{
}

SpectrogramLayer::Cache::~Cache()
{
    for (size_t i = 0; i < m_height; ++i) {
	if (m_magnitude && m_magnitude[i]) free(m_magnitude[i]);
	if (m_phase && m_phase[i]) free(m_phase[i]);
    }

    if (m_magnitude) free(m_magnitude);
    if (m_phase) free(m_phase);
    if (m_factor) free(m_factor);
}

void
SpectrogramLayer::Cache::resize(size_t width, size_t height)
{
    std::cerr << "SpectrogramLayer::Cache[" << this << "]::resize(" << width << "x" << height << ")" << std::endl;
    
    if (m_width == width && m_height == height) return;

    resize(m_magnitude, width, height);
    resize(m_phase, width, height);

    m_factor = (float *)realloc(m_factor, width * sizeof(float));

    m_width = width;
    m_height = height;

    std::cerr << "done, width = " << m_width << " height = " << m_height << std::endl;
}

void
SpectrogramLayer::Cache::resize(uint16_t **&array, size_t width, size_t height)
{
    for (size_t i = height; i < m_height; ++i) {
	free(array[i]);
    }

    if (height != m_height) {
	array = (uint16_t **)realloc(array, height * sizeof(uint16_t *));
	if (!array) throw std::bad_alloc();
	MUNLOCK(array, height * sizeof(uint16_t *));
    }

    for (size_t i = m_height; i < height; ++i) {
	array[i] = 0;
    }

    for (size_t i = 0; i < height; ++i) {
	array[i] = (uint16_t *)realloc(array[i], width * sizeof(uint16_t));
	if (!array[i]) throw std::bad_alloc();
	MUNLOCK(array[i], width * sizeof(uint16_t));
    }
}

void
SpectrogramLayer::Cache::reset()
{
    for (size_t x = 0; x < m_width; ++x) {
	for (size_t y = 0; y < m_height; ++y) {
	    m_magnitude[y][x] = 0;
	    m_phase[y][x] = 0;
	}
	m_factor[x] = 1.0;
    }
}	    

void
SpectrogramLayer::CacheFillThread::run()
{
//    std::cerr << "SpectrogramLayer::CacheFillThread::run" << std::endl;

    m_layer.m_mutex.lock();

    while (!m_layer.m_exiting) {

	bool interrupted = false;

//	std::cerr << "SpectrogramLayer::CacheFillThread::run in loop" << std::endl;

	if (m_layer.m_dormant) {

	    if (m_layer.m_cacheInvalid) {
		delete m_layer.m_cache;
		m_layer.m_cache = 0;
	    }

	} else if (m_layer.m_model && m_layer.m_cacheInvalid) {

//	    std::cerr << "SpectrogramLayer::CacheFillThread::run: something to do" << std::endl;

	    while (!m_layer.m_model->isReady()) {
		m_layer.m_condition.wait(&m_layer.m_mutex, 100);
	    }

	    m_layer.m_cacheInvalid = false;
	    m_fillExtent = 0;
	    m_fillCompletion = 0;

	    std::cerr << "SpectrogramLayer::CacheFillThread::run: model is ready" << std::endl;

	    size_t start = m_layer.m_model->getStartFrame();
	    size_t end = m_layer.m_model->getEndFrame();

	    std::cerr << "start = " << start << ", end = " << end << std::endl;

	    WindowType windowType = m_layer.m_windowType;
	    size_t windowSize = m_layer.m_windowSize;
	    size_t windowIncrement = m_layer.getWindowIncrement();

	    size_t visibleStart = start;
	    size_t visibleEnd = end;

	    if (m_layer.m_view) {
		if (m_layer.m_view->getStartFrame() < 0) {
		    visibleStart = 0;
		} else {
		    visibleStart = m_layer.m_view->getStartFrame();
		    visibleStart = (visibleStart / windowIncrement) *
			windowIncrement;
		}
		visibleEnd = m_layer.m_view->getEndFrame();
	    }

	    size_t width = (end - start) / windowIncrement + 1;
	    size_t height = windowSize / 2;

	    if (!m_layer.m_cache) {
		m_layer.m_cache = new Cache;
	    }

	    m_layer.m_cache->resize(width, height);
	    m_layer.setCacheColourmap();
	    m_layer.m_cache->reset();

	    // We don't need a lock when writing to or reading from
	    // the pixels in the cache.  We do need to ensure we have
	    // the width and height of the cache and the FFT
	    // parameters known before we unlock, in case they change
	    // in the model while we aren't holding a lock.  It's safe
	    // for us to continue to use the "old" values if that
	    // happens, because they will continue to match the
	    // dimensions of the actual cache (which we manage, not
	    // the model).
	    m_layer.m_mutex.unlock();

	    double *input = (double *)
		fftw_malloc(windowSize * sizeof(double));

	    fftw_complex *output = (fftw_complex *)
		fftw_malloc(windowSize * sizeof(fftw_complex));

	    fftw_plan plan = fftw_plan_dft_r2c_1d(windowSize, input,
						  output, FFTW_ESTIMATE);

	    Window<double> windower(windowType, windowSize);

	    if (!plan) {
		std::cerr << "WARNING: fftw_plan_dft_r2c_1d(" << windowSize << ") failed!" << std::endl;
		fftw_free(input);
		fftw_free(output);
		m_layer.m_mutex.lock();
		continue;
	    }

	    int counter = 0;
	    int updateAt = (end / windowIncrement) / 20;
	    if (updateAt < 100) updateAt = 100;

	    bool doVisibleFirst = (visibleStart != start && visibleEnd != end);

	    if (doVisibleFirst) {

		for (size_t f = visibleStart; f < visibleEnd; f += windowIncrement) {
	    
		    m_layer.fillCacheColumn(int((f - start) / windowIncrement),
					    input, output, plan,
					    windowSize, windowIncrement,
					    windower);

		    if (m_layer.m_cacheInvalid || m_layer.m_exiting) {
			interrupted = true;
			m_fillExtent = 0;
			break;
		    }

		    if (++counter == updateAt ||
			(f >= visibleEnd - 1 && f < visibleEnd + windowIncrement)) {
			if (f < end) m_fillExtent = f;
			m_fillCompletion = size_t(100 * fabsf(float(f - visibleStart) /
							      float(end - start)));
			counter = 0;
		    }
		}

		std::cerr << "SpectrogramLayer::CacheFillThread::run: visible bit done" << std::endl;
		m_layer.m_view->update();
	    }

	    if (!interrupted && doVisibleFirst) {
		
		for (size_t f = visibleEnd; f < end; f += windowIncrement) {
	    
		    m_layer.fillCacheColumn(int((f - start) / windowIncrement),
					    input, output, plan,
					    windowSize, windowIncrement,
					    windower);

		    if (m_layer.m_cacheInvalid || m_layer.m_exiting) {
			interrupted = true;
			m_fillExtent = 0;
			break;
		    }

		    if (++counter == updateAt) {
			m_fillExtent = f;
			m_fillCompletion = size_t(100 * fabsf(float(f - visibleStart) /
							      float(end - start)));
			counter = 0;
		    }
		}
	    }

	    if (!interrupted) {

		size_t remainingEnd = end;
		if (doVisibleFirst) {
		    remainingEnd = visibleStart;
		    if (remainingEnd > start) --remainingEnd;
		    else remainingEnd = start;
		}
		size_t baseCompletion = m_fillCompletion;

		for (size_t f = start; f < remainingEnd; f += windowIncrement) {

		    m_layer.fillCacheColumn(int((f - start) / windowIncrement),
					    input, output, plan,
					    windowSize, windowIncrement,
					    windower);

		    if (m_layer.m_cacheInvalid || m_layer.m_exiting) {
			interrupted = true;
			m_fillExtent = 0;
			break;
		    }
		    
		    if (++counter == updateAt ||
			(f >= visibleEnd - 1 && f < visibleEnd + windowIncrement)) {
			m_fillExtent = f;
			m_fillCompletion = baseCompletion +
			    size_t(100 * fabsf(float(f - start) /
					       float(end - start)));
			counter = 0;
		    }
		}
	    }

	    fftw_destroy_plan(plan);
	    fftw_free(output);
	    fftw_free(input);

	    if (!interrupted) {
		m_fillExtent = end;
		m_fillCompletion = 100;
	    }

	    m_layer.m_mutex.lock();
	}

	if (!interrupted) m_layer.m_condition.wait(&m_layer.m_mutex, 2000);
    }
}

float
SpectrogramLayer::getEffectiveMinFrequency() const
{
    int sr = m_model->getSampleRate();
    float minf = float(sr) / m_windowSize;

    if (m_minFrequency > 0.0) {
	size_t minbin = size_t((double(m_minFrequency) * m_windowSize) / sr + 0.01);
	if (minbin < 1) minbin = 1;
	minf = minbin * sr / m_windowSize;
    }

    return minf;
}

float
SpectrogramLayer::getEffectiveMaxFrequency() const
{
    int sr = m_model->getSampleRate();
    float maxf = float(sr) / 2;

    if (m_maxFrequency > 0.0) {
	size_t maxbin = size_t((double(m_maxFrequency) * m_windowSize) / sr + 0.1);
	if (maxbin > m_windowSize / 2) maxbin = m_windowSize / 2;
	maxf = maxbin * sr / m_windowSize;
    }

    return maxf;
}

bool
SpectrogramLayer::getYBinRange(int y, float &q0, float &q1) const
{
    int h = m_view->height();
    if (y < 0 || y >= h) return false;

    int sr = m_model->getSampleRate();
    float minf = getEffectiveMinFrequency();
    float maxf = getEffectiveMaxFrequency();

    bool logarithmic = (m_frequencyScale == LogFrequencyScale);

    q0 = m_view->getFrequencyForY(y, minf, maxf, logarithmic);
    q1 = m_view->getFrequencyForY(y - 1, minf, maxf, logarithmic);

    // Now map these on to actual bins

    int b0 = int((q0 * m_windowSize) / sr);
    int b1 = int((q1 * m_windowSize) / sr);
    
    //!!! this is supposed to return fractions-of-bins, as it were, hence the floats
    q0 = b0;
    q1 = b1;
    
//    q0 = (b0 * sr) / m_windowSize;
//    q1 = (b1 * sr) / m_windowSize;

    return true;
}
    
bool
SpectrogramLayer::getXBinRange(int x, float &s0, float &s1) const
{
    size_t modelStart = m_model->getStartFrame();
    size_t modelEnd = m_model->getEndFrame();

    // Each pixel column covers an exact range of sample frames:
    int f0 = getFrameForX(x) - modelStart;
    int f1 = getFrameForX(x + 1) - modelStart - 1;

    if (f1 < int(modelStart) || f0 > int(modelEnd)) {
	return false;
    }
      
    // And that range may be drawn from a possibly non-integral
    // range of spectrogram windows:

    size_t windowIncrement = getWindowIncrement();
    s0 = float(f0) / windowIncrement;
    s1 = float(f1) / windowIncrement;

    return true;
}
 
bool
SpectrogramLayer::getXBinSourceRange(int x, RealTime &min, RealTime &max) const
{
    float s0 = 0, s1 = 0;
    if (!getXBinRange(x, s0, s1)) return false;
    
    int s0i = int(s0 + 0.001);
    int s1i = int(s1);

    int windowIncrement = getWindowIncrement();
    int w0 = s0i * windowIncrement - (m_windowSize - windowIncrement)/2;
    int w1 = s1i * windowIncrement + windowIncrement +
	(m_windowSize - windowIncrement)/2 - 1;
    
    min = RealTime::frame2RealTime(w0, m_model->getSampleRate());
    max = RealTime::frame2RealTime(w1, m_model->getSampleRate());
    return true;
}

bool
SpectrogramLayer::getYBinSourceRange(int y, float &freqMin, float &freqMax)
const
{
    float q0 = 0, q1 = 0;
    if (!getYBinRange(y, q0, q1)) return false;

    int q0i = int(q0 + 0.001);
    int q1i = int(q1);

    int sr = m_model->getSampleRate();

    for (int q = q0i; q <= q1i; ++q) {
	int binfreq = (sr * q) / m_windowSize;
	if (q == q0i) freqMin = binfreq;
	if (q == q1i) freqMax = binfreq;
    }
    return true;
}

bool
SpectrogramLayer::getAdjustedYBinSourceRange(int x, int y,
					     float &freqMin, float &freqMax,
					     float &adjFreqMin, float &adjFreqMax)
const
{
    float s0 = 0, s1 = 0;
    if (!getXBinRange(x, s0, s1)) return false;

    float q0 = 0, q1 = 0;
    if (!getYBinRange(y, q0, q1)) return false;

    int s0i = int(s0 + 0.001);
    int s1i = int(s1);

    int q0i = int(q0 + 0.001);
    int q1i = int(q1);

    int sr = m_model->getSampleRate();

    size_t windowSize = m_windowSize;
    size_t windowIncrement = getWindowIncrement();

    bool haveAdj = false;

    bool peaksOnly = (m_binDisplay == PeakBins ||
		      m_binDisplay == PeakFrequencies);

    for (int q = q0i; q <= q1i; ++q) {

	for (int s = s0i; s <= s1i; ++s) {

	    float binfreq = (sr * q) / m_windowSize;
	    if (q == q0i) freqMin = binfreq;
	    if (q == q1i) freqMax = binfreq;

	    if (!m_cache || m_cacheInvalid) break; //!!! lock?

	    if (peaksOnly && !m_cache->isLocalPeak(s, q)) continue;

	    if (!m_cache->isOverThreshold(s, q, m_threshold)) continue;

	    float freq = binfreq;
	    bool steady = false;
	    
	    if (s < int(m_cache->getWidth()) - 1) {

		freq = calculateFrequency(q, 
					  windowSize,
					  windowIncrement,
					  sr, 
					  m_cache->getPhaseAt(s, q),
					  m_cache->getPhaseAt(s+1, q),
					  steady);
	    
		if (!haveAdj || freq < adjFreqMin) adjFreqMin = freq;
		if (!haveAdj || freq > adjFreqMax) adjFreqMax = freq;

		haveAdj = true;
	    }
	}
    }

    if (!haveAdj) {
	adjFreqMin = adjFreqMax = 0.0;
    }

    return haveAdj;
}
    
bool
SpectrogramLayer::getXYBinSourceRange(int x, int y,
				      float &min, float &max,
				      float &phaseMin, float &phaseMax) const
{
    float q0 = 0, q1 = 0;
    if (!getYBinRange(y, q0, q1)) return false;

    float s0 = 0, s1 = 0;
    if (!getXBinRange(x, s0, s1)) return false;
    
    int q0i = int(q0 + 0.001);
    int q1i = int(q1);

    int s0i = int(s0 + 0.001);
    int s1i = int(s1);

    bool rv = false;

    if (m_mutex.tryLock()) {
	if (m_cache && !m_cacheInvalid) {

	    int cw = m_cache->getWidth();
	    int ch = m_cache->getHeight();

	    min = 0.0;
	    max = 0.0;
	    phaseMin = 0.0;
	    phaseMax = 0.0;
	    bool have = false;

	    for (int q = q0i; q <= q1i; ++q) {
		for (int s = s0i; s <= s1i; ++s) {
		    if (s >= 0 && q >= 0 && s < cw && q < ch) {

			float value;

			value = m_cache->getPhaseAt(s, q);
			if (!have || value < phaseMin) { phaseMin = value; }
			if (!have || value > phaseMax) { phaseMax = value; }

			value = m_cache->getMagnitudeAt(s, q);
			if (!have || value < min) { min = value; }
			if (!have || value > max) { max = value; }

			have = true;
		    }	
		}
	    }

	    if (have) {
		rv = true;
	    }
	}

	m_mutex.unlock();
    }

    return rv;
}
   
void
SpectrogramLayer::paint(QPainter &paint, QRect rect) const
{
//    Profiler profiler("SpectrogramLayer::paint", true);
#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << "SpectrogramLayer::paint(): m_model is " << m_model << ", zoom level is " << m_view->getZoomLevel() << ", m_updateTimer " << m_updateTimer << ", pixmap cache invalid " << m_pixmapCacheInvalid << std::endl;
#endif

    if (!m_model || !m_model->isOK() || !m_model->isReady()) {
	return;
    }

    if (m_dormant) {
	std::cerr << "SpectrogramLayer::paint(): Layer is dormant" << std::endl;
	return;
    }

#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << "SpectrogramLayer::paint(): About to lock" << std::endl;
#endif

    m_mutex.lock();

#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << "SpectrogramLayer::paint(): locked" << std::endl;
#endif

    if (m_cacheInvalid) { // lock the mutex before checking this
	m_mutex.unlock();
#ifdef DEBUG_SPECTROGRAM_REPAINT
	std::cerr << "SpectrogramLayer::paint(): Cache invalid, returning" << std::endl;
#endif
	return;
    }

    bool stillCacheing = (m_updateTimer != 0);

#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << "SpectrogramLayer::paint(): Still cacheing = " << stillCacheing << std::endl;
#endif

    long startFrame = m_view->getStartFrame();
    int zoomLevel = m_view->getZoomLevel();

    int x0 = 0;
    int x1 = m_view->width();
    int y0 = 0;
    int y1 = m_view->height();

    bool recreateWholePixmapCache = true;

    if (!m_pixmapCacheInvalid) {

	//!!! This cache may have been obsoleted entirely by the
	//scrolling cache in View.  Perhaps experiment with
	//removing it and see if it makes things even quicker (or else
	//make it optional)

	if (int(m_pixmapCacheZoomLevel) == zoomLevel &&
	    m_pixmapCache->width() == m_view->width() &&
	    m_pixmapCache->height() == m_view->height()) {

	    if (getXForFrame(m_pixmapCacheStartFrame) ==
		getXForFrame(startFrame)) {
	    
#ifdef DEBUG_SPECTROGRAM_REPAINT
		std::cerr << "SpectrogramLayer: pixmap cache good" << std::endl;
#endif

		m_mutex.unlock();
		paint.drawPixmap(rect, *m_pixmapCache, rect);
		return;

	    } else {

#ifdef DEBUG_SPECTROGRAM_REPAINT
		std::cerr << "SpectrogramLayer: pixmap cache partially OK" << std::endl;
#endif

		recreateWholePixmapCache = false;

		int dx = getXForFrame(m_pixmapCacheStartFrame) -
		         getXForFrame(startFrame);

#ifdef DEBUG_SPECTROGRAM_REPAINT
		std::cerr << "SpectrogramLayer: dx = " << dx << " (pixmap cache " << m_pixmapCache->width() << "x" << m_pixmapCache->height() << ")" << std::endl;
#endif

		if (dx > -m_pixmapCache->width() && dx < m_pixmapCache->width()) {

#if defined(Q_WS_WIN32) || defined(Q_WS_MAC)
		    // Copying a pixmap to itself doesn't work
		    // properly on Windows or Mac (it only works when
		    // moving in one direction).

		    //!!! Need a utility function for this

		    static QPixmap *tmpPixmap = 0;
		    if (!tmpPixmap ||
			tmpPixmap->width() != m_pixmapCache->width() ||
			tmpPixmap->height() != m_pixmapCache->height()) {
			delete tmpPixmap;
			tmpPixmap = new QPixmap(m_pixmapCache->width(),
						m_pixmapCache->height());
		    }
		    QPainter cachePainter;
		    cachePainter.begin(tmpPixmap);
		    cachePainter.drawPixmap(0, 0, *m_pixmapCache);
		    cachePainter.end();
		    cachePainter.begin(m_pixmapCache);
		    cachePainter.drawPixmap(dx, 0, *tmpPixmap);
		    cachePainter.end();
#else
		    QPainter cachePainter(m_pixmapCache);
		    cachePainter.drawPixmap(dx, 0, *m_pixmapCache);
		    cachePainter.end();
#endif

		    paint.drawPixmap(rect, *m_pixmapCache, rect);

		    if (dx < 0) {
			x0 = m_pixmapCache->width() + dx;
			x1 = m_pixmapCache->width();
		    } else {
			x0 = 0;
			x1 = dx;
		    }
		}
	    }
	} else {
#ifdef DEBUG_SPECTROGRAM_REPAINT
	    std::cerr << "SpectrogramLayer: pixmap cache useless" << std::endl;
#endif
	}
    }

    if (stillCacheing) {
	x0 = rect.left();
	x1 = rect.right() + 1;
	y0 = rect.top();
	y1 = rect.bottom() + 1;
    }

    int w = x1 - x0;
    int h = y1 - y0;

//    std::cerr << "x0 " << x0 << ", x1 " << x1 << ", w " << w << ", h " << h << std::endl;

    QImage scaled(w, h, QImage::Format_RGB32);
    scaled.fill(m_cache->getColour(0).rgb());

    float ymag[h];
    float ydiv[h];

    int sr = m_model->getSampleRate();
    
    size_t bins = m_windowSize / 2;
    if (m_maxFrequency > 0) {
	bins = int((double(m_maxFrequency) * m_windowSize) / sr + 0.1);
	if (bins > m_windowSize / 2) bins = m_windowSize / 2;
    }
	
    size_t minbin = 1;
    if (m_minFrequency > 0) {
	minbin = int((double(m_minFrequency) * m_windowSize) / sr + 0.1);
	if (minbin < 1) minbin = 1;
	if (minbin >= bins) minbin = bins - 1;
    }

    float minFreq = (float(minbin) * sr) / m_windowSize;
    float maxFreq = (float(bins) * sr) / m_windowSize;

    size_t increment = getWindowIncrement();
    
    bool logarithmic = (m_frequencyScale == LogFrequencyScale);

    m_mutex.unlock();

    for (int x = 0; x < w; ++x) {

	m_mutex.lock();
	if (m_cacheInvalid) {
	    m_mutex.unlock();
	    break;
	}

	for (int y = 0; y < h; ++y) {
	    ymag[y] = 0.0;
	    ydiv[y] = 0.0;
	}

	float s0 = 0, s1 = 0;

	if (!getXBinRange(x0 + x, s0, s1)) {
	    assert(x <= scaled.width());
	    m_mutex.unlock();
	    continue;
	}

	int s0i = int(s0 + 0.001);
	int s1i = int(s1);

	for (size_t q = minbin; q < bins; ++q) {

	    float f0 = (float(q) * sr) / m_windowSize;
	    float f1 = (float(q + 1) * sr) / m_windowSize;

	    float y0 = 0, y1 = 0;

	    if (m_binDisplay != PeakFrequencies ||
		s1i >= int(m_cache->getWidth())) {
		y0 = m_view->getYForFrequency(f1, minFreq, maxFreq, logarithmic);
		y1 = m_view->getYForFrequency(f0, minFreq, maxFreq, logarithmic);
	    }

	    for (int s = s0i; s <= s1i; ++s) {

		if (m_binDisplay == PeakBins ||
		    m_binDisplay == PeakFrequencies) {
		    if (!m_cache->isLocalPeak(s, q)) continue;
		}
		
		if (!m_cache->isOverThreshold(s, q, m_threshold)) continue;

		float sprop = 1.0;
		if (s == s0i) sprop *= (s + 1) - s0;
		if (s == s1i) sprop *= s1 - s;
 
		if (m_binDisplay == PeakFrequencies &&
		    s < int(m_cache->getWidth()) - 1) {

		    bool steady = false;
		    f0 = f1 = calculateFrequency(q,
						 m_windowSize,
						 increment,
						 sr,
						 m_cache->getPhaseAt(s, q),
						 m_cache->getPhaseAt(s+1, q),
						 steady);

		    y0 = y1 = m_view->getYForFrequency
			(f0, minFreq, maxFreq, logarithmic);
		}
		
		int y0i = int(y0 + 0.001);
		int y1i = int(y1);

		for (int y = y0i; y <= y1i; ++y) {
		    
		    if (y < 0 || y >= h) continue;

		    float yprop = sprop;
		    if (y == y0i) yprop *= (y + 1) - y0;
		    if (y == y1i) yprop *= y1 - y;

		    float value;

		    if (m_colourScale == PhaseColourScale) {
			value = m_cache->getPhaseAt(s, q);
		    } else if (m_normalizeColumns) {
			value = m_cache->getNormalizedMagnitudeAt(s, q) * m_gain;
		    } else {
			value = m_cache->getMagnitudeAt(s, q) * m_gain;
		    }

		    ymag[y] += yprop * value;
		    ydiv[y] += yprop;
		}
	    }
	}

	for (int y = 0; y < h; ++y) {

	    if (ydiv[y] > 0.0) {

		unsigned char pixel = 0;

		float avg = ymag[y] / ydiv[y];
		pixel = getDisplayValue(avg);

		assert(x <= scaled.width());
		QColor c = m_cache->getColour(pixel);
		scaled.setPixel(x, y,
				qRgb(c.red(), c.green(), c.blue()));
	    }
	}

	m_mutex.unlock();
    }

    paint.drawImage(x0, y0, scaled);

    if (recreateWholePixmapCache) {
	delete m_pixmapCache;
	m_pixmapCache = new QPixmap(w, h);
    }

    QPainter cachePainter(m_pixmapCache);
    cachePainter.drawImage(x0, y0, scaled);
    cachePainter.end();
    
    m_pixmapCacheInvalid = false;
    m_pixmapCacheStartFrame = startFrame;
    m_pixmapCacheZoomLevel = zoomLevel;

#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << "SpectrogramLayer::paint() returning" << std::endl;
#endif
}

int
SpectrogramLayer::getCompletion() const
{
    if (m_updateTimer == 0) return 100;
    size_t completion = m_fillThread->getFillCompletion();
//    std::cerr << "SpectrogramLayer::getCompletion: completion = " << completion << std::endl;
    return completion;
}

bool
SpectrogramLayer::snapToFeatureFrame(int &frame,
				     size_t &resolution,
				     SnapType snap) const
{
    resolution = getWindowIncrement();
    int left = (frame / resolution) * resolution;
    int right = left + resolution;

    switch (snap) {
    case SnapLeft:  frame = left;  break;
    case SnapRight: frame = right; break;
    case SnapNearest:
    case SnapNeighbouring:
	if (frame - left > right - frame) frame = right;
	else frame = left;
	break;
    }
    
    return true;
} 

QString
SpectrogramLayer::getFeatureDescription(QPoint &pos) const
{
    int x = pos.x();
    int y = pos.y();

    if (!m_model || !m_model->isOK()) return "";

    float magMin = 0, magMax = 0;
    float phaseMin = 0, phaseMax = 0;
    float freqMin = 0, freqMax = 0;
    float adjFreqMin = 0, adjFreqMax = 0;
    QString pitchMin, pitchMax;
    RealTime rtMin, rtMax;

    bool haveValues = false;

    if (!getXBinSourceRange(x, rtMin, rtMax)) {
	return "";
    }
    if (getXYBinSourceRange(x, y, magMin, magMax, phaseMin, phaseMax)) {
	haveValues = true;
    }

    QString adjFreqText = "", adjPitchText = "";

    if (m_binDisplay == PeakFrequencies) {

	if (!getAdjustedYBinSourceRange(x, y, freqMin, freqMax,
					adjFreqMin, adjFreqMax)) {
	    return "";
	}

	if (adjFreqMin != adjFreqMax) {
	    adjFreqText = tr("Adjusted Frequency:\t%1 - %2 Hz\n")
		.arg(adjFreqMin).arg(adjFreqMax);
	} else {
	    adjFreqText = tr("Adjusted Frequency:\t%1 Hz\n")
		.arg(adjFreqMin);
	}

	QString pmin = Pitch::getPitchLabelForFrequency(adjFreqMin);
	QString pmax = Pitch::getPitchLabelForFrequency(adjFreqMax);

	if (pmin != pmax) {
	    adjPitchText = tr("Adjusted Pitch:\t%3 - %4\n").arg(pmin).arg(pmax);
	} else {
	    adjPitchText = tr("Adjusted Pitch:\t%2\n").arg(pmin);
	}

    } else {
	
	if (!getYBinSourceRange(y, freqMin, freqMax)) return "";
    }

    QString text;

    if (rtMin != rtMax) {
	text += tr("Time:\t%1 - %2\n")
	    .arg(rtMin.toText(true).c_str())
	    .arg(rtMax.toText(true).c_str());
    } else {
	text += tr("Time:\t%1\n")
	    .arg(rtMin.toText(true).c_str());
    }

    if (freqMin != freqMax) {
	text += tr("Frequency:\t%1 - %2 Hz\n%3Pitch:\t%4 - %5\n%6")
	    .arg(freqMin)
	    .arg(freqMax)
	    .arg(adjFreqText)
	    .arg(Pitch::getPitchLabelForFrequency(freqMin))
	    .arg(Pitch::getPitchLabelForFrequency(freqMax))
	    .arg(adjPitchText);
    } else {
	text += tr("Frequency:\t%1 Hz\n%2Pitch:\t%3\n%4")
	    .arg(freqMin)
	    .arg(adjFreqText)
	    .arg(Pitch::getPitchLabelForFrequency(freqMin))
	    .arg(adjPitchText);
    }	

    if (haveValues) {
	float dbMin = AudioLevel::multiplier_to_dB(magMin);
	float dbMax = AudioLevel::multiplier_to_dB(magMax);
	if (lrintf(dbMin) != lrintf(dbMax)) {
	    text += tr("dB:\t%1 - %2").arg(lrintf(dbMin)).arg(lrintf(dbMax));
	} else {
	    text += tr("dB:\t%1").arg(lrintf(dbMin));
	}
	if (phaseMin != phaseMax) {
	    text += tr("\nPhase:\t%1 - %2").arg(phaseMin).arg(phaseMax);
	} else {
	    text += tr("\nPhase:\t%1").arg(phaseMin);
	}
    }

    return text;
}

int
SpectrogramLayer::getColourScaleWidth(QPainter &paint) const
{
    int cw;

    switch (m_colourScale) {
    default:
    case LinearColourScale:
	cw = paint.fontMetrics().width(QString("0.00"));
	break;

    case MeterColourScale:
    case dBColourScale:
	cw = std::max(paint.fontMetrics().width(tr("-Inf")),
		      paint.fontMetrics().width(tr("-90")));
	break;

    case PhaseColourScale:
	cw = paint.fontMetrics().width(QString("-") + QChar(0x3c0));
	break;
    }

    return cw;
}

int
SpectrogramLayer::getVerticalScaleWidth(QPainter &paint) const
{
    if (!m_model || !m_model->isOK()) return 0;

    int cw = getColourScaleWidth(paint);

    int tw = paint.fontMetrics().width(QString("%1")
				     .arg(m_maxFrequency > 0 ?
					  m_maxFrequency - 1 :
					  m_model->getSampleRate() / 2));

    int fw = paint.fontMetrics().width(QString("43Hz"));
    if (tw < fw) tw = fw;

    int tickw = (m_frequencyScale == LogFrequencyScale ? 10 : 4);
    
    return cw + tickw + tw + 13;
}

void
SpectrogramLayer::paintVerticalScale(QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) {
	return;
    }

    int h = rect.height(), w = rect.width();

    int tickw = (m_frequencyScale == LogFrequencyScale ? 10 : 4);
    int pkw = (m_frequencyScale == LogFrequencyScale ? 10 : 0);

    size_t bins = m_windowSize / 2;
    int sr = m_model->getSampleRate();

    if (m_maxFrequency > 0) {
	bins = int((double(m_maxFrequency) * m_windowSize) / sr + 0.1);
	if (bins > m_windowSize / 2) bins = m_windowSize / 2;
    }

    int cw = getColourScaleWidth(paint);

    int py = -1;
    int textHeight = paint.fontMetrics().height();
    int toff = -textHeight + paint.fontMetrics().ascent() + 2;

    if (m_cache && !m_cacheInvalid && h > textHeight * 2 + 10) { //!!! lock?

	int ch = h - textHeight * 2 - 8;
	paint.drawRect(4, textHeight + 4, cw - 1, ch + 1);

	QString top, bottom;

	switch (m_colourScale) {
	default:
	case LinearColourScale:
	    top = (m_normalizeColumns ? "1.0" : "0.02");
	    bottom = (m_normalizeColumns ? "0.0" : "0.00");
	    break;

	case MeterColourScale:
	    top = (m_normalizeColumns ? QString("0") :
		   QString("%1").arg(int(AudioLevel::multiplier_to_dB(0.02))));
	    bottom = QString("%1").
		arg(int(AudioLevel::multiplier_to_dB
			(AudioLevel::preview_to_multiplier(0, 255))));
	    break;

	case dBColourScale:
	    top = "0";
	    bottom = "-80";
	    break;

	case PhaseColourScale:
	    top = QChar(0x3c0);
	    bottom = "-" + top;
	    break;
	}

	paint.drawText((cw + 6 - paint.fontMetrics().width(top)) / 2,
		       2 + textHeight + toff, top);

	paint.drawText((cw + 6 - paint.fontMetrics().width(bottom)) / 2,
		       h + toff - 3, bottom);

	paint.save();
	paint.setBrush(Qt::NoBrush);
	for (int i = 0; i < ch; ++i) {
	    int v = (i * 255) / ch + 1;
	    paint.setPen(m_cache->getColour(v));
	    paint.drawLine(5, 4 + textHeight + ch - i,
			   cw + 2, 4 + textHeight + ch - i);
	}
	paint.restore();
    }

    paint.drawLine(cw + 7, 0, cw + 7, h);

    int bin = -1;

    for (int y = 0; y < m_view->height(); ++y) {

	float q0, q1;
	if (!getYBinRange(m_view->height() - y, q0, q1)) continue;

	int vy;

	if (int(q0) > bin) {
	    vy = y;
	    bin = int(q0);
	} else {
	    continue;
	}

	int freq = (sr * bin) / m_windowSize;

	if (py >= 0 && (vy - py) < textHeight - 1) {
	    if (m_frequencyScale == LinearFrequencyScale) {
		paint.drawLine(w - tickw, h - vy, w, h - vy);
	    }
	    continue;
	}

	QString text = QString("%1").arg(freq);
	if (bin == 1) text = QString("%1Hz").arg(freq); // bin 0 is DC
	paint.drawLine(cw + 7, h - vy, w - pkw - 1, h - vy);

	if (h - vy - textHeight >= -2) {
	    int tx = w - 3 - paint.fontMetrics().width(text) - std::max(tickw, pkw);
	    paint.drawText(tx, h - vy + toff, text);
	}

	py = vy;
    }

    if (m_frequencyScale == LogFrequencyScale) {

	paint.drawLine(w - pkw - 1, 0, w - pkw - 1, h);

	int sr = m_model->getSampleRate();//!!! lock?
	float minf = getEffectiveMinFrequency();
	float maxf = getEffectiveMaxFrequency();

	int py = h;
	paint.setBrush(paint.pen().color());

	for (int i = 0; i < 128; ++i) {

	    float f = Pitch::getFrequencyForPitch(i);
	    int y = lrintf(m_view->getYForFrequency(f, minf, maxf, true));
	    int n = (i % 12);
	    if (n == 1 || n == 3 || n == 6 || n == 8 || n == 10) {
		// black notes
		paint.drawLine(w - pkw, y, w, y);
		int rh = ((py - y) / 4) * 2;
		if (rh < 2) rh = 2;
		paint.drawRect(w - pkw, y - (py-y)/4, pkw/2, rh);
	    } else if (n == 0 || n == 5) {
		// C, A
		if (py < h) {
		    paint.drawLine(w - pkw, (y + py) / 2, w, (y + py) / 2);
		}
	    }

	    py = y;
	}
    }
}

QString
SpectrogramLayer::toXmlString(QString indent, QString extraAttributes) const
{
    QString s;
    
    s += QString("channel=\"%1\" "
		 "windowSize=\"%2\" "
		 "windowType=\"%3\" "
		 "windowOverlap=\"%4\" "
		 "gain=\"%5\" "
		 "threshold=\"%6\" ")
	.arg(m_channel)
	.arg(m_windowSize)
	.arg(m_windowType)
	.arg(m_windowOverlap)
	.arg(m_gain)
	.arg(m_threshold);

    s += QString("minFrequency=\"%1\" "
		 "maxFrequency=\"%2\" "
		 "colourScale=\"%3\" "
		 "colourScheme=\"%4\" "
		 "colourRotation=\"%5\" "
		 "frequencyScale=\"%6\" "
		 "binDisplay=\"%7\" "
		 "normalizeColumns=\"%8\"")
	.arg(m_minFrequency)
	.arg(m_maxFrequency)
	.arg(m_colourScale)
	.arg(m_colourScheme)
	.arg(m_colourRotation)
	.arg(m_frequencyScale)
	.arg(m_binDisplay)
	.arg(m_normalizeColumns ? "true" : "false");

    return Layer::toXmlString(indent, extraAttributes + " " + s);
}

void
SpectrogramLayer::setProperties(const QXmlAttributes &attributes)
{
    bool ok = false;

    int channel = attributes.value("channel").toInt(&ok);
    if (ok) setChannel(channel);

    size_t windowSize = attributes.value("windowSize").toUInt(&ok);
    if (ok) setWindowSize(windowSize);

    WindowType windowType = (WindowType)
	attributes.value("windowType").toInt(&ok);
    if (ok) setWindowType(windowType);

    size_t windowOverlap = attributes.value("windowOverlap").toUInt(&ok);
    if (ok) setWindowOverlap(windowOverlap);

    float gain = attributes.value("gain").toFloat(&ok);
    if (ok) setGain(gain);

    float threshold = attributes.value("threshold").toFloat(&ok);
    if (ok) setThreshold(threshold);

    size_t minFrequency = attributes.value("minFrequency").toUInt(&ok);
    if (ok) setMinFrequency(minFrequency);

    size_t maxFrequency = attributes.value("maxFrequency").toUInt(&ok);
    if (ok) setMaxFrequency(maxFrequency);

    ColourScale colourScale = (ColourScale)
	attributes.value("colourScale").toInt(&ok);
    if (ok) setColourScale(colourScale);

    ColourScheme colourScheme = (ColourScheme)
	attributes.value("colourScheme").toInt(&ok);
    if (ok) setColourScheme(colourScheme);

    int colourRotation = attributes.value("colourRotation").toInt(&ok);
    if (ok) setColourRotation(colourRotation);

    FrequencyScale frequencyScale = (FrequencyScale)
	attributes.value("frequencyScale").toInt(&ok);
    if (ok) setFrequencyScale(frequencyScale);

    BinDisplay binDisplay = (BinDisplay)
	attributes.value("binDisplay").toInt(&ok);
    if (ok) setBinDisplay(binDisplay);

    bool normalizeColumns =
	(attributes.value("normalizeColumns").trimmed() == "true");
    setNormalizeColumns(normalizeColumns);
}
    

#ifdef INCLUDE_MOCFILES
#include "SpectrogramLayer.moc.cpp"
#endif

