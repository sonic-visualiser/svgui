
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

#include "SpectrumLayer.h"

#include "data/model/FFTModel.h"
#include "view/View.h"
#include "base/AudioLevel.h"
#include "base/Preferences.h"
#include "base/RangeMapper.h"

SpectrumLayer::SpectrumLayer() :
    m_originModel(0),
    m_channel(-1),
    m_channelSet(false),
    m_windowSize(1024),
    m_windowType(HanningWindow),
    m_windowHopLevel(2)
{
    Preferences *prefs = Preferences::getInstance();
    connect(prefs, SIGNAL(propertyChanged(PropertyContainer::PropertyName)),
            this, SLOT(preferenceChanged(PropertyContainer::PropertyName)));
    setWindowType(prefs->getWindowType());

    setBinScale(LogBins);
}

SpectrumLayer::~SpectrumLayer()
{
    //!!! delete parent's model
//    for (size_t i = 0; i < m_fft.size(); ++i) delete m_fft[i];
}

void
SpectrumLayer::setModel(DenseTimeValueModel *model)
{
    if (m_originModel == model) return;
    m_originModel = model;
    setupFFT();
}

void
SpectrumLayer::setupFFT()
{
    FFTModel *oldFFT = dynamic_cast<FFTModel *>
        (const_cast<DenseThreeDimensionalModel *>(m_sliceableModel));
    
    if (oldFFT) {
        setSliceableModel(0);
        delete oldFFT;
    }

    FFTModel *newFFT = new FFTModel(m_originModel,
                                    m_channel,
                                    m_windowType,
                                    m_windowSize,
                                    getWindowIncrement(),
                                    m_windowSize,
                                    true);

    setSliceableModel(newFFT);

    newFFT->resume();
}

void
SpectrumLayer::setChannel(int channel)
{
    m_channelSet = true;

    FFTModel *fft = dynamic_cast<FFTModel *>
        (const_cast<DenseThreeDimensionalModel *>(m_sliceableModel));

    if (m_channel == channel) {
        if (fft) fft->resume();
        return;
    }

    m_channel = channel;

    if (!fft) setupFFT();

    emit layerParametersChanged();
}

Layer::PropertyList
SpectrumLayer::getProperties() const
{
    PropertyList list = SliceLayer::getProperties();
    list.push_back("Window Size");
    list.push_back("Window Increment");
    return list;
}

QString
SpectrumLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Window Size") return tr("Window Size");
    if (name == "Window Increment") return tr("Window Overlap");
    return SliceLayer::getPropertyLabel(name);
}

Layer::PropertyType
SpectrumLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Window Size") return ValueProperty;
    if (name == "Window Increment") return ValueProperty;
    return SliceLayer::getPropertyType(name);
}

QString
SpectrumLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Window Size" ||
	name == "Window Increment") return tr("Window");
    return SliceLayer::getPropertyGroupName(name);
}

int
SpectrumLayer::getPropertyRangeAndValue(const PropertyName &name,
                                        int *min, int *max, int *deflt) const
{
    int val = 0;

    int garbage0, garbage1, garbage2;
    if (!min) min = &garbage0;
    if (!max) max = &garbage1;
    if (!deflt) deflt = &garbage2;

    if (name == "Window Size") {

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
    
    } else {

        val = SliceLayer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
SpectrumLayer::getPropertyValueLabel(const PropertyName &name,
				    int value) const
{
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
    return SliceLayer::getPropertyValueLabel(name, value);
}

RangeMapper *
SpectrumLayer::getNewPropertyRangeMapper(const PropertyName &name) const
{
    return SliceLayer::getNewPropertyRangeMapper(name);
}

void
SpectrumLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Window Size") {
	setWindowSize(32 << value);
    } else if (name == "Window Increment") {
        setWindowHopLevel(value);
    } else {
        SliceLayer::setProperty(name, value);
    }
}

void
SpectrumLayer::setWindowSize(size_t ws)
{
    if (m_windowSize == ws) return;
    m_windowSize = ws;
    setupFFT();
    emit layerParametersChanged();
}

void
SpectrumLayer::setWindowHopLevel(size_t v)
{
    if (m_windowHopLevel == v) return;
    m_windowHopLevel = v;
    setupFFT();
    emit layerParametersChanged();
}

