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
#include "base/Pitch.h"
#include "base/Strings.h"

#include "ColourMapper.h"
#include "PaintAssistant.h"
#include "PianoScale.h"
#include "HorizontalFrequencyScale.h"

#include <QPainter>
#include <QTextStream>


SpectrumLayer::SpectrumLayer() :
    m_originModel(0),
    m_channel(-1),
    m_channelSet(false),
    m_windowSize(4096),
    m_windowType(HanningWindow),
    m_windowHopLevel(3),
    m_oversampling(1),
    m_showPeaks(false),
    m_newFFTNeeded(true)
{
    Preferences *prefs = Preferences::getInstance();
    connect(prefs, SIGNAL(propertyChanged(PropertyContainer::PropertyName)),
            this, SLOT(preferenceChanged(PropertyContainer::PropertyName)));
    setWindowType(prefs->getWindowType());

    setBinScale(LogBins);
}

SpectrumLayer::~SpectrumLayer()
{
    Model *m = const_cast<Model *>
        (static_cast<const Model *>(m_sliceableModel));
    if (m) m->aboutToDelete();
    m_sliceableModel = 0;
    delete m;
}

void
SpectrumLayer::setModel(DenseTimeValueModel *model)
{
    SVDEBUG << "SpectrumLayer::setModel(" << model << ") from " << m_originModel << endl;
    
    if (m_originModel == model) return;

    m_originModel = model;

    if (m_sliceableModel) {
        Model *m = const_cast<Model *>
            (static_cast<const Model *>(m_sliceableModel));
        m->aboutToDelete();
        setSliceableModel(0);
        delete m;
    }

    m_newFFTNeeded = true;

    emit layerParametersChanged();
}

void
SpectrumLayer::setChannel(int channel)
{
    SVDEBUG << "SpectrumLayer::setChannel(" << channel << ") from " << m_channel << endl;
    
    m_channelSet = true;
    
    if (m_channel == channel) return;

    m_channel = channel;

    m_newFFTNeeded = true;

    emit layerParametersChanged();
}

void
SpectrumLayer::setupFFT()
{
    if (m_sliceableModel) {
        Model *m = const_cast<Model *>
            (static_cast<const Model *>(m_sliceableModel));
        m->aboutToDelete();
        setSliceableModel(0);
        delete m;
    }

    if (!m_originModel) {
        return;
    }

    int fftSize = getFFTSize();

    FFTModel *newFFT = new FFTModel(m_originModel,
                                    m_channel,
                                    m_windowType,
                                    m_windowSize,
                                    getWindowIncrement(),
                                    fftSize);

    if (m_minbin == 0 && m_maxbin == 0) {
        m_minbin = 1;
        m_maxbin = newFFT->getHeight();
    }
    
    setSliceableModel(newFFT);

    m_biasCurve.clear();
    for (int i = 0; i < fftSize; ++i) {
        m_biasCurve.push_back(1.f / (float(fftSize)/2.f));
    }

    m_newFFTNeeded = false;
}

Layer::PropertyList
SpectrumLayer::getProperties() const
{
    PropertyList list = SliceLayer::getProperties();
    list.push_back("Window Size");
    list.push_back("Window Increment");
    list.push_back("Oversampling");
    list.push_back("Show Peak Frequencies");
    return list;
}

QString
SpectrumLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Window Size") return tr("Window Size");
    if (name == "Window Increment") return tr("Window Overlap");
    if (name == "Oversampling") return tr("Oversampling");
    if (name == "Show Peak Frequencies") return tr("Show Peak Frequencies");
    return SliceLayer::getPropertyLabel(name);
}

QString
SpectrumLayer::getPropertyIconName(const PropertyName &name) const
{
    if (name == "Show Peak Frequencies") return "show-peaks";
    return SliceLayer::getPropertyIconName(name);
}

Layer::PropertyType
SpectrumLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Window Size") return ValueProperty;
    if (name == "Window Increment") return ValueProperty;
    if (name == "Oversampling") return ValueProperty;
    if (name == "Show Peak Frequencies") return ToggleProperty;
    return SliceLayer::getPropertyType(name);
}

