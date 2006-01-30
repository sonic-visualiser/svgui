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
    m_colourRotation(0),
    m_maxFrequency(8000),
    m_colourScale(dBColourScale),
    m_colourScheme(DefaultColours),
    m_frequencyScale(LinearFrequencyScale),
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
    m_mutex.lock();
    m_model = model;
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
    list.push_back(tr("Gain"));
    list.push_back(tr("Colour Rotation"));
    list.push_back(tr("Max Frequency"));
    list.push_back(tr("Frequency Scale"));
    return list;
}

Layer::PropertyType
SpectrogramLayer::getPropertyType(const PropertyName &name) const
{
    if (name == tr("Gain")) return RangeProperty;
    if (name == tr("Colour Rotation")) return RangeProperty;
    return ValueProperty;
}

QString
SpectrogramLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == tr("Window Size") ||
	name == tr("Window Overlap")) return tr("Window");
    if (name == tr("Gain") ||
	name == tr("Colour Rotation") ||
	name == tr("Colour Scale")) return tr("Scale");
    if (name == tr("Max Frequency") ||
	name == tr("Frequency Scale")) return tr("Frequency");
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
	case 5: return tr("Red on Black");
	}
    }
    if (name == tr("Colour Scale")) {
	switch (value) {
	default:
	case 0: return tr("Level Linear");
	case 1: return tr("Level Meter");
	case 2: return tr("Level dB");
	case 3: return tr("Phase");
	}
    }
    if (name == tr("Window Type")) {
	switch ((WindowType)value) {
	default:
	case RectangularWindow: return tr("Rectangular");
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
	case 0: return tr("None");
	case 1: return tr("25 %");
	case 2: return tr("50 %");
	case 3: return tr("75 %");
	case 4: return tr("90 %");
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
	case 9: return tr("All");
	}
    }
    if (name == tr("Frequency Scale")) {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Log");
	}
    }
    return tr("<unknown>");
}

void
SpectrogramLayer::setProperty(const PropertyName &name, int value)
{
    if (name == tr("Gain")) {
	setGain(pow(10, float(value)/20.0));
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
	case 5: setColourScheme(RedOnBlack); break;
	}
    } else if (name == tr("Window Type")) {
	setWindowType(WindowType(value));
    } else if (name == tr("Window Size")) {
	setWindowSize(32 << value);
    } else if (name == tr("Window Overlap")) {
	if (value == 4) setWindowOverlap(90);
	else setWindowOverlap(25 * value);
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
    if (m_gain == gain) return; //!!! inadequate for floats!

    m_mutex.lock();
    m_cacheInvalid = true;
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
SpectrogramLayer::setMaxFrequency(size_t mf)
{
    if (m_maxFrequency == mf) return;

    m_mutex.lock();
    // don't need to invalidate main cache here
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
    // don't need to invalidate main cache here
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
    m_cacheInvalid = true;
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
    // don't need to invalidate main cache here
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
    // don't need to invalidate main cache here
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
SpectrogramLayer::cacheInvalid()
{
    m_cacheInvalid = true;
    m_pixmapCacheInvalid = true;
    m_cachedInitialVisibleArea = false;
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

    m_cache->setNumColors(256);
    
    m_cache->setColor(0, qRgb(255, 255, 255));

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

	case RedOnBlack:
	    colour = QColor::fromHsv(10, pixel, pixel);
	    break;
	}

	m_cache->setColor
	    (pixel, qRgb(colour.red(), colour.green(), colour.blue()));
    }

    m_colourRotation = 0;
    rotateCacheColourmap(m_colourRotation - formerRotation);
    m_colourRotation = formerRotation;
}

void
SpectrogramLayer::rotateCacheColourmap(int distance)
{
    if (!m_cache) return;

    QRgb newPixels[256];

    newPixels[0] = m_cache->color(0);

    for (int pixel = 1; pixel < 256; ++pixel) {
	int target = pixel + distance;
	while (target < 1) target += 255;
	while (target > 255) target -= 255;
	newPixels[target] = m_cache->color(pixel);
    }

    for (int pixel = 0; pixel < 256; ++pixel) {
	m_cache->setColor(pixel, newPixels[pixel]);
    }
}

