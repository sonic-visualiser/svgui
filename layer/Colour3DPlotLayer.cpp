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

#include "Colour3DPlotLayer.h"

#include "base/Profiler.h"
#include "base/LogRange.h"
#include "base/RangeMapper.h"

#include "ColourMapper.h"
#include "LayerGeometryProvider.h"
#include "PaintAssistant.h"

#include "data/model/Dense3DModelPeakCache.h"

#include "view/ViewManager.h"

#include <QPainter>
#include <QImage>
#include <QRect>
#include <QTextStream>
#include <QSettings>

#include <iostream>

#include <cassert>

#ifndef __GNUC__
#include <alloca.h>
#endif

using std::vector;

//#define DEBUG_COLOUR_3D_PLOT_LAYER_PAINT 1


Colour3DPlotLayer::Colour3DPlotLayer() :
    m_model(0),
    m_colourScale(ColourScaleType::Linear),
    m_colourScaleSet(false),
    m_colourMap(0),
    m_gain(1.0),
    m_binScale(BinScale::Linear),
    m_normalization(ColumnNormalization::None),
    m_normalizeVisibleArea(false),
    m_invertVertical(false),
    m_opaque(false),
    m_smooth(false),
    m_peakResolution(256),
    m_miny(0),
    m_maxy(0),
    m_synchronous(false),
    m_peakCache(0),
    m_peakCacheDivisor(8)
{
    QSettings settings;
    settings.beginGroup("Preferences");
    setColourMap(settings.value("colour-3d-plot-colour", ColourMapper::Green).toInt());
    settings.endGroup();
}

Colour3DPlotLayer::~Colour3DPlotLayer()
{
    invalidateRenderers();
    delete m_peakCache;
}

ColourScaleType
Colour3DPlotLayer::convertToColourScale(int value)
{
    switch (value) {
    default:
    case 0: return ColourScaleType::Linear;
    case 1: return ColourScaleType::Log;
    case 2: return ColourScaleType::PlusMinusOne;
    case 3: return ColourScaleType::Absolute;
    }
}

int
Colour3DPlotLayer::convertFromColourScale(ColourScaleType scale)
{
    switch (scale) {
    case ColourScaleType::Linear: return 0;
    case ColourScaleType::Log: return 1;
    case ColourScaleType::PlusMinusOne: return 2;
    case ColourScaleType::Absolute: return 3;

    case ColourScaleType::Meter:
    case ColourScaleType::Phase:
    default: return 0;
    }
}

std::pair<ColumnNormalization, bool>
Colour3DPlotLayer::convertToColumnNorm(int value)
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
Colour3DPlotLayer::convertFromColumnNorm(ColumnNormalization norm, bool visible)
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
Colour3DPlotLayer::setSynchronousPainting(bool synchronous)
{
    m_synchronous = synchronous;
}

void
Colour3DPlotLayer::setModel(const DenseThreeDimensionalModel *model)
{
    if (m_model == model) return;
    const DenseThreeDimensionalModel *oldModel = m_model;
    m_model = model;
    if (!m_model || !m_model->isOK()) return;

    connectSignals(m_model);

    connect(m_model, SIGNAL(modelChanged()), this, SLOT(modelChanged()));
    connect(m_model, SIGNAL(modelChangedWithin(sv_frame_t, sv_frame_t)),
	    this, SLOT(modelChangedWithin(sv_frame_t, sv_frame_t)));

    m_peakResolution = 256;
    if (model->getResolution() > 512) {
        m_peakResolution = 16;
    } else if (model->getResolution() > 128) {
        m_peakResolution = 64;
    } else if (model->getResolution() > 2) {
        m_peakResolution = 128;
    }

    delete m_peakCache;
    m_peakCache = 0;

    invalidateRenderers();

    emit modelReplaced();
    emit sliceableModelReplaced(oldModel, model);
}

void
Colour3DPlotLayer::cacheInvalid()
{
    invalidateRenderers();
}

void
Colour3DPlotLayer::cacheInvalid(sv_frame_t /* startFrame */,
                                sv_frame_t /* endFrame */)
{
    //!!! should do this only if the range is visible
    delete m_peakCache;
    m_peakCache = 0;
    
    invalidateRenderers();
}

void
Colour3DPlotLayer::invalidateRenderers()
{
    for (ViewRendererMap::iterator i = m_renderers.begin();
         i != m_renderers.end(); ++i) {
        delete i->second;
    }
    m_renderers.clear();
}

Dense3DModelPeakCache *
Colour3DPlotLayer::getPeakCache() const
{
    if (!m_peakCache) {
        m_peakCache = new Dense3DModelPeakCache(m_model, m_peakCacheDivisor);
    }
    return m_peakCache;
}