void
SpectrumLayer::setWindowType(WindowType w)
{
    if (m_windowType == w) return;
    m_windowType = w;
    setupFFT();
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

bool
SpectrumLayer::getValueExtents(float &, float &, bool &, QString &) const
{
    return false;
}

QString
SpectrumLayer::getFeatureDescription(View *v, QPoint &p) const
{
    if (!m_sliceableModel) return "";

    int minbin = 0, maxbin = 0, range = 0;
    QString genericDesc = SliceLayer::getFeatureDescription
        (v, p, false, minbin, maxbin, range);

    if (genericDesc == "") return "";

    float minvalue = 0.f;
    if (minbin < int(m_values.size())) minvalue = m_values[minbin];

    float maxvalue = minvalue;
    if (maxbin < int(m_values.size())) maxvalue = m_values[maxbin];
        
    if (minvalue > maxvalue) std::swap(minvalue, maxvalue);
    
    QString binstr;
    QString hzstr;
    int minfreq = lrintf((minbin * m_sliceableModel->getSampleRate()) /
                         m_windowSize);
    int maxfreq = lrintf((std::max(maxbin, minbin+1)
                           * m_sliceableModel->getSampleRate()) /
                          m_windowSize);

    if (maxbin != minbin) {
        binstr = tr("%1 - %2").arg(minbin+1).arg(maxbin+1);
    } else {
        binstr = QString("%1").arg(minbin+1);
    }
    if (minfreq != maxfreq) {
        hzstr = tr("%1 - %2 Hz").arg(minfreq).arg(maxfreq);
    } else {
        hzstr = tr("%1 Hz").arg(minfreq);
    }
    
    QString valuestr;
    if (maxvalue != minvalue) {
        valuestr = tr("%1 - %2").arg(minvalue).arg(maxvalue);
    } else {
        valuestr = QString("%1").arg(minvalue);
    }
    
    QString dbstr;
    float mindb = AudioLevel::multiplier_to_dB(minvalue);
    float maxdb = AudioLevel::multiplier_to_dB(maxvalue);
    QString mindbstr;
    QString maxdbstr;
    if (mindb == AudioLevel::DB_FLOOR) {
        mindbstr = tr("-Inf");
    } else {
        mindbstr = QString("%1").arg(lrintf(mindb));
    }
    if (maxdb == AudioLevel::DB_FLOOR) {
        maxdbstr = tr("-Inf");
    } else {
        maxdbstr = QString("%1").arg(lrintf(maxdb));
    }
    if (lrintf(mindb) != lrintf(maxdb)) {
        dbstr = tr("%1 - %2").arg(mindbstr).arg(maxdbstr);
    } else {
        dbstr = tr("%1").arg(mindbstr);
    }

    QString description;

    if (range > int(m_sliceableModel->getResolution())) {
        description = tr("%1\nBin:\t%2 (%3)\n%4 value:\t%5\ndB:\t%6")
            .arg(genericDesc)
            .arg(binstr)
            .arg(hzstr)
            .arg(m_samplingMode == NearestSample ? tr("First") :
                 m_samplingMode == SampleMean ? tr("Mean") : tr("Peak"))
            .arg(valuestr)
            .arg(dbstr);
    } else {
        description = tr("%1\nBin:\t%2 (%3)\nValue:\t%4\ndB:\t%5")
            .arg(genericDesc)
            .arg(binstr)
            .arg(hzstr)
            .arg(valuestr)
            .arg(dbstr);
    }
    
    return description;
}


QString
SpectrumLayer::toXmlString(QString indent, QString extraAttributes) const
{
    QString s;
    
    s += QString("windowSize=\"%1\" "
		 "windowHopLevel=\"%2\"")
        .arg(m_windowSize)
        .arg(m_windowHopLevel);

    return SliceLayer::toXmlString(indent, extraAttributes + " " + s);
}

void
SpectrumLayer::setProperties(const QXmlAttributes &attributes)
{
    SliceLayer::setProperties(attributes);

    bool ok = false;

    size_t windowSize = attributes.value("windowSize").toUInt(&ok);
    if (ok) setWindowSize(windowSize);

    size_t windowHopLevel = attributes.value("windowHopLevel").toUInt(&ok);
    if (ok) setWindowHopLevel(windowHopLevel);
}

    