bool
SpectrogramLayer::fillCacheColumn(int column, double *input,
				  fftw_complex *output,
				  fftw_plan plan, 
				  size_t windowSize,
				  size_t increment,
				  const Window<double> &windower,
				  bool lock) const
{
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

    if (m_gain != 1.0) {
	for (size_t i = 0; i < windowSize; ++i) {
	    input[i] *= m_gain;
	}
    }

    windower.cut(input);

    fftw_execute(plan);

    if (lock) m_mutex.lock();
    bool interrupted = false;

    for (size_t i = 0; i < windowSize / 2; ++i) {

	int value = 0;

	if (m_colourScale == PhaseColourScale) {

	    double phase = atan2(-output[i][1], output[i][0]);
	    value = int((phase * 128 / M_PI) + 128);

	} else {

	    double mag = sqrt(output[i][0] * output[i][0] +
			      output[i][1] * output[i][1]);
	    mag /= windowSize / 2;

	    switch (m_colourScale) {
		
	    default:
	    case LinearColourScale:
		value = int(mag * 50 * 256);
		break;
		
	    case MeterColourScale:
		value = AudioLevel::multiplier_to_preview(mag * 50, 256);
	    break;

	    case dBColourScale:
		mag = 20.0 * log10(mag);
		mag = (mag + 80.0) / 80.0;
		if (mag < 0.0) mag = 0.0;
		if (mag > 1.0) mag = 1.0;
		value = int(mag * 256);
	    }
	}

	if (value > 254) value = 254;
	if (value < 0) value = 0;

	if (m_cacheInvalid || m_exiting) {
	    interrupted = true;
	    break;
	}

	if (column < m_cache->width() && (int)i < m_cache->height()) {
	    m_cache->setPixel(column, i, value + 1); // 0 is "unset"
	}
    }

    if (lock) m_mutex.unlock();
    return !interrupted;
}