void
Colour3DPlotLayer::modelChanged()
{
    if (!m_colourScaleSet && m_colourScale == ColourScaleType::Linear) {
        if (m_model) {
            if (m_model->shouldUseLogValueScale()) {
                setColourScale(ColourScaleType::Log);
            } else {
                m_colourScaleSet = true;
            }
        }
    }
    cacheInvalid();
}

void
Colour3DPlotLayer::modelChangedWithin(sv_frame_t startFrame, sv_frame_t endFrame)
{
    if (!m_colourScaleSet && m_colourScale == ColourScaleType::Linear) {
        if (m_model && m_model->getWidth() > 50) {
            if (m_model->shouldUseLogValueScale()) {
                setColourScale(ColourScaleType::Log);
            } else {
                m_colourScaleSet = true;
            }
        }
    }
    cacheInvalid(startFrame, endFrame);
}

Layer::PropertyList
Colour3DPlotLayer::getProperties() const
{
    PropertyList list;
    list.push_back("Colour");
    list.push_back("Colour Scale");
    list.push_back("Normalization");
    list.push_back("Gain");
    list.push_back("Bin Scale");
    list.push_back("Invert Vertical Scale");
    list.push_back("Opaque");
    list.push_back("Smooth");
    return list;
}

QString
Colour3DPlotLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Colour") return tr("Colour");
    if (name == "Colour Scale") return tr("Scale");
    if (name == "Normalization") return tr("Normalization");
    if (name == "Invert Vertical Scale") return tr("Invert Vertical Scale");
    if (name == "Gain") return tr("Gain");
    if (name == "Opaque") return tr("Always Opaque");
    if (name == "Smooth") return tr("Smooth");
    if (name == "Bin Scale") return tr("Bin Scale");
    return "";
}

QString
Colour3DPlotLayer::getPropertyIconName(const PropertyName &name) const
{
    if (name == "Invert Vertical Scale") return "invert-vertical";
    if (name == "Opaque") return "opaque";
    if (name == "Smooth") return "smooth";
    return "";
}

Layer::PropertyType
Colour3DPlotLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Gain") return RangeProperty;
    if (name == "Invert Vertical Scale") return ToggleProperty;
    if (name == "Opaque") return ToggleProperty;
    if (name == "Smooth") return ToggleProperty;
    return ValueProperty;
}

QString
Colour3DPlotLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Normalization" ||
        name == "Colour Scale" ||
        name == "Gain") return tr("Scale");
    if (name == "Bin Scale" ||
        name == "Invert Vertical Scale") return tr("Bins");
    if (name == "Opaque" ||
        name == "Smooth" ||
        name == "Colour") return tr("Colour");
    return QString();
}

int
Colour3DPlotLayer::getPropertyRangeAndValue(const PropertyName &name,
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

        *deflt = int(lrint(log10(1.0) * 20.0));
	if (*deflt < *min) *deflt = *min;
	if (*deflt > *max) *deflt = *max;

	val = int(lrint(log10(m_gain) * 20.0));
	if (val < *min) val = *min;
	if (val > *max) val = *max;

    } else if (name == "Colour Scale") {

        // linear, log, +/-1, abs
	*min = 0;
	*max = 3;
        *deflt = 0;

	val = convertFromColourScale(m_colourScale);

    } else if (name == "Colour") {

	*min = 0;
	*max = ColourMapper::getColourMapCount() - 1;
        *deflt = 0;

	val = m_colourMap;

    } else if (name == "Normalization") {
	
        *min = 0;
        *max = 3;
        *deflt = 0;

        val = convertFromColumnNorm(m_normalization, m_normalizeVisibleArea);

    } else if (name == "Invert Vertical Scale") {
	
        *deflt = 0;
	val = (m_invertVertical ? 1 : 0);

    } else if (name == "Bin Scale") {

	*min = 0;
	*max = 1;
        *deflt = int(BinScale::Linear);
	val = (int)m_binScale;

    } else if (name == "Opaque") {
	
        *deflt = 0;
	val = (m_opaque ? 1 : 0);
        
    } else if (name == "Smooth") {
	
        *deflt = 0;
	val = (m_smooth ? 1 : 0);
        
    } else {
	val = Layer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
Colour3DPlotLayer::getPropertyValueLabel(const PropertyName &name,
				    int value) const
{
    if (name == "Colour") {
        return ColourMapper::getColourMapName(value);
    }
    if (name == "Colour Scale") {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Log");
	case 2: return tr("+/-1");
	case 3: return tr("Absolute");
	}
    }
    if (name == "Normalization") {
        return ""; // icon only
    }
    if (name == "Bin Scale") {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Log");
	}
    }
    return tr("<unknown>");
}

