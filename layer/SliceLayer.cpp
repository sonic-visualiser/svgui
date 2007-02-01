
/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2007 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "SliceLayer.h"

#include "view/View.h"
#include "base/AudioLevel.h"
#include "base/RangeMapper.h"
#include "base/RealTime.h"

#include "ColourMapper.h"
#include "PaintAssistant.h"

#include <QPainter>
#include <QPainterPath>

SliceLayer::SliceLayer() :
    m_sliceableModel(0),
    m_colour(Qt::darkBlue),
    m_colourMap(0),
    m_energyScale(dBScale),
    m_samplingMode(SamplePeak),
    m_plotStyle(PlotSteps),
    m_binScale(LinearBins),
    m_normalize(false),
    m_bias(false),
    m_gain(1.0)
{
}

SliceLayer::~SliceLayer()
{

}

void
SliceLayer::setSliceableModel(const Model *model)
{
    const DenseThreeDimensionalModel *sliceable =
        dynamic_cast<const DenseThreeDimensionalModel *>(model);

    if (model && !sliceable) {
        std::cerr << "WARNING: SliceLayer::setSliceableModel(" << model
                  << "): model is not a DenseThreeDimensionalModel" << std::endl;
    }

    if (m_sliceableModel == sliceable) return;

    m_sliceableModel = sliceable;

    connect(m_sliceableModel, SIGNAL(modelChanged()),
            this, SIGNAL(modelChanged()));

    connect(m_sliceableModel, SIGNAL(modelChanged(size_t, size_t)),
	    this, SIGNAL(modelChanged(size_t, size_t)));

    connect(m_sliceableModel, SIGNAL(completionChanged()),
	    this, SIGNAL(modelCompletionChanged()));

    emit modelReplaced();
}

void
SliceLayer::sliceableModelReplaced(const Model *orig, const Model *replacement)
{
    std::cerr << "SliceLayer::sliceableModelReplaced(" << orig << ", " << replacement << ")" << std::endl;

    if (orig == m_sliceableModel) {
        setSliceableModel
            (dynamic_cast<const DenseThreeDimensionalModel *>(replacement));
    }
}

void
SliceLayer::modelAboutToBeDeleted(Model *m)
{
    std::cerr << "SliceLayer::modelAboutToBeDeleted(" << m << ")" << std::endl;

    if (m == m_sliceableModel) {
        setSliceableModel(0);
    }
}