void
SpectrogramLayer::CacheFillThread::run()
{
//    std::cerr << "SpectrogramLayer::CacheFillThread::run" << std::endl;

    m_layer.m_mutex.lock();

    while (!m_layer.m_exiting) {

	bool interrupted = false;

//	std::cerr << "SpectrogramLayer::CacheFillThread::run in loop" << std::endl;

	if (m_layer.m_model && m_layer.m_cacheInvalid) {

//	    std::cerr << "SpectrogramLayer::CacheFillThread::run: something to do" << std::endl;

	    while (!m_layer.m_model->isReady()) {
		m_layer.m_condition.wait(&m_layer.m_mutex, 100);
	    }

	    m_layer.m_cachedInitialVisibleArea = false;
	    m_layer.m_cacheInvalid = false;
	    m_fillExtent = 0;
	    m_fillCompletion = 0;

	    std::cerr << "SpectrogramLayer::CacheFillThread::run: model is ready" << std::endl;

	    size_t start = m_layer.m_model->getStartFrame();
	    size_t end = m_layer.m_model->getEndFrame();

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

	    delete m_layer.m_cache;
	    size_t width = (end - start) / windowIncrement + 1;
	    size_t height = windowSize / 2;
	    m_layer.m_cache = new QImage(width, height,
					 QImage::Format_Indexed8);

	    // If we're using JACK in mlock mode, this will be locked
	    // and we ought to unlock to avoid memory exhaustion.
	    // Shame it doesn't appear to be possible to allocate
	    // unlocked in the first place.
	    //!!! hm, I don't think this is working.
	    MUNLOCK((void *)m_layer.m_cache, width * height);
    
	    m_layer.setCacheColourmap();

	    m_layer.m_cache->fill(0);
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

		m_layer.m_mutex.lock();

		for (size_t f = visibleStart; f < visibleEnd; f += windowIncrement) {
	    
		    m_layer.fillCacheColumn(int((f - start) / windowIncrement),
					    input, output, plan,
					    windowSize, windowIncrement,
					    windower, false);

		    m_layer.m_mutex.unlock();
		    m_layer.m_mutex.lock();

		    if (m_layer.m_cacheInvalid || m_layer.m_exiting) {
			interrupted = true;
			m_fillExtent = 0;
			break;
		    }

		    if (++counter == updateAt) {
			if (f < end) m_fillExtent = f;
			m_fillCompletion = size_t(100 * fabsf(float(f - visibleStart) /
							      float(end - start)));
			counter = 0;
		    }
		}

		m_layer.m_mutex.unlock();
	    }

	    m_layer.m_cachedInitialVisibleArea = true;

	    if (!interrupted && doVisibleFirst) {
		
		for (size_t f = visibleEnd; f < end; f += windowIncrement) {
	    
		    if (!m_layer.fillCacheColumn(int((f - start) / windowIncrement),
						 input, output, plan,
						 windowSize, windowIncrement,
						 windower, true)) {
			interrupted = true;
			m_fillExtent = 0;
			break;
		    }


		    if (++counter == updateAt) {
			if (f < end) m_fillExtent = f;
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

		    if (!m_layer.fillCacheColumn(int((f - start) / windowIncrement),
						 input, output, plan,
						 windowSize, windowIncrement,
						 windower, true)) {
			interrupted = true;
			m_fillExtent = 0;
			break;
		    }
		    
		    if (++counter == updateAt) {
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

bool
SpectrogramLayer::getYBinRange(int y, float &q0, float &q1) const
{
    int h = m_view->height();
    if (y < 0 || y >= h) return false;

    // Each pixel in a column is drawn from a possibly non-
    // integral set of frequency bins.

    if (m_frequencyScale == LinearFrequencyScale) {

	size_t bins = m_windowSize / 2;
    
	if (m_maxFrequency > 0) {
	    int sr = m_model->getSampleRate();
	    bins = int((double(m_maxFrequency) * m_windowSize) / sr + 0.1);
	    if (bins > m_windowSize / 2) bins = m_windowSize / 2;
	}
	
	q0 = float(h - y - 1) * bins / h;
	q1 = float(h - y) * bins / h;

    } else {

	// This is all most ad-hoc.  I'm not at my brightest.

	int sr = m_model->getSampleRate();

	float maxf = m_maxFrequency;
	if (maxf == 0.0) maxf = float(sr) / 2;

	float minf = float(sr) / m_windowSize;
	
	float maxlogf = log10f(maxf);
	float minlogf = log10f(minf);

	float logf0 = minlogf + ((maxlogf - minlogf) * (h - y - 1)) / h;
	float logf1 = minlogf + ((maxlogf - minlogf) * (h - y)) / h;
	
	float f0 = pow(10.f, logf0);
	float f1 = pow(10.f, logf1);

	q0 = ((f0 * m_windowSize) / sr) - 1;
	q1 = ((f1 * m_windowSize) / sr) - 1;

//	std::cout << "y=" << y << " h=" << h << " maxf=" << maxf << " maxlogf="
//		  << maxlogf << " logf0=" << logf0 << " f0=" << f0 << " q0="
//		  << q0 << std::endl;
    }	

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

    if (f1 < int(modelStart) || f0 > int(modelEnd)) return false;
      
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
	int binfreq = (sr * (q + 1)) / m_windowSize;
	if (q == q0i) freqMin = binfreq;
	if (q == q1i) freqMax = binfreq;
    }
    return true;
}
    
bool
SpectrogramLayer::getXYBinSourceRange(int x, int y, float &dbMin, float &dbMax) const
{
    float q0 = 0, q1 = 0;
    if (!getYBinRange(y, q0, q1)) return false;

    float s0 = 0, s1 = 0;
    if (!getXBinRange(x, s0, s1)) return false;
    
    int q0i = int(q0 + 0.001);
    int q1i = int(q1);

    int s0i = int(s0 + 0.001);
    int s1i = int(s1);

    if (m_mutex.tryLock()) {
	if (m_cache && !m_cacheInvalid) {

	    int cw = m_cache->width();
	    int ch = m_cache->height();

	    int min = -1, max = -1;

	    for (int q = q0i; q <= q1i; ++q) {
		for (int s = s0i; s <= s1i; ++s) {
		    if (s >= 0 && q >= 0 && s < cw && q < ch) {
			int value = m_cache->scanLine(q)[s];
			if (min == -1 || value < min) min = value;
			if (max == -1 || value > max) max = value;
		    }	
		}
	    }

	    if (min < 0) return false;

	    dbMin = (float(min) / 256.0) * 80.0 - 80.0;
	    dbMax = (float(max + 1) / 256.0) * 80.0 - 80.1;

	    m_mutex.unlock();
	    return true;
	}

	m_mutex.unlock();
    }

    return false;
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

#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << "SpectrogramLayer::paint(): About to lock" << std::endl;
#endif

/*
    if (m_cachedInitialVisibleArea) {
	if (!m_mutex.tryLock()) {
	    m_view->update();
	    return;
	}
    } else {
*/
	m_mutex.lock();
//    }

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

    m_mutex.unlock();

    for (int y = 0; y < h; ++y) {

	m_mutex.lock();
	if (m_cacheInvalid) {
	    m_mutex.unlock();
	    break;
	}

	int cw = m_cache->width();
	int ch = m_cache->height();

	float q0 = 0, q1 = 0;

	if (!getYBinRange(y0 + y, q0, q1)) {
	    for (int x = 0; x < w; ++x) {
		assert(x <= scaled.width());
		scaled.setPixel(x, y, qRgb(0, 0, 0));
	    }
	    m_mutex.unlock();
	    continue;
	}

	int q0i = int(q0 + 0.001);
	int q1i = int(q1);

	for (int x = 0; x < w; ++x) {

	    float s0 = 0, s1 = 0;

	    if (!getXBinRange(x0 + x, s0, s1)) {
		assert(x <= scaled.width());
		scaled.setPixel(x, y, qRgb(0, 0, 0));
		continue;
	    }

	    int s0i = int(s0 + 0.001);
	    int s1i = int(s1);

	    float total = 0, divisor = 0;

	    for (int s = s0i; s <= s1i; ++s) {

		float sprop = 1.0;
		if (s == s0i) sprop *= (s + 1) - s0;
		if (s == s1i) sprop *= s1 - s;

		for (int q = q0i; q <= q1i; ++q) {

		    float qprop = sprop;
		    if (q == q0i) qprop *= (q + 1) - q0;
		    if (q == q1i) qprop *= q1 - q;

		    if (s >= 0 && q >= 0 && s < cw && q < ch) {
			total += qprop * m_cache->scanLine(q)[s];
			divisor += qprop;
		    }
		}
	    }
		    
	    if (divisor > 0.0) {
/*
		int pixel = int(total / divisor);
		if (pixel > 255) pixel = 255;
		if (pixel < 1) pixel = 1;
		assert(x <= scaled.width());
		scaled.setPixel(x, y, m_cache->color(pixel));
*/
		float pixel = total / divisor;
		float lq = pixel - int(pixel);
		float hq = int(pixel) + 1 - pixel;
		int pixNum = int(pixel);
		QRgb low = m_cache->color(pixNum > 255 ? 255 : pixNum);
		QRgb high = m_cache->color(pixNum > 254 ? 255 : pixNum + 1);
		QRgb mixed = qRgb
		    (qRed(low) * lq + qRed(high) * hq + 0.01,
		     qGreen(low) * lq + qGreen(high) * hq + 0.01,
		     qBlue(low) * lq + qBlue(high) * hq + 0.01);
		scaled.setPixel(x, y, mixed);

	    } else {
		assert(x <= scaled.width());
		scaled.setPixel(x, y, qRgb(0, 0, 0));
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

//!!!    drawLocalFeatureDescription(paint);
}

int
SpectrogramLayer::getCompletion() const
{
    if (m_updateTimer == 0) return 100;
    size_t completion = m_fillThread->getFillCompletion();
//    std::cerr << "SpectrogramLayer::getCompletion: completion = " << completion << std::endl;
    return completion;
}

QRect
SpectrogramLayer::getFeatureDescriptionRect(QPainter &paint, QPoint pos) const
{
    if (!m_model || !m_model->isOK()) return QRect();

    QString timeLabel = tr("Time: ");
    QString freqLabel = tr("Hz: ");
    QString dBLabel = tr("dB: ");

    // assume time is widest
    RealTime rtMin, rtMax;
    if (!getXBinSourceRange(pos.x(), rtMin, rtMax)) return QRect();
    QString timeMinText = QString("%1").arg(rtMin.toText(true).c_str());
    QString timeMaxText = QString(" - %1").arg(rtMax.toText(true).c_str());

    QFontMetrics metrics = paint.fontMetrics();

    int labelwidth = 
	std::max(std::max(metrics.width(timeLabel),
			  metrics.width(freqLabel)),
		 metrics.width(dBLabel));

    int boxwidth = labelwidth + 
	metrics.width(timeMinText) + metrics.width(timeMaxText);

    int fontHeight = metrics.height();
    int boxheight = fontHeight * 3 + 4;

    return QRect(0, 0, boxwidth + 20, boxheight + 15);
}

void
SpectrogramLayer::paintLocalFeatureDescription(QPainter &paint,
					       QRect rect, QPoint pos) const
{
    int x = pos.x();
    int y = pos.y();

    if (!m_model || !m_model->isOK()) return;

    float dbMin = 0, dbMax = 0;
    float freqMin = 0, freqMax = 0;
    RealTime rtMin, rtMax;

    bool haveDb = false;

    if (!getXBinSourceRange(x, rtMin, rtMax)) return;
    if (!getYBinSourceRange(y, freqMin, freqMax)) return;
    if (getXYBinSourceRange(x, y, dbMin, dbMax)) haveDb = true;

    QString timeLabel = tr("Time: ");
    QString freqLabel = tr("Hz: ");
    QString dBLabel = tr("dB: ");

    QString timeMinText = QString("%1").arg(rtMin.toText(true).c_str());
    QString timeMaxText = QString(" - %1").arg(rtMax.toText(true).c_str());

    QString freqMinText = QString("%1").arg(freqMin);
    QString freqMaxText = "";
    if (freqMax != freqMin) {
	freqMaxText = QString(" - %1").arg(freqMax);
    }

    QString dBMinText = "";
    QString dBMaxText = "";

    if (haveDb) {
	int dbmxi = int(dbMax - 0.001);
	int dbmni = int(dbMin - 0.001);
	dBMinText = QString("%1").arg(dbmni);
	if (dbmxi != dbmni) dBMaxText = QString(" - %1").arg(dbmxi);
    }

    QFontMetrics metrics = paint.fontMetrics();

    int labelwidth = 
	std::max(std::max(metrics.width(timeLabel),
			  metrics.width(freqLabel)),
		 metrics.width(dBLabel));

    int minwidth = 
	std::max(std::max(metrics.width(timeMinText),
			  metrics.width(freqMinText)),
		 metrics.width(dBMinText));

    int maxwidth = 
	std::max(std::max(metrics.width(timeMaxText),
			  metrics.width(freqMaxText)),
		 metrics.width(dBMaxText));

    int boxwidth = labelwidth + minwidth + maxwidth;

    int fontAscent = metrics.ascent();
    int fontHeight = metrics.height();

    int boxheight = fontHeight * 3 + 4;

//    paint.setPen(Qt::white);
//    paint.setBrush(Qt::NoBrush);

//!!!    int xbase = m_view->width() - boxwidth - 20;
    int xbase = rect.x() + 5;
    int ybase = rect.y() + 5;

    paint.drawRect(xbase, ybase, boxwidth + 10,
		   boxheight + 10 - metrics.descent() + 1);

    paint.drawText(xbase + 5 + labelwidth - metrics.width(timeLabel),
		   ybase + 5 + fontAscent, timeLabel);

    paint.drawText(xbase + 5 + labelwidth - metrics.width(freqLabel),
		   ybase + 7 + fontAscent + fontHeight, freqLabel);

    paint.drawText(xbase + 5 + labelwidth - metrics.width(dBLabel),
		   ybase + 9 + fontAscent + fontHeight * 2, dBLabel);
    
    paint.drawText(xbase + 5 + labelwidth + minwidth - metrics.width(timeMinText),
		   ybase + 5 + fontAscent, timeMinText);

    paint.drawText(xbase + 5 + labelwidth + minwidth - metrics.width(freqMinText),
		   ybase + 7 + fontAscent + fontHeight, freqMinText);

    paint.drawText(xbase + 5 + labelwidth + minwidth - metrics.width(dBMinText),
		   ybase + 9 + fontAscent + fontHeight * 2, dBMinText);
    
    paint.drawText(xbase + 5 + labelwidth + minwidth,
		   ybase + 5 + fontAscent, timeMaxText);

    paint.drawText(xbase + 5 + labelwidth + minwidth,
		   ybase + 7 + fontAscent + fontHeight, freqMaxText);
    
    paint.drawText(xbase + 5 + labelwidth + minwidth,
		   ybase + 9 + fontAscent + fontHeight * 2, dBMaxText);
}

int
SpectrogramLayer::getNearestFeatureFrame(int frame, 
					 size_t &resolution,
					 bool snapRight) const
{
    resolution = getWindowIncrement();
    int snapFrame = (frame / resolution) * resolution;
    if (snapRight) snapFrame += resolution;
    return snapFrame;
}

/*!!!

bool
SpectrogramLayer::identifyLocalFeatures(bool on, int x, int y)
{
    return true; //!!!

    m_identify = on;
    m_identifyX = x;
    m_identifyY = y;

    m_view->update();
*/
/*
    if (!m_model || !m_model->isOK()) return false;

    std::cerr << "SpectrogramLayer::identifyLocalFeatures(" << on << "," << x << "," << y << ")" << std::endl;

    float dbMin = 0, dbMax = 0;
    float freqMin = 0, freqMax = 0;
    RealTime rtMin, rtMax;

    if (getXBinSourceRange(x, rtMin, rtMax)) {
	std::cerr << "Times: " << rtMin << " -> " << rtMax << std::endl;
    } else return false;

    if (getYBinSourceRange(y, freqMin, freqMax)) {
	std::cerr << "Frequencies: " << freqMin << " -> " << freqMax << std::endl;
    } else return false;

    if (getXYBinSourceRange(x, y, dbMin, dbMax)) {
	std::cerr << "dB: " << dbMin << " -> " << dbMax << std::endl;
    }

    m_identifyX = x;
    m_identifyY = y;
    m_identify = true;
*/
    /*!!!
    return true;
}
    */
int
SpectrogramLayer::getVerticalScaleWidth(QPainter &paint) const
{
    if (!m_model || !m_model->isOK()) return 0;

    int tw = paint.fontMetrics().width(QString("%1")
				     .arg(m_maxFrequency > 0 ?
					  m_maxFrequency - 1 :
					  m_model->getSampleRate() / 2));

    int fw = paint.fontMetrics().width(QString("43Hz"));
    if (tw < fw) tw = fw;
    
    return tw + 13;
}

void
SpectrogramLayer::paintVerticalScale(QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) {
	return;
    }

    int h = rect.height(), w = rect.width();

    size_t bins = m_windowSize / 2;
    int sr = m_model->getSampleRate();

    if (m_maxFrequency > 0) {
	bins = int((double(m_maxFrequency) * m_windowSize) / sr + 0.1);
	if (bins > m_windowSize / 2) bins = m_windowSize / 2;
    }

    int py = -1;
    int textHeight = paint.fontMetrics().height();
    int toff = -textHeight + paint.fontMetrics().ascent() + 2;

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

	int freq = (sr * (bin + 1)) / m_windowSize;

	if (py >= 0 && (vy - py) < textHeight - 1) {
	    paint.drawLine(w - 4, h - vy, w, h - vy);
	    continue;
	}

	QString text = QString("%1").arg(freq);
	if (bin == 0) text = QString("%1Hz").arg(freq);
	paint.drawLine(0, h - vy, w, h - vy);

	if (h - vy - textHeight >= -2) {
	    int tx = w - 10 - paint.fontMetrics().width(text);
	    paint.drawText(tx, h - vy + toff, text);
	}

	py = vy;
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
		 "maxFrequency=\"%6\" "
		 "colourScale=\"%7\" "
		 "colourScheme=\"%8\" "
		 "frequencyScale=\"%9\"")
	.arg(m_channel)
	.arg(m_windowSize)
	.arg(m_windowType)
	.arg(m_windowOverlap)
	.arg(m_gain)
	.arg(m_maxFrequency)
	.arg(m_colourScale)
	.arg(m_colourScheme)
	.arg(m_frequencyScale);

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

    size_t maxFrequency = attributes.value("maxFrequency").toUInt(&ok);
    if (ok) setMaxFrequency(maxFrequency);

    ColourScale colourScale = (ColourScale)
	attributes.value("colourScale").toInt(&ok);
    if (ok) setColourScale(colourScale);

    ColourScheme colourScheme = (ColourScheme)
	attributes.value("colourScheme").toInt(&ok);
    if (ok) setColourScheme(colourScheme);

    FrequencyScale frequencyScale = (FrequencyScale)
	attributes.value("frequencyScale").toInt(&ok);
    if (ok) setFrequencyScale(frequencyScale);
}
    

#ifdef INCLUDE_MOCFILES
#include "SpectrogramLayer.moc.cpp"
#endif