QString
Colour3DPlotLayer::getPropertyValueIconName(const PropertyName &name,
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
Colour3DPlotLayer::getNewPropertyRangeMapper(const PropertyName &name) const
{
    if (name == "Gain") {
        return new LinearRangeMapper(-50, 50, -25, 25, tr("dB"));
    }
    return 0;
}

void
Colour3DPlotLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Gain") {
	setGain(float(pow(10, value/20.0)));
    } else if (name == "Colour Scale") {
        setColourScale(convertToColourScale(value));
    } else if (name == "Colour") {
        setColourMap(value);
    } else if (name == "Invert Vertical Scale") {
	setInvertVertical(value ? true : false);
    } else if (name == "Opaque") {
	setOpaque(value ? true : false);
    } else if (name == "Smooth") {
	setSmooth(value ? true : false);
    } else if (name == "Bin Scale") {
	switch (value) {
	default:
	case 0: setBinScale(BinScale::Linear); break;
	case 1: setBinScale(BinScale::Log); break;
	}
    } else if (name == "Normalization") {
        auto n = convertToColumnNorm(value);
        setNormalization(n.first);
        setNormalizeVisibleArea(n.second);
    }
}

void
Colour3DPlotLayer::setColourScale(ColourScaleType scale)
{
    if (m_colourScale == scale) return;
    m_colourScale = scale;
    m_colourScaleSet = true;
    invalidateRenderers();
    emit layerParametersChanged();
}

void
Colour3DPlotLayer::setColourMap(int map)
{
    if (m_colourMap == map) return;
    m_colourMap = map;
    invalidateRenderers();
    emit layerParametersChanged();
}

void
Colour3DPlotLayer::setGain(float gain)
{
    if (m_gain == gain) return;
    m_gain = gain;
    invalidateRenderers();
    emit layerParametersChanged();
}

float
Colour3DPlotLayer::getGain() const
{
    return m_gain;
}

void
Colour3DPlotLayer::setBinScale(BinScale binScale)
{
    if (m_binScale == binScale) return;
    m_binScale = binScale;
    invalidateRenderers();
    emit layerParametersChanged();
}

BinScale
Colour3DPlotLayer::getBinScale() const
{
    return m_binScale;
}

void
Colour3DPlotLayer::setNormalization(ColumnNormalization n)
{
    if (m_normalization == n) return;

    m_normalization = n;
    invalidateRenderers();
    
    emit layerParametersChanged();
}

ColumnNormalization
Colour3DPlotLayer::getNormalization() const
{
    return m_normalization;
}

void
Colour3DPlotLayer::setNormalizeVisibleArea(bool n)
{
    if (m_normalizeVisibleArea == n) return;

    m_normalizeVisibleArea = n;
    invalidateRenderers();
    
    emit layerParametersChanged();
}

bool
Colour3DPlotLayer::getNormalizeVisibleArea() const
{
    return m_normalizeVisibleArea;
}

void
Colour3DPlotLayer::setInvertVertical(bool n)
{
    if (m_invertVertical == n) return;
    m_invertVertical = n;
    invalidateRenderers();
    emit layerParametersChanged();
}

void
Colour3DPlotLayer::setOpaque(bool n)
{
    if (m_opaque == n) return;
    m_opaque = n;
    invalidateRenderers();
    emit layerParametersChanged();
}

void
Colour3DPlotLayer::setSmooth(bool n)
{
    if (m_smooth == n) return;
    m_smooth = n;
    invalidateRenderers();
    emit layerParametersChanged();
}

bool
Colour3DPlotLayer::getInvertVertical() const
{
    return m_invertVertical;
}

bool
Colour3DPlotLayer::getOpaque() const
{
    return m_opaque;
}

bool
Colour3DPlotLayer::getSmooth() const
{
    return m_smooth;
}

void
Colour3DPlotLayer::setLayerDormant(const LayerGeometryProvider *v, bool dormant)
{
    if (dormant) {

#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
        cerr << "Colour3DPlotLayer::setLayerDormant(" << dormant << ")"
                  << endl;
#endif

        if (isLayerDormant(v)) {
            return;
        }

        Layer::setLayerDormant(v, true);

        cacheInvalid();
	
    } else {

        Layer::setLayerDormant(v, false);
    }
}