QString
SpectrumLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Window Size" ||
        name == "Window Increment" ||
        name == "Oversampling") return tr("Window");
    if (name == "Show Peak Frequencies") return tr("Bins");
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
        *max = 15;
        *deflt = 5;
        
        val = 0;
        int ws = m_windowSize;
        while (ws > 32) { ws >>= 1; val ++; }

    } else if (name == "Window Increment") {
        
        *min = 0;
        *max = 5;
        *deflt = 2;
        
        val = m_windowHopLevel;
    
    } else if (name == "Oversampling") {

        *min = 0;
        *max = 3;
        *deflt = 0;

        val = 0;
        int ov = m_oversampling;
        while (ov > 1) { ov >>= 1; val ++; }
        
    } else if (name == "Show Peak Frequencies") {

        return m_showPeaks ? 1 : 0;

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
    if (name == "Oversampling") {
        switch (value) {
        default:
        case 0: return tr("1x");
        case 1: return tr("2x");
        case 2: return tr("4x");
        case 3: return tr("8x");
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
    } else if (name == "Oversampling") {
        setOversampling(1 << value);
    } else if (name == "Show Peak Frequencies") {
        setShowPeaks(value ? true : false);
    } else {
        SliceLayer::setProperty(name, value);
    }
}

void
SpectrumLayer::setWindowSize(int ws)
{
    if (m_windowSize == ws) return;

    SVDEBUG << "setWindowSize: from " << m_windowSize
            << " to " << ws << ": updating min and max bins from "
            << m_minbin << " and " << m_maxbin << " to ";
    
    m_minbin = int(round((double(m_minbin) / m_windowSize) * ws));
    m_maxbin = int(round((double(m_maxbin) / m_windowSize) * ws));

    SVDEBUG << m_minbin << " and " << m_maxbin << endl;

    m_windowSize = ws;
    m_newFFTNeeded = true;
    emit layerParametersChanged();
}

void
SpectrumLayer::setWindowHopLevel(int v)
{
    if (m_windowHopLevel == v) return;
    m_windowHopLevel = v;
    m_newFFTNeeded = true;
    emit layerParametersChanged();
}

void
SpectrumLayer::setWindowType(WindowType w)
{
    if (m_windowType == w) return;
    m_windowType = w;
    m_newFFTNeeded = true;
    emit layerParametersChanged();
}

void
SpectrumLayer::setOversampling(int oversampling)
{
    if (m_oversampling == oversampling) return;

    SVDEBUG << "setOversampling: from " << m_oversampling
            << " to " << oversampling << ": updating min and max bins from "
            << m_minbin << " and " << m_maxbin << " to ";
    
    m_minbin = int(round((double(m_minbin) / m_oversampling) * oversampling));
    m_maxbin = int(round((double(m_maxbin) / m_oversampling) * oversampling));

    SVDEBUG << m_minbin << " and " << m_maxbin << endl;
    
    m_oversampling = oversampling;
    m_newFFTNeeded = true;
    
    emit layerParametersChanged();
}

int
SpectrumLayer::getOversampling() const
{
    return m_oversampling;
}

void
SpectrumLayer::setShowPeaks(bool show)
{
    if (m_showPeaks == show) return;
    m_showPeaks = show;
    emit layerParametersChanged();
}

void
SpectrumLayer::preferenceChanged(PropertyContainer::PropertyName name)
{
    if (name == "Window Type") {
        auto type = Preferences::getInstance()->getWindowType();
        SVDEBUG << "SpectrumLayer::preferenceChanged: Window type changed to "
                << type << endl;
        setWindowType(type);
        return;
    }
}

double
SpectrumLayer::getBinForFrequency(double freq) const
{
    if (!m_sliceableModel) return 0;
    double bin = (freq * getFFTSize()) / m_sliceableModel->getSampleRate();
    // we assume the frequency of a bin corresponds to the centre of
    // its visual range
    bin += 0.5;
    return bin;
}

double
SpectrumLayer::getBinForX(const LayerGeometryProvider *v, double x) const
{
    if (!m_sliceableModel) return 0;
    double bin = getBinForFrequency(getFrequencyForX(v, x));
    return bin;
}

double
SpectrumLayer::getFrequencyForX(const LayerGeometryProvider *v, double x) const
{
    if (!m_sliceableModel) return 0;

    double fmin = getFrequencyForBin(m_minbin);

    if (m_binScale == LogBins && m_minbin == 0) {
        // Avoid too much space going to the first bin, but do so in a
        // way that usually avoids us shifting left/right as the
        // window size or oversampling ratio change - i.e. base this
        // on frequency rather than bin number unless we have a lot of
        // very low-resolution content
        fmin = getFrequencyForBin(0.8);
        if (fmin > 6.0) fmin = 6.0;
    }
    
    double fmax = getFrequencyForBin(m_maxbin);

    double freq = getScalePointForX(v, x, fmin, fmax);
    return freq;
}

double
SpectrumLayer::getFrequencyForBin(double bin) const
{
    if (!m_sliceableModel) return 0;
    // we assume the frequency of a bin corresponds to the centre of
    // its visual range
    bin -= 0.5;
    double freq = (bin * m_sliceableModel->getSampleRate()) / getFFTSize();
    return freq;
}

double
SpectrumLayer::getXForBin(const LayerGeometryProvider *v, double bin) const
{
    if (!m_sliceableModel) return 0;
    double x = getXForFrequency(v, getFrequencyForBin(bin));
    return x;
}

double
SpectrumLayer::getXForFrequency(const LayerGeometryProvider *v, double freq) const
{
    if (!m_sliceableModel) return 0;

    double fmin = getFrequencyForBin(m_minbin);

    if (m_binScale == LogBins && m_minbin == 0) {
        // See comment in getFrequencyForX above
        fmin = getFrequencyForBin(0.8);
        if (fmin > 6.0) fmin = 6.0;
    }
    
    double fmax = getFrequencyForBin(m_maxbin);
    double x = getXForScalePoint(v, freq, fmin, fmax);
    
    return x;
}

bool
SpectrumLayer::getXScaleValue(const LayerGeometryProvider *v, int x, 
                              double &value, QString &unit) const
{
    value = getFrequencyForX(v, x);
    unit = "Hz";
    return true;
}

bool
SpectrumLayer::getYScaleValue(const LayerGeometryProvider *v, int y,
                              double &value, QString &unit) const
{
    value = getValueForY(v, y);

    if (m_energyScale == dBScale || m_energyScale == MeterScale) {

        if (value > 0.0) {
            value = 10.0 * log10(value);
            if (value < m_threshold) value = m_threshold;
        } else value = m_threshold;

        unit = "dBV";

    } else {
        unit = "V";
    }

    return true;
}

bool
SpectrumLayer::getYScaleDifference(const LayerGeometryProvider *v, int y0, int y1,
                                   double &diff, QString &unit) const
{
    bool rv = SliceLayer::getYScaleDifference(v, y0, y1, diff, unit);
    if (rv && (unit == "dBV")) unit = "dB";
    return rv;
}


bool
SpectrumLayer::getCrosshairExtents(LayerGeometryProvider *v, QPainter &paint,
                                   QPoint cursorPos,
                                   std::vector<QRect> &extents) const
{
    QRect vertical(cursorPos.x(), cursorPos.y(), 1, v->getPaintHeight() - cursorPos.y());
    extents.push_back(vertical);

    QRect horizontal(0, cursorPos.y(), v->getPaintWidth(), 12);
    extents.push_back(horizontal);

    int hoffset = 2;
    if (m_binScale == LogBins) hoffset = 13;

    int sw = getVerticalScaleWidth(v, false, paint);

    QRect value(sw, cursorPos.y() - paint.fontMetrics().ascent() - 2,
                paint.fontMetrics().width("0.0000001 V") + 2,
                paint.fontMetrics().height());
    extents.push_back(value);

    QRect log(sw, cursorPos.y() + 2,
              paint.fontMetrics().width("-80.000 dBV") + 2,
              paint.fontMetrics().height());
    extents.push_back(log);

    QRect freq(cursorPos.x(),
               v->getPaintHeight() - paint.fontMetrics().height() - hoffset,
               paint.fontMetrics().width("123456 Hz") + 2,
               paint.fontMetrics().height());
    extents.push_back(freq);

    int w(paint.fontMetrics().width("C#10+50c") + 2);
    QRect pitch(cursorPos.x() - w,
                v->getPaintHeight() - paint.fontMetrics().height() - hoffset,
                w,
                paint.fontMetrics().height());
    extents.push_back(pitch);

    return true;
}

void
SpectrumLayer::paintCrosshairs(LayerGeometryProvider *v, QPainter &paint,
                               QPoint cursorPos) const
{
    if (!m_sliceableModel) return;

    paint.save();
    QFont fn = paint.font();
    if (fn.pointSize() > 8) {
        fn.setPointSize(fn.pointSize() - 1);
        paint.setFont(fn);
    }

    ColourMapper mapper(m_colourMap, m_colourInverted, 0, 1);
    paint.setPen(mapper.getContrastingColour());

    int xorigin = m_xorigins[v->getId()];
    paint.drawLine(xorigin, cursorPos.y(), v->getPaintWidth(), cursorPos.y());
    paint.drawLine(cursorPos.x(), cursorPos.y(), cursorPos.x(), v->getPaintHeight());
    
    double fundamental = getFrequencyForX(v, cursorPos.x());

    int hoffset = getHorizontalScaleHeight(v, paint) +
        2 * paint.fontMetrics().height();

    PaintAssistant::drawVisibleText(v, paint,
                                    cursorPos.x() + 2,
                                    v->getPaintHeight() - 2 - hoffset,
                                    tr("%1 Hz").arg(fundamental),
                                    PaintAssistant::OutlinedText);

    if (Pitch::isFrequencyInMidiRange(fundamental)) {
        QString pitchLabel = Pitch::getPitchLabelForFrequency(fundamental);
        PaintAssistant::drawVisibleText(v, paint,
                                        cursorPos.x() -
                                        paint.fontMetrics().width(pitchLabel) - 2,
                                        v->getPaintHeight() - 2 - hoffset,
                                        pitchLabel,
                                        PaintAssistant::OutlinedText);
    }

    double value = getValueForY(v, cursorPos.y());

    PaintAssistant::drawVisibleText(v, paint,
                       xorigin + 2,
                       cursorPos.y() - 2,
                       QString("%1 V").arg(value),
                       PaintAssistant::OutlinedText);

    if (value > m_threshold) {
        double db = 10.0 * log10(value);
        PaintAssistant::drawVisibleText(v, paint,
                                        xorigin + 2,
                                        cursorPos.y() + 2 +
                                        paint.fontMetrics().ascent(),
                                        QString("%1 dBV").arg(db),
                                        PaintAssistant::OutlinedText);
    }
    
    int harmonic = 2;

    while (harmonic < 100) {

        int hx = int(lrint(getXForFrequency(v, fundamental * harmonic)));

        if (hx < xorigin || hx > v->getPaintWidth()) break;
        
        int len = 7;

        if (harmonic % 2 == 0) {
            if (harmonic % 4 == 0) {
                len = 12;
            } else {
                len = 10;
            }
        }

        paint.drawLine(hx,
                       cursorPos.y(),
                       hx,
                       cursorPos.y() + len);

        ++harmonic;
    }

    paint.restore();
}

QString
SpectrumLayer::getFeatureDescription(LayerGeometryProvider *v, QPoint &p) const
{
    if (!m_sliceableModel) return "";

    int minbin = 0, maxbin = 0, range = 0;
    QString genericDesc = SliceLayer::getFeatureDescriptionAux
        (v, p, false, minbin, maxbin, range);

    if (genericDesc == "") return "";

    int i0 = minbin - m_minbin;
    int i1 = maxbin - m_minbin;
        
    float minvalue = 0.0;
    if (in_range_for(m_values, i0)) minvalue = m_values[i0];

    float maxvalue = minvalue;
    if (in_range_for(m_values, i1)) maxvalue = m_values[i1];
    
    if (minvalue > maxvalue) std::swap(minvalue, maxvalue);
    
    QString binstr;
    QString hzstr;
    int minfreq = int(lrint((minbin * m_sliceableModel->getSampleRate()) /
                            getFFTSize()));
    int maxfreq = int(lrint((std::max(maxbin, minbin)
                             * m_sliceableModel->getSampleRate()) /
                            getFFTSize()));

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
    double mindb = AudioLevel::multiplier_to_dB(minvalue);
    double maxdb = AudioLevel::multiplier_to_dB(maxvalue);
    QString mindbstr;
    QString maxdbstr;
    if (mindb == AudioLevel::DB_FLOOR) {
        mindbstr = Strings::minus_infinity;
    } else {
        mindbstr = QString("%1").arg(lrint(mindb));
    }
    if (maxdb == AudioLevel::DB_FLOOR) {
        maxdbstr = Strings::minus_infinity;
    } else {
        maxdbstr = QString("%1").arg(lrint(maxdb));
    }
    if (lrint(mindb) != lrint(maxdb)) {
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

void
SpectrumLayer::paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
    if (!m_originModel || !m_originModel->isOK() ||
        !m_originModel->isReady()) {
        SVDEBUG << "SpectrumLayer::paint: no origin model, or origin model not OK or not ready" << endl;
        return;
    }

    if (m_newFFTNeeded) {
        SVDEBUG << "SpectrumLayer::paint: new FFT needed, calling setupFFT" << endl;
        const_cast<SpectrumLayer *>(this)->setupFFT(); //ugh
    }

    FFTModel *fft = dynamic_cast<FFTModel *>
        (const_cast<DenseThreeDimensionalModel *>(m_sliceableModel));

    double thresh = (pow(10, -6) / m_gain) * (getFFTSize() / 2.0); // -60dB adj

    int xorigin = getVerticalScaleWidth(v, false, paint) + 1;
    int scaleHeight = getHorizontalScaleHeight(v, paint);

    QPoint localPos;
    bool shouldIlluminate = v->shouldIlluminateLocalFeatures(this, localPos);

    int illuminateX = 0;
    double illuminateFreq = 0.0;
    double illuminateLevel = 0.0;

    ColourMapper mapper =
        hasLightBackground() ?
        ColourMapper(ColourMapper::BlackOnWhite, m_colourInverted, 0, 1) :
        ColourMapper(ColourMapper::WhiteOnBlack, m_colourInverted, 0, 1);

//    cerr << "shouldIlluminate = " << shouldIlluminate << ", localPos = " << localPos.x() << "," << localPos.y() << endl;

    if (fft && m_showPeaks) {

        // draw peak lines

        int col = int(v->getCentreFrame() / fft->getResolution());

        paint.save();
        paint.setRenderHint(QPainter::Antialiasing, false);
        
        int peakminbin = 0;
        int peakmaxbin = fft->getHeight() - 1;
        double peakmaxfreq = Pitch::getFrequencyForPitch(128);
        peakmaxbin = int(((peakmaxfreq * fft->getHeight() * 2) /
                          fft->getSampleRate()));
        
        FFTModel::PeakSet peaks = fft->getPeakFrequencies
            (FFTModel::MajorPitchAdaptivePeaks, col, peakminbin, peakmaxbin);

        BiasCurve curve;
        getBiasCurve(curve);
        int cs = int(curve.size());

        int px = -1;

        int fuzz = ViewManager::scalePixelSize(3);
        
        for (FFTModel::PeakSet::iterator i = peaks.begin();
             i != peaks.end(); ++i) {

            double freq = i->second;
            int x = int(lrint(getXForFrequency(v, freq)));
            if (x == px) {
                continue;
            }
            
            int bin = i->first;
            
//            cerr << "bin = " << bin << ", thresh = " << thresh << ", value = " << fft->getMagnitudeAt(col, bin) << endl;

            double value = fft->getValueAt(col, bin);
            if (value < thresh) continue;
            if (bin < cs) value *= curve[bin];
            
            double norm = 0.f;
            // we only need the norm here, for the colour map
            (void)getYForValue(v, value, norm);

            QColor colour = mapper.map(norm);
            
            paint.setPen(QPen(colour, 1));
            paint.drawLine(x, 0, x, v->getPaintHeight() - scaleHeight - 1);

            if (shouldIlluminate && std::abs(localPos.x() - x) <= fuzz) {
                illuminateX = x;
                illuminateFreq = freq;
                illuminateLevel = norm;
            }
            
            px = x;
        }

        paint.restore();
    }
    
    paint.save();
    
    SliceLayer::paint(v, paint, rect);
    
    paintHorizontalScale(v, paint, xorigin);

    paint.restore();

    if (illuminateFreq > 0.0) {

        QColor colour = mapper.map(illuminateLevel);
        paint.setPen(QPen(colour, 1));

        int labelY = v->getPaintHeight() -
            getHorizontalScaleHeight(v, paint) -
            paint.fontMetrics().height() * 4;
        
        QString text = tr("%1 Hz").arg(illuminateFreq);
        int lw = paint.fontMetrics().width(text);

        int gap = ViewManager::scalePixelSize(v->getXForViewX(3));
        double half = double(gap)/2.0;

        int labelX = illuminateX - lw - gap;
        if (labelX < getVerticalScaleWidth(v, false, paint)) {
            labelX = illuminateX + gap;
        }

        PaintAssistant::drawVisibleText
            (v, paint, labelX, labelY,
             text, PaintAssistant::OutlinedText);

        if (Pitch::isFrequencyInMidiRange(illuminateFreq)) {
            QString pitchLabel = Pitch::getPitchLabelForFrequency
                (illuminateFreq);
            PaintAssistant::drawVisibleText
                (v, paint,
                 labelX, labelY + paint.fontMetrics().ascent() + gap,
                 pitchLabel, PaintAssistant::OutlinedText);
        }
        paint.fillRect(QRectF(illuminateX - half, labelY + gap, gap, gap),
                       colour);
    }
}

int
SpectrumLayer::getHorizontalScaleHeight(LayerGeometryProvider *v,
                                        QPainter &paint) const
{
    int pkh = int(paint.fontMetrics().height() * 0.7 + 0.5);
    if (pkh < 10) pkh = 10;

    int scaleh = HorizontalFrequencyScale().getHeight(v, paint);

    return pkh + scaleh;
}

void
SpectrumLayer::paintHorizontalScale(LayerGeometryProvider *v,
                                    QPainter &paint,
                                    int xorigin) const
{
    //!!! All of this stuff relating to depicting frequencies
    // (keyboard, crosshairs etc) should be applicable to any slice
    // layer whose model has a vertical scale unit of Hz.  However,
    // the dense 3d model at the moment doesn't record its vertical
    // scale unit -- we need to fix that and hoist this code as
    // appropriate.  Same really goes for any code in SpectrogramLayer
    // that could be relevant to Colour3DPlotLayer with unit Hz, but
    // that's a bigger proposition.

    if (!v->getViewManager()->shouldShowHorizontalValueScale()) {
        return;
    }
    
    int totalScaleHeight = getHorizontalScaleHeight(v, paint); // inc piano
    int freqScaleHeight = HorizontalFrequencyScale().getHeight(v, paint);
    int paintHeight = v->getPaintHeight();
    int paintWidth = v->getPaintWidth();

    PianoScale().paintPianoHorizontal
        (v, this, paint,
         QRect(xorigin, paintHeight - totalScaleHeight - 1,
               paintWidth - 1, totalScaleHeight - freqScaleHeight));

    int scaleLeft = int(getXForBin(v, 1));
    
    paint.drawLine(int(getXForBin(v, 0)), paintHeight - freqScaleHeight,
                   scaleLeft, paintHeight - freqScaleHeight);

    QString hz = tr("Hz");
    int hzw = paint.fontMetrics().width(hz);
    if (scaleLeft > hzw + 5) {
        paint.drawText
            (scaleLeft - hzw - 5,
             paintHeight - freqScaleHeight + paint.fontMetrics().ascent() + 5,
             hz);
    }

    HorizontalFrequencyScale().paintScale
        (v, this, paint,
         QRect(scaleLeft, paintHeight - freqScaleHeight,
               paintWidth, totalScaleHeight),
         m_binScale == LogBins);
}

void
SpectrumLayer::getBiasCurve(BiasCurve &curve) const
{
    curve = m_biasCurve;
}

void
SpectrumLayer::toXml(QTextStream &stream,
                     QString indent, QString extraAttributes) const
{
    QString s = QString("windowSize=\"%1\" "
                        "windowHopLevel=\"%2\" "
                        "oversampling=\"%3\" "
                        "showPeaks=\"%4\" ")
        .arg(m_windowSize)
        .arg(m_windowHopLevel)
        .arg(m_oversampling)
        .arg(m_showPeaks ? "true" : "false");

    SliceLayer::toXml(stream, indent, extraAttributes + " " + s);
}

void
SpectrumLayer::setProperties(const QXmlAttributes &attributes)
{
    SliceLayer::setProperties(attributes);

    bool ok = false;

    int windowSize = attributes.value("windowSize").toUInt(&ok);
    if (ok) setWindowSize(windowSize);

    int windowHopLevel = attributes.value("windowHopLevel").toUInt(&ok);
    if (ok) setWindowHopLevel(windowHopLevel);

    int oversampling = attributes.value("oversampling").toUInt(&ok);
    if (ok) setOversampling(oversampling);

    bool showPeaks = (attributes.value("showPeaks").trimmed() == "true");
    setShowPeaks(showPeaks);
}

    
