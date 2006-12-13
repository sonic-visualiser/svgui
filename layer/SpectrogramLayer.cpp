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

#include "SpectrogramLayer.h"

#include "view/View.h"
#include "base/Profiler.h"
#include "base/AudioLevel.h"
#include "base/Window.h"
#include "base/Pitch.h"
#include "base/Preferences.h"
#include "base/RangeMapper.h"

#include <QPainter>
#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QTimer>
#include <QApplication>
#include <QMessageBox>

#include <iostream>

#include <cassert>
#include <cmath>

//#define DEBUG_SPECTROGRAM_REPAINT 1

SpectrogramLayer::SpectrogramLayer(Configuration config) :
    Layer(),
    m_model(0),
    m_channel(0),
    m_windowSize(1024),
    m_windowType(HanningWindow),
    m_windowHopLevel(2),
    m_zeroPadLevel(0),
    m_fftSize(1024),
    m_gain(1.0),
    m_threshold(0.0),
    m_colourRotation(0),
    m_minFrequency(10),
    m_maxFrequency(8000),
    m_initialMaxFrequency(8000),
    m_colourScale(dBColourScale),
    m_colourScheme(DefaultColours),
    m_frequencyScale(LinearFrequencyScale),
    m_binDisplay(AllBins),
    m_normalizeColumns(false),
    m_normalizeVisibleArea(false),
    m_lastEmittedZoomStep(-1),
    m_updateTimer(0),
    m_candidateFillStartFrame(0),
    m_exiting(false)
{
    if (config == MelodicRange) {
	setWindowSize(8192);
	setWindowHopLevel(4);
//	setWindowType(ParzenWindow);
        m_initialMaxFrequency = 1000;
	setMaxFrequency(1000);
	setColourScale(LinearColourScale);
    } else if (config == MelodicPeaks) {
	setWindowSize(4096);
	setWindowHopLevel(5);
//	setWindowType(BlackmanWindow);
        m_initialMaxFrequency = 2000;
	setMaxFrequency(2000);
	setMinFrequency(40);
	setFrequencyScale(LogFrequencyScale);
	setColourScale(MeterColourScale);
	setBinDisplay(PeakFrequencies);
	setNormalizeColumns(true);
    }

    Preferences *prefs = Preferences::getInstance();
    connect(prefs, SIGNAL(propertyChanged(PropertyContainer::PropertyName)),
            this, SLOT(preferenceChanged(PropertyContainer::PropertyName)));
    setWindowType(prefs->getWindowType());

    setColourmap();
}

SpectrogramLayer::~SpectrogramLayer()
{
    delete m_updateTimer;
    m_updateTimer = 0;
    
    invalidateFFTModels();
}

void
SpectrogramLayer::setModel(const DenseTimeValueModel *model)
{
//    std::cerr << "SpectrogramLayer(" << this << "): setModel(" << model << ")" << std::endl;

    if (model == m_model) return;

    m_model = model;
    invalidateFFTModels();

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
}

Layer::PropertyList
SpectrogramLayer::getProperties() const
{
    PropertyList list;
    list.push_back("Colour");
    list.push_back("Colour Scale");
    list.push_back("Window Size");
    list.push_back("Window Increment");
    list.push_back("Normalize Columns");
    list.push_back("Normalize Visible Area");
    list.push_back("Bin Display");
    list.push_back("Threshold");
    list.push_back("Gain");
    list.push_back("Colour Rotation");
//    list.push_back("Min Frequency");
//    list.push_back("Max Frequency");
    list.push_back("Frequency Scale");
////    list.push_back("Zero Padding");
    return list;
}

QString
SpectrogramLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Colour") return tr("Colour");
    if (name == "Colour Scale") return tr("Colour Scale");
    if (name == "Window Size") return tr("Window Size");
    if (name == "Window Increment") return tr("Window Overlap");
    if (name == "Normalize Columns") return tr("Normalize Columns");
    if (name == "Normalize Visible Area") return tr("Normalize Visible Area");
    if (name == "Bin Display") return tr("Bin Display");
    if (name == "Threshold") return tr("Threshold");
    if (name == "Gain") return tr("Gain");
    if (name == "Colour Rotation") return tr("Colour Rotation");
    if (name == "Min Frequency") return tr("Min Frequency");
    if (name == "Max Frequency") return tr("Max Frequency");
    if (name == "Frequency Scale") return tr("Frequency Scale");
    if (name == "Zero Padding") return tr("Smoothing");
    return "";
}

Layer::PropertyType
SpectrogramLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Gain") return RangeProperty;
    if (name == "Colour Rotation") return RangeProperty;
    if (name == "Normalize Columns") return ToggleProperty;
    if (name == "Normalize Visible Area") return ToggleProperty;
    if (name == "Threshold") return RangeProperty;
    if (name == "Zero Padding") return ToggleProperty;
    return ValueProperty;
}

QString
SpectrogramLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Bin Display" ||
        name == "Frequency Scale") return tr("Bins");
    if (name == "Window Size" ||
	name == "Window Increment" ||
        name == "Zero Padding") return tr("Window");
    if (name == "Colour" ||
//	name == "Gain" ||
	name == "Threshold" ||
	name == "Colour Rotation") return tr("Colour");
    if (name == "Normalize Columns" ||
        name == "Normalize Visible Area" ||
//	name == "Bin Display" ||
        name == "Gain" ||
	name == "Colour Scale") return tr("Scale");
//    if (name == "Max Frequency" ||
//	name == "Min Frequency" ||
//	name == "Frequency Scale" ||
//	name == "Frequency Adjustment") return tr("Range");
    return QString();
}

int
SpectrogramLayer::getPropertyRangeAndValue(const PropertyName &name,
					   int *min, int *max) const
{
    int deft = 0;

    int garbage0, garbage1;
    if (!min) min = &garbage0;
    if (!max) max = &garbage1;

    if (name == "Gain") {

	*min = -50;
	*max = 50;

	deft = lrint(log10(m_gain) * 20.0);
	if (deft < *min) deft = *min;
	if (deft > *max) deft = *max;

    } else if (name == "Threshold") {

	*min = -50;
	*max = 0;

	deft = lrintf(AudioLevel::multiplier_to_dB(m_threshold));
	if (deft < *min) deft = *min;
	if (deft > *max) deft = *max;

    } else if (name == "Colour Rotation") {

	*min = 0;
	*max = 256;

	deft = m_colourRotation;

    } else if (name == "Colour Scale") {

	*min = 0;
	*max = 4;

	deft = (int)m_colourScale;

    } else if (name == "Colour") {

	*min = 0;
	*max = 6;

	deft = (int)m_colourScheme;

    } else if (name == "Window Size") {

	*min = 0;
	*max = 10;
	
	deft = 0;
	int ws = m_windowSize;
	while (ws > 32) { ws >>= 1; deft ++; }

    } else if (name == "Window Increment") {
	
	*min = 0;
	*max = 5;
	
        deft = m_windowHopLevel;
    
    } else if (name == "Zero Padding") {
	
	*min = 0;
	*max = 1;
	
        deft = m_zeroPadLevel > 0 ? 1 : 0;
    
    } else if (name == "Min Frequency") {

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
    
    } else if (name == "Max Frequency") {

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

    } else if (name == "Frequency Scale") {

	*min = 0;
	*max = 1;
	deft = (int)m_frequencyScale;

    } else if (name == "Bin Display") {

	*min = 0;
	*max = 2;
	deft = (int)m_binDisplay;

    } else if (name == "Normalize Columns") {
	
	deft = (m_normalizeColumns ? 1 : 0);

    } else if (name == "Normalize Visible Area") {
	
	deft = (m_normalizeVisibleArea ? 1 : 0);

    } else {
	deft = Layer::getPropertyRangeAndValue(name, min, max);
    }

    return deft;
}

QString
SpectrogramLayer::getPropertyValueLabel(const PropertyName &name,
					int value) const
{
    if (name == "Colour") {
	switch (value) {
	default:
	case 0: return tr("Default");
	case 1: return tr("White on Black");
	case 2: return tr("Black on White");
	case 3: return tr("Red on Blue");
	case 4: return tr("Yellow on Black");
	case 5: return tr("Blue on Black");
	case 6: return tr("Fruit Salad");
	}
    }
    if (name == "Colour Scale") {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Meter");
	case 2: return tr("dB");
	case 3: return tr("dB^2");
	case 4: return tr("Phase");
	}
    }
    if (name == "Window Size") {
	return QString("%1").arg(32 << value);
    }
    if (name == "Window Increment") {
	switch (value) {
	default:
	case 0: return tr("None");
	case 1: return tr("25 %");
	case 2: return tr("50 %");
	case 3: return tr("75 %");
	case 4: return tr("87.5 %");
	case 5: return tr("93.75 %");
	}
    }
    if (name == "Zero Padding") {
        if (value == 0) return tr("None");
        return QString("%1x").arg(value + 1);
    }
    if (name == "Min Frequency") {
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
    if (name == "Max Frequency") {
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
    if (name == "Frequency Scale") {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Log");
	}
    }
    if (name == "Bin Display") {
	switch (value) {
	default:
	case 0: return tr("All Bins");
	case 1: return tr("Peak Bins");
	case 2: return tr("Frequencies");
	}
    }
    return tr("<unknown>");
}

