/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2009 Chris Cannam and QMUL.
    
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
#include "base/LogRange.h"
#include "base/ColumnOp.h"
#include "widgets/CommandHistory.h"
#include "data/model/Dense3DModelPeakCache.h"

#include "ColourMapper.h"
#include "ImageRegionFinder.h"
#include "PianoScale.h"
#include "PaintAssistant.h"
#include "Colour3DPlotRenderer.h"

#include <QPainter>
#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QApplication>
#include <QMessageBox>
#include <QMouseEvent>
#include <QTextStream>
#include <QSettings>

#include <iostream>

#include <cassert>
#include <cmath>

#ifndef __GNUC__
#include <alloca.h>
#endif

#define DEBUG_SPECTROGRAM 1
#define DEBUG_SPECTROGRAM_REPAINT 1

using namespace std;

SpectrogramLayer::SpectrogramLayer(Configuration config) :
    m_model(0),
    m_channel(0),
    m_windowSize(1024),
    m_windowType(HanningWindow),
    m_windowHopLevel(2),
    m_gain(1.0),
    m_initialGain(1.0),
    m_threshold(0.0),
    m_initialThreshold(0.0),
    m_colourRotation(0),
    m_initialRotation(0),
    m_minFrequency(10),
    m_maxFrequency(8000),
    m_initialMaxFrequency(8000),
    m_colourScale(ColourScale::LogColourScale),
    m_colourMap(0),
    m_binScale(BinScale::Linear),
    m_binDisplay(BinDisplay::AllBins),
    m_normalization(ColumnNormalization::None),
    m_normalizeVisibleArea(false),
    m_lastEmittedZoomStep(-1),
    m_synchronous(false),
    m_haveDetailedScale(false),
    m_exiting(false),
    m_fftModel(0),
    m_peakCache(0),
    m_peakCacheDivisor(8)
{
    QString colourConfigName = "spectrogram-colour";
    int colourConfigDefault = int(ColourMapper::Green);
    
    if (config == FullRangeDb) {
        m_initialMaxFrequency = 0;
        setMaxFrequency(0);
    } else if (config == MelodicRange) {
	setWindowSize(8192);
	setWindowHopLevel(4);
        m_initialMaxFrequency = 1500;
	setMaxFrequency(1500);
        setMinFrequency(40);
	setColourScale(ColourScale::LinearColourScale);
        setColourMap(ColourMapper::Sunset);
        setBinScale(BinScale::Log);
        colourConfigName = "spectrogram-melodic-colour";
        colourConfigDefault = int(ColourMapper::Sunset);
//        setGain(20);
    } else if (config == MelodicPeaks) {
	setWindowSize(4096);
	setWindowHopLevel(5);
        m_initialMaxFrequency = 2000;
	setMaxFrequency(2000);
	setMinFrequency(40);
	setBinScale(BinScale::Log);
	setColourScale(ColourScale::LinearColourScale);
	setBinDisplay(BinDisplay::PeakFrequencies);
        setNormalization(ColumnNormalization::Max1);
        colourConfigName = "spectrogram-melodic-colour";
        colourConfigDefault = int(ColourMapper::Sunset);
    }

    QSettings settings;
    settings.beginGroup("Preferences");
    setColourMap(settings.value(colourConfigName, colourConfigDefault).toInt());
    settings.endGroup();
    
    Preferences *prefs = Preferences::getInstance();
    connect(prefs, SIGNAL(propertyChanged(PropertyContainer::PropertyName)),
            this, SLOT(preferenceChanged(PropertyContainer::PropertyName)));
    setWindowType(prefs->getWindowType());

    initialisePalette();
}

SpectrogramLayer::~SpectrogramLayer()
{
    invalidateImageCaches();
    invalidateFFTModel();
}

ColourScale::Scale
SpectrogramLayer::convertToColourScale(int value)
{
    switch (value) {
    case 0: return ColourScale::LinearColourScale;
    case 1: return ColourScale::MeterColourScale;
    case 2: return ColourScale::LogColourScale; //!!! db^2
    case 3: return ColourScale::LogColourScale;
    case 4: return ColourScale::PhaseColourScale;
    default: return ColourScale::LinearColourScale;
    }
}

int
SpectrogramLayer::convertFromColourScale(ColourScale::Scale scale)
{
    switch (scale) {
    case ColourScale::LinearColourScale: return 0;
    case ColourScale::MeterColourScale: return 1;
    case ColourScale::LogColourScale: return 3; //!!! + db^2
    case ColourScale::PhaseColourScale: return 4;

    case ColourScale::PlusMinusOneScale:
    case ColourScale::AbsoluteScale:
    default: return 0;
    }
}

std::pair<ColumnNormalization, bool>
SpectrogramLayer::convertToColumnNorm(int value)
{
    switch (value) {
    default:
    case 0: return { ColumnNormalization::None, false };
    case 1: return { ColumnNormalization::Max1, false };
    case 2: return { ColumnNormalization::None, true }; // visible area
    case 3: return { ColumnNormalization::Hybrid, false };
    }
}

int
SpectrogramLayer::convertFromColumnNorm(ColumnNormalization norm, bool visible)
{
    if (visible) return 2;
    switch (norm) {
    case ColumnNormalization::None: return 0;
    case ColumnNormalization::Max1: return 1;
    case ColumnNormalization::Hybrid: return 3;

    case ColumnNormalization::Sum1:
    default: return 0;
    }
}

void
SpectrogramLayer::setModel(const DenseTimeValueModel *model)
{
//    cerr << "SpectrogramLayer(" << this << "): setModel(" << model << ")" << endl;

    if (model == m_model) return;

    m_model = model;
    invalidateFFTModel();

    if (!m_model || !m_model->isOK()) return;

    connectSignals(m_model);

    connect(m_model, SIGNAL(modelChanged()), this, SLOT(cacheInvalid()));
    connect(m_model, SIGNAL(modelChangedWithin(sv_frame_t, sv_frame_t)),
	    this, SLOT(cacheInvalid(sv_frame_t, sv_frame_t)));

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
    list.push_back("Normalization");
    list.push_back("Bin Display");
    list.push_back("Threshold");
    list.push_back("Gain");
    list.push_back("Colour Rotation");
//    list.push_back("Min Frequency");
//    list.push_back("Max Frequency");
    list.push_back("Frequency Scale");
    return list;
}

QString
SpectrogramLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Colour") return tr("Colour");
    if (name == "Colour Scale") return tr("Colour Scale");
    if (name == "Window Size") return tr("Window Size");
    if (name == "Window Increment") return tr("Window Overlap");
    if (name == "Normalization") return tr("Normalization");
    if (name == "Bin Display") return tr("Bin Display");
    if (name == "Threshold") return tr("Threshold");
    if (name == "Gain") return tr("Gain");
    if (name == "Colour Rotation") return tr("Colour Rotation");
    if (name == "Min Frequency") return tr("Min Frequency");
    if (name == "Max Frequency") return tr("Max Frequency");
    if (name == "Frequency Scale") return tr("Frequency Scale");
    return "";
}

QString
SpectrogramLayer::getPropertyIconName(const PropertyName &) const
{
    return "";
}

Layer::PropertyType
SpectrogramLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Gain") return RangeProperty;
    if (name == "Colour Rotation") return RangeProperty;
    if (name == "Threshold") return RangeProperty;
    return ValueProperty;
}

QString
SpectrogramLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Bin Display" ||
        name == "Frequency Scale") return tr("Bins");
    if (name == "Window Size" ||
	name == "Window Increment") return tr("Window");
    if (name == "Colour" ||
	name == "Threshold" ||
	name == "Colour Rotation") return tr("Colour");
    if (name == "Normalization" ||
        name == "Gain" ||
	name == "Colour Scale") return tr("Scale");
    return QString();
}

