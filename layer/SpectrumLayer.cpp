
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

#include "SpectrumLayer.h"

#include "data/model/FFTModel.h"
#include "view/View.h"
#include "base/AudioLevel.h"
#include "base/Preferences.h"
#include "base/RangeMapper.h"

#include <QPainter>
#include <QPainterPath>

SpectrumLayer::SpectrumLayer() :
    m_model(0),
    m_channelMode(MixChannels),
    m_channel(-1),
    m_channelSet(false),
    m_colour(Qt::darkBlue),
    m_energyScale(dBScale),
    m_normalize(false),
    m_gain(1.0),
    m_windowSize(1024),
    m_windowType(HanningWindow),
    m_windowHopLevel(2)
{
    Preferences *prefs = Preferences::getInstance();
    connect(prefs, SIGNAL(propertyChanged(PropertyContainer::PropertyName)),
            this, SLOT(preferenceChanged(PropertyContainer::PropertyName)));
    setWindowType(prefs->getWindowType());
}

SpectrumLayer::~SpectrumLayer()
{
    for (size_t i = 0; i < m_fft.size(); ++i) delete m_fft[i];
}

void
SpectrumLayer::setModel(DenseTimeValueModel *model)
{
    m_model = model;
    setupFFTs();
}

void
SpectrumLayer::setupFFTs()
{
    for (size_t i = 0; i < m_fft.size(); ++i) delete m_fft[i];
    m_fft.clear();

    int minChannel = m_channel, maxChannel = m_channel;
    
    if (m_channel == -1 &&
        m_channelMode != MixChannels) {
        minChannel = 0;
        maxChannel = 0;
        if (m_model->getChannelCount() > 1) {
            maxChannel = m_model->getChannelCount() - 1;
        }
    }

    for (int c = minChannel; c <= maxChannel; ++c) {
        
        m_fft.push_back(new FFTModel(m_model,
                                     c,
                                     HanningWindow,
                                     m_windowSize,
                                     getWindowIncrement(),
                                     m_windowSize,
                                     true));

        if (m_channelSet) m_fft[m_fft.size()-1]->resume();
    }
}

void
SpectrumLayer::setChannel(int channel)
{
    m_channelSet = true;

    if (m_channel == channel) {
        for (size_t i = 0; i < m_fft.size(); ++i) {
            m_fft[i]->resume();
        }
        return;
    }

    m_channel = channel;

    if (!m_fft.empty()) setupFFTs();

    emit layerParametersChanged();
}

void
SpectrumLayer::paint(View *v, QPainter &paint, QRect rect) const
{
    if (m_fft.empty()) return;
    if (!m_channelSet) {
        for (size_t i = 0; i < m_fft.size(); ++i) {
            m_fft[i]->resume();
        }
    }

    FFTModel *fft = m_fft[0]; //!!! for now

    int windowIncrement = getWindowIncrement();

    size_t f = v->getCentreFrame();

    int w = (v->width() * 2) / 3;
    int xorigin = (v->width() / 2) - (w / 2);
    
    int h = (v->height() * 2) / 3;
    int yorigin = (v->height() / 2) + (h / 2);

    size_t column = f / windowIncrement;

    paint.save();
    paint.setPen(m_colour);
    paint.setRenderHint(QPainter::Antialiasing, false);
    
    QPainterPath path;
    float thresh = -80.f;

    for (size_t bin = 0; bin < fft->getHeight(); ++bin) {

        float x = xorigin + (float(w) * bin) / fft->getHeight();
        float mag;
        if (m_normalize) {
            mag = fft->getNormalizedMagnitudeAt(column, bin);
        } else {
            mag = fft->getMagnitudeAt(column, bin);
        }
        mag *= m_gain;
        float y = 0.f;
 
        switch (m_energyScale) {

        case dBScale:
        {
            float db = thresh;
            if (mag > 0.f) db = 10.f * log10f(mag);
            if (db < thresh) db = thresh;
            float val = (db - thresh) / -thresh;
            y = yorigin - (float(h) * val);
            break;
        }

        case MeterScale:
            y = yorigin - AudioLevel::multiplier_to_preview(mag, h);
            break;

        default:
            y = yorigin - (float(h) * mag);
            break;
        }

        if (bin == 0) {
            path.moveTo(x, y);
        } else {
            path.lineTo(x, y);
        }
    }

    paint.drawPath(path);
    paint.restore();

}