RangeMapper *
SpectrogramLayer::getNewPropertyRangeMapper(const PropertyName &name) const
{
    if (name == "Gain") {
        return new LinearRangeMapper(-50, 50, -25, 25, tr("dB"));
    }
    if (name == "Threshold") {
        return new LinearRangeMapper(-50, 0, -50, 0, tr("dB"));
    }
    return 0;
}

void
SpectrogramLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Gain") {
	setGain(pow(10, float(value)/20.0));
    } else if (name == "Threshold") {
	if (value == -50) setThreshold(0.0);
	else setThreshold(AudioLevel::dB_to_multiplier(value));
    } else if (name == "Colour Rotation") {
	setColourRotation(value);
    } else if (name == "Colour") {
	switch (value) {
	default:
	case 0:	setColourScheme(DefaultColours); break;
	case 1: setColourScheme(WhiteOnBlack); break;
	case 2: setColourScheme(BlackOnWhite); break;
	case 3: setColourScheme(RedOnBlue); break;
	case 4: setColourScheme(YellowOnBlack); break;
	case 5: setColourScheme(BlueOnBlack); break;
	case 6: setColourScheme(Rainbow); break;
	}
    } else if (name == "Window Size") {
	setWindowSize(32 << value);
    } else if (name == "Window Increment") {
        setWindowHopLevel(value);
    } else if (name == "Zero Padding") {
        setZeroPadLevel(value > 0.1 ? 3 : 0);
    } else if (name == "Min Frequency") {
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
        int vs = getCurrentVerticalZoomStep();
        if (vs != m_lastEmittedZoomStep) {
            emit verticalZoomChanged();
            m_lastEmittedZoomStep = vs;
        }
    } else if (name == "Max Frequency") {
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
        int vs = getCurrentVerticalZoomStep();
        if (vs != m_lastEmittedZoomStep) {
            emit verticalZoomChanged();
            m_lastEmittedZoomStep = vs;
        }
    } else if (name == "Colour Scale") {
	switch (value) {
	default:
	case 0: setColourScale(LinearColourScale); break;
	case 1: setColourScale(MeterColourScale); break;
	case 2: setColourScale(dBColourScale); break;
	case 3: setColourScale(OtherColourScale); break;
	case 4: setColourScale(PhaseColourScale); break;
	}
    } else if (name == "Frequency Scale") {
	switch (value) {
	default:
	case 0: setFrequencyScale(LinearFrequencyScale); break;
	case 1: setFrequencyScale(LogFrequencyScale); break;
	}
    } else if (name == "Bin Display") {
	switch (value) {
	default:
	case 0: setBinDisplay(AllBins); break;
	case 1: setBinDisplay(PeakBins); break;
	case 2: setBinDisplay(PeakFrequencies); break;
	}
    } else if (name == "Normalize Columns") {
	setNormalizeColumns(value ? true : false);
    } else if (name == "Normalize Visible Area") {
	setNormalizeVisibleArea(value ? true : false);
    }
}

void
SpectrogramLayer::invalidatePixmapCaches()
{
    for (ViewPixmapCache::iterator i = m_pixmapCaches.begin();
         i != m_pixmapCaches.end(); ++i) {
        i->second.validArea = QRect();
    }
}

void
SpectrogramLayer::invalidatePixmapCaches(size_t startFrame, size_t endFrame)
{
    for (ViewPixmapCache::iterator i = m_pixmapCaches.begin();
         i != m_pixmapCaches.end(); ++i) {

        //!!! when are views removed from the map? on setLayerDormant?
        const View *v = i->first;

        if (startFrame < v->getEndFrame() && int(endFrame) >= v->getStartFrame()) {
            i->second.validArea = QRect();
        }
    }
}

void
SpectrogramLayer::preferenceChanged(PropertyContainer::PropertyName name)
{
    std::cerr << "SpectrogramLayer::preferenceChanged(" << name.toStdString() << ")" << std::endl;

    if (name == "Window Type") {
        setWindowType(Preferences::getInstance()->getWindowType());
        return;
    }
    if (name == "Smooth Spectrogram") {
        invalidatePixmapCaches();
        invalidateMagnitudes();
        emit layerParametersChanged();
    }
    if (name == "Tuning Frequency") {
        emit layerParametersChanged();
    }
}

void
SpectrogramLayer::setChannel(int ch)
{
    if (m_channel == ch) return;

    invalidatePixmapCaches();
    m_channel = ch;
    invalidateFFTModels();

    emit layerParametersChanged();
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

    invalidatePixmapCaches();
    
    m_windowSize = ws;
    m_fftSize = ws * (m_zeroPadLevel + 1);
    
    invalidateFFTModels();

    emit layerParametersChanged();
}

size_t
SpectrogramLayer::getWindowSize() const
{
    return m_windowSize;
}

void
SpectrogramLayer::setWindowHopLevel(size_t v)
{
    if (m_windowHopLevel == v) return;

    invalidatePixmapCaches();
    
    m_windowHopLevel = v;
    
    invalidateFFTModels();

    emit layerParametersChanged();

//    fillCache();
}

size_t
SpectrogramLayer::getWindowHopLevel() const
{
    return m_windowHopLevel;
}

void
SpectrogramLayer::setZeroPadLevel(size_t v)
{
    if (m_zeroPadLevel == v) return;

    invalidatePixmapCaches();
    
    m_zeroPadLevel = v;
    m_fftSize = m_windowSize * (v + 1);

    invalidateFFTModels();

    emit layerParametersChanged();
}

size_t
SpectrogramLayer::getZeroPadLevel() const
{
    return m_zeroPadLevel;
}

void
SpectrogramLayer::setWindowType(WindowType w)
{
    if (m_windowType == w) return;

    invalidatePixmapCaches();
    
    m_windowType = w;

    invalidateFFTModels();

    emit layerParametersChanged();
}

WindowType
SpectrogramLayer::getWindowType() const
{
    return m_windowType;
}

void
SpectrogramLayer::setGain(float gain)
{
//    std::cerr << "SpectrogramLayer::setGain(" << gain << ") (my gain is now "
//	      << m_gain << ")" << std::endl;

    if (m_gain == gain) return;

    invalidatePixmapCaches();
    
    m_gain = gain;
    
    emit layerParametersChanged();
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

    invalidatePixmapCaches();
    
    m_threshold = threshold;

    emit layerParametersChanged();
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

    invalidatePixmapCaches();
    invalidateMagnitudes();
    
    m_minFrequency = mf;

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

    invalidatePixmapCaches();
    invalidateMagnitudes();
    
    m_maxFrequency = mf;
    
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
    invalidatePixmapCaches();

    if (r < 0) r = 0;
    if (r > 256) r = 256;
    int distance = r - m_colourRotation;

    if (distance != 0) {
	rotateColourmap(-distance);
	m_colourRotation = r;
    }
    
    emit layerParametersChanged();
}

void
SpectrogramLayer::setColourScale(ColourScale colourScale)
{
    if (m_colourScale == colourScale) return;

    invalidatePixmapCaches();
    
    m_colourScale = colourScale;
    
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

    invalidatePixmapCaches();
    
    m_colourScheme = scheme;
    setColourmap();

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

    invalidatePixmapCaches();
    m_frequencyScale = frequencyScale;

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

    invalidatePixmapCaches();
    m_binDisplay = binDisplay;

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

    invalidatePixmapCaches();
    invalidateMagnitudes();
    m_normalizeColumns = n;

    emit layerParametersChanged();
}

bool
SpectrogramLayer::getNormalizeColumns() const
{
    return m_normalizeColumns;
}

void
SpectrogramLayer::setNormalizeVisibleArea(bool n)
{
    if (m_normalizeVisibleArea == n) return;

    invalidatePixmapCaches();
    invalidateMagnitudes();
    m_normalizeVisibleArea = n;

    emit layerParametersChanged();
}

bool
SpectrogramLayer::getNormalizeVisibleArea() const
{
    return m_normalizeVisibleArea;
}

void
SpectrogramLayer::setLayerDormant(const View *v, bool dormant)
{
    if (dormant) {

        if (isLayerDormant(v)) {
            return;
        }

        Layer::setLayerDormant(v, true);

	invalidatePixmapCaches();
        m_pixmapCaches.erase(v);

        if (m_fftModels.find(v) != m_fftModels.end()) {
            delete m_fftModels[v].first;
            m_fftModels.erase(v);
        }
	
    } else {

        Layer::setLayerDormant(v, false);
    }
}

void
SpectrogramLayer::cacheInvalid()
{
    invalidatePixmapCaches();
    invalidateMagnitudes();
}

void
SpectrogramLayer::cacheInvalid(size_t, size_t)
{
    // for now (or forever?)
    cacheInvalid();
}