bool
Colour3DPlotLayer::isLayerScrollable(const LayerGeometryProvider *v) const
{
    if (m_normalizeVisibleArea) {
        return false;
    }
//!!!    if (shouldPaintDenseIn(v)) {
//        return true;
//    }
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

bool
Colour3DPlotLayer::getValueExtents(double &min, double &max,
                                   bool &logarithmic, QString &unit) const
{
    if (!m_model) return false;

    min = 0;
    max = double(m_model->getHeight());

    logarithmic = false;
    unit = "";

    return true;
}

bool
Colour3DPlotLayer::getDisplayExtents(double &min, double &max) const
{
    if (!m_model) return false;

    double hmax = double(m_model->getHeight());
    
    min = m_miny;
    max = m_maxy;
    if (max <= min) {
        min = 0;
        max = hmax;
    }
    if (min < 0) min = 0;
    if (max > hmax) max = hmax;

    return true;
}

bool
Colour3DPlotLayer::setDisplayExtents(double min, double max)
{
    if (!m_model) return false;

    m_miny = int(lrint(min));
    m_maxy = int(lrint(max));
    
    emit layerParametersChanged();
    return true;
}

bool
Colour3DPlotLayer::getYScaleValue(const LayerGeometryProvider *, int,
                                  double &, QString &) const
{
    return false;//!!!
}

int
Colour3DPlotLayer::getVerticalZoomSteps(int &defaultStep) const
{
    if (!m_model) return 0;

    defaultStep = 0;
    int h = m_model->getHeight();
    return h;
}

int
Colour3DPlotLayer::getCurrentVerticalZoomStep() const
{
    if (!m_model) return 0;

    double min, max;
    getDisplayExtents(min, max);
    return m_model->getHeight() - int(lrint(max - min));
}

void
Colour3DPlotLayer::setVerticalZoomStep(int step)
{
    if (!m_model) return;

//    SVDEBUG << "Colour3DPlotLayer::setVerticalZoomStep(" <<step <<"): before: miny = " << m_miny << ", maxy = " << m_maxy << endl;

    int dist = m_model->getHeight() - step;
    if (dist < 1) dist = 1;
    double centre = m_miny + (m_maxy - m_miny) / 2.0;
    m_miny = int(lrint(centre - dist/2.0));
    if (m_miny < 0) m_miny = 0;
    m_maxy = m_miny + dist;
    if (m_maxy > m_model->getHeight()) m_maxy = m_model->getHeight();

//    SVDEBUG << "Colour3DPlotLayer::setVerticalZoomStep(" <<step <<"):  after: miny = " << m_miny << ", maxy = " << m_maxy << endl;
    
    emit layerParametersChanged();
}

RangeMapper *
Colour3DPlotLayer::getNewVerticalZoomRangeMapper() const
{
    if (!m_model) return 0;

    return new LinearRangeMapper(0, m_model->getHeight(),
                                 0, m_model->getHeight(), "");
}

double
Colour3DPlotLayer::getYForBin(LayerGeometryProvider *v, double bin) const
{
    double y = bin;
    if (!m_model) return y;
    double mn = 0, mx = m_model->getHeight();
    getDisplayExtents(mn, mx);
    double h = v->getPaintHeight();
    if (m_binScale == BinScale::Linear) {
        y = h - (((bin - mn) * h) / (mx - mn));
    } else {
        double logmin = mn + 1, logmax = mx + 1;
        LogRange::mapRange(logmin, logmax);
        y = h - (((LogRange::map(bin + 1) - logmin) * h) / (logmax - logmin));
    }
    return y;
}

double
Colour3DPlotLayer::getBinForY(LayerGeometryProvider *v, double y) const
{
    double bin = y;
    if (!m_model) return bin;
    double mn = 0, mx = m_model->getHeight();
    getDisplayExtents(mn, mx);
    double h = v->getPaintHeight();
    if (m_binScale == BinScale::Linear) {
        bin = mn + ((h - y) * (mx - mn)) / h;
    } else {
        double logmin = mn + 1, logmax = mx + 1;
        LogRange::mapRange(logmin, logmax);
        bin = LogRange::unmap(logmin + ((h - y) * (logmax - logmin)) / h) - 1;
    }
    return bin;
}

QString
Colour3DPlotLayer::getFeatureDescription(LayerGeometryProvider *v, QPoint &pos) const
{
    if (!m_model) return "";

    int x = pos.x();
    int y = pos.y();

    sv_frame_t modelStart = m_model->getStartFrame();
    int modelResolution = m_model->getResolution();

    double srRatio =
        v->getViewManager()->getMainModelSampleRate() /
        m_model->getSampleRate();

    int sx0 = int((double(v->getFrameForX(x)) / srRatio - double(modelStart)) /
                  modelResolution);

    int f0 = sx0 * modelResolution;
    int f1 =  f0 + modelResolution;

    int sh = m_model->getHeight();

    int symin = m_miny;
    int symax = m_maxy;
    if (symax <= symin) {
        symin = 0;
        symax = sh;
    }
    if (symin < 0) symin = 0;
    if (symax > sh) symax = sh;

 //    double binHeight = double(v->getPaintHeight()) / (symax - symin);
//    int sy = int((v->getPaintHeight() - y) / binHeight) + symin;

    int sy = getIBinForY(v, y);

    if (sy < 0 || sy >= m_model->getHeight()) {
        return "";
    }

    if (m_invertVertical) sy = m_model->getHeight() - sy - 1;

    float value = m_model->getValueAt(sx0, sy);

//    cerr << "bin value (" << sx0 << "," << sy << ") is " << value << endl;
    
    QString binName = m_model->getBinName(sy);
    if (binName == "") binName = QString("[%1]").arg(sy + 1);
    else binName = QString("%1 [%2]").arg(binName).arg(sy + 1);

    QString text = tr("Time:\t%1 - %2\nBin:\t%3\nValue:\t%4")
	.arg(RealTime::frame2RealTime(f0, m_model->getSampleRate())
	     .toText(true).c_str())
	.arg(RealTime::frame2RealTime(f1, m_model->getSampleRate())
	     .toText(true).c_str())
	.arg(binName)
	.arg(value);

    return text;
}

int
Colour3DPlotLayer::getColourScaleWidth(QPainter &p) const
{
    // Font is rotated
    int cw = p.fontMetrics().height();
    return cw;
}

int
Colour3DPlotLayer::getVerticalScaleWidth(LayerGeometryProvider *, bool, QPainter &paint) const
{
    if (!m_model) return 0;

    QString sampleText = QString("[%1]").arg(m_model->getHeight());
    int tw = paint.fontMetrics().width(sampleText);
    bool another = false;

    for (int i = 0; i < m_model->getHeight(); ++i) {
	if (m_model->getBinName(i).length() > sampleText.length()) {
	    sampleText = m_model->getBinName(i);
            another = true;
	}
    }
    if (another) {
	tw = std::max(tw, paint.fontMetrics().width(sampleText));
    }

    return tw + 13 + getColourScaleWidth(paint);
}

void
Colour3DPlotLayer::paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect rect) const
{
    if (!m_model) return;

    int h = rect.height(), w = rect.width();

    int cw = getColourScaleWidth(paint);
    
    int ch = h - 20;
    if (ch > 20) {

        double min = m_model->getMinimumLevel();
        double max = m_model->getMaximumLevel();

        double mmin = min;
        double mmax = max;

        if (m_colourScale == ColourScaleType::Log) {
            LogRange::mapRange(mmin, mmax);
        } else if (m_colourScale == ColourScaleType::PlusMinusOne) {
            mmin = -1.f;
            mmax = 1.f;
        } else if (m_colourScale == ColourScaleType::Absolute) {
            if (mmin < 0) {
                if (fabs(mmin) > fabs(mmax)) mmax = fabs(mmin);
                else mmax = fabs(mmax);
                mmin = 0;
            } else {
                mmin = fabs(mmin);
                mmax = fabs(mmax);
            }
        }
    
        if (max == min) max = min + 1.f;
        if (mmax == mmin) mmax = mmin + 1.f;
    
        paint.setPen(v->getForeground());
        paint.drawRect(4, 10, cw - 8, ch+1);

        for (int y = 0; y < ch; ++y) {
            double value = ((max - min) * (double(ch-y) - 1.0)) / double(ch) + min;
            if (m_colourScale == ColourScaleType::Log) {
                value = LogRange::map(value);
            }
            int pixel = int(((value - mmin) * 256) / (mmax - mmin));
            if (pixel >= 0 && pixel < 256) {
//!!! replace this
                // QRgb c = m_cache->color(pixel);
                // paint.setPen(QColor(qRed(c), qGreen(c), qBlue(c)));
                paint.drawLine(5, 11 + y, cw - 5, 11 + y);
            } else {
                cerr << "WARNING: Colour3DPlotLayer::paintVerticalScale: value " << value << ", mmin " << mmin << ", mmax " << mmax << " leads to invalid pixel " << pixel << endl;
            }
        }

        QString minstr = QString("%1").arg(min);
        QString maxstr = QString("%1").arg(max);
        
        paint.save();

        QFont font = paint.font();
        if (font.pixelSize() > 0) {
            int newSize = int(font.pixelSize() * 0.65);
            if (newSize < 6) newSize = 6;
            font.setPixelSize(newSize);
            paint.setFont(font);
        }

        int msw = paint.fontMetrics().width(maxstr);

        QMatrix m;
        m.translate(cw - 6, ch + 10);
        m.rotate(-90);

        paint.setWorldMatrix(m);

        PaintAssistant::drawVisibleText(v, paint, 2, 0, minstr, PaintAssistant::OutlinedText);

        m.translate(ch - msw - 2, 0);
        paint.setWorldMatrix(m);

        PaintAssistant::drawVisibleText(v, paint, 0, 0, maxstr, PaintAssistant::OutlinedText);

        paint.restore();
    }

    paint.setPen(v->getForeground());

    int sh = m_model->getHeight();

    int symin = m_miny;
    int symax = m_maxy;
    if (symax <= symin) {
        symin = 0;
        symax = sh;
    }
    if (symin < 0) symin = 0;
    if (symax > sh) symax = sh;

    paint.save();

    int py = h;

    int defaultFontHeight = paint.fontMetrics().height();
    
    for (int i = symin; i <= symax; ++i) {

        int y0;

        y0 = getIYForBin(v, i);
        int h = py - y0;

        if (i > symin) {
            if (paint.fontMetrics().height() >= h) {
                if (h >= defaultFontHeight * 0.8) {
                    QFont tf = paint.font();
                    tf.setPixelSize(int(h * 0.8));
                    paint.setFont(tf);
                } else {
                    continue;
                }
            }
        }
	
        py = y0;

        if (i < symax) {
            paint.drawLine(cw, y0, w, y0);
        }

        if (i > symin) {

            int idx = i - 1;
            if (m_invertVertical) idx = m_model->getHeight() - idx - 1;

            QString text = m_model->getBinName(idx);
            if (text == "") text = QString("[%1]").arg(idx + 1);

            int ty = y0 + (h/2) - (paint.fontMetrics().height()/2) +
                paint.fontMetrics().ascent() + 1;

            paint.drawText(cw + 5, ty, text);
        }
    }

    paint.restore();
}