Layer::PropertyList
SpectrumLayer::getProperties() const
{
    PropertyList list;
    list.push_back("Colour");
    list.push_back("Scale");
    list.push_back("Normalize");
    list.push_back("Gain");
    list.push_back("Window Size");
    list.push_back("Window Increment");

    if (m_model && m_model->getChannelCount() > 1 && m_channel == -1) {
        list.push_back("Channels");
    }

    return list;
}

QString
SpectrumLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Colour") return tr("Colour");
    if (name == "Energy Scale") return tr("Scale");
    if (name == "Channels") return tr("Channels");
    if (name == "Window Size") return tr("Window Size");
    if (name == "Window Increment") return tr("Window Overlap");
    if (name == "Normalize") return tr("Normalize");
    if (name == "Gain") return tr("Gain");
    return "";
}

Layer::PropertyType
SpectrumLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Gain") return RangeProperty;
    if (name == "Normalize") return ToggleProperty;
    return ValueProperty;
}

QString
SpectrumLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Window Size" ||
	name == "Window Increment") return tr("Window");
    if (name == "Scale" ||
        name == "Normalize" ||
        name == "Gain") return tr("Energy Scale");
    return QString();
}

int
SpectrumLayer::getPropertyRangeAndValue(const PropertyName &name,
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

    } else if (name == "Normalize") {
	
	deft = (m_normalize ? 1 : 0);

    } else if (name == "Colour") {

	*min = 0;
	*max = 5;

	if (m_colour == Qt::black) deft = 0;
	else if (m_colour == Qt::darkRed) deft = 1;
	else if (m_colour == Qt::darkBlue) deft = 2;
	else if (m_colour == Qt::darkGreen) deft = 3;
	else if (m_colour == QColor(200, 50, 255)) deft = 4;
	else if (m_colour == QColor(255, 150, 50)) deft = 5;

    } else if (name == "Channels") {

        *min = 0;
        *max = 2;
        if (m_channelMode == MixChannels) deft = 1;
        else if (m_channelMode == OverlayChannels) deft = 2;
        else deft = 0;

    } else if (name == "Scale") {

	*min = 0;
	*max = 2;

	deft = (int)m_energyScale;

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
    
    } else {
	deft = Layer::getPropertyRangeAndValue(name, min, max);
    }

    return deft;
}

QString
SpectrumLayer::getPropertyValueLabel(const PropertyName &name,
				    int value) const
{
    if (name == "Colour") {
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
    if (name == "Scale") {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Meter");
	case 2: return tr("dB");
	}
    }
    if (name == "Channels") {
        switch (value) {
        default:
        case 0: return tr("Separate");
        case 1: return tr("Mean");
        case 2: return tr("Overlay");
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
    return tr("<unknown>");
}

RangeMapper *
SpectrumLayer::getNewPropertyRangeMapper(const PropertyName &name) const
{
    if (name == "Gain") {
        return new LinearRangeMapper(-50, 50, -25, 25, tr("dB"));
    }
    return 0;
}

void
SpectrumLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Gain") {
	setGain(pow(10, float(value)/20.0));
    } else if (name == "Colour") {
	switch (value) {
	default:
	case 0:	setBaseColour(Qt::black); break;
	case 1: setBaseColour(Qt::darkRed); break;
	case 2: setBaseColour(Qt::darkBlue); break;
	case 3: setBaseColour(Qt::darkGreen); break;
	case 4: setBaseColour(QColor(200, 50, 255)); break;
	case 5: setBaseColour(QColor(255, 150, 50)); break;
	}
    } else if (name == "Channels") {
        if (value == 1) setChannelMode(MixChannels);
        else if (value == 2) setChannelMode(OverlayChannels);
        else setChannelMode(SeparateChannels);
    } else if (name == "Scale") {
	switch (value) {
	default:
	case 0: setEnergyScale(LinearScale); break;
	case 1: setEnergyScale(MeterScale); break;
	case 2: setEnergyScale(dBScale); break;
	}
    } else if (name == "Window Size") {
	setWindowSize(32 << value);
    } else if (name == "Window Increment") {
        setWindowHopLevel(value);
    } else if (name == "Normalize") {
	setNormalize(value ? true : false);
    }
}