void
SliceLayer::paint(View *v, QPainter &paint, QRect rect) const
{
    if (!m_sliceableModel) return;

    paint.save();
    paint.setRenderHint(QPainter::Antialiasing, false);

    if (v->getViewManager() && v->getViewManager()->shouldShowScaleGuides()) {
        if (!m_scalePoints.empty()) {
            paint.setPen(QColor(240, 240, 240)); //!!! and dark background?
            for (size_t i = 0; i < m_scalePoints.size(); ++i) {
                paint.drawLine(0, m_scalePoints[i], rect.width(), m_scalePoints[i]);
            }
        }
    }

    paint.setPen(m_colour);

//    int w = (v->width() * 2) / 3;
    int xorigin = getVerticalScaleWidth(v, paint) + 1; //!!! (v->width() / 2) - (w / 2);
    int w = v->width() - xorigin - 1;
    
    int yorigin = v->height() - 20 - paint.fontMetrics().height() - 7;
    int h = yorigin - paint.fontMetrics().height() - 8;
    if (h < 0) return;

//    int h = (v->height() * 3) / 4;
//    int yorigin = (v->height() / 2) + (h / 2);
    
    QPainterPath path;
    float thresh = -80.f;

    int mh = m_sliceableModel->getHeight();

    float *values = new float[mh];
    int divisor = 0;

    for (size_t bin = 0; bin < mh; ++bin) {
        values[bin] = 0.f;
    }

    size_t f0 = v->getCentreFrame();
    int f0x = v->getXForFrame(f0);
    f0 = v->getFrameForX(f0x);
    size_t f1 = v->getFrameForX(f0x + 1);
    if (f1 > f0) --f1;

    size_t col0 = f0 / m_sliceableModel->getResolution();
    size_t col1 = col0;
    if (m_samplingMode != NearestSample) {
        col1 = f1 / m_sliceableModel->getResolution();
    }
    f0 = col0 * m_sliceableModel->getResolution();
    f1 = (col1 + 1) * m_sliceableModel->getResolution() - 1;

    for (size_t col = col0; col <= col1; ++col) {
        for (size_t bin = 0; bin < mh; ++bin) {
            float value = m_sliceableModel->getValueAt(col, bin);
            if (m_bias) value *= bin + 1;
            if (m_samplingMode == SamplePeak) {
                if (value > values[bin]) values[bin] = value;
            } else {
                values[bin] += value;
            }
        }
        ++divisor;
    }

    float max = 0.f;
    for (size_t bin = 0; bin < mh; ++bin) {
        if (m_samplingMode == SampleMean) values[bin] /= divisor;
        if (values[bin] > max) max = values[bin];
    }
    if (max != 0.f && m_normalize) {
        for (size_t bin = 0; bin < mh; ++bin) {
            values[bin] /= max;
        }
    }

    float py = 0;
    float nx = xorigin;

    ColourMapper mapper(m_colourMap, 0, 1);

    for (size_t bin = 0; bin < mh; ++bin) {

        float x;

        switch (m_binScale) {

        case LinearBins:
            x = nx;
            nx = xorigin + (float(w) * (bin + 1)) / mh;
            break;

        case LogBins:
            x = nx;
            nx = xorigin + (float(w) * (log10f(bin + 2) - log10f(1))) /
                (log10f(mh + 1) - log10f(1));
            break;

        case InvertedLogBins:
            x = nx;
            nx = xorigin + w - (float(w) * (log10f(mh - bin) - log10f(1))) /
                (log10f(mh) - log10f(1));
            break;
        }

        float value = values[bin];

        value *= m_gain;
        float norm = 0.f;
        float y = 0.f;
 
        switch (m_energyScale) {

        case dBScale:
        {
            float db = thresh;
            if (value > 0.f) db = 10.f * log10f(value);
            if (db < thresh) db = thresh;
            norm = (db - thresh) / -thresh;
            y = yorigin - (float(h) * norm);
            break;
        }

        case MeterScale:
            y = AudioLevel::multiplier_to_preview(value, h);
            norm = float(y) / float(h);
            y = yorigin - y;
            break;

        default:
            norm = value;
            y = yorigin - (float(h) * value);
            break;
        }

        if (m_plotStyle == PlotLines) {

            if (bin == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }

        } else if (m_plotStyle == PlotSteps) {

            if (bin == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }
            path.lineTo(nx, y);

        } else if (m_plotStyle == PlotBlocks) {

            path.moveTo(x, yorigin);
            path.lineTo(x, y);
            path.lineTo(nx, y);
            path.lineTo(nx, yorigin);
            path.lineTo(x, yorigin);

        } else if (m_plotStyle == PlotFilledBlocks) {

            paint.fillRect(QRectF(x, y, nx - x, yorigin - y), mapper.map(norm));
        }

        py = y;
    }

    if (m_plotStyle != PlotFilledBlocks) {
        paint.drawPath(path);
    }
    paint.restore();

    QPoint discard;

    if (v->getViewManager() && v->getViewManager()->shouldShowFrameCount() &&
        v->shouldIlluminateLocalFeatures(this, discard)) {

        int sampleRate = m_sliceableModel->getSampleRate();

        QString startText = QString("%1 / %2")
            .arg(QString::fromStdString
                 (RealTime::frame2RealTime
                  (f0, sampleRate).toText(true)))
            .arg(f0);

        QString endText = QString(" %1 / %2")
            .arg(QString::fromStdString
                 (RealTime::frame2RealTime
                  (f1, sampleRate).toText(true)))
            .arg(f1);

        QString durationText = QString("(%1 / %2) ")
            .arg(QString::fromStdString
                 (RealTime::frame2RealTime
                  (f1 - f0 + 1, sampleRate).toText(true)))
            .arg(f1 - f0 + 1);

        v->drawVisibleText
            (paint, xorigin + 5,
             paint.fontMetrics().ascent() + 5,
             startText, View::OutlinedText);
        
        v->drawVisibleText
            (paint, xorigin + 5,
             paint.fontMetrics().ascent() + paint.fontMetrics().height() + 10,
             endText, View::OutlinedText);
        
        v->drawVisibleText
            (paint, xorigin + 5,
             paint.fontMetrics().ascent() + 2*paint.fontMetrics().height() + 15,
             durationText, View::OutlinedText);
    }
    
/*

    QString frameRange;
    if (f1 != f0) {
        frameRange = QString("%1 - %2").arg(f0).arg(f1);
    } else {
        frameRange = QString("%1").arg(f0);
    }

    QString colRange;
    if (col1 != col0) {
        colRange = tr("%1 hops").arg(col1 - col0 + 1);
    } else {
        colRange = tr("1 hop");
    }

    if (v->getViewManager() && v->getViewManager()->shouldShowFrameCount()) {

        v->drawVisibleText
            (paint, xorigin + 5,
             paint.fontMetrics().ascent() + 5,
             frameRange, View::OutlinedText);
        
        v->drawVisibleText
            (paint, xorigin + 5,
             paint.fontMetrics().ascent() + paint.fontMetrics().height() + 10,
             colRange, View::OutlinedText);
    }
*/
}