DenseThreeDimensionalModel::Column
Colour3DPlotLayer::getColumn(int col) const
{
    Profiler profiler("Colour3DPlotLayer::getColumn");

    DenseThreeDimensionalModel::Column values = m_model->getColumn(col);
    values.resize(m_model->getHeight(), 0.f);
    if (m_normalization != ColumnNormalization::Max1 &&
        m_normalization != ColumnNormalization::Hybrid) {
        return values;
    }

    double colMax = 0.f, colMin = 0.f;
    double min = 0.f, max = 0.f;

    int nv = int(values.size());
    
    min = m_model->getMinimumLevel();
    max = m_model->getMaximumLevel();

    for (int y = 0; y < nv; ++y) {
        if (y == 0 || values.at(y) > colMax) colMax = values.at(y);
        if (y == 0 || values.at(y) < colMin) colMin = values.at(y);
    }
    if (colMin == colMax) colMax = colMin + 1;
    
    for (int y = 0; y < nv; ++y) {
    
        double value = values.at(y);
        double norm = (value - colMin) / (colMax - colMin);
        double newvalue = min + (max - min) * norm;

        if (value != newvalue) values[y] = float(newvalue);
    }

    if (m_normalization == ColumnNormalization::Hybrid
        && (colMax > 0.0)) {
        double logmax = log10(colMax);
        for (int y = 0; y < nv; ++y) {
            values[y] = float(values[y] * logmax);
        }
    }

    return values;
}
/*!!! replace this    
bool
Colour3DPlotLayer::shouldPaintDenseIn(const LayerGeometryProvider *v) const
{
    if (!m_model || !v || !(v->getViewManager())) {
        return false;
    }
    double srRatio =
        v->getViewManager()->getMainModelSampleRate() / m_model->getSampleRate();
    if (m_opaque || 
        m_smooth ||
        m_model->getHeight() >= v->getPaintHeight() ||
        ((m_model->getResolution() * srRatio) / v->getZoomLevel()) < 2) {
        return true;
    }
    return false;
}
*/
Colour3DPlotRenderer *
Colour3DPlotLayer::getRenderer(LayerGeometryProvider *v) const
{
    if (m_renderers.find(v->getId()) == m_renderers.end()) {

        Colour3DPlotRenderer::Sources sources;
        sources.verticalBinLayer = this;
        sources.fft = 0;
        sources.source = m_model;
        sources.peaks = getPeakCache();

        ColourScale::Parameters cparams;
        cparams.colourMap = m_colourMap;
        cparams.scale = m_colourScale;
        cparams.gain = m_gain;

        Colour3DPlotRenderer::Parameters params;
        params.colourScale = ColourScale(cparams);
        params.normalization = m_normalization;
        params.binScale = m_binScale;
        params.alwaysOpaque = m_opaque;
        params.invertVertical = m_invertVertical;
        params.interpolate = m_smooth;

        m_renderers[v->getId()] = new Colour3DPlotRenderer(sources, params);
    }

    return m_renderers[v->getId()];
}