void
SpectrogramLayer::fillTimerTimedOut()
{
    if (!m_model) return;

    bool allDone = true;

#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << "SpectrogramLayer::fillTimerTimedOut: have " << m_fftModels.size() << " FFT models associated with views" << std::endl;
#endif

    for (ViewFFTMap::iterator i = m_fftModels.begin();
         i != m_fftModels.end(); ++i) {

        const View *v = i->first;
        const FFTModel *model = i->second.first;
        size_t lastFill = i->second.second;

        if (model) {

            size_t fill = model->getFillExtent();

#ifdef DEBUG_SPECTROGRAM_REPAINT
            std::cerr << "SpectrogramLayer::fillTimerTimedOut: extent for " << model << ": " << fill << ", last " << lastFill << ", total " << m_model->getEndFrame() << std::endl;
#endif

            if (fill >= lastFill) {
                if (fill >= m_model->getEndFrame() && lastFill > 0) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
                    std::cerr << "complete!" << std::endl;
#endif
                    invalidatePixmapCaches();
                    i->second.second = -1;
                    emit modelChanged();

                } else if (fill > lastFill) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
                    std::cerr << "SpectrogramLayer: emitting modelChanged("
                              << lastFill << "," << fill << ")" << std::endl;
#endif
                    invalidatePixmapCaches(lastFill, fill);
                    i->second.second = fill;
                    emit modelChanged(lastFill, fill);
                }
            } else {
#ifdef DEBUG_SPECTROGRAM_REPAINT
                std::cerr << "SpectrogramLayer: going backwards, emitting modelChanged("
                          << m_model->getStartFrame() << "," << m_model->getEndFrame() << ")" << std::endl;
#endif
                invalidatePixmapCaches();
                i->second.second = fill;
                emit modelChanged(m_model->getStartFrame(), m_model->getEndFrame());
            }

            if (i->second.second >= 0) {
                allDone = false;
            }
        }
    }

    if (allDone) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
        std::cerr << "SpectrogramLayer: all complete!" << std::endl;
#endif
        delete m_updateTimer;
        m_updateTimer = 0;
    }
}

void
SpectrogramLayer::setColourmap()
{
    int formerRotation = m_colourRotation;

    if (m_colourScheme == BlackOnWhite) {
	m_colourMap.setColour(NO_VALUE, Qt::white);
    } else {
	m_colourMap.setColour(NO_VALUE, Qt::black);
    }

    for (int pixel = 1; pixel < 256; ++pixel) {

	QColor colour;
	int hue, px;

	switch (m_colourScheme) {

	default:
	case DefaultColours:
	    hue = 256 - pixel;
	    colour = QColor::fromHsv(hue, pixel/2 + 128, pixel);
            m_crosshairColour = QColor(255, 150, 50);
//            m_crosshairColour = QColor::fromHsv(240, 160, 255);
	    break;

	case WhiteOnBlack:
	    colour = QColor(pixel, pixel, pixel);
            m_crosshairColour = Qt::red;
	    break;

	case BlackOnWhite:
	    colour = QColor(256-pixel, 256-pixel, 256-pixel);
            m_crosshairColour = Qt::darkGreen;
	    break;

	case RedOnBlue:
	    colour = QColor(pixel > 128 ? (pixel - 128) * 2 : 0, 0,
			    pixel < 128 ? pixel : (256 - pixel));
            m_crosshairColour = Qt::green;
	    break;

	case YellowOnBlack:
	    px = 256 - pixel;
	    colour = QColor(px < 64 ? 255 - px/2 :
			    px < 128 ? 224 - (px - 64) :
			    px < 192 ? 160 - (px - 128) * 3 / 2 :
			    256 - px,
			    pixel,
			    pixel / 4);
            m_crosshairColour = QColor::fromHsv(240, 255, 255);
	    break;

        case BlueOnBlack:
            colour = QColor::fromHsv
                (240, pixel > 226 ? 256 - (pixel - 226) * 8 : 255,
                 (pixel * pixel) / 255);
            m_crosshairColour = Qt::red;
            break;

	case Rainbow:
	    hue = 250 - pixel;
	    if (hue < 0) hue += 256;
	    colour = QColor::fromHsv(pixel, 255, 255);
            m_crosshairColour = Qt::white;
	    break;
	}

	m_colourMap.setColour(pixel, colour);
    }

    m_colourRotation = 0;
    rotateColourmap(m_colourRotation - formerRotation);
    m_colourRotation = formerRotation;
}