int
SliceLayer::getVerticalScaleWidth(View *v, QPainter &paint) const
{
    if (m_energyScale == LinearScale) {
	return paint.fontMetrics().width("0.0") + 13;
    } else {
	return std::max(paint.fontMetrics().width(tr("0dB")),
			paint.fontMetrics().width(tr("-Inf"))) + 13;
    }
}

void
SliceLayer::paintVerticalScale(View *v, QPainter &paint, QRect rect) const
{
    float thresh = 0;
    if (m_energyScale != LinearScale) {
        thresh = AudioLevel::dB_to_multiplier(-80); //!!! thresh
    }
    
//    int h = (rect.height() * 3) / 4;
//    int y = (rect.height() / 2) - (h / 2);
    
    int yorigin = v->height() - 20 - paint.fontMetrics().height() - 6;
    int h = yorigin - paint.fontMetrics().height() - 8;
    if (h < 0) return;

    QRect actual(rect.x(), rect.y() + yorigin - h, rect.width(), h);

    PaintAssistant::paintVerticalLevelScale
        (paint, actual, thresh, 1.0 / m_gain,
         PaintAssistant::Scale(m_energyScale),
         const_cast<std::vector<int> *>(&m_scalePoints));
}

Layer::PropertyList
SliceLayer::getProperties() const
{
    PropertyList list;
    list.push_back("Colour");
    list.push_back("Plot Type");
    list.push_back("Sampling Mode");
    list.push_back("Scale");
    list.push_back("Normalize");
    list.push_back("Gain");
    list.push_back("Bin Scale");

    return list;
}

QString
SliceLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Colour") return tr("Colour");
    if (name == "Plot Type") return tr("Plot Type");
    if (name == "Energy Scale") return tr("Scale");
    if (name == "Normalize") return tr("Normalize");
    if (name == "Gain") return tr("Gain");
    if (name == "Sampling Mode") return tr("Sampling Mode");
    if (name == "Bin Scale") return tr("Plot X Scale");
    return "";
}

Layer::PropertyType
SliceLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Gain") return RangeProperty;
    if (name == "Normalize") return ToggleProperty;
    return ValueProperty;
}

QString
SliceLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Scale" ||
        name == "Normalize" ||
        name == "Sampling Mode" ||
        name == "Gain") return tr("Scale");
    if (name == "Plot Type" ||
        name == "Bin Scale") return tr("Plot Type");
    return QString();
}