void
Colour3DPlotLayer::paintWithRenderer(LayerGeometryProvider *v,
                                     QPainter &paint, QRect rect) const
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

void
Colour3DPlotLayer::paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
/*
    if (m_model) {
        SVDEBUG << "Colour3DPlotLayer::paint: model says shouldUseLogValueScale = " << m_model->shouldUseLogValueScale() << endl;
    }
*/
    Profiler profiler("Colour3DPlotLayer::paint");
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
    cerr << "Colour3DPlotLayer::paint(): m_model is " << m_model << ", zoom level is " << v->getZoomLevel() << ", rect is (" << rect.x() << "," << rect.y() << ") " << rect.width() << "x" << rect.height() << endl;
#endif

    int completion = 0;
    if (!m_model || !m_model->isOK() || !m_model->isReady(&completion)) {
	if (completion > 0) {
	    paint.fillRect(0, 10, v->getPaintWidth() * completion / 100,
			   10, QColor(120, 120, 120));
	}
	return;
    }

    if (m_model->getWidth() == 0) {
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
        cerr << "Colour3DPlotLayer::paint(): model width == 0, "
             << "nothing to paint (yet)" << endl;
#endif
        return;
    }

    //!!!???
    
    if (m_normalizeVisibleArea) {
        rect = v->getPaintRect();
    }

    //!!! why is the setLayerDormant(false) found here in
    //!!! SpectrogramLayer not present in Colour3DPlotLayer?
    //!!! unnecessary? vestigial? forgotten?

    paintWithRenderer(v, paint, rect);
}