void
SpectrumLayer::setBaseColour(QColor colour)
{
    if (m_colour == colour) return;
    m_colour = colour;
    emit layerParametersChanged();
}

void
SpectrumLayer::setChannelMode(ChannelMode channelMode)
{
    if (m_channelMode == channelMode) return;
    m_channelMode = channelMode;
    emit layerParametersChanged();
}

void
SpectrumLayer::setEnergyScale(EnergyScale scale)
{
    if (m_energyScale == scale) return;
    m_energyScale = scale;
    emit layerParametersChanged();
}

void
SpectrumLayer::setWindowSize(size_t ws)
{
    if (m_windowSize == ws) return;
    m_windowSize = ws;
    setupFFTs();
    emit layerParametersChanged();
}

void
SpectrumLayer::setWindowHopLevel(size_t v)
{
    if (m_windowHopLevel == v) return;
    m_windowHopLevel = v;
    setupFFTs();
    emit layerParametersChanged();
}

void
SpectrumLayer::setWindowType(WindowType w)
{
    if (m_windowType == w) return;
    m_windowType = w;
    setupFFTs();
    emit layerParametersChanged();
}

void
SpectrumLayer::setNormalize(bool n)
{
    if (m_normalize == n) return;
    m_normalize = n;
    emit layerParametersChanged();
}

void
SpectrumLayer::setGain(float gain)
{
    if (m_gain == gain) return;
    m_gain = gain;
    emit layerParametersChanged();
}

void
SpectrumLayer::preferenceChanged(PropertyContainer::PropertyName name)
{
    if (name == "Window Type") {
        setWindowType(Preferences::getInstance()->getWindowType());
        return;
    }
}

QString
SpectrumLayer::toXmlString(QString indent, QString extraAttributes) const
{
    QString s;
    
    s += QString("colour=\"%1\" "
		 "channelMode=\"%2\" "
		 "channel=\"%3\" "
		 "energyScale=\"%4\" "
		 "windowSize=\"%5\" "
		 "windowHopLevel=\"%6\" "
                 "gain=\"%7\" "
                 "normalize=\"%8\"")
	.arg(encodeColour(m_colour))
	.arg(m_channelMode)
	.arg(m_channel)
	.arg(m_energyScale)
        .arg(m_windowSize)
        .arg(m_windowHopLevel)
        .arg(m_gain)
        .arg(m_normalize ? "true" : "false");

    return Layer::toXmlString(indent, extraAttributes + " " + s);
}

void
SpectrumLayer::setProperties(const QXmlAttributes &attributes)
{
    bool ok = false;

    QString colourSpec = attributes.value("colour");
    if (colourSpec != "") {
	QColor colour(colourSpec);
	if (colour.isValid()) {
	    setBaseColour(QColor(colourSpec));
	}
    }

    ChannelMode channelMode = (ChannelMode)
	attributes.value("channelMode").toInt(&ok);
    if (ok) setChannelMode(channelMode);

    int channel = attributes.value("channel").toInt(&ok);
    if (ok) setChannel(channel);

    EnergyScale scale = (EnergyScale)
	attributes.value("energyScale").toInt(&ok);
    if (ok) setEnergyScale(scale);

    size_t windowSize = attributes.value("windowSize").toUInt(&ok);
    if (ok) setWindowSize(windowSize);

    size_t windowHopLevel = attributes.value("windowHopLevel").toUInt(&ok);
    if (ok) setWindowHopLevel(windowHopLevel);

    float gain = attributes.value("gain").toFloat(&ok);
    if (ok) setGain(gain);

    bool normalize = (attributes.value("normalize").trimmed() == "true");
    setNormalize(normalize);
}

bool
SpectrumLayer::getValueExtents(float &min, float &max, bool &logarithmic,
                               QString &units) const
{
    return false;
}