int
SliceLayer::getPropertyRangeAndValue(const PropertyName &name,
                                        int *min, int *max) const
{
    int deft = 0;

    int garbage0, garbage1;
    if (!min) min = &garbage0;
    if (!max) max = &garbage1;

    if (name == "Gain") {

	*min = -50;
	*max = 50;

        std::cerr << "gain is " << m_gain << ", mode is " << m_samplingMode << std::endl;

	deft = lrint(log10(m_gain) * 20.0);
	if (deft < *min) deft = *min;
	if (deft > *max) deft = *max;

    } else if (name == "Normalize") {
	
	deft = (m_normalize ? 1 : 0);

    } else if (name == "Colour") {

        if (m_plotStyle == PlotFilledBlocks) {
            
            *min = 0;
            *max = ColourMapper::getColourMapCount() - 1;

            deft = m_colourMap;

        } else {

            *min = 0;
            *max = 5;

            if (m_colour == Qt::black) deft = 0;
            else if (m_colour == Qt::darkRed) deft = 1;
            else if (m_colour == Qt::darkBlue) deft = 2;
            else if (m_colour == Qt::darkGreen) deft = 3;
            else if (m_colour == QColor(200, 50, 255)) deft = 4;
            else if (m_colour == QColor(255, 150, 50)) deft = 5;
        }

    } else if (name == "Scale") {

	*min = 0;
	*max = 2;

	deft = (int)m_energyScale;

    } else if (name == "Sampling Mode") {

	*min = 0;
	*max = 2;
        
	deft = (int)m_samplingMode;

    } else if (name == "Plot Type") {
        
        *min = 0;
        *max = 3;

        deft = (int)m_plotStyle;

    } else if (name == "Bin Scale") {
        
        *min = 0;
//        *max = 2;
        *max = 1; // I don't think we really do want to offer inverted log

        deft = (int)m_binScale;

    } else {
	deft = Layer::getPropertyRangeAndValue(name, min, max);
    }

    return deft;
}

QString
SliceLayer::getPropertyValueLabel(const PropertyName &name,
				    int value) const
{
    if (name == "Colour") {
        if (m_plotStyle == PlotFilledBlocks) {
            return ColourMapper::getColourMapName(value);
        } else {
            switch (value) {
            default:
            case 0: return tr("Black");
            case 1: return tr("Red");
            case 2: return tr("Blue");
            case 3: return tr("Green");
            case 4: return tr("Purple");
            case 5: return tr("Orange");
            }
	}
    }
    if (name == "Scale") {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Meter");
	case 2: return tr("dB");
	}
    }
    if (name == "Sampling Mode") {
	switch (value) {
	default:
	case 0: return tr("Any");
	case 1: return tr("Mean");
	case 2: return tr("Peak");
	}
    }
    if (name == "Plot Type") {
	switch (value) {
	default:
	case 0: return tr("Lines");
	case 1: return tr("Steps");
	case 2: return tr("Blocks");
	case 3: return tr("Colours");
	}
    }
    if (name == "Bin Scale") {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Log");
	case 2: return tr("Rev Log");
	}
    }
    return tr("<unknown>");
}

RangeMapper *
SliceLayer::getNewPropertyRangeMapper(const PropertyName &name) const
{
    if (name == "Gain") {
        return new LinearRangeMapper(-50, 50, -25, 25, tr("dB"));
    }
    return 0;
}

