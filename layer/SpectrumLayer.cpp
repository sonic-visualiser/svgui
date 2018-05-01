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
#include "LogNumericalScale.h"

#include <QPainter>
#include <QTextStream>


SpectrumLayer::SpectrumLayer() :
    m_originModel(0),
    m_channel(-1),
    m_channelSet(false),
    m_windowSize(4096),
    m_windowType(HanningWindow),
    m_windowHopLevel(3),
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

    FFTModel *newFFT = new FFTModel(m_originModel,
                                    m_channel,
                                    m_windowType,
                                    m_windowSize,
                                    getWindowIncrement(),
                                    m_windowSize);

    setSliceableModel(newFFT);

    m_biasCurve.clear();
    for (int i = 0; i < m_windowSize; ++i) {
        m_biasCurve.push_back(1.f / (float(m_windowSize)/2.f));
    }

    m_newFFTNeeded = false;
}

Layer::PropertyList
SpectrumLayer::getProperties() const
{
    PropertyList list = SliceLayer::getProperties();
    list.push_back("Window Size");
    list.push_back("Window Increment");
    list.push_back("Show Peak Frequencies");
    return list;
}

QString
SpectrumLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Window Size") return tr("Window Size");
    if (name == "Window Increment") return tr("Window Overlap");
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
    if (name == "Show Peak Frequencies") return ToggleProperty;
    return SliceLayer::getPropertyType(name);
}

QString
SpectrumLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Window Size" ||
        name == "Window Increment") return tr("Window");
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
        setWindowType(Preferences::getInstance()->getWindowType());
        return;
    }
}

double
SpectrumLayer::getFrequencyForX(const LayerGeometryProvider *v, double x) const
{
    if (!m_sliceableModel) return 0;
    double bin = getBinForX(v, x);
    return (m_sliceableModel->getSampleRate() * bin) /
        (m_sliceableModel->getHeight() * 2);
}

double
SpectrumLayer::getXForFrequency(const LayerGeometryProvider *v, double freq) const
{
    if (!m_sliceableModel) return 0;
    double bin = (freq * m_sliceableModel->getHeight() * 2) /
        m_sliceableModel->getSampleRate();
    return getXForBin(v, bin);
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

    ColourMapper mapper(m_colourMap, 0, 1);
    paint.setPen(mapper.getContrastingColour());

    int xorigin = m_xorigins[v->getId()];
    paint.drawLine(xorigin, cursorPos.y(), v->getPaintWidth(), cursorPos.y());
    paint.drawLine(cursorPos.x(), cursorPos.y(), cursorPos.x(), v->getPaintHeight());
    
    double fundamental = getFrequencyForX(v, cursorPos.x());

    int hoffset = 2;
    if (m_binScale == LogBins) hoffset = 13;

    PaintAssistant::drawVisibleText(v, paint,
                                    cursorPos.x() + 2,
                                    v->getPaintHeight() - 2 - hoffset,
                                    QString("%1 Hz").arg(fundamental),
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
    double thresh = m_threshold;
    double db = thresh;
    if (value > 0.0) db = 10.0 * log10(value);
    if (db < thresh) db = thresh;

    PaintAssistant::drawVisibleText(v, paint,
                       xorigin + 2,
                       cursorPos.y() - 2,
                       QString("%1 V").arg(value),
                       PaintAssistant::OutlinedText);

    PaintAssistant::drawVisibleText(v, paint,
                       xorigin + 2,
                       cursorPos.y() + 2 + paint.fontMetrics().ascent(),
                       QString("%1 dBV").arg(db),
                       PaintAssistant::OutlinedText);
    
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
                            m_windowSize));
    int maxfreq = int(lrint((std::max(maxbin, minbin)
                             * m_sliceableModel->getSampleRate()) /
                            m_windowSize));

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

    double thresh = (pow(10, -6) / m_gain) * (m_windowSize / 2.0); // -60dB adj

    int xorigin = getVerticalScaleWidth(v, false, paint) + 1;
    int w = v->getPaintWidth() - xorigin - 1;

    int pkh = int(paint.fontMetrics().height() * 0.7 + 0.5);
    if (pkh < 10) pkh = 10;

    int scaleh = LogNumericalScale().getWidth(v, paint, true);

    paint.save();

    if (fft && m_showPeaks) {

        // draw peak lines

//        SVDEBUG << "Showing peaks..." << endl;

        int col = int(v->getCentreFrame() / fft->getResolution());

        paint.save();
        paint.setRenderHint(QPainter::Antialiasing, false);
        paint.setPen(QColor(160, 160, 160)); //!!!

        int peakminbin = 0;
        int peakmaxbin = fft->getHeight() - 1;
        double peakmaxfreq = Pitch::getFrequencyForPitch(128);
        peakmaxbin = int(((peakmaxfreq * fft->getHeight() * 2) / fft->getSampleRate()));
        
        FFTModel::PeakSet peaks = fft->getPeakFrequencies
            (FFTModel::MajorPitchAdaptivePeaks, col, peakminbin, peakmaxbin);

        ColourMapper mapper(ColourMapper::BlackOnWhite, 0, 1);

        BiasCurve curve;
        getBiasCurve(curve);
        int cs = int(curve.size());

        std::vector<double> values;
        
        for (int bin = 0; bin < fft->getHeight(); ++bin) {
            double value = m_sliceableModel->getValueAt(col, bin);
            if (bin < cs) value *= curve[bin];
            values.push_back(value);
        }

        for (FFTModel::PeakSet::iterator i = peaks.begin();
             i != peaks.end(); ++i) {

            int bin = i->first;
            
//            cerr << "bin = " << bin << ", thresh = " << thresh << ", value = " << fft->getMagnitudeAt(col, bin) << endl;

            if (!fft->isOverThreshold(col, bin, float(thresh))) continue;
            
            double freq = i->second;
          
            int x = int(lrint(getXForFrequency(v, freq)));

            double norm = 0.f;
            (void)getYForValue(v, values[bin], norm); // don't need return value, need norm

            paint.setPen(mapper.map(norm));
            paint.drawLine(xorigin + x, 0, xorigin + x,
                           v->getPaintHeight() - scaleh - pkh - 1);
        }

        paint.restore();
    }
    
    SliceLayer::paint(v, paint, rect);

    //!!! All of this stuff relating to depicting frequencies
    // (keyboard, crosshairs etc) should be applicable to any slice
    // layer whose model has a vertical scale unit of Hz.  However,
    // the dense 3d model at the moment doesn't record its vertical
    // scale unit -- we need to fix that and hoist this code as
    // appropriate.  Same really goes for any code in SpectrogramLayer
    // that could be relevant to Colour3DPlotLayer with unit Hz, but
    // that's a bigger proposition.

    int h = v->getPaintHeight();

    PianoScale().paintPianoHorizontal
        (v, this, paint,
         QRect(xorigin, h - scaleh - pkh - 1, w + xorigin, pkh));

    LogNumericalScale().paintHorizontal
        (v, this, paint,
         QRect(int(getXForBin(v, 0)), h - scaleh, w + xorigin, scaleh)); 
    
    paint.restore();
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
                        "showPeaks=\"%3\" ")
        .arg(m_windowSize)
        .arg(m_windowHopLevel)
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

    bool showPeaks = (attributes.value("showPeaks").trimmed() == "true");
    setShowPeaks(showPeaks);
}

    