/*!!! This will be needed in some form still.

    sv_frame_t modelStart = m_model->getStartFrame();
    sv_frame_t modelEnd = m_model->getEndFrame();
    int modelResolution = m_model->getResolution();

    // The cache is from the model's start frame to the model's end
    // frame at the model's window increment frames per pixel.  We
    // want to draw from our start frame + x0 * zoomLevel to our start
    // frame + x1 * zoomLevel at zoomLevel frames per pixel.

    //  We have quite different paint mechanisms for rendering "large"
    //  bins (more than one bin per pixel in both directions) and
    //  "small".  This is "large"; see paintDense below for "small".

    int x0 = rect.left();
    int x1 = rect.right() + 1;

    int h = v->getPaintHeight();

    double srRatio =
        v->getViewManager()->getMainModelSampleRate() / m_model->getSampleRate();

    // the s-prefix values are source, i.e. model, column and bin numbers
    int sx0 = int((double(v->getFrameForX(x0)) / srRatio - double(modelStart))
                  / modelResolution);
    int sx1 = int((double(v->getFrameForX(x1)) / srRatio - double(modelStart))
                  / modelResolution);
    int sh = m_model->getHeight();

    int symin = m_miny;
    int symax = m_maxy;
    if (symax <= symin) {
        symin = 0;
        symax = sh;
    }
    if (symin < 0) symin = 0;
    if (symax > sh) symax = sh;

    if (sx0 > 0) --sx0;
    fillCache(sx0 < 0 ? 0 : sx0,
              sx1 < 0 ? 0 : sx1);

#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
    cerr << "Colour3DPlotLayer::paint: height = "<< m_model->getHeight() << ", modelStart = " << modelStart << ", resolution = " << modelResolution << ", model rate = " << m_model->getSampleRate() << " (zoom level = " << v->getZoomLevel() << ", srRatio = " << srRatio << ")" << endl;
#endif

    if (shouldPaintDenseIn(v)) {
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
        cerr << "calling paintDense" << endl;
#endif
        paintDense(v, paint, rect);
        return;
    }

#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
    cerr << "Colour3DPlotLayer::paint: w " << x1-x0 << ", h " << h << ", sx0 " << sx0 << ", sx1 " << sx1 << ", sw " << sx1-sx0 << ", sh " << sh << endl;
    cerr << "Colour3DPlotLayer: sample rate is " << m_model->getSampleRate() << ", resolution " << m_model->getResolution() << endl;
#endif

    QPoint illuminatePos;
    bool illuminate = v->shouldIlluminateLocalFeatures(this, illuminatePos);
    
    const int buflen = 40;
    char labelbuf[buflen];

    for (int sx = sx0; sx <= sx1; ++sx) {

	sv_frame_t fx = sx * modelResolution + modelStart;

	if (fx + modelResolution <= modelStart || fx > modelEnd) continue;

        int rx0 = v->getXForFrame(int(double(fx) * srRatio));
	int rx1 = v->getXForFrame(int(double(fx + modelResolution + 1) * srRatio));

	int rw = rx1 - rx0;
	if (rw < 1) rw = 1;

	bool showLabel = (rw > 10 &&
			  paint.fontMetrics().width("0.000000") < rw - 3 &&
			  paint.fontMetrics().height() < (h / sh));
        
	for (int sy = symin; sy < symax; ++sy) {

            int ry0 = getIYForBin(v, sy);
            int ry1 = getIYForBin(v, sy + 1);
            QRect r(rx0, ry1, rw, ry0 - ry1);

	    QRgb pixel = qRgb(255, 255, 255);
	    if (sx >= 0 && sx < m_cache->width() &&
		sy >= 0 && sy < m_cache->height()) {
		pixel = m_cache->pixel(sx, sy);
	    }

            if (rw == 1) {
                paint.setPen(pixel);
                paint.setBrush(Qt::NoBrush);
                paint.drawLine(r.x(), r.y(), r.x(), r.y() + r.height() - 1);
                continue;
            }

	    QColor pen(255, 255, 255, 80);
	    QColor brush(pixel);

            if (rw > 3 && r.height() > 3) {
                brush.setAlpha(160);
            }

	    paint.setPen(Qt::NoPen);
	    paint.setBrush(brush);

	    if (illuminate) {
		if (r.contains(illuminatePos)) {
		    paint.setPen(v->getForeground());
		}
	    }
            
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
//            cerr << "rect " << r.x() << "," << r.y() << " "
//                      << r.width() << "x" << r.height() << endl;
#endif

	    paint.drawRect(r);

	    if (showLabel) {
		if (sx >= 0 && sx < m_cache->width() &&
		    sy >= 0 && sy < m_cache->height()) {
		    double value = m_model->getValueAt(sx, sy);
		    snprintf(labelbuf, buflen, "%06f", value);
		    QString text(labelbuf);
                    PaintAssistant::drawVisibleText
                        (v,
                         paint,
                         rx0 + 2,
                         ry0 - h / sh - 1 + 2 + paint.fontMetrics().ascent(),
                         text,
                         PaintAssistant::OutlinedText);
		}
	    }
	}
    }
}
*/