void
SpectrogramLayer::rotateColourmap(int distance)
{
    QColor newPixels[256];

    newPixels[NO_VALUE] = m_colourMap.getColour(NO_VALUE);

    for (int pixel = 1; pixel < 256; ++pixel) {
	int target = pixel + distance;
	while (target < 1) target += 255;
	while (target > 255) target -= 255;
	newPixels[target] = m_colourMap.getColour(pixel);
    }

    for (int pixel = 0; pixel < 256; ++pixel) {
	m_colourMap.setColour(pixel, newPixels[pixel]);
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

    float phaseError = princargf(newPhase - expectedPhase);
	    
    if (fabsf(phaseError) < (1.1f * (windowIncrement * M_PI) / windowSize)) {

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

unsigned char
SpectrogramLayer::getDisplayValue(View *v, float input) const
{
    int value;

    float min = 0.f;
    float max = 1.f;

    if (m_normalizeVisibleArea) {
        min = m_viewMags[v].getMin();
        max = m_viewMags[v].getMax();
    } else if (!m_normalizeColumns) {
        if (m_colourScale == LinearColourScale ||
            m_colourScale == MeterColourScale) {
            max = 0.1f;
        }
    }

    float thresh = -80.f;

    if (max == 0.f) max = 1.f;
    if (max == min) min = max - 0.0001f;

    switch (m_colourScale) {
	
    default:
    case LinearColourScale:
//	value = int
//	    (input * (m_normalizeColumns ? 1.0 : 50.0) * 255.0) + 1;
        value = int(((input - min) / (max - min)) * 255.f) + 1;
	break;
	
    case MeterColourScale:
//	value = AudioLevel::multiplier_to_preview
//	    (input * (m_normalizeColumns ? 1.0 : 50.0), 255) + 1;
        value = AudioLevel::multiplier_to_preview((input - min) / (max - min), 255) + 1;
	break;
	
    case dBColourScale:
        //!!! experiment with normalizing the visible area this way.
        //In any case, we need to have some indication of what the dB
        //scale is relative to.
        input = input / max;
        if (input > 0.f) {
            input = 10.f * log10f(input);
        } else {
            input = thresh;
        }
        if (min > 0.f) {
            thresh = 10.f * log10f(min);
            if (thresh < -80.f) thresh = -80.f;
        }
	input = (input - thresh) / (-thresh);
	if (input < 0.f) input = 0.f;
	if (input > 1.f) input = 1.f;
	value = int(input * 255.f) + 1;
	break;

    case OtherColourScale:
        //!!! the "Other" scale is just where our current experiments go
        //!!! power rather than v
        input = (input * input) / (max * max);
        if (input > 0.f) {
            input = 10.f * log10f(input);
        } else {
            input = thresh;
        }
        if (min > 0.f) {
            thresh = 10.f * log10f(min * min);
            if (thresh < -80.f) thresh = -80.f;
        }
	input = (input - thresh) / (-thresh);
	if (input < 0.f) input = 0.f;
	if (input > 1.f) input = 1.f;
	value = int(input * 255.f) + 1;
	break;
        
/*!!!
	input = 10.f * log10f(input * input);
        input = 1.f / (1.f + expf(- (input + 20.f) / 10.f));

	if (input < 0.f) input = 0.f;
	if (input > 1.f) input = 1.f;
	value = int(input * 255.f) + 1;
*/
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
    //!!! unused

    int value = uc;
    float input;

    //!!! incorrect for normalizing visible area (and also out of date)
    
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

    case OtherColourScale:
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

float
SpectrogramLayer::getEffectiveMinFrequency() const
{
    int sr = m_model->getSampleRate();
    float minf = float(sr) / m_fftSize;

    if (m_minFrequency > 0.0) {
	size_t minbin = size_t((double(m_minFrequency) * m_fftSize) / sr + 0.01);
	if (minbin < 1) minbin = 1;
	minf = minbin * sr / m_fftSize;
    }

    return minf;
}

float
SpectrogramLayer::getEffectiveMaxFrequency() const
{
    int sr = m_model->getSampleRate();
    float maxf = float(sr) / 2;

    if (m_maxFrequency > 0.0) {
	size_t maxbin = size_t((double(m_maxFrequency) * m_fftSize) / sr + 0.1);
	if (maxbin > m_fftSize / 2) maxbin = m_fftSize / 2;
	maxf = maxbin * sr / m_fftSize;
    }

    return maxf;
}

bool
SpectrogramLayer::getYBinRange(View *v, int y, float &q0, float &q1) const
{
    int h = v->height();
    if (y < 0 || y >= h) return false;

    int sr = m_model->getSampleRate();
    float minf = getEffectiveMinFrequency();
    float maxf = getEffectiveMaxFrequency();

    bool logarithmic = (m_frequencyScale == LogFrequencyScale);

    //!!! wrong for smoothing -- wrong fft size for fft model

    q0 = v->getFrequencyForY(y, minf, maxf, logarithmic);
    q1 = v->getFrequencyForY(y - 1, minf, maxf, logarithmic);

    // Now map these on to actual bins

    int b0 = int((q0 * m_fftSize) / sr);
    int b1 = int((q1 * m_fftSize) / sr);
    
    //!!! this is supposed to return fractions-of-bins, as it were, hence the floats
    q0 = b0;
    q1 = b1;
    
//    q0 = (b0 * sr) / m_fftSize;
//    q1 = (b1 * sr) / m_fftSize;

    return true;
}
    
bool
SpectrogramLayer::getXBinRange(View *v, int x, float &s0, float &s1) const
{
    size_t modelStart = m_model->getStartFrame();
    size_t modelEnd = m_model->getEndFrame();

    // Each pixel column covers an exact range of sample frames:
    int f0 = v->getFrameForX(x) - modelStart;
    int f1 = v->getFrameForX(x + 1) - modelStart - 1;

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
SpectrogramLayer::getXBinSourceRange(View *v, int x, RealTime &min, RealTime &max) const
{
    float s0 = 0, s1 = 0;
    if (!getXBinRange(v, x, s0, s1)) return false;
    
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
SpectrogramLayer::getYBinSourceRange(View *v, int y, float &freqMin, float &freqMax)
const
{
    float q0 = 0, q1 = 0;
    if (!getYBinRange(v, y, q0, q1)) return false;

    int q0i = int(q0 + 0.001);
    int q1i = int(q1);

    int sr = m_model->getSampleRate();

    for (int q = q0i; q <= q1i; ++q) {
	if (q == q0i) freqMin = (sr * q) / m_fftSize;
	if (q == q1i) freqMax = (sr * (q+1)) / m_fftSize;
    }
    return true;
}

bool
SpectrogramLayer::getAdjustedYBinSourceRange(View *v, int x, int y,
					     float &freqMin, float &freqMax,
					     float &adjFreqMin, float &adjFreqMax)
const
{
    FFTModel *fft = getFFTModel(v);
    if (!fft) return false;

    float s0 = 0, s1 = 0;
    if (!getXBinRange(v, x, s0, s1)) return false;

    float q0 = 0, q1 = 0;
    if (!getYBinRange(v, y, q0, q1)) return false;

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

            if (!fft->isColumnAvailable(s)) continue;

	    float binfreq = (sr * q) / m_windowSize;
	    if (q == q0i) freqMin = binfreq;
	    if (q == q1i) freqMax = binfreq;

	    if (peaksOnly && !fft->isLocalPeak(s, q)) continue;

	    if (!fft->isOverThreshold(s, q, m_threshold)) continue;

	    float freq = binfreq;
	    bool steady = false;
	    
	    if (s < int(fft->getWidth()) - 1) {

		freq = calculateFrequency(q, 
					  windowSize,
					  windowIncrement,
					  sr, 
					  fft->getPhaseAt(s, q),
					  fft->getPhaseAt(s+1, q),
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
SpectrogramLayer::getXYBinSourceRange(View *v, int x, int y,
				      float &min, float &max,
				      float &phaseMin, float &phaseMax) const
{
    float q0 = 0, q1 = 0;
    if (!getYBinRange(v, y, q0, q1)) return false;

    float s0 = 0, s1 = 0;
    if (!getXBinRange(v, x, s0, s1)) return false;
    
    int q0i = int(q0 + 0.001);
    int q1i = int(q1);

    int s0i = int(s0 + 0.001);
    int s1i = int(s1);

    bool rv = false;

    size_t zp = getZeroPadLevel(v);
    q0i *= zp + 1;
    q1i *= zp + 1;

    FFTModel *fft = getFFTModel(v);

    if (fft) {

        int cw = fft->getWidth();
        int ch = fft->getHeight();

        min = 0.0;
        max = 0.0;
        phaseMin = 0.0;
        phaseMax = 0.0;
        bool have = false;

        for (int q = q0i; q <= q1i; ++q) {
            for (int s = s0i; s <= s1i; ++s) {
                if (s >= 0 && q >= 0 && s < cw && q < ch) {

                    if (!fft->isColumnAvailable(s)) continue;
                    
                    float value;

                    value = fft->getPhaseAt(s, q);
                    if (!have || value < phaseMin) { phaseMin = value; }
                    if (!have || value > phaseMax) { phaseMax = value; }

                    value = fft->getMagnitudeAt(s, q);
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

    return rv;
}
   
size_t
SpectrogramLayer::getZeroPadLevel(const View *v) const
{
    //!!! tidy all this stuff

    if (m_binDisplay != AllBins) return 0;
    if (!Preferences::getInstance()->getSmoothSpectrogram()) return 0;
    if (m_frequencyScale == LogFrequencyScale) return 3;

    int sr = m_model->getSampleRate();
    
    size_t maxbin = m_fftSize / 2;
    if (m_maxFrequency > 0) {
	maxbin = int((double(m_maxFrequency) * m_fftSize) / sr + 0.1);
	if (maxbin > m_fftSize / 2) maxbin = m_fftSize / 2;
    }

    size_t minbin = 1;
    if (m_minFrequency > 0) {
	minbin = int((double(m_minFrequency) * m_fftSize) / sr + 0.1);
	if (minbin < 1) minbin = 1;
	if (minbin >= maxbin) minbin = maxbin - 1;
    }

    float perPixel =
        float(v->height()) /
        float((maxbin - minbin) / (m_zeroPadLevel + 1));

    if (perPixel > 2.8) {
        return 3; // 4x oversampling
    } else if (perPixel > 1.5) {
        return 1; // 2x
    } else {
        return 0; // 1x
    }
}

size_t
SpectrogramLayer::getFFTSize(const View *v) const
{
    return m_fftSize * (getZeroPadLevel(v) + 1);
}
	
FFTModel *
SpectrogramLayer::getFFTModel(const View *v) const
{
    if (!m_model) return 0;

    size_t fftSize = getFFTSize(v);

    if (m_fftModels.find(v) != m_fftModels.end()) {
        if (m_fftModels[v].first == 0) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
            std::cerr << "SpectrogramLayer::getFFTModel(" << v << "): Found null model" << std::endl;
#endif
            return 0;
        }
        if (m_fftModels[v].first->getHeight() != fftSize / 2 + 1) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
            std::cerr << "SpectrogramLayer::getFFTModel(" << v << "): Found a model with the wrong height (" << m_fftModels[v].first->getHeight() << ", wanted " << (fftSize / 2 + 1) << ")" << std::endl;
#endif
            delete m_fftModels[v].first;
            m_fftModels.erase(v);
        } else {
#ifdef DEBUG_SPECTROGRAM_REPAINT
            std::cerr << "SpectrogramLayer::getFFTModel(" << v << "): Found a good model" << std::endl;
#endif
            return m_fftModels[v].first;
        }
    }

    if (m_fftModels.find(v) == m_fftModels.end()) {

        FFTModel *model = new FFTModel(m_model,
                                       m_channel,
                                       m_windowType,
                                       m_windowSize,
                                       getWindowIncrement(),
                                       fftSize,
                                       true,
                                       m_candidateFillStartFrame);

        if (!model->isOK()) {
            QMessageBox::critical
                (0, tr("FFT cache failed"),
                 tr("Failed to create the FFT model for this spectrogram.\n"
                    "There may be insufficient memory or disc space to continue."));
            delete model;
            m_fftModels[v] = FFTFillPair(0, 0);
            return 0;
        }

        m_fftModels[v] = FFTFillPair(model, 0);

        model->resume();
        
        delete m_updateTimer;
        m_updateTimer = new QTimer((SpectrogramLayer *)this);
        connect(m_updateTimer, SIGNAL(timeout()),
                this, SLOT(fillTimerTimedOut()));
        m_updateTimer->start(200);
    }

    return m_fftModels[v].first;
}

void
SpectrogramLayer::invalidateFFTModels()
{
    for (ViewFFTMap::iterator i = m_fftModels.begin();
         i != m_fftModels.end(); ++i) {
        delete i->second.first;
    }
    
    m_fftModels.clear();
}

void
SpectrogramLayer::invalidateMagnitudes()
{
    m_viewMags.clear();
    for (std::vector<MagnitudeRange>::iterator i = m_columnMags.begin();
         i != m_columnMags.end(); ++i) {
        *i = MagnitudeRange();
    }
}

bool
SpectrogramLayer::updateViewMagnitudes(View *v) const
{
    MagnitudeRange mag;

    int x0 = 0, x1 = v->width();
    float s00 = 0, s01 = 0, s10 = 0, s11 = 0;
    
    getXBinRange(v, x0, s00, s01);
    getXBinRange(v, x1, s10, s11);

    int s0 = int(std::min(s00, s10) + 0.0001);
    int s1 = int(std::max(s01, s11));

    if (m_columnMags.size() <= s1) {
        m_columnMags.resize(s1 + 1);
    }

    for (int s = s0; s <= s1; ++s) {
        if (m_columnMags[s].isSet()) {
            mag.sample(m_columnMags[s]);
        }
    }

#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << "SpectrogramLayer::updateViewMagnitudes returning from cols "
              << s0 << " -> " << s1 << " inclusive" << std::endl;
#endif

    if (!mag.isSet()) return false;
    if (mag == m_viewMags[v]) return false;
    m_viewMags[v] = mag;
    return true;
}

void
SpectrogramLayer::paint(View *v, QPainter &paint, QRect rect) const
{
    if (m_colourScheme == BlackOnWhite) {
	v->setLightBackground(true);
    } else {
	v->setLightBackground(false);
    }

    Profiler profiler("SpectrogramLayer::paint", true);
#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << "SpectrogramLayer::paint(): m_model is " << m_model << ", zoom level is " << v->getZoomLevel() << ", m_updateTimer " << m_updateTimer << std::endl;
    
    std::cerr << "rect is " << rect.x() << "," << rect.y() << " " << rect.width() << "x" << rect.height() << std::endl;
#endif

    long startFrame = v->getStartFrame();
    if (startFrame < 0) m_candidateFillStartFrame = 0;
    else m_candidateFillStartFrame = startFrame;

    if (!m_model || !m_model->isOK() || !m_model->isReady()) {
	return;
    }

    if (isLayerDormant(v)) {
	std::cerr << "SpectrogramLayer::paint(): Layer is dormant, making it undormant again" << std::endl;
    }

    // Need to do this even if !isLayerDormant, as that could mean v
    // is not in the dormancy map at all -- we need it to be present
    // and accountable for when determining whether we need the cache
    // in the cache-fill thread above.
    //!!! no longer use cache-fill thread
    const_cast<SpectrogramLayer *>(this)->Layer::setLayerDormant(v, false);

    size_t fftSize = getFFTSize(v);
    FFTModel *fft = getFFTModel(v);
    if (!fft) {
	std::cerr << "ERROR: SpectrogramLayer::paint(): No FFT model, returning" << std::endl;
	return;
    }

    PixmapCache &cache = m_pixmapCaches[v];

#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << "SpectrogramLayer::paint(): pixmap cache valid area " << cache.validArea.x() << ", " << cache.validArea.y() << ", " << cache.validArea.width() << "x" << cache.validArea.height() << std::endl;
#endif

    bool stillCacheing = (m_updateTimer != 0);

#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << "SpectrogramLayer::paint(): Still cacheing = " << stillCacheing << std::endl;
#endif

    int zoomLevel = v->getZoomLevel();

    int x0 = 0;
    int x1 = v->width();
    int y0 = 0;
    int y1 = v->height();

    bool recreateWholePixmapCache = true;

    x0 = rect.left();
    x1 = rect.right() + 1;
    y0 = rect.top();
    y1 = rect.bottom() + 1;

    if (cache.validArea.width() > 0) {

	if (int(cache.zoomLevel) == zoomLevel &&
	    cache.pixmap.width() == v->width() &&
	    cache.pixmap.height() == v->height()) {

	    if (v->getXForFrame(cache.startFrame) ==
		v->getXForFrame(startFrame) &&
                cache.validArea.x() <= x0 &&
                cache.validArea.x() + cache.validArea.width() >= x1) {
	    
#ifdef DEBUG_SPECTROGRAM_REPAINT
		std::cerr << "SpectrogramLayer: pixmap cache good" << std::endl;
#endif

		paint.drawPixmap(rect, cache.pixmap, rect);
                illuminateLocalFeatures(v, paint);
		return;

	    } else {

#ifdef DEBUG_SPECTROGRAM_REPAINT
		std::cerr << "SpectrogramLayer: pixmap cache partially OK" << std::endl;
#endif

		recreateWholePixmapCache = false;

		int dx = v->getXForFrame(cache.startFrame) -
		         v->getXForFrame(startFrame);

#ifdef DEBUG_SPECTROGRAM_REPAINT
		std::cerr << "SpectrogramLayer: dx = " << dx << " (pixmap cache " << cache.pixmap.width() << "x" << cache.pixmap.height() << ")" << std::endl;
#endif

		if (dx != 0 &&
                    dx > -cache.pixmap.width() &&
                    dx <  cache.pixmap.width()) {

#if defined(Q_WS_WIN32) || defined(Q_WS_MAC)
		    // Copying a pixmap to itself doesn't work
		    // properly on Windows or Mac (it only works when
		    // moving in one direction).

		    //!!! Need a utility function for this

		    static QPixmap *tmpPixmap = 0;
		    if (!tmpPixmap ||
			tmpPixmap->width() != cache.pixmap.width() ||
			tmpPixmap->height() != cache.pixmap.height()) {
			delete tmpPixmap;
			tmpPixmap = new QPixmap(cache.pixmap.width(),
						cache.pixmap.height());
		    }
		    QPainter cachePainter;
		    cachePainter.begin(tmpPixmap);
		    cachePainter.drawPixmap(0, 0, cache.pixmap);
		    cachePainter.end();
		    cachePainter.begin(&cache.pixmap);
		    cachePainter.drawPixmap(dx, 0, *tmpPixmap);
		    cachePainter.end();
#else
		    QPainter cachePainter(&cache.pixmap);
		    cachePainter.drawPixmap(dx, 0, cache.pixmap);
		    cachePainter.end();
#endif

                    int px = cache.validArea.x();
                    int pw = cache.validArea.width();

		    if (dx < 0) {
			x0 = cache.pixmap.width() + dx;
			x1 = cache.pixmap.width();
                        px += dx;
                        if (px < 0) {
                            pw += px;
                            px = 0;
                            if (pw < 0) pw = 0;
                        }
		    } else {
			x0 = 0;
			x1 = dx;
                        px += dx;
                        if (px + pw > cache.pixmap.width()) {
                            pw = int(cache.pixmap.width()) - px;
                            if (pw < 0) pw = 0;
                        }
		    }
                    
                    cache.validArea =
                        QRect(px, cache.validArea.y(),
                              pw, cache.validArea.height());

		    paint.drawPixmap(rect & cache.validArea,
                                     cache.pixmap,
                                     rect & cache.validArea);
		}
	    }
	} else {
#ifdef DEBUG_SPECTROGRAM_REPAINT
	    std::cerr << "SpectrogramLayer: pixmap cache useless" << std::endl;
#endif
            cache.validArea = QRect();
	}
    }

/*
    if (stillCacheing) {
	x0 = rect.left();
	x1 = rect.right() + 1;
	y0 = rect.top();
	y1 = rect.bottom() + 1;
    }
*/

    if (updateViewMagnitudes(v)) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
        std::cerr << "SpectrogramLayer: magnitude range changed to [" << m_viewMags[v].getMin() << "->" << m_viewMags[v].getMax() << "]" << std::endl;
#endif
        recreateWholePixmapCache = true;
    } else {
#ifdef DEBUG_SPECTROGRAM_REPAINT
        std::cerr << "No change in magnitude range [" << m_viewMags[v].getMin() << "->" << m_viewMags[v].getMax() << "]" << std::endl;
#endif
    }

    if (recreateWholePixmapCache) {
        x0 = 0;
        x1 = v->width();
    }

    int paintBlockWidth = (300000 / zoomLevel);
    if (paintBlockWidth < 20) paintBlockWidth = 20;

    if (cache.validArea.width() > 0) {

        int vx0 = 0, vx1 = 0;
        vx0 = cache.validArea.x();
        vx1 = cache.validArea.x() + cache.validArea.width();
        
#ifdef DEBUG_SPECTROGRAM_REPAINT
        std::cerr << "x0 " << x0 << ", x1 " << x1 << ", vx0 " << vx0 << ", vx1 " << vx1 << ", paintBlockWidth " << paintBlockWidth << std::endl;
#endif            
        if (x0 < vx0) {
            if (x0 + paintBlockWidth < vx0) {
                x0 = vx0 - paintBlockWidth;
            } else {
                x0 = 0;
            }
        } else if (x0 > vx1) {
            x0 = vx1;
        }
            
        if (x1 < vx0) {
            x1 = vx0;
        } else if (x1 > vx1) {
            if (vx1 + paintBlockWidth < x1) {
                x1 = vx1 + paintBlockWidth;
            } else {
                x1 = v->width();
            }
        }
            
        cache.validArea = QRect
            (std::min(vx0, x0), cache.validArea.y(),
             std::max(vx1 - std::min(vx0, x0),
                      x1 - std::min(vx0, x0)),
             cache.validArea.height());
            
    } else {
        if (x1 > x0 + paintBlockWidth) {
            int sfx = x1;
            if (startFrame < 0) sfx = v->getXForFrame(0);
            if (sfx >= x0 && sfx + paintBlockWidth <= x1) {
                x0 = sfx;
                x1 = x0 + paintBlockWidth;
            } else {
                int mid = (x1 + x0) / 2;
                x0 = mid - paintBlockWidth/2;
                x1 = x0 + paintBlockWidth;
            }
        }
        cache.validArea = QRect(x0, 0, x1 - x0, v->height());
    }

    int w = x1 - x0;
    int h = y1 - y0;

#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << "x0 " << x0 << ", x1 " << x1 << ", w " << w << ", h " << h << std::endl;
#endif

    if (m_drawBuffer.width() < w || m_drawBuffer.height() < h) {
        m_drawBuffer = QImage(w, h, QImage::Format_RGB32);
    }

    m_drawBuffer.fill(m_colourMap.getColour(0).rgb());

    int sr = m_model->getSampleRate();

    // Set minFreq and maxFreq to the frequency extents of the possibly
    // zero-padded visible bin range, and displayMinFreq and displayMaxFreq
    // to the actual scale frequency extents (presumably not zero padded).
    
    size_t maxbin = fftSize / 2;
    if (m_maxFrequency > 0) {
	maxbin = int((double(m_maxFrequency) * fftSize) / sr + 0.1);
	if (maxbin > fftSize / 2) maxbin = fftSize / 2;
    }

    size_t minbin = 1;
    if (m_minFrequency > 0) {
	minbin = int((double(m_minFrequency) * fftSize) / sr + 0.1);
	if (minbin < 1) minbin = 1;
	if (minbin >= maxbin) minbin = maxbin - 1;
    }

    float minFreq = (float(minbin) * sr) / fftSize;
    float maxFreq = (float(maxbin) * sr) / fftSize;

    float displayMinFreq = minFreq;
    float displayMaxFreq = maxFreq;

    if (fftSize != m_fftSize) {
        displayMinFreq = getEffectiveMinFrequency();
        displayMaxFreq = getEffectiveMaxFrequency();
    }

    float ymag[h];
    float ydiv[h];
    float yval[maxbin + 1]; //!!! cache this?

    size_t increment = getWindowIncrement();
    
    bool logarithmic = (m_frequencyScale == LogFrequencyScale);

    for (size_t q = minbin; q <= maxbin; ++q) {
        float f0 = (float(q) * sr) / fftSize;
        yval[q] = v->getYForFrequency(f0, displayMinFreq, displayMaxFreq,
                                      logarithmic);
//        std::cerr << "min: " << minFreq << ", max: " << maxFreq << ", yval[" << q << "]: " << yval[q] << std::endl;
    }

    MagnitudeRange overallMag = m_viewMags[v];
    bool overallMagChanged = false;

    bool fftSuspended = false;

#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << (float(v->getFrameForX(1) - v->getFrameForX(0)) / increment) << " bins per pixel" << std::endl;
#endif

    for (int x = 0; x < w; ++x) {

	for (int y = 0; y < h; ++y) {
	    ymag[y] = 0.f;
	    ydiv[y] = 0.f;
	}

	float s0 = 0, s1 = 0;

	if (!getXBinRange(v, x0 + x, s0, s1)) {
	    assert(x <= m_drawBuffer.width());
	    continue;
	}

	int s0i = int(s0 + 0.001);
	int s1i = int(s1);

	if (s1i >= fft->getWidth()) {
	    if (s0i >= fft->getWidth()) {
		continue;
	    } else {
		s1i = s0i;
	    }
	}

        for (int s = s0i; s <= s1i; ++s) {

            if (!fft->isColumnAvailable(s)) continue;
            
            if (!fftSuspended) {
                fft->suspendWrites();
                fftSuspended = true;
            }

            MagnitudeRange mag;

            for (size_t q = minbin; q < maxbin; ++q) {

                float y0 = yval[q + 1];
                float y1 = yval[q];

		if (m_binDisplay == PeakBins ||
		    m_binDisplay == PeakFrequencies) {
		    if (!fft->isLocalPeak(s, q)) continue;
		}

                if (m_threshold != 0.f &&
                    !fft->isOverThreshold(s, q, m_threshold)) {
                    continue;
                }

		float sprop = 1.0;
		if (s == s0i) sprop *= (s + 1) - s0;
		if (s == s1i) sprop *= s1 - s;
 
		if (m_binDisplay == PeakFrequencies &&
		    s < int(fft->getWidth()) - 1) {

		    bool steady = false;
                    float f = calculateFrequency(q,
						 m_windowSize,
						 increment,
						 sr,
						 fft->getPhaseAt(s, q),
						 fft->getPhaseAt(s+1, q),
						 steady);

		    y0 = y1 = v->getYForFrequency
			(f, displayMinFreq, displayMaxFreq, logarithmic);
		}
		
		int y0i = int(y0 + 0.001);
		int y1i = int(y1);

                float value;
                
                if (m_colourScale == PhaseColourScale) {
                    value = fft->getPhaseAt(s, q);
                } else if (m_normalizeColumns) {
                    value = fft->getNormalizedMagnitudeAt(s, q);
                    mag.sample(value);
                    value *= m_gain;
                } else {
                    value = fft->getMagnitudeAt(s, q);
                    mag.sample(value);
                    value *= m_gain;
                }

		for (int y = y0i; y <= y1i; ++y) {
		    
		    if (y < 0 || y >= h) continue;

		    float yprop = sprop;
		    if (y == y0i) yprop *= (y + 1) - y0;
		    if (y == y1i) yprop *= y1 - y;
		    ymag[y] += yprop * value;
		    ydiv[y] += yprop;
		}
	    }

            if (mag.isSet()) {

                m_columnMags[s].sample(mag);

                if (overallMag.sample(mag)) {
                    //!!! scaling would change here
                    overallMagChanged = true;
                    std::cerr << "Overall mag changed (again?) at column " << s << ", to [" << overallMag.getMin() << "->" << overallMag.getMax() << "]" << std::endl;
                }
            }
	}

	for (int y = 0; y < h; ++y) {

	    if (ydiv[y] > 0.0) {

		unsigned char pixel = 0;

		float avg = ymag[y] / ydiv[y];
		pixel = getDisplayValue(v, avg);

		assert(x <= m_drawBuffer.width());
		QColor c = m_colourMap.getColour(pixel);
		m_drawBuffer.setPixel(x, y,
                                      qRgb(c.red(), c.green(), c.blue()));
	    }
	}
    }

    if (overallMagChanged) {
        m_viewMags[v] = overallMag;
        std::cerr << "Overall mag is now [" << m_viewMags[v].getMin() << "->" << m_viewMags[v].getMax() << "] - will be updating" << std::endl;
    } else {
        std::cerr << "Overall mag unchanged at [" << m_viewMags[v].getMin() << "->" << m_viewMags[v].getMax() << "]" << std::endl;
    }

    Profiler profiler2("SpectrogramLayer::paint: draw image", true);

    paint.drawImage(x0, y0, m_drawBuffer, 0, 0, w, h);

    if (recreateWholePixmapCache) {
	cache.pixmap = QPixmap(v->width(), v->height());
    }

    QPainter cachePainter(&cache.pixmap);
    cachePainter.drawImage(x0, y0, m_drawBuffer, 0, 0, w, h);
    cachePainter.end();

    if (!m_normalizeVisibleArea || !overallMagChanged) {
    
        cache.startFrame = startFrame;
        cache.zoomLevel = zoomLevel;

        if (cache.validArea.x() > 0) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
            std::cerr << "SpectrogramLayer::paint() updating left (0, "
                      << cache.validArea.x() << ")" << std::endl;
#endif
            v->update(0, 0, cache.validArea.x(), v->height());
        }
        
        if (cache.validArea.x() + cache.validArea.width() <
            cache.pixmap.width()) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
            std::cerr << "SpectrogramLayer::paint() updating right ("
                      << cache.validArea.x() + cache.validArea.width()
                      << ", "
                      << cache.pixmap.width() - (cache.validArea.x() +
                                                 cache.validArea.width())
                      << ")" << std::endl;
#endif
            v->update(cache.validArea.x() + cache.validArea.width(),
                      0,
                      cache.pixmap.width() - (cache.validArea.x() +
                                              cache.validArea.width()),
                      v->height());
        }
    } else {
        // overallMagChanged
        cache.validArea = QRect();
        v->update();
    }

    illuminateLocalFeatures(v, paint);

#ifdef DEBUG_SPECTROGRAM_REPAINT
    std::cerr << "SpectrogramLayer::paint() returning" << std::endl;
#endif

    if (fftSuspended) fft->resume();
}

void
SpectrogramLayer::illuminateLocalFeatures(View *v, QPainter &paint) const
{
    QPoint localPos;
    if (!v->shouldIlluminateLocalFeatures(this, localPos) || !m_model) {
        return;
    }

//    std::cerr << "SpectrogramLayer: illuminateLocalFeatures("
//              << localPos.x() << "," << localPos.y() << ")" << std::endl;

    float s0, s1;
    float f0, f1;

    if (getXBinRange(v, localPos.x(), s0, s1) &&
        getYBinSourceRange(v, localPos.y(), f0, f1)) {
        
        int s0i = int(s0 + 0.001);
        int s1i = int(s1);
        
        int x0 = v->getXForFrame(s0i * getWindowIncrement());
        int x1 = v->getXForFrame((s1i + 1) * getWindowIncrement());

        int y1 = getYForFrequency(v, f1);
        int y0 = getYForFrequency(v, f0);
        
//        std::cerr << "SpectrogramLayer: illuminate "
//                  << x0 << "," << y1 << " -> " << x1 << "," << y0 << std::endl;
        
        paint.setPen(Qt::white);

        //!!! should we be using paintCrosshairs for this?

        paint.drawRect(x0, y1, x1 - x0 + 1, y0 - y1 + 1);
    }
}

float
SpectrogramLayer::getYForFrequency(View *v, float frequency) const
{
    return v->getYForFrequency(frequency,
			       getEffectiveMinFrequency(),
			       getEffectiveMaxFrequency(),
			       m_frequencyScale == LogFrequencyScale);
}

float
SpectrogramLayer::getFrequencyForY(View *v, int y) const
{
    return v->getFrequencyForY(y,
			       getEffectiveMinFrequency(),
			       getEffectiveMaxFrequency(),
			       m_frequencyScale == LogFrequencyScale);
}

int
SpectrogramLayer::getCompletion(View *v) const
{
    if (m_updateTimer == 0) return 100;
    if (m_fftModels.find(v) == m_fftModels.end()) return 100;

    size_t completion = m_fftModels[v].first->getCompletion();
    std::cerr << "SpectrogramLayer::getCompletion: completion = " << completion << std::endl;
    return completion;
}

bool
SpectrogramLayer::getValueExtents(float &min, float &max,
                                  bool &logarithmic, QString &unit) const
{
//!!!
//    min = getEffectiveMinFrequency();
//    max = getEffectiveMaxFrequency();

    if (!m_model) return false;

    int sr = m_model->getSampleRate();
    min = float(sr) / m_fftSize;
    max = float(sr) / 2;
    
    logarithmic = (m_frequencyScale == LogFrequencyScale);
    unit = "Hz";
    return true;
}

bool
SpectrogramLayer::getDisplayExtents(float &min, float &max) const
{
    min = getEffectiveMinFrequency();
    max = getEffectiveMaxFrequency();
    return true;
}    

bool
SpectrogramLayer::setDisplayExtents(float min, float max)
{
    if (!m_model) return false;
    if (min < 0) min = 0;
    if (max > m_model->getSampleRate()/2) max = m_model->getSampleRate()/2;
    
    size_t minf = lrintf(min);
    size_t maxf = lrintf(max);

    if (m_minFrequency == minf && m_maxFrequency == maxf) return true;

    invalidatePixmapCaches();
    invalidateMagnitudes();

    m_minFrequency = minf;
    m_maxFrequency = maxf;
    
    emit layerParametersChanged();

    int vs = getCurrentVerticalZoomStep();
    if (vs != m_lastEmittedZoomStep) {
        emit verticalZoomChanged();
        m_lastEmittedZoomStep = vs;
    }

    return true;
}

bool
SpectrogramLayer::snapToFeatureFrame(View *v, int &frame,
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

bool
SpectrogramLayer::getCrosshairExtents(View *v, QPainter &paint,
                                      QPoint cursorPos,
                                      std::vector<QRect> &extents) const
{
    QRect vertical(cursorPos.x() - 12, 0, 12, v->height());
    extents.push_back(vertical);

    QRect horizontal(0, cursorPos.y(), cursorPos.x(), 1);
    extents.push_back(horizontal);

    return true;
}

void
SpectrogramLayer::paintCrosshairs(View *v, QPainter &paint,
                                  QPoint cursorPos) const
{
    paint.save();
    paint.setPen(m_crosshairColour);

    paint.drawLine(0, cursorPos.y(), cursorPos.x() - 1, cursorPos.y());
    paint.drawLine(cursorPos.x(), 0, cursorPos.x(), v->height());
    
    float fundamental = getFrequencyForY(v, cursorPos.y());

    int harmonic = 2;

    while (harmonic < 100) {

        float hy = lrintf(getYForFrequency(v, fundamental * harmonic));
        if (hy < 0 || hy > v->height()) break;
        
        int len = 7;

        if (harmonic % 2 == 0) {
            if (harmonic % 4 == 0) {
                len = 12;
            } else {
                len = 10;
            }
        }

        paint.drawLine(cursorPos.x() - len,
                       hy,
                       cursorPos.x(),
                       hy);

        ++harmonic;
    }

    paint.restore();
}

QString
SpectrogramLayer::getFeatureDescription(View *v, QPoint &pos) const
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

    if (!getXBinSourceRange(v, x, rtMin, rtMax)) {
	return "";
    }
    if (getXYBinSourceRange(v, x, y, magMin, magMax, phaseMin, phaseMax)) {
	haveValues = true;
    }

    QString adjFreqText = "", adjPitchText = "";

    if (m_binDisplay == PeakFrequencies) {

	if (!getAdjustedYBinSourceRange(v, x, y, freqMin, freqMax,
					adjFreqMin, adjFreqMax)) {
	    return "";
	}

	if (adjFreqMin != adjFreqMax) {
	    adjFreqText = tr("Peak Frequency:\t%1 - %2 Hz\n")
		.arg(adjFreqMin).arg(adjFreqMax);
	} else {
	    adjFreqText = tr("Peak Frequency:\t%1 Hz\n")
		.arg(adjFreqMin);
	}

	QString pmin = Pitch::getPitchLabelForFrequency(adjFreqMin);
	QString pmax = Pitch::getPitchLabelForFrequency(adjFreqMax);

	if (pmin != pmax) {
	    adjPitchText = tr("Peak Pitch:\t%3 - %4\n").arg(pmin).arg(pmax);
	} else {
	    adjPitchText = tr("Peak Pitch:\t%2\n").arg(pmin);
	}

    } else {
	
	if (!getYBinSourceRange(v, y, freqMin, freqMax)) return "";
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
	text += tr("%1Bin Frequency:\t%2 - %3 Hz\n%4Bin Pitch:\t%5 - %6\n")
	    .arg(adjFreqText)
	    .arg(freqMin)
	    .arg(freqMax)
	    .arg(adjPitchText)
	    .arg(Pitch::getPitchLabelForFrequency(freqMin))
	    .arg(Pitch::getPitchLabelForFrequency(freqMax));
    } else {
	text += tr("%1Bin Frequency:\t%2 Hz\n%3Bin Pitch:\t%4\n")
	    .arg(adjFreqText)
	    .arg(freqMin)
	    .arg(adjPitchText)
	    .arg(Pitch::getPitchLabelForFrequency(freqMin));
    }	

    if (haveValues) {
	float dbMin = AudioLevel::multiplier_to_dB(magMin);
	float dbMax = AudioLevel::multiplier_to_dB(magMax);
	QString dbMinString;
	QString dbMaxString;
	if (dbMin == AudioLevel::DB_FLOOR) {
	    dbMinString = tr("-Inf");
	} else {
	    dbMinString = QString("%1").arg(lrintf(dbMin));
	}
	if (dbMax == AudioLevel::DB_FLOOR) {
	    dbMaxString = tr("-Inf");
	} else {
	    dbMaxString = QString("%1").arg(lrintf(dbMax));
	}
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

    cw = paint.fontMetrics().width("-80dB");

/*!!!
    switch (m_colourScale) {
    default:
    case LinearColourScale:
	cw = paint.fontMetrics().width(QString("0.00"));
	break;

    case MeterColourScale:
    case dBColourScale:
    case OtherColourScale:
	cw = std::max(paint.fontMetrics().width(tr("-Inf")),
		      paint.fontMetrics().width(tr("-90")));
	break;

    case PhaseColourScale:
	cw = paint.fontMetrics().width(QString("-") + QChar(0x3c0));
	break;
    }
*/


    return cw;
}

int
SpectrogramLayer::getVerticalScaleWidth(View *v, QPainter &paint) const
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
SpectrogramLayer::paintVerticalScale(View *v, QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) {
	return;
    }

    Profiler profiler("SpectrogramLayer::paintVerticalScale", true);

    //!!! cache this?

    int h = rect.height(), w = rect.width();

    int tickw = (m_frequencyScale == LogFrequencyScale ? 10 : 4);
    int pkw = (m_frequencyScale == LogFrequencyScale ? 10 : 0);

    size_t bins = m_fftSize / 2;
    int sr = m_model->getSampleRate();

    if (m_maxFrequency > 0) {
	bins = int((double(m_maxFrequency) * m_fftSize) / sr + 0.1);
	if (bins > m_fftSize / 2) bins = m_fftSize / 2;
    }

    int cw = getColourScaleWidth(paint);
    int cbw = paint.fontMetrics().width("dB");

    int py = -1;
    int textHeight = paint.fontMetrics().height();
    int toff = -textHeight + paint.fontMetrics().ascent() + 2;

    if (h > textHeight * 3 + 10) {

        int topLines = 2;
        if (m_colourScale == PhaseColourScale) topLines = 1;

	int ch = h - textHeight * (topLines + 1) - 8;
//	paint.drawRect(4, textHeight + 4, cw - 1, ch + 1);
	paint.drawRect(4 + cw - cbw, textHeight * topLines + 4, cbw - 1, ch + 1);

	QString top, bottom;
/*!!!
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
	case OtherColourScale:
	    top = "0";
	    bottom = "-80";
	    break;

	case PhaseColourScale:
	    top = QChar(0x3c0);
	    bottom = "-" + top;
	    break;
	}
*/
        float min = m_viewMags[v].getMin();
        float max = m_viewMags[v].getMax();

        float dBmin = AudioLevel::multiplier_to_dB(min);
        float dBmax = AudioLevel::multiplier_to_dB(max);

        if (dBmax < -60.f) dBmax = -60.f;
        else top = QString("%1").arg(lrintf(dBmax));

        if (dBmin < dBmax - 60.f) dBmin = dBmax - 60.f;
        bottom = QString("%1").arg(lrintf(dBmin));

        //!!! & phase etc

        if (m_colourScale != PhaseColourScale) {
            paint.drawText((cw + 6 - paint.fontMetrics().width("dBFS")) / 2,
                           2 + textHeight + toff, "dBFS");
        }

//	paint.drawText((cw + 6 - paint.fontMetrics().width(top)) / 2,
	paint.drawText(3 + cw - cbw - paint.fontMetrics().width(top),
		       2 + textHeight * topLines + toff + textHeight/2, top);

	paint.drawText(3 + cw - cbw - paint.fontMetrics().width(bottom),
		       h + toff - 3 - textHeight/2, bottom);

	paint.save();
	paint.setBrush(Qt::NoBrush);

        int lasty = 0;
        int lastdb = 0;

	for (int i = 0; i < ch; ++i) {

            float dBval = dBmin + (((dBmax - dBmin) * i) / (ch - 1));
            int idb = int(dBval);

            float value = AudioLevel::dB_to_multiplier(dBval);
            int colour = getDisplayValue(v, value * m_gain);
/*
            float value = min + (((max - min) * i) / (ch - 1));
            if (value < m_threshold) value = 0.f;
            int colour = getDisplayValue(v, value * m_gain);
*/
/*
	    int colour = (i * 255) / ch + 1;
*/
	    paint.setPen(m_colourMap.getColour(colour));

            int y = textHeight * topLines + 4 + ch - i;

            paint.drawLine(5 + cw - cbw, y, cw + 2, y);

//	    paint.drawLine(5, 4 + textHeight + ch - i,
//			   cw + 2, 4 + textHeight + ch - i);


            if (i == 0) {
                lasty = y;
                lastdb = idb;
            } else if (i < ch - paint.fontMetrics().ascent() &&
                       idb != lastdb &&
                       ((abs(y - lasty) > textHeight && 
                         idb % 10 == 0) ||
                        (abs(y - lasty) > paint.fontMetrics().ascent() && 
                         idb % 5 == 0))) {
                paint.setPen(Qt::black);
                QString text = QString("%1").arg(idb);
                paint.drawText(3 + cw - cbw - paint.fontMetrics().width(text),
                               y + toff + textHeight/2, text);
                paint.setPen(Qt::white);
                paint.drawLine(5 + cw - cbw, y, 8 + cw - cbw, y);
                lasty = y;
                lastdb = idb;
            }
	}
	paint.restore();
    }

    paint.drawLine(cw + 7, 0, cw + 7, h);

    int bin = -1;

    for (int y = 0; y < v->height(); ++y) {

	float q0, q1;
	if (!getYBinRange(v, v->height() - y, q0, q1)) continue;

	int vy;

	if (int(q0) > bin) {
	    vy = y;
	    bin = int(q0);
	} else {
	    continue;
	}

	int freq = (sr * bin) / m_fftSize;

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

	int sr = m_model->getSampleRate();
	float minf = getEffectiveMinFrequency();
	float maxf = getEffectiveMaxFrequency();

	int py = h, ppy = h;
	paint.setBrush(paint.pen().color());

	for (int i = 0; i < 128; ++i) {

	    float f = Pitch::getFrequencyForPitch(i);
	    int y = lrintf(v->getYForFrequency(f, minf, maxf, true));

            if (y < -2) break;
            if (y > h + 2) {
                continue;
            }

	    int n = (i % 12);

            if (n == 1) {
                // C# -- fill the C from here
                if (ppy - y > 2) {
                    paint.fillRect(w - pkw,
//                                   y - (py - y) / 2 - (py - y) / 4, 
                                   y,
                                   pkw,
                                   (py + ppy) / 2 - y,
//                                   py - y + 1,
                                   Qt::gray);
                }
            }

	    if (n == 1 || n == 3 || n == 6 || n == 8 || n == 10) {
		// black notes
		paint.drawLine(w - pkw, y, w, y);
		int rh = ((py - y) / 4) * 2;
		if (rh < 2) rh = 2;
		paint.drawRect(w - pkw, y - (py-y)/4, pkw/2, rh);
	    } else if (n == 0 || n == 5) {
		// C, F
		if (py < h) {
		    paint.drawLine(w - pkw, (y + py) / 2, w, (y + py) / 2);
		}
	    }

            ppy = py;
	    py = y;
	}
    }
}

int
SpectrogramLayer::getVerticalZoomSteps(int &defaultStep) const
{
    // Vertical zoom step 0 shows the entire range from DC -> Nyquist
    // frequency.  Step 1 shows 2^(1/4) of the range of step 0, and so
    // on until the visible range is smaller than the frequency step
    // between bins at the current fft size.

    if (!m_model) return 0;
    
    float min, max;
    int sr = m_model->getSampleRate();
    min = float(sr) / m_fftSize;
    max = float(sr) / 2;
    
    float dist = max - min;

    int n = 0;
    defaultStep = 0;
    bool haveDefault = false;
    float s2 = sqrtf(sqrtf(2));
    while (dist > min) {
        if (!haveDefault && max <= m_initialMaxFrequency) {
            defaultStep = n;
            haveDefault = true;
        }
        ++n;
        dist /= s2;
        max = min + dist;
    }

    return n;
}

int
SpectrogramLayer::getCurrentVerticalZoomStep() const
{
    if (!m_model) return 0;

    float dmin, dmax;
    getDisplayExtents(dmin, dmax);
    
    float mmin, mmax;
    int sr = m_model->getSampleRate();
    mmin = float(sr) / m_fftSize;
    mmax = float(sr) / 2;
    
    float mdist = mmax - mmin;
    float ddist = dmax - dmin;
    
    int n = 0;
    int discard = 0;
    int m = getVerticalZoomSteps(discard);
    float s2 = sqrtf(sqrtf(2));
    while (mdist > ddist) {
        if (++n > m) break;
        mdist /= s2;
    }

    return n;
}

void
SpectrogramLayer::setVerticalZoomStep(int step)
{
    //!!! does not do the right thing for log scale

    float dmin, dmax;
    getDisplayExtents(dmin, dmax);
    
    float mmin, mmax;
    int sr = m_model->getSampleRate();
    mmin = float(sr) / m_fftSize;
    mmax = float(sr) / 2;
    
    float ddist = mmax - mmin;
    
    int n = 0;
    float s2 = sqrtf(sqrtf(2));
    while (n < step) {
        ddist /= s2;
        ++n;
    }

    float dmid = (dmax + dmin) / 2;
    float newmin = dmid - ddist / 2;
    float newmax = dmid + ddist / 2;
    
    if (newmin < mmin) newmin = mmin;
    if (newmax > mmax) newmax = mmax;
    
    setMinFrequency(newmin);
    setMaxFrequency(newmax);
}

QString
SpectrogramLayer::toXmlString(QString indent, QString extraAttributes) const
{
    QString s;
    
    s += QString("channel=\"%1\" "
		 "windowSize=\"%2\" "
		 "windowHopLevel=\"%3\" "
		 "gain=\"%4\" "
		 "threshold=\"%5\" ")
	.arg(m_channel)
	.arg(m_windowSize)
	.arg(m_windowHopLevel)
	.arg(m_gain)
	.arg(m_threshold);

    s += QString("minFrequency=\"%1\" "
		 "maxFrequency=\"%2\" "
		 "colourScale=\"%3\" "
		 "colourScheme=\"%4\" "
		 "colourRotation=\"%5\" "
		 "frequencyScale=\"%6\" "
		 "binDisplay=\"%7\" "
		 "normalizeColumns=\"%8\" "
                 "normalizeVisibleArea=\"%9\"")
	.arg(m_minFrequency)
	.arg(m_maxFrequency)
	.arg(m_colourScale)
	.arg(m_colourScheme)
	.arg(m_colourRotation)
	.arg(m_frequencyScale)
	.arg(m_binDisplay)
	.arg(m_normalizeColumns ? "true" : "false")
        .arg(m_normalizeVisibleArea ? "true" : "false");

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

    size_t windowHopLevel = attributes.value("windowHopLevel").toUInt(&ok);
    if (ok) setWindowHopLevel(windowHopLevel);
    else {
        size_t windowOverlap = attributes.value("windowOverlap").toUInt(&ok);
        // a percentage value
        if (ok) {
            if (windowOverlap == 0) setWindowHopLevel(0);
            else if (windowOverlap == 25) setWindowHopLevel(1);
            else if (windowOverlap == 50) setWindowHopLevel(2);
            else if (windowOverlap == 75) setWindowHopLevel(3);
            else if (windowOverlap == 90) setWindowHopLevel(4);
        }
    }

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

    bool normalizeVisibleArea =
	(attributes.value("normalizeVisibleArea").trimmed() == "true");
    setNormalizeVisibleArea(normalizeVisibleArea);
}
    