int
SpectrogramLayer::getPropertyRangeAndValue(const PropertyName &name,
					   int *min, int *max, int *deflt) const
{
    int val = 0;

    int garbage0, garbage1, garbage2;
    if (!min) min = &garbage0;
    if (!max) max = &garbage1;
    if (!deflt) deflt = &garbage2;

    if (name == "Gain") {

	*min = -50;
	*max = 50;

        *deflt = int(lrint(log10(m_initialGain) * 20.0));
	if (*deflt < *min) *deflt = *min;
	if (*deflt > *max) *deflt = *max;

	val = int(lrint(log10(m_gain) * 20.0));
	if (val < *min) val = *min;
	if (val > *max) val = *max;

    } else if (name == "Threshold") {

	*min = -50;
	*max = 0;

        *deflt = int(lrint(AudioLevel::multiplier_to_dB(m_initialThreshold)));
	if (*deflt < *min) *deflt = *min;
	if (*deflt > *max) *deflt = *max;

	val = int(lrint(AudioLevel::multiplier_to_dB(m_threshold)));
	if (val < *min) val = *min;
	if (val > *max) val = *max;

    } else if (name == "Colour Rotation") {

	*min = 0;
	*max = 256;
        *deflt = m_initialRotation;

	val = m_colourRotation;

    } else if (name == "Colour Scale") {

        // linear, meter, db^2, db, phase
	*min = 0;
	*max = 4;
        *deflt = 2;

	val = convertFromColourScale(m_colourScale);

    } else if (name == "Colour") {

	*min = 0;
	*max = ColourMapper::getColourMapCount() - 1;
        *deflt = 0;

	val = m_colourMap;

    } else if (name == "Window Size") {

	*min = 0;
	*max = 10;
        *deflt = 5;
	
	val = 0;
	int ws = m_windowSize;
	while (ws > 32) { ws >>= 1; val ++; }

    } else if (name == "Window Increment") {
	
	*min = 0;
	*max = 5;
        *deflt = 2;

        val = m_windowHopLevel;
    
    } else if (name == "Min Frequency") {

	*min = 0;
	*max = 9;
        *deflt = 1;

	switch (m_minFrequency) {
	case 0: default: val = 0; break;
	case 10: val = 1; break;
	case 20: val = 2; break;
	case 40: val = 3; break;
	case 100: val = 4; break;
	case 250: val = 5; break;
	case 500: val = 6; break;
	case 1000: val = 7; break;
	case 4000: val = 8; break;
	case 10000: val = 9; break;
	}
    
    } else if (name == "Max Frequency") {

	*min = 0;
	*max = 9;
        *deflt = 6;

	switch (m_maxFrequency) {
	case 500: val = 0; break;
	case 1000: val = 1; break;
	case 1500: val = 2; break;
	case 2000: val = 3; break;
	case 4000: val = 4; break;
	case 6000: val = 5; break;
	case 8000: val = 6; break;
	case 12000: val = 7; break;
	case 16000: val = 8; break;
	default: val = 9; break;
	}

    } else if (name == "Frequency Scale") {

	*min = 0;
	*max = 1;
        *deflt = int(BinScale::Linear);
	val = (int)m_binScale;

    } else if (name == "Bin Display") {

	*min = 0;
	*max = 2;
        *deflt = int(BinDisplay::AllBins);
	val = (int)m_binDisplay;

    } else if (name == "Normalization") {
	
        *min = 0;
        *max = 3;
        *deflt = 0;
        
        val = convertFromColumnNorm(m_normalization, m_normalizeVisibleArea);

    } else {
	val = Layer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
SpectrogramLayer::getPropertyValueLabel(const PropertyName &name,
					int value) const
{
    if (name == "Colour") {
        return ColourMapper::getColourMapName(value);
    }
    if (name == "Colour Scale") {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Meter");
	case 2: return tr("dBV^2");
	case 3: return tr("dBV");
	case 4: return tr("Phase");
	}
    }
    if (name == "Normalization") {
        return ""; // icon only
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

QString
SpectrogramLayer::getPropertyValueIconName(const PropertyName &name,
                                           int value) const
{
    if (name == "Normalization") {
        switch(value) {
        default:
        case 0: return "normalise-none";
        case 1: return "normalise-columns";
        case 2: return "normalise";
        case 3: return "normalise-hybrid";
        }
    }
    return "";
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
	setGain(float(pow(10, float(value)/20.0)));
    } else if (name == "Threshold") {
	if (value == -50) setThreshold(0.0);
	else setThreshold(float(AudioLevel::dB_to_multiplier(value)));
    } else if (name == "Colour Rotation") {
	setColourRotation(value);
    } else if (name == "Colour") {
        setColourMap(value);
    } else if (name == "Window Size") {
	setWindowSize(32 << value);
    } else if (name == "Window Increment") {
        setWindowHopLevel(value);
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
	case 0: setColourScale(ColourScale::LinearColourScale); break;
	case 1: setColourScale(ColourScale::MeterColourScale); break;
	case 2: setColourScale(ColourScale::LogColourScale); break; //!!! dB^2
	case 3: setColourScale(ColourScale::LogColourScale); break;
	case 4: setColourScale(ColourScale::PhaseColourScale); break;
	}
    } else if (name == "Frequency Scale") {
	switch (value) {
	default:
	case 0: setBinScale(BinScale::Linear); break;
	case 1: setBinScale(BinScale::Log); break;
	}
    } else if (name == "Bin Display") {
	switch (value) {
	default:
	case 0: setBinDisplay(BinDisplay::AllBins); break;
	case 1: setBinDisplay(BinDisplay::PeakBins); break;
	case 2: setBinDisplay(BinDisplay::PeakFrequencies); break;
	}
    } else if (name == "Normalization") {
        auto n = convertToColumnNorm(value);
        setNormalization(n.first);
        setNormalizeVisibleArea(n.second);
    }
}

void
SpectrogramLayer::invalidateImageCaches()
{
#ifdef DEBUG_SPECTROGRAM
    cerr << "SpectrogramLayer::invalidateImageCaches called" << endl;
#endif
    for (ViewImageCache::iterator i = m_imageCaches.begin();
         i != m_imageCaches.end(); ++i) {
        i->second.invalidate();
    }

    //!!!
    for (ViewRendererMap::iterator i = m_renderers.begin();
         i != m_renderers.end(); ++i) {
        delete i->second;
    }
    m_renderers.clear();
}

void
SpectrogramLayer::preferenceChanged(PropertyContainer::PropertyName name)
{
    SVDEBUG << "SpectrogramLayer::preferenceChanged(" << name << ")" << endl;

    if (name == "Window Type") {
        setWindowType(Preferences::getInstance()->getWindowType());
        return;
    }
    if (name == "Spectrogram Y Smoothing") {
        setWindowSize(m_windowSize);
        invalidateImageCaches();
        invalidateMagnitudes();
        emit layerParametersChanged();
    }
    if (name == "Spectrogram X Smoothing") {
        invalidateImageCaches();
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

    invalidateImageCaches();
    m_channel = ch;
    invalidateFFTModel();

    emit layerParametersChanged();
}

int
SpectrogramLayer::getChannel() const
{
    return m_channel;
}

int
SpectrogramLayer::getFFTOversampling() const
{
    if (m_binDisplay != BinDisplay::AllBins) {
        return 1;
    }

    Preferences::SpectrogramSmoothing smoothing = 
        Preferences::getInstance()->getSpectrogramSmoothing();
    
    if (smoothing == Preferences::NoSpectrogramSmoothing ||
        smoothing == Preferences::SpectrogramInterpolated) {
        return 1;
    }

    return 4;
}

int
SpectrogramLayer::getFFTSize() const
{
    return m_windowSize * getFFTOversampling();
}

void
SpectrogramLayer::setWindowSize(int ws)
{
    if (m_windowSize == ws) return;

    invalidateImageCaches();
    
    m_windowSize = ws;
    
    invalidateFFTModel();

    emit layerParametersChanged();
}

int
SpectrogramLayer::getWindowSize() const
{
    return m_windowSize;
}

void
SpectrogramLayer::setWindowHopLevel(int v)
{
    if (m_windowHopLevel == v) return;

    invalidateImageCaches();
    
    m_windowHopLevel = v;
    
    invalidateFFTModel();

    emit layerParametersChanged();

//    fillCache();
}

int
SpectrogramLayer::getWindowHopLevel() const
{
    return m_windowHopLevel;
}

void
SpectrogramLayer::setWindowType(WindowType w)
{
    if (m_windowType == w) return;

    invalidateImageCaches();
    
    m_windowType = w;

    invalidateFFTModel();

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
//    SVDEBUG << "SpectrogramLayer::setGain(" << gain << ") (my gain is now "
//	      << m_gain << ")" << endl;

    if (m_gain == gain) return;

    invalidateImageCaches();
    
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

    invalidateImageCaches();
    
    m_threshold = threshold;

    emit layerParametersChanged();
}

float
SpectrogramLayer::getThreshold() const
{
    return m_threshold;
}

void
SpectrogramLayer::setMinFrequency(int mf)
{
    if (m_minFrequency == mf) return;

//    SVDEBUG << "SpectrogramLayer::setMinFrequency: " << mf << endl;

    invalidateImageCaches();
    invalidateMagnitudes();
    
    m_minFrequency = mf;

    emit layerParametersChanged();
}

int
SpectrogramLayer::getMinFrequency() const
{
    return m_minFrequency;
}

void
SpectrogramLayer::setMaxFrequency(int mf)
{
    if (m_maxFrequency == mf) return;

//    SVDEBUG << "SpectrogramLayer::setMaxFrequency: " << mf << endl;

    invalidateImageCaches();
    invalidateMagnitudes();
    
    m_maxFrequency = mf;
    
    emit layerParametersChanged();
}

int
SpectrogramLayer::getMaxFrequency() const
{
    return m_maxFrequency;
}

void
SpectrogramLayer::setColourRotation(int r)
{
    invalidateImageCaches();

    if (r < 0) r = 0;
    if (r > 256) r = 256;
    int distance = r - m_colourRotation;

    if (distance != 0) {
	rotatePalette(-distance);
	m_colourRotation = r;
    }
    
    emit layerParametersChanged();
}

void
SpectrogramLayer::setColourScale(ColourScale::Scale colourScale)
{
    if (m_colourScale == colourScale) return;

    invalidateImageCaches();
    
    m_colourScale = colourScale;
    
    emit layerParametersChanged();
}

ColourScale::Scale
SpectrogramLayer::getColourScale() const
{
    return m_colourScale;
}

void
SpectrogramLayer::setColourMap(int map)
{
    if (m_colourMap == map) return;

    invalidateImageCaches();
    
    m_colourMap = map;
    initialisePalette();

    emit layerParametersChanged();
}

int
SpectrogramLayer::getColourMap() const
{
    return m_colourMap;
}

void
SpectrogramLayer::setBinScale(BinScale binScale)
{
    if (m_binScale == binScale) return;

    invalidateImageCaches();
    m_binScale = binScale;

    emit layerParametersChanged();
}

BinScale
SpectrogramLayer::getBinScale() const
{
    return m_binScale;
}

void
SpectrogramLayer::setBinDisplay(BinDisplay binDisplay)
{
    if (m_binDisplay == binDisplay) return;

    invalidateImageCaches();
    m_binDisplay = binDisplay;

    emit layerParametersChanged();
}

BinDisplay
SpectrogramLayer::getBinDisplay() const
{
    return m_binDisplay;
}

void
SpectrogramLayer::setNormalization(ColumnNormalization n)
{
    if (m_normalization == n) return;

    invalidateImageCaches();
    invalidateMagnitudes();
    m_normalization = n;

    emit layerParametersChanged();
}

ColumnNormalization
SpectrogramLayer::getNormalization() const
{
    return m_normalization;
}

void
SpectrogramLayer::setNormalizeVisibleArea(bool n)
{
    if (m_normalizeVisibleArea == n) return;

    invalidateImageCaches();
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
SpectrogramLayer::setLayerDormant(const LayerGeometryProvider *v, bool dormant)
{
    if (dormant) {

#ifdef DEBUG_SPECTROGRAM_REPAINT
        cerr << "SpectrogramLayer::setLayerDormant(" << dormant << ")"
                  << endl;
#endif

        if (isLayerDormant(v)) {
            return;
        }

        Layer::setLayerDormant(v, true);

        const View *view = v->getView();
        
	invalidateImageCaches();

        m_imageCaches.erase(view->getId());

        //!!! in theory we should call invalidateFFTModel() if and
        //!!! only if there are no remaining views in which we are not
        //!!! dormant
	
    } else {

        Layer::setLayerDormant(v, false);
    }
}

void
SpectrogramLayer::cacheInvalid()
{
#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer::cacheInvalid()" << endl;
#endif

    invalidateImageCaches();
    invalidateMagnitudes();
}

void
SpectrogramLayer::cacheInvalid(
#ifdef DEBUG_SPECTROGRAM_REPAINT
    sv_frame_t from, sv_frame_t to
#else 
    sv_frame_t     , sv_frame_t
#endif
    )
{
#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer::cacheInvalid(" << from << ", " << to << ")" << endl;
#endif

    // We used to call invalidateMagnitudes(from, to) to invalidate
    // only those caches whose views contained some of the (from, to)
    // range. That's the right thing to do; it has been lost in
    // pulling out the image cache code, but it might not matter very
    // much, since the underlying models for spectrogram layers don't
    // change very often. Let's see.
    invalidateImageCaches();
    invalidateMagnitudes();
}

bool
SpectrogramLayer::hasLightBackground() const 
{
    return ColourMapper(m_colourMap, 1.f, 255.f).hasLightBackground();
}

void
SpectrogramLayer::initialisePalette()
{
    int formerRotation = m_colourRotation;

    if (m_colourMap == (int)ColourMapper::BlackOnWhite) {
	m_palette.setColour(NO_VALUE, Qt::white);
    } else {
	m_palette.setColour(NO_VALUE, Qt::black);
    }

    ColourMapper mapper(m_colourMap, 1.f, 255.f);
    
    for (int pixel = 1; pixel < 256; ++pixel) {
        m_palette.setColour((unsigned char)pixel, mapper.map(pixel));
    }

    m_crosshairColour = mapper.getContrastingColour();

    m_colourRotation = 0;
    rotatePalette(m_colourRotation - formerRotation);
    m_colourRotation = formerRotation;

    m_drawBuffer = QImage();
}

void
SpectrogramLayer::rotatePalette(int distance)
{
    QColor newPixels[256];

    newPixels[NO_VALUE] = m_palette.getColour(NO_VALUE);

    for (int pixel = 1; pixel < 256; ++pixel) {
	int target = pixel + distance;
	while (target < 1) target += 255;
	while (target > 255) target -= 255;
	newPixels[target] = m_palette.getColour((unsigned char)pixel);
    }

    for (int pixel = 0; pixel < 256; ++pixel) {
	m_palette.setColour((unsigned char)pixel, newPixels[pixel]);
    }

    m_drawBuffer = QImage();
}

unsigned char
SpectrogramLayer::getDisplayValue(LayerGeometryProvider *v, double input) const
{
    int value = 0;

    double min = 0.0;
    double max = 1.0;

    if (m_normalizeVisibleArea) {
        min = m_viewMags[v->getId()].getMin();
        max = m_viewMags[v->getId()].getMax();
    } else if (m_normalization != ColumnNormalization::Max1) {
        if (m_colourScale == ColourScale::LinearColourScale //||
//            m_colourScale == MeterColourScale) {
            ) {
            max = 0.1;
        }
    }

    double thresh = -80.0;

    if (max == 0.0) max = 1.0;
    if (max == min) min = max - 0.0001;

    switch (m_colourScale) {
	
    case ColourScale::LinearColourScale:
        value = int(((input - min) / (max - min)) * 255.0) + 1;
	break;
	
    case ColourScale::MeterColourScale:
        value = AudioLevel::multiplier_to_preview
            ((input - min) / (max - min), 254) + 1;
	break;

        //!!! check this
/*    case dBSquaredColourScale:
        input = ((input - min) * (input - min)) / ((max - min) * (max - min));
        if (input > 0.0) {
            input = 10.0 * log10(input);
        } else {
            input = thresh;
        }
        if (min > 0.0) {
            thresh = 10.0 * log10(min * min);
            if (thresh < -80.0) thresh = -80.0;
        }
	input = (input - thresh) / (-thresh);
	if (input < 0.0) input = 0.0;
	if (input > 1.0) input = 1.0;
	value = int(input * 255.0) + 1;
	break;
*/	
    case ColourScale::LogColourScale:
        //!!! experiment with normalizing the visible area this way.
        //In any case, we need to have some indication of what the dB
        //scale is relative to.
        input = (input - min) / (max - min);
        if (input > 0.0) {
            input = 10.0 * log10(input);
        } else {
            input = thresh;
        }
        if (min > 0.0) {
            thresh = 10.0 * log10(min);
            if (thresh < -80.0) thresh = -80.0;
        }
	input = (input - thresh) / (-thresh);
	if (input < 0.0) input = 0.0;
	if (input > 1.0) input = 1.0;
	value = int(input * 255.0) + 1;
	break;
	
    case ColourScale::PhaseColourScale:
	value = int((input * 127.0 / M_PI) + 128);
	break;

    case ColourScale::PlusMinusOneScale:
    case ColourScale::AbsoluteScale:
    default:
        ;
    }

    if (value > UCHAR_MAX) value = UCHAR_MAX;
    if (value < 0) value = 0;
    return (unsigned char)value;
}

double
SpectrogramLayer::getEffectiveMinFrequency() const
{
    sv_samplerate_t sr = m_model->getSampleRate();
    double minf = double(sr) / getFFTSize();

    if (m_minFrequency > 0.0) {
	int minbin = int((double(m_minFrequency) * getFFTSize()) / sr + 0.01);
	if (minbin < 1) minbin = 1;
	minf = minbin * sr / getFFTSize();
    }

    return minf;
}

double
SpectrogramLayer::getEffectiveMaxFrequency() const
{
    sv_samplerate_t sr = m_model->getSampleRate();
    double maxf = double(sr) / 2;

    if (m_maxFrequency > 0.0) {
	int maxbin = int((double(m_maxFrequency) * getFFTSize()) / sr + 0.1);
	if (maxbin > getFFTSize() / 2) maxbin = getFFTSize() / 2;
	maxf = maxbin * sr / getFFTSize();
    }

    return maxf;
}

bool
SpectrogramLayer::getYBinRange(LayerGeometryProvider *v, int y, double &q0, double &q1) const
{
    Profiler profiler("SpectrogramLayer::getYBinRange");
    
    int h = v->getPaintHeight();
    if (y < 0 || y >= h) return false;

    sv_samplerate_t sr = m_model->getSampleRate();
    double minf = getEffectiveMinFrequency();
    double maxf = getEffectiveMaxFrequency();

    bool logarithmic = (m_binScale == BinScale::Log);

    q0 = v->getFrequencyForY(y, minf, maxf, logarithmic);
    q1 = v->getFrequencyForY(y - 1, minf, maxf, logarithmic);

    // Now map these on to ("proportions of") actual bins

    q0 = (q0 * getFFTSize()) / sr;
    q1 = (q1 * getFFTSize()) / sr;

    return true;
}

double
SpectrogramLayer::getYForBin(LayerGeometryProvider *, double) const {
    //!!! not implemented
    throw std::logic_error("not implemented");
}

double
SpectrogramLayer::getBinForY(LayerGeometryProvider *v, double y) const
{
    //!!! overlap with range methods above (but using double arg)
    //!!! tidy this
    
    int h = v->getPaintHeight();
    if (y < 0 || y >= h) return false;

    sv_samplerate_t sr = m_model->getSampleRate();
    double minf = getEffectiveMinFrequency();
    double maxf = getEffectiveMaxFrequency();

    bool logarithmic = (m_binScale == BinScale::Log);

    double q = v->getFrequencyForY(y, minf, maxf, logarithmic);

    // Now map on to ("proportions of") actual bins

    q = (q * getFFTSize()) / sr;

    return q;
}

bool
SpectrogramLayer::getXBinRange(LayerGeometryProvider *v, int x, double &s0, double &s1) const
{
    sv_frame_t modelStart = m_model->getStartFrame();
    sv_frame_t modelEnd = m_model->getEndFrame();

    // Each pixel column covers an exact range of sample frames:
    sv_frame_t f0 = v->getFrameForX(x) - modelStart;
    sv_frame_t f1 = v->getFrameForX(x + 1) - modelStart - 1;

    if (f1 < int(modelStart) || f0 > int(modelEnd)) {
	return false;
    }
      
    // And that range may be drawn from a possibly non-integral
    // range of spectrogram windows:

    int windowIncrement = getWindowIncrement();
    s0 = double(f0) / windowIncrement;
    s1 = double(f1) / windowIncrement;

    return true;
}
 
bool
SpectrogramLayer::getXBinSourceRange(LayerGeometryProvider *v, int x, RealTime &min, RealTime &max) const
{
    double s0 = 0, s1 = 0;
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
SpectrogramLayer::getYBinSourceRange(LayerGeometryProvider *v, int y, double &freqMin, double &freqMax)
const
{
    double q0 = 0, q1 = 0;
    if (!getYBinRange(v, y, q0, q1)) return false;

    int q0i = int(q0 + 0.001);
    int q1i = int(q1);

    sv_samplerate_t sr = m_model->getSampleRate();

    for (int q = q0i; q <= q1i; ++q) {
	if (q == q0i) freqMin = (sr * q) / getFFTSize();
	if (q == q1i) freqMax = (sr * (q+1)) / getFFTSize();
    }
    return true;
}

bool
SpectrogramLayer::getAdjustedYBinSourceRange(LayerGeometryProvider *v, int x, int y,
					     double &freqMin, double &freqMax,
					     double &adjFreqMin, double &adjFreqMax)
const
{
    if (!m_model || !m_model->isOK() || !m_model->isReady()) {
	return false;
    }

    FFTModel *fft = getFFTModel();
    if (!fft) return false;

    double s0 = 0, s1 = 0;
    if (!getXBinRange(v, x, s0, s1)) return false;

    double q0 = 0, q1 = 0;
    if (!getYBinRange(v, y, q0, q1)) return false;

    int s0i = int(s0 + 0.001);
    int s1i = int(s1);

    int q0i = int(q0 + 0.001);
    int q1i = int(q1);

    sv_samplerate_t sr = m_model->getSampleRate();

    bool haveAdj = false;

    bool peaksOnly = (m_binDisplay == BinDisplay::PeakBins ||
		      m_binDisplay == BinDisplay::PeakFrequencies);

    for (int q = q0i; q <= q1i; ++q) {

	for (int s = s0i; s <= s1i; ++s) {

	    double binfreq = (double(sr) * q) / m_windowSize;
	    if (q == q0i) freqMin = binfreq;
	    if (q == q1i) freqMax = binfreq;

	    if (peaksOnly && !fft->isLocalPeak(s, q)) continue;

	    if (!fft->isOverThreshold
                (s, q, float(m_threshold * double(getFFTSize())/2.0))) {
                continue;
            }

            double freq = binfreq;
	    
	    if (s < int(fft->getWidth()) - 1) {

                fft->estimateStableFrequency(s, q, freq);
	    
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
SpectrogramLayer::getXYBinSourceRange(LayerGeometryProvider *v, int x, int y,
				      double &min, double &max,
				      double &phaseMin, double &phaseMax) const
{
    if (!m_model || !m_model->isOK() || !m_model->isReady()) {
	return false;
    }

    double q0 = 0, q1 = 0;
    if (!getYBinRange(v, y, q0, q1)) return false;

    double s0 = 0, s1 = 0;
    if (!getXBinRange(v, x, s0, s1)) return false;
    
    int q0i = int(q0 + 0.001);
    int q1i = int(q1);

    int s0i = int(s0 + 0.001);
    int s1i = int(s1);

    bool rv = false;

    FFTModel *fft = getFFTModel();

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

                    double value;

                    value = fft->getPhaseAt(s, q);
                    if (!have || value < phaseMin) { phaseMin = value; }
                    if (!have || value > phaseMax) { phaseMax = value; }

                    value = fft->getMagnitudeAt(s, q) / (getFFTSize()/2.0);
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
	
FFTModel *
SpectrogramLayer::getFFTModel() const
{
    if (!m_model) return 0;

    int fftSize = getFFTSize();

    //!!! it is now surely slower to do this on every getFFTModel()
    //!!! request than it would be to recreate the model immediately
    //!!! when something changes instead of just invalidating it
    
    if (m_fftModel &&
        m_fftModel->getHeight() == fftSize / 2 + 1 &&
        m_fftModel->getWindowIncrement() == getWindowIncrement()) {
        return m_fftModel;
    }
    
    delete m_peakCache;
    m_peakCache = 0;

    delete m_fftModel;
    m_fftModel = new FFTModel(m_model,
                              m_channel,
                              m_windowType,
                              m_windowSize,
                              getWindowIncrement(),
                              fftSize);

    if (!m_fftModel->isOK()) {
        QMessageBox::critical
            (0, tr("FFT cache failed"),
             tr("Failed to create the FFT model for this spectrogram.\n"
                "There may be insufficient memory or disc space to continue."));
        delete m_fftModel;
        m_fftModel = 0;
        return 0;
    }

    ((SpectrogramLayer *)this)->sliceableModelReplaced(0, m_fftModel);

    return m_fftModel;
}

Dense3DModelPeakCache *
SpectrogramLayer::getPeakCache() const
{
    //!!! see comment in getFFTModel
    
    if (!m_peakCache) {
        FFTModel *f = getFFTModel();
        if (!f) return 0;
        m_peakCache = new Dense3DModelPeakCache(f, m_peakCacheDivisor);
    }
    return m_peakCache;
}

const Model *
SpectrogramLayer::getSliceableModel() const
{
    return m_fftModel;
}

void
SpectrogramLayer::invalidateFFTModel()
{
#ifdef DEBUG_SPECTROGRAM
    cerr << "SpectrogramLayer::invalidateFFTModel called" << endl;
#endif

    emit sliceableModelReplaced(m_fftModel, 0);

    delete m_fftModel;
    delete m_peakCache;

    m_fftModel = 0;
    m_peakCache = 0;
}

void
SpectrogramLayer::invalidateMagnitudes()
{
#ifdef DEBUG_SPECTROGRAM
    cerr << "SpectrogramLayer::invalidateMagnitudes called" << endl;
#endif
    m_viewMags.clear();
    for (vector<MagnitudeRange>::iterator i = m_columnMags.begin();
         i != m_columnMags.end(); ++i) {
        *i = MagnitudeRange();
    }
}

bool
SpectrogramLayer::updateViewMagnitudes(LayerGeometryProvider *v) const
{
    MagnitudeRange mag;

    int x0 = 0, x1 = v->getPaintWidth();
    double s00 = 0, s01 = 0, s10 = 0, s11 = 0;
    
    if (!getXBinRange(v, x0, s00, s01)) {
        s00 = s01 = double(m_model->getStartFrame()) / getWindowIncrement();
    }

    if (!getXBinRange(v, x1, s10, s11)) {
        s10 = s11 = double(m_model->getEndFrame()) / getWindowIncrement();
    }

    int s0 = int(min(s00, s10) + 0.0001);
    int s1 = int(max(s01, s11) + 0.0001);

//    SVDEBUG << "SpectrogramLayer::updateViewMagnitudes: x0 = " << x0 << ", x1 = " << x1 << ", s00 = " << s00 << ", s11 = " << s11 << " s0 = " << s0 << ", s1 = " << s1 << endl;

    if (int(m_columnMags.size()) <= s1) {
        m_columnMags.resize(s1 + 1);
    }

    for (int s = s0; s <= s1; ++s) {
        if (m_columnMags[s].isSet()) {
            mag.sample(m_columnMags[s]);
        }
    }

#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer::updateViewMagnitudes returning from cols "
         << s0 << " -> " << s1 << " inclusive" << endl;
    cerr << "SpectrogramLayer::updateViewMagnitudes: for view id " << v->getId()
         << ": min is " << mag.getMin() << ", max is " << mag.getMax() << endl;
#endif

    if (!mag.isSet()) return false;
    if (mag == m_viewMags[v->getId()]) return false;
    m_viewMags[v->getId()] = mag;
    return true;
}

void
SpectrogramLayer::setSynchronousPainting(bool synchronous)
{
    m_synchronous = synchronous;
}

ScrollableImageCache &
SpectrogramLayer::getImageCacheReference(const LayerGeometryProvider *view) const
{
    //!!! to go?
    if (m_imageCaches.find(view->getId()) == m_imageCaches.end()) {
        m_imageCaches[view->getId()] = ScrollableImageCache();
    }
    return m_imageCaches.at(view->getId());
}

void
SpectrogramLayer::paintAlternative(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
    static int depth = 0;
    
    Colour3DPlotRenderer *renderer = getRenderer(v);

    if (m_synchronous) {
        (void)renderer->render(v, paint, rect);
        return;
    }

    ++depth;
    cerr << "paint depth " << depth << endl;
    
    (void)renderer->renderTimeConstrained(v, paint, rect);

    //!!! + mag range

    QRect uncached = renderer->getLargestUncachedRect();
    if (uncached.width() > 0) {
        cerr << "updating rect at " << uncached.x() << " width "
             << uncached.width() << endl;
        v->updatePaintRect(uncached);
    }

    cerr << "exiting paint depth " << depth << endl;
    --depth;
}

Colour3DPlotRenderer *
SpectrogramLayer::getRenderer(LayerGeometryProvider *v) const
{
    if (m_renderers.find(v->getId()) == m_renderers.end()) {

        Colour3DPlotRenderer::Sources sources;
        sources.verticalBinLayer = this;
        sources.fft = getFFTModel();
        sources.source = sources.fft;
        sources.peaks = getPeakCache();

        ColourScale::Parameters cparams;
        cparams.colourMap = m_colourMap;
        cparams.scale = m_colourScale;
        cparams.threshold = m_threshold;
        cparams.gain = m_gain;

        if (m_colourScale != ColourScale::PhaseColourScale &&
            m_normalization == ColumnNormalization::None) {
            cparams.gain *= 2.f / float(getFFTSize());
        }
        
        Colour3DPlotRenderer::Parameters params;
        params.colourScale = ColourScale(cparams);
        params.normalization = m_normalization;
        params.binDisplay = m_binDisplay;
        params.binScale = m_binScale;
        params.alwaysOpaque = true;
        params.invertVertical = false;

        Preferences::SpectrogramSmoothing smoothing = 
            Preferences::getInstance()->getSpectrogramSmoothing();
        params.interpolate = 
            (smoothing == Preferences::SpectrogramInterpolated ||
             smoothing == Preferences::SpectrogramZeroPaddedAndInterpolated);

        m_renderers[v->getId()] = new Colour3DPlotRenderer(sources, params);
    }

    return m_renderers[v->getId()];
}

void
SpectrogramLayer::paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
    Profiler profiler("SpectrogramLayer::paint", false);

#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer::paint() entering: m_model is " << m_model << ", zoom level is " << v->getZoomLevel() << endl;
    
    cerr << "SpectrogramLayer::paint(): rect is " << rect.x() << "," << rect.y() << " " << rect.width() << "x" << rect.height() << endl;
#endif

    sv_frame_t startFrame = v->getStartFrame();

    if (!m_model || !m_model->isOK() || !m_model->isReady()) {
	return;
    }

    if (isLayerDormant(v)) {
	SVDEBUG << "SpectrogramLayer::paint(): Layer is dormant, making it undormant again" << endl;
    }

    paintAlternative(v, paint, rect);
    return;

    //!!!
    
    // Need to do this even if !isLayerDormant, as that could mean v
    // is not in the dormancy map at all -- we need it to be present
    // and accountable for when determining whether we need the cache
    // in the cache-fill thread above.
    //!!! no inter use cache-fill thread
    const_cast<SpectrogramLayer *>(this)->Layer::setLayerDormant(v, false);

    int fftSize = getFFTSize();

    const View *view = v->getView();
    ScrollableImageCache &cache = getImageCacheReference(view);

#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer::paint(): image cache valid area from " << cache.getValidLeft() << " width " << cache.getValidWidth() << ", height " << cache.getSize().height() << endl;
    if (rect.x() + rect.width() + 1 < cache.getValidLeft() ||
        rect.x() > cache.getValidRight()) {
        cerr << "SpectrogramLayer: NOTE: requested rect is not contiguous with cache valid area" << endl;
    }
#endif

    int zoomLevel = v->getZoomLevel();

    int x0 = v->getXForViewX(rect.x());
    int x1 = v->getXForViewX(rect.x() + rect.width());
    if (x0 < 0) x0 = 0;
    if (x1 > v->getPaintWidth()) x1 = v->getPaintWidth();

    if (updateViewMagnitudes(v)) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
        cerr << "SpectrogramLayer: magnitude range changed to [" << m_viewMags[v->getId()].getMin() << "->" << m_viewMags[v->getId()].getMax() << "]" << endl;
#endif
        if (m_normalizeVisibleArea) {
            cache.invalidate();
        }
    }

    if (cache.getZoomLevel() != zoomLevel ||
        cache.getSize() != v->getPaintSize()) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
        cerr << "SpectrogramLayer: resizing image cache from "
             << cache.getSize().width() << "x" << cache.getSize().height()
             << " to "
             << v->getPaintSize().width() << "x" << v->getPaintSize().height()
             << " and updating zoom level from " << cache.getZoomLevel()
             << " to " << zoomLevel
             << endl;
#endif
        cache.resize(v->getPaintSize());
        cache.setZoomLevel(zoomLevel);
        cache.setStartFrame(startFrame);
    }
    
    if (cache.isValid()) {
        
        if (v->getXForFrame(cache.getStartFrame()) ==
            v->getXForFrame(startFrame) &&
            cache.getValidLeft() <= x0 &&
            cache.getValidRight() >= x1) {
                
#ifdef DEBUG_SPECTROGRAM_REPAINT
            cerr << "SpectrogramLayer: image cache hit!" << endl;
#endif

            paint.drawImage(rect, cache.getImage(), rect);

            illuminateLocalFeatures(v, paint);
            return;

        } else {

            // cache doesn't begin at the right frame or doesn't
            // contain the complete view, but might be scrollable or
            // partially usable
                
#ifdef DEBUG_SPECTROGRAM_REPAINT
            cerr << "SpectrogramLayer: scrolling the image cache if applicable" << endl;
#endif

            cache.scrollTo(v, startFrame);
            
#ifdef DEBUG_SPECTROGRAM_REPAINT
            cerr << "SpectrogramLayer: after scrolling, cache valid from "
                 << cache.getValidLeft() << " width " << cache.getValidWidth()
                 << endl;
#endif
        }
    }

    bool rightToLeft = false;
    
    if (!cache.isValid()) {
        if (!m_synchronous) {
            // When rendering the whole thing, start from somewhere near
            // the middle so that the region of interest appears first

            //!!! (perhaps we should have some cunning test to avoid
            //!!! doing this if past repaints have appeared fast
            //!!! enough to do the whole width in one shot)
            if (x0 == 0 && x1 == v->getPaintWidth()) {
                x0 = int(x1 * 0.3);
            }
        }
    } else {
        // When rendering only a part of the cache, we need to make
        // sure that the part we're rendering is adjacent to (or
        // overlapping) a valid area of cache, if we have one. The
        // alternative is to ditch the valid area of cache and render
        // only the requested area, but that's risky because this can
        // happen when just waving the pointer over a small part of
        // the view -- if we lose the partly-built cache every time
        // the user does that, we'll never finish building it.
        int left = x0;
        int width = x1 - x0;
        bool isLeftOfValidArea = false;
        cache.adjustToTouchValidArea(left, width, isLeftOfValidArea);
        x0 = left;
        x1 = x0 + width;

        // That call also told us whether we should be painting
        // sub-regions of our target region in right-to-left order in
        // order to ensure contiguity
        rightToLeft = isLeftOfValidArea;
    }
    
    // We always paint the full height when refreshing the cache.
    // Smaller heights can be used when painting direct from cache
    // (further up in this function), but we want to ensure the cache
    // is coherent without having to worry about vertical matching of
    // required and valid areas as well as horizontal.
    int h = v->getPaintHeight();
    
    int repaintWidth = x1 - x0;

#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer: x0 " << x0 << ", x1 " << x1
         << ", repaintWidth " << repaintWidth << ", h " << h
         << ", rightToLeft " << rightToLeft << endl;
#endif

    sv_samplerate_t sr = m_model->getSampleRate();

    // Set minFreq and maxFreq to the frequency extents of the possibly
    // zero-padded visible bin range, and displayMinFreq and displayMaxFreq
    // to the actual scale frequency extents (presumably not zero padded).

    // If we are zero padding (i.e. oversampling) we want to use the
    // zero-padded equivalents of the bins that we would be using if
    // not zero padded, to avoid spaces at the top and bottom of the
    // display.
    
    int maxbin = fftSize / 2;
    if (m_maxFrequency > 0) {
	maxbin = int((double(m_maxFrequency) * fftSize) / sr + 0.001);
	if (maxbin > fftSize / 2) maxbin = fftSize / 2;
    }

    int minbin = 1;
    if (m_minFrequency > 0) {
	minbin = int((double(m_minFrequency) * fftSize) / sr + 0.001);
//        cerr << "m_minFrequency = " << m_minFrequency << " -> minbin = " << minbin << endl;
	if (minbin < 1) minbin = 1;
	if (minbin >= maxbin) minbin = maxbin - 1;
    }

    int over = getFFTOversampling();
    minbin = minbin * over;
    maxbin = (maxbin + 1) * over - 1;

    double minFreq = (double(minbin) * sr) / fftSize;
    double maxFreq = (double(maxbin) * sr) / fftSize;

    double displayMinFreq = minFreq;
    double displayMaxFreq = maxFreq;

//!!!    if (fftSize != getFFTSize()) {
//        displayMinFreq = getEffectiveMinFrequency();
//        displayMaxFreq = getEffectiveMaxFrequency();
//    }

//    cerr << "(giving actual minFreq " << minFreq << " and display minFreq " << displayMinFreq << ")" << endl;

    int increment = getWindowIncrement();
    
    bool logarithmic = (m_binScale == BinScale::Log);

    MagnitudeRange overallMag = m_viewMags[v->getId()];
    bool overallMagChanged = false;

#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer: " << ((double(v->getFrameForX(1) - v->getFrameForX(0))) / increment) << " bin(s) per pixel" << endl;
#endif

    if (repaintWidth == 0) {
        SVDEBUG << "*** NOTE: repaintWidth == 0" << endl;
    }

    Profiler outerprof("SpectrogramLayer::paint: all cols");

    // The draw buffer contains a fragment at either our pixel
    // resolution (if there is more than one time-bin per pixel) or
    // time-bin resolution (if a time-bin spans more than one pixel).
    // We need to ensure that it starts and ends at points where a
    // time-bin boundary occurs at an exact pixel boundary, and with a
    // certain amount of overlap across existing pixels so that we can
    // scale and draw from it without smoothing errors at the edges.

    // If (getFrameForX(x) / increment) * increment ==
    // getFrameForX(x), then x is a time-bin boundary.  We want two
    // such boundaries at either side of the draw buffer -- one which
    // we draw up to, and one which we subsequently crop at.

    bool bufferIsBinResolution = false;
    if (increment > zoomLevel) bufferIsBinResolution = true;

    sv_frame_t leftBoundaryFrame = -1, leftCropFrame = -1;
    sv_frame_t rightBoundaryFrame = -1, rightCropFrame = -1;

    int bufwid;

    if (bufferIsBinResolution) {

        for (int x = x0; ; --x) {
            sv_frame_t f = v->getFrameForX(x);
            if ((f / increment) * increment == f) {
                if (leftCropFrame == -1) leftCropFrame = f;
                else if (x < x0 - 2) {
                    leftBoundaryFrame = f;
                    break;
                }
            }
        }
        for (int x = x0 + repaintWidth; ; ++x) {
            sv_frame_t f = v->getFrameForX(x);
            if ((f / increment) * increment == f) {
                if (rightCropFrame == -1) rightCropFrame = f;
                else if (x > x0 + repaintWidth + 2) {
                    rightBoundaryFrame = f;
                    break;
                }
            }
        }
#ifdef DEBUG_SPECTROGRAM_REPAINT
        cerr << "Left: crop: " << leftCropFrame << " (bin " << leftCropFrame/increment << "); boundary: " << leftBoundaryFrame << " (bin " << leftBoundaryFrame/increment << ")" << endl;
        cerr << "Right: crop: " << rightCropFrame << " (bin " << rightCropFrame/increment << "); boundary: " << rightBoundaryFrame << " (bin " << rightBoundaryFrame/increment << ")" << endl;
#endif

        bufwid = int((rightBoundaryFrame - leftBoundaryFrame) / increment);

    } else {
        
        bufwid = repaintWidth;
    }

    vector<int> binforx(bufwid);
    vector<double> binfory(h);
    
    bool usePeaksCache = false;

    if (bufferIsBinResolution) {
        for (int x = 0; x < bufwid; ++x) {
            binforx[x] = int(leftBoundaryFrame / increment) + x;
        }
        m_drawBuffer = QImage(bufwid, h, QImage::Format_Indexed8);
    } else {
        for (int x = 0; x < bufwid; ++x) {
            double s0 = 0, s1 = 0;
            if (getXBinRange(v, x + x0, s0, s1)) {
                binforx[x] = int(s0 + 0.0001);
            } else {
                binforx[x] = -1; //???
            }
        }
        if (m_drawBuffer.width() < bufwid || m_drawBuffer.height() != h) {
            m_drawBuffer = QImage(bufwid, h, QImage::Format_Indexed8);
        }
        usePeaksCache = (increment * m_peakCacheDivisor) < zoomLevel;
        if (m_colourScale == ColourScale::PhaseColourScale) usePeaksCache = false;
    }

    for (int pixel = 0; pixel < 256; ++pixel) {
        m_drawBuffer.setColor((unsigned char)pixel,
                              m_palette.getColour((unsigned char)pixel).rgb());
    }

    m_drawBuffer.fill(0);
    int attainedBufwid = bufwid;

    double softTimeLimit;

    if (m_synchronous) {

        // must paint the whole thing for synchronous mode, so give
        // "no timeout"
        softTimeLimit = 0.0;
        
    } else if (bufferIsBinResolution) {
        
        // calculating boundaries later will be too fiddly for partial
        // paints, and painting should be fast anyway when this is the
        // case because it means we're well zoomed in
        softTimeLimit = 0.0;

    } else {

        // neither limitation applies, so use a short soft limit

        if (m_binDisplay == BinDisplay::PeakFrequencies) {
            softTimeLimit = 0.15;
        } else {
            softTimeLimit = 0.1;
        }
    }

    if (m_binDisplay != BinDisplay::PeakFrequencies) {

        for (int y = 0; y < h; ++y) {
            double q0 = 0, q1 = 0;
            if (!getYBinRange(v, h-y-1, q0, q1)) {
                binfory[y] = -1;
            } else {
                binfory[y] = q0;
            }
        }

        attainedBufwid = 
            paintDrawBuffer(v, bufwid, h, binforx, binfory,
                            usePeaksCache,
                            overallMag, overallMagChanged,
                            rightToLeft,
                            softTimeLimit);

    } else {

        attainedBufwid = 
            paintDrawBufferPeakFrequencies(v, bufwid, h, binforx,
                                           minbin, maxbin,
                                           displayMinFreq, displayMaxFreq,
                                           logarithmic,
                                           overallMag, overallMagChanged,
                                           rightToLeft,
                                           softTimeLimit);
    }

    int failedToRepaint = bufwid - attainedBufwid;

    int paintedLeft = x0;
    int paintedWidth = x1 - x0;
    
    if (failedToRepaint > 0) {

#ifdef DEBUG_SPECTROGRAM_REPAINT
        cerr << "SpectrogramLayer::paint(): Failed to repaint " << failedToRepaint << " of " << bufwid
             << " columns in time (so managed to repaint " << bufwid - failedToRepaint << ")" << endl;
#endif

        if (rightToLeft) {
            paintedLeft += failedToRepaint;
        }

        paintedWidth -= failedToRepaint;

        if (paintedWidth < 0) {
            paintedWidth = 0;
        }
        
    } else if (failedToRepaint < 0) {
        cerr << "WARNING: failedToRepaint < 0 (= " << failedToRepaint << ")"
             << endl;
        failedToRepaint = 0;
    }

    if (overallMagChanged) {
        m_viewMags[v->getId()] = overallMag;
#ifdef DEBUG_SPECTROGRAM_REPAINT
        cerr << "SpectrogramLayer: Overall mag is now [" << m_viewMags[v->getId()].getMin() << "->" << m_viewMags[v->getId()].getMax() << "] - will be updating" << endl;
#endif
    }

    outerprof.end();

    Profiler profiler2("SpectrogramLayer::paint: draw image");

    if (paintedWidth > 0) {

#ifdef DEBUG_SPECTROGRAM_REPAINT
        cerr << "SpectrogramLayer: Copying " << paintedWidth << "x" << h
                  << " from draw buffer at " << paintedLeft - x0 << "," << 0
                  << " to " << paintedWidth << "x" << h << " on cache at "
                  << x0 << "," << 0 << endl;
#endif

        if (bufferIsBinResolution) {

            int scaledLeft = v->getXForFrame(leftBoundaryFrame);
            int scaledRight = v->getXForFrame(rightBoundaryFrame);

#ifdef DEBUG_SPECTROGRAM_REPAINT
            cerr << "SpectrogramLayer: Rescaling image from " << bufwid
                 << "x" << h << " to "
                 << scaledRight-scaledLeft << "x" << h << endl;
#endif

            Preferences::SpectrogramXSmoothing xsmoothing = 
                Preferences::getInstance()->getSpectrogramXSmoothing();

            QImage scaled = m_drawBuffer.scaled
                (scaledRight - scaledLeft, h,
                 Qt::IgnoreAspectRatio,
                 ((xsmoothing == Preferences::SpectrogramXInterpolated) ?
                  Qt::SmoothTransformation : Qt::FastTransformation));
            
            int scaledLeftCrop = v->getXForFrame(leftCropFrame);
            int scaledRightCrop = v->getXForFrame(rightCropFrame);

#ifdef DEBUG_SPECTROGRAM_REPAINT
            cerr << "SpectrogramLayer: Drawing image region of width " << scaledRightCrop - scaledLeftCrop << " to "
                 << scaledLeftCrop << " from " << scaledLeftCrop - scaledLeft << endl;
#endif

            int targetLeft = scaledLeftCrop;
            if (targetLeft < 0) {
                targetLeft = 0;
            }

            int targetWidth = scaledRightCrop - targetLeft;
            if (targetLeft + targetWidth > cache.getSize().width()) {
                targetWidth = cache.getSize().width() - targetLeft;
            }
            
            int sourceLeft = targetLeft - scaledLeft;
            if (sourceLeft < 0) {
                sourceLeft = 0;
            }
            
            int sourceWidth = targetWidth;

            if (targetWidth > 0) {
                cache.drawImage
                    (targetLeft,
                     targetWidth,
                     scaled,
                     sourceLeft,
                     sourceWidth);
            }

        } else {

            cache.drawImage(paintedLeft, paintedWidth,
                            m_drawBuffer,
                            paintedLeft - x0, paintedWidth);
        }
    }

#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer: Cache valid area now from " << cache.getValidLeft()
         << " width " << cache.getValidWidth() << ", height "
         << cache.getSize().height() << endl;
#endif

    QRect pr = rect & cache.getValidArea();

#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer: Copying " << pr.width() << "x" << pr.height()
              << " from cache at " << pr.x() << "," << pr.y()
              << " to window" << endl;
#endif

    paint.drawImage(pr.x(), pr.y(), cache.getImage(),
                    pr.x(), pr.y(), pr.width(), pr.height());

    if (!m_synchronous) {

        if (!m_normalizeVisibleArea || !overallMagChanged) {

            QRect areaLeft(0, 0, cache.getValidLeft(), h);
            QRect areaRight(cache.getValidRight(), 0,
                            cache.getSize().width() - cache.getValidRight(), h);

            bool haveSpaceLeft = (areaLeft.width() > 0);
            bool haveSpaceRight = (areaRight.width() > 0);

            bool updateLeft = haveSpaceLeft;
            bool updateRight = haveSpaceRight;
            
            if (updateLeft && updateRight) {
                if (rightToLeft) {
                    // we just did something adjoining the cache on
                    // its left side, so now do something on its right
                    updateLeft = false;
                } else {
                    updateRight = false;
                }
            }
            
            if (updateLeft) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
                cerr << "SpectrogramLayer::paint() updating left ("
                     << areaLeft.x() << ", "
                     << areaLeft.width() << ")" << endl;
#endif
                v->updatePaintRect(areaLeft);
            }
            
            if (updateRight) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
                cerr << "SpectrogramLayer::paint() updating right ("
                     << areaRight.x() << ", "
                     << areaRight.width() << ")" << endl;
#endif
                v->updatePaintRect(areaRight);
            }
            
        } else {
            // overallMagChanged
            cerr << "\noverallMagChanged - updating all\n" << endl;
            cache.invalidate();
            v->updatePaintRect(v->getPaintRect());
        }
    }

    illuminateLocalFeatures(v, paint);

#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer::paint() returning" << endl;
#endif
}

int
SpectrogramLayer::paintDrawBufferPeakFrequencies(LayerGeometryProvider *v,
                                                 int w,
                                                 int h,
                                                 const vector<int> &binforx,
                                                 int minbin,
                                                 int maxbin,
                                                 double displayMinFreq,
                                                 double displayMaxFreq,
                                                 bool logarithmic,
                                                 MagnitudeRange &overallMag,
                                                 bool &overallMagChanged,
                                                 bool rightToLeft,
                                                 double softTimeLimit) const
{
    Profiler profiler("SpectrogramLayer::paintDrawBufferPeakFrequencies");

#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer::paintDrawBufferPeakFrequencies: minbin " << minbin << ", maxbin " << maxbin << "; w " << w << ", h " << h << endl;
#endif
    if (minbin < 0) minbin = 0;
    if (maxbin < 0) maxbin = minbin+1;

    FFTModel *fft = getFFTModel();
    if (!fft) return 0;

    FFTModel::PeakSet peakfreqs;
    vector<float> preparedColumn;
            
    int psx = -1;

    int minColumns = 4;
    bool haveTimeLimits = (softTimeLimit > 0.0);
    double hardTimeLimit = softTimeLimit * 2.0;
    bool overridingSoftLimit = false;
    auto startTime = chrono::steady_clock::now();
    
    int start = 0;
    int finish = w;
    int step = 1;

    if (rightToLeft) {
        start = w-1;
        finish = -1;
        step = -1;
    }
    
    int columnCount = 0;
    
    for (int x = start; x != finish; x += step) {
        
        ++columnCount;
        
        if (binforx[x] < 0) continue;

        int sx0 = binforx[x];
        int sx1 = sx0;
        if (x+1 < w) sx1 = binforx[x+1];
        if (sx0 < 0) sx0 = sx1 - 1;
        if (sx0 < 0) continue;
        if (sx1 <= sx0) sx1 = sx0 + 1;

        vector<float> pixelPeakColumn;
        
        for (int sx = sx0; sx < sx1; ++sx) {

            if (sx < 0 || sx >= int(fft->getWidth())) {
                continue;
            }

            if (sx != psx) {

                ColumnOp::Column column;

                column = getColumnFromFFTModel(fft,
                                               sx,
                                               minbin,
                                               maxbin - minbin + 1);

                if (m_colourScale != ColourScale::PhaseColourScale) {
                    column = ColumnOp::fftScale(column, getFFTSize());
                }

                recordColumnExtents(column,
                                    sx,
                                    overallMag,
                                    overallMagChanged);

                if (m_colourScale != ColourScale::PhaseColourScale) {
                    column = ColumnOp::normalize(column, m_normalization);
                }

                preparedColumn = ColumnOp::applyGain(column, m_gain);
                
                psx = sx;
            }

            if (sx == sx0) {
                pixelPeakColumn = preparedColumn;
                peakfreqs = fft->getPeakFrequencies(FFTModel::AllPeaks, sx,
                                                    minbin, maxbin - 1);
            } else {
                for (int i = 0; in_range_for(pixelPeakColumn, i); ++i) {
                    pixelPeakColumn[i] = std::max(pixelPeakColumn[i],
                                                  preparedColumn[i]);
                }
            }
        }

        if (!pixelPeakColumn.empty()) {
            for (FFTModel::PeakSet::const_iterator pi = peakfreqs.begin();
                 pi != peakfreqs.end(); ++pi) {

                int bin = pi->first;
                double freq = pi->second;

                if (bin < minbin) continue;
                if (bin > maxbin) break;
            
                double value = pixelPeakColumn[bin - minbin];
            
                double y = v->getYForFrequency
                    (freq, displayMinFreq, displayMaxFreq, logarithmic);
            
                int iy = int(y + 0.5);
                if (iy < 0 || iy >= h) continue;

                m_drawBuffer.setPixel(x, iy, getDisplayValue(v, value));
            }
        }
        
        if (haveTimeLimits) {
            if (columnCount >= minColumns) {
                auto t = chrono::steady_clock::now();
                double diff = chrono::duration<double>(t - startTime).count();
                if (diff > hardTimeLimit) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
                    cerr << "SpectrogramLayer::paintDrawBufferPeakFrequencies: hard limit " << hardTimeLimit << " sec exceeded after "
                         << columnCount << " columns with time " << diff << endl;
#endif
                    return columnCount;
                } else if (diff > softTimeLimit && !overridingSoftLimit) {
                    // If we're more than half way through by the time
                    // we reach the soft limit, ignore it (though
                    // still respect the hard limit, above). Otherwise
                    // respect the soft limit and return now.
                    if (columnCount > w/2) {
                        overridingSoftLimit = true;
                    } else {
#ifdef DEBUG_SPECTROGRAM_REPAINT
                        cerr << "SpectrogramLayer::paintDrawBufferPeakFrequencies: soft limit " << softTimeLimit << " sec exceeded after "
                             << columnCount << " columns with time " << diff << endl;
#endif
                        return columnCount;
                    }
                }                        
            }
        }
    }

    return columnCount;
}

vector<float>
SpectrogramLayer::getColumnFromFFTModel(FFTModel *fft,
                                        int sx, // column number in model
                                        int minbin,
                                        int bincount) const
{
    vector<float> values(bincount, 0.f);
    
    if (m_colourScale == ColourScale::PhaseColourScale) {
        fft->getPhasesAt(sx, values.data(), minbin, bincount);
    } else {
        fft->getMagnitudesAt(sx, values.data(), minbin, bincount);
    }

    return values;
}

vector<float>
SpectrogramLayer::getColumnFromGenericModel(DenseThreeDimensionalModel *model,
                                            int sx, // column number in model
                                            int minbin,
                                            int bincount) const
{
    if (m_colourScale == ColourScale::PhaseColourScale) {
        throw std::logic_error("can't use phase scale with generic 3d model");
    }

    auto col = model->getColumn(sx);
        
    return vector<float>(col.data() + minbin,
                         col.data() + minbin + bincount);
}

void
SpectrogramLayer::recordColumnExtents(const vector<float> &col,
                                      int sx, // column index, for m_columnMags
                                      MagnitudeRange &overallMag,
                                      bool &overallMagChanged) const
{
    if (!in_range_for(m_columnMags, sx)) {
        m_columnMags.resize(sx + 1);
    }
    MagnitudeRange mr;
    for (auto v: col) {
        mr.sample(v);
    }
    m_columnMags[sx] = mr;
    if (overallMag.sample(mr)) {
        overallMagChanged = true;
    }
}

int
SpectrogramLayer::paintDrawBuffer(LayerGeometryProvider *v,
                                  int w,
                                  int h,
                                  const vector<int> &binforx,
                                  const vector<double> &binfory,
                                  bool usePeaksCache,
                                  MagnitudeRange &overallMag,
                                  bool &overallMagChanged,
                                  bool rightToLeft,
                                  double softTimeLimit) const
{
    Profiler profiler("SpectrogramLayer::paintDrawBuffer");

    int minbin = int(binfory[0] + 0.0001);
    int maxbin = int(binfory[h-1]);

#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer::paintDrawBuffer: minbin " << minbin << ", maxbin " << maxbin << "; w " << w << ", h " << h << endl;
#endif
    if (minbin < 0) minbin = 0;
    if (maxbin < 0) maxbin = minbin+1;

    DenseThreeDimensionalModel *peakCacheModel = 0;
    FFTModel *fftModel = 0;
    DenseThreeDimensionalModel *sourceModel = 0;
    
#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer::paintDrawBuffer: Note: bin display = " << int(m_binDisplay) << ", w = " << w << ", binforx[" << w-1 << "] = " << binforx[w-1] << ", binforx[0] = " << binforx[0] << endl;
#endif

    int divisor = 1;
    if (usePeaksCache) {
        peakCacheModel = getPeakCache();
        divisor = m_peakCacheDivisor;
        sourceModel = peakCacheModel;
    } else {
        fftModel = getFFTModel();
        sourceModel = fftModel;
    }

    if (!sourceModel) return 0;

    bool interpolate = false;
    Preferences::SpectrogramSmoothing smoothing = 
        Preferences::getInstance()->getSpectrogramSmoothing();
    if (smoothing == Preferences::SpectrogramInterpolated ||
        smoothing == Preferences::SpectrogramZeroPaddedAndInterpolated) {
        if (m_binDisplay != BinDisplay::PeakBins &&
            m_binDisplay != BinDisplay::PeakFrequencies) {
            interpolate = true;
        }
    }

    int psx = -1;

    int minColumns = 4;
    bool haveTimeLimits = (softTimeLimit > 0.0);
    double hardTimeLimit = softTimeLimit * 2.0;
    bool overridingSoftLimit = false;
    auto startTime = chrono::steady_clock::now();
    
    int start = 0;
    int finish = w;
    int step = 1;

    if (rightToLeft) {
        start = w-1;
        finish = -1;
        step = -1;
    }

    int columnCount = 0;
    
    vector<float> preparedColumn;
            
    for (int x = start; x != finish; x += step) {

        // x is the on-canvas pixel coord; sx (later) will be the
        // source column index
        
        ++columnCount;
        
        if (binforx[x] < 0) continue;

        int sx0 = binforx[x] / divisor;
        int sx1 = sx0;
        if (x+1 < w) sx1 = binforx[x+1] / divisor;
        if (sx0 < 0) sx0 = sx1 - 1;
        if (sx0 < 0) continue;
        if (sx1 <= sx0) sx1 = sx0 + 1;

        vector<float> pixelPeakColumn;
        
        for (int sx = sx0; sx < sx1; ++sx) {

#ifdef DEBUG_SPECTROGRAM_REPAINT
//            cerr << "sx = " << sx << endl;
#endif

            if (sx < 0 || sx >= sourceModel->getWidth()) {
                continue;
            }

            if (sx != psx) {

                // order:
                // get column -> scale -> record extents ->
                // normalise -> peak pick -> apply display gain ->
                // distribute/interpolate

                ColumnOp::Column column;

                if (peakCacheModel) {
                    column = getColumnFromGenericModel(peakCacheModel,
                                                       sx,
                                                       minbin,
                                                       maxbin - minbin + 1);
                } else {
                    column = getColumnFromFFTModel(fftModel,
                                                   sx,
                                                   minbin,
                                                   maxbin - minbin + 1);
                }

                if (m_colourScale != ColourScale::PhaseColourScale) {
                    column = ColumnOp::fftScale(column, getFFTSize());
                }

                recordColumnExtents(column,
                                    sx,
                                    overallMag,
                                    overallMagChanged);

                if (m_colourScale != ColourScale::PhaseColourScale) {
                    column = ColumnOp::normalize(column, m_normalization);
                }

                if (m_binDisplay == BinDisplay::PeakBins) {
                    column = ColumnOp::peakPick(column);
                }

                preparedColumn =
                    ColumnOp::distribute(ColumnOp::applyGain(column, m_gain),
                                         h,
                                         binfory,
                                         minbin,
                                         interpolate);
                
                psx = sx;
            }

            if (sx == sx0) {
                pixelPeakColumn = preparedColumn;
            } else {
                for (int i = 0; in_range_for(pixelPeakColumn, i); ++i) {
                    pixelPeakColumn[i] = std::max(pixelPeakColumn[i],
                                                  preparedColumn[i]);
                }
            }
        }

        if (!pixelPeakColumn.empty()) {
            for (int y = 0; y < h; ++y) {
                m_drawBuffer.setPixel(x,
                                      h-y-1,
                                      getDisplayValue(v, pixelPeakColumn[y]));
            }
        }

        if (haveTimeLimits) {
            if (columnCount >= minColumns) {
                auto t = chrono::steady_clock::now();
                double diff = chrono::duration<double>(t - startTime).count();
                if (diff > hardTimeLimit) {
#ifdef DEBUG_SPECTROGRAM_REPAINT
                    cerr << "SpectrogramLayer::paintDrawBuffer: hard limit " << hardTimeLimit << " sec exceeded after "
                         << columnCount << " columns with time " << diff << endl;
#endif
                    return columnCount;
                } else if (diff > softTimeLimit && !overridingSoftLimit) {
                    // If we're more than half way through by the time
                    // we reach the soft limit, ignore it (though
                    // still respect the hard limit, above). Otherwise
                    // respect the soft limit and return now.
                    if (columnCount > w/2) {
                        overridingSoftLimit = true;
                    } else {
#ifdef DEBUG_SPECTROGRAM_REPAINT
                        cerr << "SpectrogramLayer::paintDrawBuffer: soft limit " << softTimeLimit << " sec exceeded after "
                             << columnCount << " columns with time " << diff << endl;
#endif
                        return columnCount;
                    }
                }                        
            }
        }
    }

    return columnCount;
}

void
SpectrogramLayer::illuminateLocalFeatures(LayerGeometryProvider *v, QPainter &paint) const
{
    Profiler profiler("SpectrogramLayer::illuminateLocalFeatures");

    QPoint localPos;
    if (!v->shouldIlluminateLocalFeatures(this, localPos) || !m_model) {
        return;
    }

//    cerr << "SpectrogramLayer: illuminateLocalFeatures("
//              << localPos.x() << "," << localPos.y() << ")" << endl;

    double s0, s1;
    double f0, f1;

    if (getXBinRange(v, localPos.x(), s0, s1) &&
        getYBinSourceRange(v, localPos.y(), f0, f1)) {
        
        int s0i = int(s0 + 0.001);
        int s1i = int(s1);
        
        int x0 = v->getXForFrame(s0i * getWindowIncrement());
        int x1 = v->getXForFrame((s1i + 1) * getWindowIncrement());

        int y1 = int(getYForFrequency(v, f1));
        int y0 = int(getYForFrequency(v, f0));
        
//        cerr << "SpectrogramLayer: illuminate "
//                  << x0 << "," << y1 << " -> " << x1 << "," << y0 << endl;
        
        paint.setPen(v->getForeground());

        //!!! should we be using paintCrosshairs for this?

        paint.drawRect(x0, y1, x1 - x0 + 1, y0 - y1 + 1);
    }
}

double
SpectrogramLayer::getYForFrequency(const LayerGeometryProvider *v, double frequency) const
{
    return v->getYForFrequency(frequency,
			       getEffectiveMinFrequency(),
			       getEffectiveMaxFrequency(),
			       m_binScale == BinScale::Log);
}

double
SpectrogramLayer::getFrequencyForY(const LayerGeometryProvider *v, int y) const
{
    return v->getFrequencyForY(y,
			       getEffectiveMinFrequency(),
			       getEffectiveMaxFrequency(),
			       m_binScale == BinScale::Log);
}

int
SpectrogramLayer::getCompletion(LayerGeometryProvider *) const
{
    if (!m_fftModel) return 100;
    int completion = m_fftModel->getCompletion();
#ifdef DEBUG_SPECTROGRAM_REPAINT
    cerr << "SpectrogramLayer::getCompletion: completion = " << completion << endl;
#endif
    return completion;
}

QString
SpectrogramLayer::getError(LayerGeometryProvider *) const
{
    if (!m_fftModel) return "";
    return m_fftModel->getError();
}

bool
SpectrogramLayer::getValueExtents(double &min, double &max,
                                  bool &logarithmic, QString &unit) const
{
    if (!m_model) return false;

    sv_samplerate_t sr = m_model->getSampleRate();
    min = double(sr) / getFFTSize();
    max = double(sr) / 2;
    
    logarithmic = (m_binScale == BinScale::Log);
    unit = "Hz";
    return true;
}

bool
SpectrogramLayer::getDisplayExtents(double &min, double &max) const
{
    min = getEffectiveMinFrequency();
    max = getEffectiveMaxFrequency();

//    SVDEBUG << "SpectrogramLayer::getDisplayExtents: " << min << "->" << max << endl;
    return true;
}    

bool
SpectrogramLayer::setDisplayExtents(double min, double max)
{
    if (!m_model) return false;

//    SVDEBUG << "SpectrogramLayer::setDisplayExtents: " << min << "->" << max << endl;

    if (min < 0) min = 0;
    if (max > m_model->getSampleRate()/2.0) max = m_model->getSampleRate()/2.0;
    
    int minf = int(lrint(min));
    int maxf = int(lrint(max));

    if (m_minFrequency == minf && m_maxFrequency == maxf) return true;

    invalidateImageCaches();
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
SpectrogramLayer::getYScaleValue(const LayerGeometryProvider *v, int y,
                                 double &value, QString &unit) const
{
    value = getFrequencyForY(v, y);
    unit = "Hz";
    return true;
}

bool
SpectrogramLayer::snapToFeatureFrame(LayerGeometryProvider *,
                                     sv_frame_t &frame,
				     int &resolution,
				     SnapType snap) const
{
    resolution = getWindowIncrement();
    sv_frame_t left = (frame / resolution) * resolution;
    sv_frame_t right = left + resolution;

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

void
SpectrogramLayer::measureDoubleClick(LayerGeometryProvider *v, QMouseEvent *e)
{
    const View *view = v->getView();
    ScrollableImageCache &cache = getImageCacheReference(view);

    cerr << "cache width: " << cache.getSize().width() << ", height: "
         << cache.getSize().height() << endl;

    QImage image = cache.getImage();

    ImageRegionFinder finder;
    QRect rect = finder.findRegionExtents(&image, e->pos());
    if (rect.isValid()) {
        MeasureRect mr;
        setMeasureRectFromPixrect(v, mr, rect);
        CommandHistory::getInstance()->addCommand
            (new AddMeasurementRectCommand(this, mr));
    }
}

bool
SpectrogramLayer::getCrosshairExtents(LayerGeometryProvider *v, QPainter &paint,
                                      QPoint cursorPos,
                                      vector<QRect> &extents) const
{
    QRect vertical(cursorPos.x() - 12, 0, 12, v->getPaintHeight());
    extents.push_back(vertical);

    QRect horizontal(0, cursorPos.y(), cursorPos.x(), 1);
    extents.push_back(horizontal);

    int sw = getVerticalScaleWidth(v, m_haveDetailedScale, paint);

    QRect freq(sw, cursorPos.y() - paint.fontMetrics().ascent() - 2,
               paint.fontMetrics().width("123456 Hz") + 2,
               paint.fontMetrics().height());
    extents.push_back(freq);

    QRect pitch(sw, cursorPos.y() + 2,
                paint.fontMetrics().width("C#10+50c") + 2,
                paint.fontMetrics().height());
    extents.push_back(pitch);

    QRect rt(cursorPos.x(),
             v->getPaintHeight() - paint.fontMetrics().height() - 2,
             paint.fontMetrics().width("1234.567 s"),
             paint.fontMetrics().height());
    extents.push_back(rt);

    int w(paint.fontMetrics().width("1234567890") + 2);
    QRect frame(cursorPos.x() - w - 2,
                v->getPaintHeight() - paint.fontMetrics().height() - 2,
                w,
                paint.fontMetrics().height());
    extents.push_back(frame);

    return true;
}

void
SpectrogramLayer::paintCrosshairs(LayerGeometryProvider *v, QPainter &paint,
                                  QPoint cursorPos) const
{
    paint.save();

    int sw = getVerticalScaleWidth(v, m_haveDetailedScale, paint);

    QFont fn = paint.font();
    if (fn.pointSize() > 8) {
        fn.setPointSize(fn.pointSize() - 1);
        paint.setFont(fn);
    }
    paint.setPen(m_crosshairColour);

    paint.drawLine(0, cursorPos.y(), cursorPos.x() - 1, cursorPos.y());
    paint.drawLine(cursorPos.x(), 0, cursorPos.x(), v->getPaintHeight());
    
    double fundamental = getFrequencyForY(v, cursorPos.y());

    PaintAssistant::drawVisibleText(v, paint,
                       sw + 2,
                       cursorPos.y() - 2,
                       QString("%1 Hz").arg(fundamental),
                       PaintAssistant::OutlinedText);

    if (Pitch::isFrequencyInMidiRange(fundamental)) {
        QString pitchLabel = Pitch::getPitchLabelForFrequency(fundamental);
        PaintAssistant::drawVisibleText(v, paint,
                           sw + 2,
                           cursorPos.y() + paint.fontMetrics().ascent() + 2,
                           pitchLabel,
                           PaintAssistant::OutlinedText);
    }

    sv_frame_t frame = v->getFrameForX(cursorPos.x());
    RealTime rt = RealTime::frame2RealTime(frame, m_model->getSampleRate());
    QString rtLabel = QString("%1 s").arg(rt.toText(true).c_str());
    QString frameLabel = QString("%1").arg(frame);
    PaintAssistant::drawVisibleText(v, paint,
                       cursorPos.x() - paint.fontMetrics().width(frameLabel) - 2,
                       v->getPaintHeight() - 2,
                       frameLabel,
                       PaintAssistant::OutlinedText);
    PaintAssistant::drawVisibleText(v, paint,
                       cursorPos.x() + 2,
                       v->getPaintHeight() - 2,
                       rtLabel,
                       PaintAssistant::OutlinedText);

    int harmonic = 2;

    while (harmonic < 100) {

        int hy = int(lrint(getYForFrequency(v, fundamental * harmonic)));
        if (hy < 0 || hy > v->getPaintHeight()) break;
        
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
SpectrogramLayer::getFeatureDescription(LayerGeometryProvider *v, QPoint &pos) const
{
    int x = pos.x();
    int y = pos.y();

    if (!m_model || !m_model->isOK()) return "";

    double magMin = 0, magMax = 0;
    double phaseMin = 0, phaseMax = 0;
    double freqMin = 0, freqMax = 0;
    double adjFreqMin = 0, adjFreqMax = 0;
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

    if (m_binDisplay == BinDisplay::PeakFrequencies) {

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
	double dbMin = AudioLevel::multiplier_to_dB(magMin);
	double dbMax = AudioLevel::multiplier_to_dB(magMax);
	QString dbMinString;
	QString dbMaxString;
	if (dbMin == AudioLevel::DB_FLOOR) {
	    dbMinString = tr("-Inf");
	} else {
	    dbMinString = QString("%1").arg(lrint(dbMin));
	}
	if (dbMax == AudioLevel::DB_FLOOR) {
	    dbMaxString = tr("-Inf");
	} else {
	    dbMaxString = QString("%1").arg(lrint(dbMax));
	}
	if (lrint(dbMin) != lrint(dbMax)) {
	    text += tr("dB:\t%1 - %2").arg(dbMinString).arg(dbMaxString);
	} else {
	    text += tr("dB:\t%1").arg(dbMinString);
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

    return cw;
}

int
SpectrogramLayer::getVerticalScaleWidth(LayerGeometryProvider *, bool detailed, QPainter &paint) const
{
    if (!m_model || !m_model->isOK()) return 0;

    int cw = 0;
    if (detailed) cw = getColourScaleWidth(paint);

    int tw = paint.fontMetrics().width(QString("%1")
				     .arg(m_maxFrequency > 0 ?
					  m_maxFrequency - 1 :
					  m_model->getSampleRate() / 2));

    int fw = paint.fontMetrics().width(tr("43Hz"));
    if (tw < fw) tw = fw;

    int tickw = (m_binScale == BinScale::Log ? 10 : 4);
    
    return cw + tickw + tw + 13;
}

void
SpectrogramLayer::paintVerticalScale(LayerGeometryProvider *v, bool detailed, QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) {
	return;
    }

    Profiler profiler("SpectrogramLayer::paintVerticalScale");

    //!!! cache this?

    int h = rect.height(), w = rect.width();

    int tickw = (m_binScale == BinScale::Log ? 10 : 4);
    int pkw = (m_binScale == BinScale::Log ? 10 : 0);

    int bins = getFFTSize() / 2;
    sv_samplerate_t sr = m_model->getSampleRate();

    if (m_maxFrequency > 0) {
	bins = int((double(m_maxFrequency) * getFFTSize()) / sr + 0.1);
	if (bins > getFFTSize() / 2) bins = getFFTSize() / 2;
    }

    int cw = 0;

    if (detailed) cw = getColourScaleWidth(paint);
    int cbw = paint.fontMetrics().width("dB");

    int py = -1;
    int textHeight = paint.fontMetrics().height();
    int toff = -textHeight + paint.fontMetrics().ascent() + 2;

    if (detailed && (h > textHeight * 3 + 10)) {

        int topLines = 2;
        if (m_colourScale == ColourScale::PhaseColourScale) topLines = 1;

	int ch = h - textHeight * (topLines + 1) - 8;
//	paint.drawRect(4, textHeight + 4, cw - 1, ch + 1);
	paint.drawRect(4 + cw - cbw, textHeight * topLines + 4, cbw - 1, ch + 1);

	QString top, bottom;
        double min = m_viewMags[v->getId()].getMin();
        double max = m_viewMags[v->getId()].getMax();

        double dBmin = AudioLevel::multiplier_to_dB(min);
        double dBmax = AudioLevel::multiplier_to_dB(max);

#ifdef DEBUG_SPECTROGRAM_REPAINT
        cerr << "paintVerticalScale: for view id " << v->getId()
             << ": min = " << min << ", max = " << max
             << ", dBmin = " << dBmin << ", dBmax = " << dBmax << endl;
#endif
        
        if (dBmax < -60.f) dBmax = -60.f;
        else top = QString("%1").arg(lrint(dBmax));

        if (dBmin < dBmax - 60.f) dBmin = dBmax - 60.f;
        bottom = QString("%1").arg(lrint(dBmin));

        //!!! & phase etc

        if (m_colourScale != ColourScale::PhaseColourScale) {
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

            double dBval = dBmin + (((dBmax - dBmin) * i) / (ch - 1));
            int idb = int(dBval);

            double value = AudioLevel::dB_to_multiplier(dBval);
            int colour = getDisplayValue(v, value * m_gain);

	    paint.setPen(m_palette.getColour((unsigned char)colour));

            int y = textHeight * topLines + 4 + ch - i;

            paint.drawLine(5 + cw - cbw, y, cw + 2, y);

            if (i == 0) {
                lasty = y;
                lastdb = idb;
            } else if (i < ch - paint.fontMetrics().ascent() &&
                       idb != lastdb &&
                       ((abs(y - lasty) > textHeight && 
                         idb % 10 == 0) ||
                        (abs(y - lasty) > paint.fontMetrics().ascent() && 
                         idb % 5 == 0))) {
                paint.setPen(v->getBackground());
                QString text = QString("%1").arg(idb);
                paint.drawText(3 + cw - cbw - paint.fontMetrics().width(text),
                               y + toff + textHeight/2, text);
                paint.setPen(v->getForeground());
                paint.drawLine(5 + cw - cbw, y, 8 + cw - cbw, y);
                lasty = y;
                lastdb = idb;
            }
	}
	paint.restore();
    }

    paint.drawLine(cw + 7, 0, cw + 7, h);

    int bin = -1;

    for (int y = 0; y < v->getPaintHeight(); ++y) {

	double q0, q1;
	if (!getYBinRange(v, v->getPaintHeight() - y, q0, q1)) continue;

	int vy;

	if (int(q0) > bin) {
	    vy = y;
	    bin = int(q0);
	} else {
	    continue;
	}

	int freq = int((sr * bin) / getFFTSize());

	if (py >= 0 && (vy - py) < textHeight - 1) {
	    if (m_binScale == BinScale::Linear) {
		paint.drawLine(w - tickw, h - vy, w, h - vy);
	    }
	    continue;
	}

	QString text = QString("%1").arg(freq);
	if (bin == 1) text = tr("%1Hz").arg(freq); // bin 0 is DC
	paint.drawLine(cw + 7, h - vy, w - pkw - 1, h - vy);

	if (h - vy - textHeight >= -2) {
	    int tx = w - 3 - paint.fontMetrics().width(text) - max(tickw, pkw);
	    paint.drawText(tx, h - vy + toff, text);
	}

	py = vy;
    }

    if (m_binScale == BinScale::Log) {

        // piano keyboard

        PianoScale().paintPianoVertical
            (v, paint, QRect(w - pkw - 1, 0, pkw, h),
             getEffectiveMinFrequency(), getEffectiveMaxFrequency());
    }

    m_haveDetailedScale = detailed;
}

class SpectrogramRangeMapper : public RangeMapper
{
public:
    SpectrogramRangeMapper(sv_samplerate_t sr, int /* fftsize */) :
        m_dist(sr / 2),
        m_s2(sqrt(sqrt(2))) { }
    ~SpectrogramRangeMapper() { }
    
    virtual int getPositionForValue(double value) const {

        double dist = m_dist;
    
        int n = 0;

        while (dist > (value + 0.00001) && dist > 0.1) {
            dist /= m_s2;
            ++n;
        }

        return n;
    }
    
    virtual int getPositionForValueUnclamped(double value) const {
        // We don't really support this
        return getPositionForValue(value);
    }

    virtual double getValueForPosition(int position) const {

        // Vertical zoom step 0 shows the entire range from DC ->
        // Nyquist frequency.  Step 1 shows 2^(1/4) of the range of
        // step 0, and so on until the visible range is smaller than
        // the frequency step between bins at the current fft size.

        double dist = m_dist;
    
        int n = 0;
        while (n < position) {
            dist /= m_s2;
            ++n;
        }

        return dist;
    }
    
    virtual double getValueForPositionUnclamped(int position) const {
        // We don't really support this
        return getValueForPosition(position);
    }

    virtual QString getUnit() const { return "Hz"; }

protected:
    double m_dist;
    double m_s2;
};

int
SpectrogramLayer::getVerticalZoomSteps(int &defaultStep) const
{
    if (!m_model) return 0;

    sv_samplerate_t sr = m_model->getSampleRate();

    SpectrogramRangeMapper mapper(sr, getFFTSize());

//    int maxStep = mapper.getPositionForValue((double(sr) / getFFTSize()) + 0.001);
    int maxStep = mapper.getPositionForValue(0);
    int minStep = mapper.getPositionForValue(double(sr) / 2);

    int initialMax = m_initialMaxFrequency;
    if (initialMax == 0) initialMax = int(sr / 2);

    defaultStep = mapper.getPositionForValue(initialMax) - minStep;

//    SVDEBUG << "SpectrogramLayer::getVerticalZoomSteps: " << maxStep - minStep << " (" << maxStep <<"-" << minStep << "), default is " << defaultStep << " (from initial max freq " << initialMax << ")" << endl;

    return maxStep - minStep;
}

int
SpectrogramLayer::getCurrentVerticalZoomStep() const
{
    if (!m_model) return 0;

    double dmin, dmax;
    getDisplayExtents(dmin, dmax);
    
    SpectrogramRangeMapper mapper(m_model->getSampleRate(), getFFTSize());
    int n = mapper.getPositionForValue(dmax - dmin);
//    SVDEBUG << "SpectrogramLayer::getCurrentVerticalZoomStep: " << n << endl;
    return n;
}

void
SpectrogramLayer::setVerticalZoomStep(int step)
{
    if (!m_model) return;

    double dmin = m_minFrequency, dmax = m_maxFrequency;
//    getDisplayExtents(dmin, dmax);

//    cerr << "current range " << dmin << " -> " << dmax << ", range " << dmax-dmin << ", mid " << (dmax + dmin)/2 << endl;
    
    sv_samplerate_t sr = m_model->getSampleRate();
    SpectrogramRangeMapper mapper(sr, getFFTSize());
    double newdist = mapper.getValueForPosition(step);

    double newmin, newmax;

    if (m_binScale == BinScale::Log) {

        // need to pick newmin and newmax such that
        //
        // (log(newmin) + log(newmax)) / 2 == logmid
        // and
        // newmax - newmin = newdist
        //
        // so log(newmax - newdist) + log(newmax) == 2logmid
        // log(newmax(newmax - newdist)) == 2logmid
        // newmax.newmax - newmax.newdist == exp(2logmid)
        // newmax^2 + (-newdist)newmax + -exp(2logmid) == 0
        // quadratic with a = 1, b = -newdist, c = -exp(2logmid), all known
        // 
        // positive root
        // newmax = (newdist + sqrt(newdist^2 + 4exp(2logmid))) / 2
        //
        // but logmid = (log(dmin) + log(dmax)) / 2
        // so exp(2logmid) = exp(log(dmin) + log(dmax))
        // = exp(log(dmin.dmax))
        // = dmin.dmax
        // so newmax = (newdist + sqrtf(newdist^2 + 4dmin.dmax)) / 2

        newmax = (newdist + sqrt(newdist*newdist + 4*dmin*dmax)) / 2;
        newmin = newmax - newdist;

//        cerr << "newmin = " << newmin << ", newmax = " << newmax << endl;

    } else {
        double dmid = (dmax + dmin) / 2;
        newmin = dmid - newdist / 2;
        newmax = dmid + newdist / 2;
    }

    double mmin, mmax;
    mmin = 0;
    mmax = double(sr) / 2;
    
    if (newmin < mmin) {
        newmax += (mmin - newmin);
        newmin = mmin;
    }
    if (newmax > mmax) {
        newmax = mmax;
    }
    
//    SVDEBUG << "SpectrogramLayer::setVerticalZoomStep: " << step << ": " << newmin << " -> " << newmax << " (range " << newdist << ")" << endl;

    setMinFrequency(int(lrint(newmin)));
    setMaxFrequency(int(lrint(newmax)));
}

RangeMapper *
SpectrogramLayer::getNewVerticalZoomRangeMapper() const
{
    if (!m_model) return 0;
    return new SpectrogramRangeMapper(m_model->getSampleRate(), getFFTSize());
}

void
SpectrogramLayer::updateMeasureRectYCoords(LayerGeometryProvider *v, const MeasureRect &r) const
{
    int y0 = 0;
    if (r.startY > 0.0) y0 = int(getYForFrequency(v, r.startY));
    
    int y1 = y0;
    if (r.endY > 0.0) y1 = int(getYForFrequency(v, r.endY));

//    SVDEBUG << "SpectrogramLayer::updateMeasureRectYCoords: start " << r.startY << " -> " << y0 << ", end " << r.endY << " -> " << y1 << endl;

    r.pixrect = QRect(r.pixrect.x(), y0, r.pixrect.width(), y1 - y0);
}

void
SpectrogramLayer::setMeasureRectYCoord(LayerGeometryProvider *v, MeasureRect &r, bool start, int y) const
{
    if (start) {
        r.startY = getFrequencyForY(v, y);
        r.endY = r.startY;
    } else {
        r.endY = getFrequencyForY(v, y);
    }
//    SVDEBUG << "SpectrogramLayer::setMeasureRectYCoord: start " << r.startY << " <- " << y << ", end " << r.endY << " <- " << y << endl;

}

void
SpectrogramLayer::toXml(QTextStream &stream,
                        QString indent, QString extraAttributes) const
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
		 "binDisplay=\"%7\" ")
	.arg(m_minFrequency)
	.arg(m_maxFrequency)
	.arg(convertFromColourScale(m_colourScale))
	.arg(m_colourMap)
	.arg(m_colourRotation)
	.arg(int(m_binScale))
	.arg(int(m_binDisplay));

    // New-style normalization attributes, allowing for more types of
    // normalization in future: write out the column normalization
    // type separately, and then whether we are normalizing visible
    // area as well afterwards
    
    s += QString("columnNormalization=\"%1\" ")
        .arg(m_normalization == ColumnNormalization::Max1 ? "peak" :
             m_normalization == ColumnNormalization::Hybrid ? "hybrid" : "none");

    // Old-style normalization attribute. We *don't* write out
    // normalizeHybrid here because the only release that would accept
    // it (Tony v1.0) has a totally different scale factor for
    // it. We'll just have to accept that session files from Tony
    // v2.0+ will look odd in Tony v1.0
    
    s += QString("normalizeColumns=\"%1\" ")
	.arg(m_normalization == ColumnNormalization::Max1 ? "true" : "false");

    // And this applies to both old- and new-style attributes
    
    s += QString("normalizeVisibleArea=\"%1\" ")
        .arg(m_normalizeVisibleArea ? "true" : "false");
    
    Layer::toXml(stream, indent, extraAttributes + " " + s);
}

void
SpectrogramLayer::setProperties(const QXmlAttributes &attributes)
{
    bool ok = false;

    int channel = attributes.value("channel").toInt(&ok);
    if (ok) setChannel(channel);

    int windowSize = attributes.value("windowSize").toUInt(&ok);
    if (ok) setWindowSize(windowSize);

    int windowHopLevel = attributes.value("windowHopLevel").toUInt(&ok);
    if (ok) setWindowHopLevel(windowHopLevel);
    else {
        int windowOverlap = attributes.value("windowOverlap").toUInt(&ok);
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

    int minFrequency = attributes.value("minFrequency").toUInt(&ok);
    if (ok) {
        SVDEBUG << "SpectrogramLayer::setProperties: setting min freq to " << minFrequency << endl;
        setMinFrequency(minFrequency);
    }

    int maxFrequency = attributes.value("maxFrequency").toUInt(&ok);
    if (ok) {
        SVDEBUG << "SpectrogramLayer::setProperties: setting max freq to " << maxFrequency << endl;
        setMaxFrequency(maxFrequency);
    }

    ColourScale::Scale colourScale = convertToColourScale
        (attributes.value("colourScale").toInt(&ok));
    if (ok) setColourScale(colourScale);

    int colourMap = attributes.value("colourScheme").toInt(&ok);
    if (ok) setColourMap(colourMap);

    int colourRotation = attributes.value("colourRotation").toInt(&ok);
    if (ok) setColourRotation(colourRotation);

    BinScale binScale = (BinScale)
	attributes.value("frequencyScale").toInt(&ok);
    if (ok) setBinScale(binScale);

    BinDisplay binDisplay = (BinDisplay)
	attributes.value("binDisplay").toInt(&ok);
    if (ok) setBinDisplay(binDisplay);

    bool haveNewStyleNormalization = false;
    
    QString columnNormalization = attributes.value("columnNormalization");

    if (columnNormalization != "") {

        haveNewStyleNormalization = true;

        if (columnNormalization == "peak") {
            setNormalization(ColumnNormalization::Max1);
        } else if (columnNormalization == "hybrid") {
            setNormalization(ColumnNormalization::Hybrid);
        } else if (columnNormalization == "none") {
            setNormalization(ColumnNormalization::None);
        } else {
            cerr << "NOTE: Unknown or unsupported columnNormalization attribute \""
                 << columnNormalization << "\"" << endl;
        }
    }

    if (!haveNewStyleNormalization) {

        bool normalizeColumns =
            (attributes.value("normalizeColumns").trimmed() == "true");
        if (normalizeColumns) {
            setNormalization(ColumnNormalization::Max1);
        }

        bool normalizeHybrid =
            (attributes.value("normalizeHybrid").trimmed() == "true");
        if (normalizeHybrid) {
            setNormalization(ColumnNormalization::Hybrid);
        }
    }

    bool normalizeVisibleArea =
        (attributes.value("normalizeVisibleArea").trimmed() == "true");
    setNormalizeVisibleArea(normalizeVisibleArea);

    if (!haveNewStyleNormalization && m_normalization == ColumnNormalization::Hybrid) {
        // Tony v1.0 is (and hopefully will remain!) the only released
        // SV-a-like to use old-style attributes when saving sessions
        // that ask for hybrid normalization. It saves them with the
        // wrong gain factor, so hack in a fix for that here -- this
        // gives us backward but not forward compatibility.
        setGain(m_gain / float(getFFTSize() / 2));
    }
}
    