void
SliceLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Gain") {
	setGain(pow(10, float(value)/20.0));
    } else if (name == "Colour") {
        if (m_plotStyle == PlotFilledBlocks) {
            setFillColourMap(value);
        } else {
            switch (value) {
            default:
            case 0: setBaseColour(Qt::black); break;
            case 1: setBaseColour(Qt::darkRed); break;
            case 2: setBaseColour(Qt::darkBlue); break;
            case 3: setBaseColour(Qt::darkGreen); break;
            case 4: setBaseColour(QColor(200, 50, 255)); break;
            case 5: setBaseColour(QColor(255, 150, 50)); break;
            }
	}
    } else if (name == "Scale") {
	switch (value) {
	default:
	case 0: setEnergyScale(LinearScale); break;
	case 1: setEnergyScale(MeterScale); break;
	case 2: setEnergyScale(dBScale); break;
	}
    } else if (name == "Plot Type") {
	setPlotStyle(PlotStyle(value));
    } else if (name == "Sampling Mode") {
	switch (value) {
	default:
	case 0: setSamplingMode(NearestSample); break;
	case 1: setSamplingMode(SampleMean); break;
	case 2: setSamplingMode(SamplePeak); break;
	}
    } else if (name == "Bin Scale") {
	switch (value) {
	default:
	case 0: setBinScale(LinearBins); break;
	case 1: setBinScale(LogBins); break;
	case 2: setBinScale(InvertedLogBins); break;
	}
    } else if (name == "Normalize") {
	setNormalize(value ? true : false);
    }
}

void
SliceLayer::setBaseColour(QColor colour)
{
    if (m_colour == colour) return;
    m_colour = colour;
    emit layerParametersChanged();
}

void
SliceLayer::setFillColourMap(int map)
{
    if (m_colourMap == map) return;
    m_colourMap = map;
    emit layerParametersChanged();
}

void
SliceLayer::setEnergyScale(EnergyScale scale)
{
    if (m_energyScale == scale) return;
    m_energyScale = scale;
    emit layerParametersChanged();
}

void
SliceLayer::setSamplingMode(SamplingMode mode)
{
    if (m_samplingMode == mode) return;
    m_samplingMode = mode;
    emit layerParametersChanged();
}

void
SliceLayer::setPlotStyle(PlotStyle style)
{
    if (m_plotStyle == style) return;
    bool colourTypeChanged = (style == PlotFilledBlocks ||
                              m_plotStyle == PlotFilledBlocks);
    m_plotStyle = style;
    if (colourTypeChanged) {
        emit layerParameterRangesChanged();
    }
    emit layerParametersChanged();
}

void
SliceLayer::setBinScale(BinScale scale)
{
    if (m_binScale == scale) return;
    m_binScale = scale;
    emit layerParametersChanged();
}

void
SliceLayer::setNormalize(bool n)
{
    if (m_normalize == n) return;
    m_normalize = n;
    emit layerParametersChanged();
}

void
SliceLayer::setGain(float gain)
{
    if (m_gain == gain) return;
    m_gain = gain;
    emit layerParametersChanged();
}

QString
SliceLayer::toXmlString(QString indent, QString extraAttributes) const
{
    QString s;
    
    s += QString("colour=\"%1\" "
                 "colourScheme=\"%2\" "
		 "energyScale=\"%3\" "
                 "samplingMode=\"%4\" "
                 "gain=\"%5\" "
                 "normalize=\"%6\"")
	.arg(encodeColour(m_colour))
        .arg(m_colourMap)
	.arg(m_energyScale)
        .arg(m_samplingMode)
        .arg(m_gain)
        .arg(m_normalize ? "true" : "false");

    return Layer::toXmlString(indent, extraAttributes + " " + s);
}

void
SliceLayer::setProperties(const QXmlAttributes &attributes)
{
    bool ok = false;

    QString colourSpec = attributes.value("colour");
    if (colourSpec != "") {
	QColor colour(colourSpec);
	if (colour.isValid()) {
	    setBaseColour(QColor(colourSpec));
	}
    }

    EnergyScale scale = (EnergyScale)
	attributes.value("energyScale").toInt(&ok);
    if (ok) setEnergyScale(scale);

    SamplingMode mode = (SamplingMode)
	attributes.value("samplingMode").toInt(&ok);
    if (ok) setSamplingMode(mode);

    int colourMap = attributes.value("colourScheme").toInt(&ok);
    if (ok) setFillColourMap(colourMap);

    float gain = attributes.value("gain").toFloat(&ok);
    if (ok) setGain(gain);

    bool normalize = (attributes.value("normalize").trimmed() == "true");
    setNormalize(normalize);
}

bool
SliceLayer::getValueExtents(float &min, float &max, bool &logarithmic,
                               QString &units) const
{
    return false;
}