bool
Colour3DPlotLayer::snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
				      int &resolution,
				      SnapType snap) const
{
    if (!m_model) {
	return Layer::snapToFeatureFrame(v, frame, resolution, snap);
    }

    resolution = m_model->getResolution();
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
Colour3DPlotLayer::toXml(QTextStream &stream,
                         QString indent, QString extraAttributes) const
{
    QString s = QString("scale=\"%1\" "
                        "colourScheme=\"%2\" "
                        "minY=\"%3\" "
                        "maxY=\"%4\" "
                        "invertVertical=\"%5\" "
                        "opaque=\"%6\" %7")
	.arg(convertFromColourScale(m_colourScale))
        .arg(m_colourMap)
        .arg(m_miny)
        .arg(m_maxy)
        .arg(m_invertVertical ? "true" : "false")
        .arg(m_opaque ? "true" : "false")
        .arg(QString("binScale=\"%1\" smooth=\"%2\" gain=\"%3\" ")
             .arg(int(m_binScale))
             .arg(m_smooth ? "true" : "false")
             .arg(m_gain));
    
    // New-style normalization attributes, allowing for more types of
    // normalization in future: write out the column normalization
    // type separately, and then whether we are normalizing visible
    // area as well afterwards
    
    s += QString("columnNormalization=\"%1\" ")
        .arg(m_normalization == ColumnNormalization::Max1 ? "peak" :
             m_normalization == ColumnNormalization::Hybrid ? "hybrid" : "none");

    // Old-style normalization attribute, for backward compatibility
    
    s += QString("normalizeColumns=\"%1\" ")
	.arg(m_normalization == ColumnNormalization::Max1 ? "true" : "false");

    // And this applies to both old- and new-style attributes
    
    s += QString("normalizeVisibleArea=\"%1\" ")
        .arg(m_normalizeVisibleArea ? "true" : "false");
    
    Layer::toXml(stream, indent, extraAttributes + " " + s);
}

void
Colour3DPlotLayer::setProperties(const QXmlAttributes &attributes)
{
    bool ok = false, alsoOk = false;

    ColourScaleType colourScale = convertToColourScale
        (attributes.value("colourScale").toInt(&ok));
    if (ok) setColourScale(colourScale);

    int colourMap = attributes.value("colourScheme").toInt(&ok);
    if (ok) setColourMap(colourMap);

    BinScale binScale = (BinScale)
	attributes.value("binScale").toInt(&ok);
    if (ok) setBinScale(binScale);

    bool invertVertical =
        (attributes.value("invertVertical").trimmed() == "true");
    setInvertVertical(invertVertical);

    bool opaque =
        (attributes.value("opaque").trimmed() == "true");
    setOpaque(opaque);

    bool smooth =
        (attributes.value("smooth").trimmed() == "true");
    setSmooth(smooth);

    float gain = attributes.value("gain").toFloat(&ok);
    if (ok) setGain(gain);

    float min = attributes.value("minY").toFloat(&ok);
    float max = attributes.value("maxY").toFloat(&alsoOk);
    if (ok && alsoOk) setDisplayExtents(min, max);

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

        setNormalization(ColumnNormalization::None);

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

    //!!! todo: check save/reload scaling, compare with
    //!!! SpectrogramLayer, compare with prior SV versions, compare
    //!!! with Tony v1 and v2 and their save files
}

