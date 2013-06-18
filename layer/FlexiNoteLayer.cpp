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

#include "FlexiNoteLayer.h"

#include "data/model/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/Pitch.h"
#include "base/LogRange.h"
#include "base/RangeMapper.h"
#include "ColourDatabase.h"
#include "view/View.h"

#include "data/model/FlexiNoteModel.h"

#include "widgets/ItemEditDialog.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QTextStream>
#include <QMessageBox>

#include <iostream>
#include <cmath>
#include <utility>

FlexiNoteLayer::FlexiNoteLayer() :
    SingleColourLayer(),

        // m_model(0),
        // m_editing(false),
        // m_originalPoint(0, 0.0, 0, 1.f, tr("New Point")),
        // m_editingPoint(0, 0.0, 0, 1.f, tr("New Point")),
        // m_editingCommand(0),
        // m_verticalScale(AutoAlignScale),
        // m_scaleMinimum(0),
        // m_scaleMaximum(0)

    m_model(0),
    m_editing(false),
    m_originalPoint(0, 0.0, 0, 1.f, tr("New Point")),
    m_editingPoint(0, 0.0, 0, 1.f, tr("New Point")),
    m_editingCommand(0),
    m_verticalScale(AutoAlignScale),
    m_scaleMinimum(34), 
    m_scaleMaximum(77)
{
}

void
FlexiNoteLayer::setModel(FlexiNoteModel *model) 
{
    if (m_model == model) return;
    m_model = model;

    connectSignals(m_model);

    // m_scaleMinimum = 0;
    // m_scaleMaximum = 0;

    emit modelReplaced();
}

Layer::PropertyList
FlexiNoteLayer::getProperties() const
{
    PropertyList list = SingleColourLayer::getProperties();
    list.push_back("Vertical Scale");
    list.push_back("Scale Units");
    return list;
}

QString
FlexiNoteLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Vertical Scale") return tr("Vertical Scale");
    if (name == "Scale Units") return tr("Scale Units");
    return SingleColourLayer::getPropertyLabel(name);
}

Layer::PropertyType
FlexiNoteLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Scale Units") return UnitsProperty;
    if (name == "Vertical Scale") return ValueProperty;
    return SingleColourLayer::getPropertyType(name);
}

QString
FlexiNoteLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Vertical Scale" || name == "Scale Units") {
        return tr("Scale");
    }
    return SingleColourLayer::getPropertyGroupName(name);
}

int
FlexiNoteLayer::getPropertyRangeAndValue(const PropertyName &name,
                                    int *min, int *max, int *deflt) const
{
    int val = 0;

    if (name == "Vertical Scale") {
    
    if (min) *min = 0;
    if (max) *max = 3;
        if (deflt) *deflt = int(AutoAlignScale);
    
    val = int(m_verticalScale);

    } else if (name == "Scale Units") {

        if (deflt) *deflt = 0;
        if (m_model) {
            val = UnitDatabase::getInstance()->getUnitId
                (m_model->getScaleUnits());
        }

    } else {

    val = SingleColourLayer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
FlexiNoteLayer::getPropertyValueLabel(const PropertyName &name,
                                 int value) const
{
    if (name == "Vertical Scale") {
    switch (value) {
    default:
    case 0: return tr("Auto-Align");
    case 1: return tr("Linear");
    case 2: return tr("Log");
    case 3: return tr("MIDI Notes");
    }
    }
    return SingleColourLayer::getPropertyValueLabel(name, value);
}

void
FlexiNoteLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Vertical Scale") {
    setVerticalScale(VerticalScale(value));
    } else if (name == "Scale Units") {
        if (m_model) {
            m_model->setScaleUnits
                (UnitDatabase::getInstance()->getUnitById(value));
            emit modelChanged();
        }
    } else {
        return SingleColourLayer::setProperty(name, value);
    }
}

void
FlexiNoteLayer::setVerticalScale(VerticalScale scale)
{
    if (m_verticalScale == scale) return;
    m_verticalScale = scale;
    emit layerParametersChanged();
}

bool
FlexiNoteLayer::isLayerScrollable(const View *v) const
{
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

bool
FlexiNoteLayer::shouldConvertMIDIToHz() const
{
    QString unit = m_model->getScaleUnits();
    return (unit != "Hz");
//    if (unit == "" ||
//        unit.startsWith("MIDI") ||
//        unit.startsWith("midi")) return true;
//    return false;
}

bool
FlexiNoteLayer::getValueExtents(float &min, float &max,
                           bool &logarithmic, QString &unit) const
{
    if (!m_model) return false;
    min = m_model->getValueMinimum();
    max = m_model->getValueMaximum();

    if (shouldConvertMIDIToHz()) {
        unit = "Hz";
        min = Pitch::getFrequencyForPitch(lrintf(min));
        max = Pitch::getFrequencyForPitch(lrintf(max + 1));
    } else unit = m_model->getScaleUnits();

    if (m_verticalScale == MIDIRangeScale ||
        m_verticalScale == LogScale) logarithmic = true;

    return true;
}

bool
FlexiNoteLayer::getDisplayExtents(float &min, float &max) const
{
    if (!m_model || shouldAutoAlign()) {
        std::cerr << "No model or shouldAutoAlign()" << std::endl;
        return false;
    }

    if (m_verticalScale == MIDIRangeScale) {
        min = Pitch::getFrequencyForPitch(0);
        max = Pitch::getFrequencyForPitch(127);
        return true;
    }

    if (m_scaleMinimum == m_scaleMaximum) {
        min = m_model->getValueMinimum();
        max = m_model->getValueMaximum();
    } else {
        min = m_scaleMinimum;
        max = m_scaleMaximum;
    }

    if (shouldConvertMIDIToHz()) {
        min = Pitch::getFrequencyForPitch(lrintf(min));
        max = Pitch::getFrequencyForPitch(lrintf(max + 1));
    }

    return true;
}

bool
FlexiNoteLayer::setDisplayExtents(float min, float max)
{
    if (!m_model) return false;

    if (min == max) {
        if (min == 0.f) {
            max = 1.f;
        } else {
            max = min * 1.0001;
        }
    }

    m_scaleMinimum = min;
    m_scaleMaximum = max;

//    SVDEBUG << "FlexiNoteLayer::setDisplayExtents: min = " << min << ", max = " << max << endl;
    
    emit layerParametersChanged();
    return true;
}

int
FlexiNoteLayer::getVerticalZoomSteps(int &defaultStep) const
{
    if (shouldAutoAlign()) return 0;
    if (!m_model) return 0;

    defaultStep = 0;
    return 100;
}

int
FlexiNoteLayer::getCurrentVerticalZoomStep() const
{
    if (shouldAutoAlign()) return 0;
    if (!m_model) return 0;

    RangeMapper *mapper = getNewVerticalZoomRangeMapper();
    if (!mapper) return 0;

    float dmin, dmax;
    getDisplayExtents(dmin, dmax);

    int nr = mapper->getPositionForValue(dmax - dmin);

    delete mapper;

    return 100 - nr;
}

//!!! lots of duplication with TimeValueLayer

void
FlexiNoteLayer::setVerticalZoomStep(int step)
{
    if (shouldAutoAlign()) return;
    if (!m_model) return;

    RangeMapper *mapper = getNewVerticalZoomRangeMapper();
    if (!mapper) return;
    
    float min, max;
    bool logarithmic;
    QString unit;
    getValueExtents(min, max, logarithmic, unit);
    
    float dmin, dmax;
    getDisplayExtents(dmin, dmax);

    float newdist = mapper->getValueForPosition(100 - step);

    float newmin, newmax;

    if (logarithmic) {

        // see SpectrogramLayer::setVerticalZoomStep

        newmax = (newdist + sqrtf(newdist*newdist + 4*dmin*dmax)) / 2;
        newmin = newmax - newdist;

//        std::cerr << "newmin = " << newmin << ", newmax = " << newmax << std::endl;

    } else {
        float dmid = (dmax + dmin) / 2;
        newmin = dmid - newdist / 2;
        newmax = dmid + newdist / 2;
    }

    if (newmin < min) {
        newmax += (min - newmin);
        newmin = min;
    }
    if (newmax > max) {
        newmax = max;
    }
    
    SVDEBUG << "FlexiNoteLayer::setVerticalZoomStep: " << step << ": " << newmin << " -> " << newmax << " (range " << newdist << ")" << endl;

    setDisplayExtents(newmin, newmax);
}

RangeMapper *
FlexiNoteLayer::getNewVerticalZoomRangeMapper() const
{
    if (!m_model) return 0;
    
    RangeMapper *mapper;

    float min, max;
    bool logarithmic;
    QString unit;
    getValueExtents(min, max, logarithmic, unit);

    if (min == max) return 0;
    
    if (logarithmic) {
        mapper = new LogRangeMapper(0, 100, min, max, unit);
    } else {
        mapper = new LinearRangeMapper(0, 100, min, max, unit);
    }

    return mapper;
}

FlexiNoteModel::PointList
FlexiNoteLayer::getLocalPoints(View *v, int x) const
{
    if (!m_model) return FlexiNoteModel::PointList();

    long frame = v->getFrameForX(x);

    FlexiNoteModel::PointList onPoints =
    m_model->getPoints(frame);

    if (!onPoints.empty()) {
    return onPoints;
    }

    FlexiNoteModel::PointList prevPoints =
    m_model->getPreviousPoints(frame);
    FlexiNoteModel::PointList nextPoints =
    m_model->getNextPoints(frame);

    FlexiNoteModel::PointList usePoints = prevPoints;

    if (prevPoints.empty()) {
    usePoints = nextPoints;
    } else if (long(prevPoints.begin()->frame) < v->getStartFrame() &&
           !(nextPoints.begin()->frame > v->getEndFrame())) {
    usePoints = nextPoints;
    } else if (long(nextPoints.begin()->frame) - frame <
           frame - long(prevPoints.begin()->frame)) {
    usePoints = nextPoints;
    }

    if (!usePoints.empty()) {
    int fuzz = 2;
    int px = v->getXForFrame(usePoints.begin()->frame);
    if ((px > x && px - x > fuzz) ||
        (px < x && x - px > fuzz + 1)) {
        usePoints.clear();
    }
    }

    return usePoints;
}

bool
FlexiNoteLayer::getPointToDrag(View *v, int x, int y, FlexiNoteModel::Point &p) const
{
    if (!m_model) return false;

    long frame = v->getFrameForX(x);

    FlexiNoteModel::PointList onPoints = m_model->getPoints(frame);
    if (onPoints.empty()) return false;

//    std::cerr << "frame " << frame << ": " << onPoints.size() << " candidate points" << std::endl;

    int nearestDistance = -1;

    for (FlexiNoteModel::PointList::const_iterator i = onPoints.begin();
         i != onPoints.end(); ++i) {
        
        int distance = getYForValue(v, (*i).value) - y;
        if (distance < 0) distance = -distance;
        if (nearestDistance == -1 || distance < nearestDistance) {
            nearestDistance = distance;
            p = *i;
        }
    }

    return true;
}

bool
FlexiNoteLayer::getNoteToEdit(View *v, int x, int y, FlexiNoteModel::Point &p) const
{
    // GF: find the note that is closest to the cursor
    if (!m_model) return false;

    long frame = v->getFrameForX(x);

    FlexiNoteModel::PointList onPoints = m_model->getPoints(frame);
    if (onPoints.empty()) return false;

//    std::cerr << "frame " << frame << ": " << onPoints.size() << " candidate points" << std::endl;

    int nearestDistance = -1;

    for (FlexiNoteModel::PointList::const_iterator i = onPoints.begin();
         i != onPoints.end(); ++i) {
        
        int distance = getYForValue(v, (*i).value) - y;
        if (distance < 0) distance = -distance;
        if (nearestDistance == -1 || distance < nearestDistance) {
            nearestDistance = distance;
            p = *i;
        }
    }

    return true;
}

QString
FlexiNoteLayer::getFeatureDescription(View *v, QPoint &pos) const
{
    int x = pos.x();

    if (!m_model || !m_model->getSampleRate()) return "";

    FlexiNoteModel::PointList points = getLocalPoints(v, x);

    if (points.empty()) {
    if (!m_model->isReady()) {
        return tr("In progress");
    } else {
        return tr("No local points");
    }
    }

    FlexiNote note(0);
    FlexiNoteModel::PointList::iterator i;

    for (i = points.begin(); i != points.end(); ++i) {

    int y = getYForValue(v, i->value);
    int h = NOTE_HEIGHT; // GF: larger notes

    if (m_model->getValueQuantization() != 0.0) {
        h = y - getYForValue(v, i->value + m_model->getValueQuantization());
        if (h < NOTE_HEIGHT) h = NOTE_HEIGHT;
    }

    // GF: this is not quite correct
    if (pos.y() >= y - 4 && pos.y() <= y + h) {
        note = *i;
        break;
    }
    }

    if (i == points.end()) return tr("No local points");

    RealTime rt = RealTime::frame2RealTime(note.frame,
                       m_model->getSampleRate());
    RealTime rd = RealTime::frame2RealTime(note.duration,
                       m_model->getSampleRate());
    
    QString pitchText;

    if (shouldConvertMIDIToHz()) {

        int mnote = lrintf(note.value);
        int cents = lrintf((note.value - mnote) * 100);
        float freq = Pitch::getFrequencyForPitch(mnote, cents);
        pitchText = tr("%1 (%2, %3 Hz)")
            .arg(Pitch::getPitchLabel(mnote, cents))
            .arg(mnote)
            .arg(freq);

    } else if (m_model->getScaleUnits() == "Hz") {

        pitchText = tr("%1 Hz (%2, %3)")
            .arg(note.value)
            .arg(Pitch::getPitchLabelForFrequency(note.value))
            .arg(Pitch::getPitchForFrequency(note.value));

    } else {
        pitchText = tr("%1 %2")
            .arg(note.value).arg(m_model->getScaleUnits());
    }

    QString text;

    if (note.label == "") {
    text = QString(tr("Time:\t%1\nPitch:\t%2\nDuration:\t%3\nNo label"))
        .arg(rt.toText(true).c_str())
        .arg(pitchText)
        .arg(rd.toText(true).c_str());
    } else {
    text = QString(tr("Time:\t%1\nPitch:\t%2\nDuration:\t%3\nLabel:\t%4"))
        .arg(rt.toText(true).c_str())
        .arg(pitchText)
        .arg(rd.toText(true).c_str())
        .arg(note.label);
    }

    pos = QPoint(v->getXForFrame(note.frame),
         getYForValue(v, note.value));
    return text;
}

bool
FlexiNoteLayer::snapToFeatureFrame(View *v, int &frame,
                  size_t &resolution,
                  SnapType snap) const
{
    if (!m_model) {
    return Layer::snapToFeatureFrame(v, frame, resolution, snap);
    }

    resolution = m_model->getResolution();
    FlexiNoteModel::PointList points;

    if (snap == SnapNeighbouring) {
    
    points = getLocalPoints(v, v->getXForFrame(frame));
    if (points.empty()) return false;
    frame = points.begin()->frame;
    return true;
    }    

    points = m_model->getPoints(frame, frame);
    int snapped = frame;
    bool found = false;

    for (FlexiNoteModel::PointList::const_iterator i = points.begin();
     i != points.end(); ++i) {

    if (snap == SnapRight) {

        if (i->frame > frame) {
        snapped = i->frame;
        found = true;
        break;
        }

    } else if (snap == SnapLeft) {

        if (i->frame <= frame) {
        snapped = i->frame;
        found = true; // don't break, as the next may be better
        } else {
        break;
        }

    } else { // nearest

        FlexiNoteModel::PointList::const_iterator j = i;
        ++j;

        if (j == points.end()) {

        snapped = i->frame;
        found = true;
        break;

        } else if (j->frame >= frame) {

        if (j->frame - frame < frame - i->frame) {
            snapped = j->frame;
        } else {
            snapped = i->frame;
        }
        found = true;
        break;
        }
    }
    }

    frame = snapped;
    return found;
}

void
FlexiNoteLayer::getScaleExtents(View *v, float &min, float &max, bool &log) const
{
    min = 0.0;
    max = 0.0;
    log = false;

    QString queryUnits;
    if (shouldConvertMIDIToHz()) queryUnits = "Hz";
    else queryUnits = m_model->getScaleUnits();

    if (shouldAutoAlign()) {

        if (!v->getValueExtents(queryUnits, min, max, log)) {

            min = m_model->getValueMinimum();
            max = m_model->getValueMaximum();

            if (shouldConvertMIDIToHz()) {
                min = Pitch::getFrequencyForPitch(lrintf(min));
                max = Pitch::getFrequencyForPitch(lrintf(max + 1));
            }

            // std::cerr << "FlexiNoteLayer[" << this << "]::getScaleExtents: min = " << min << ", max = " << max << ", log = " << log << std::endl;

        } else if (log) {

            LogRange::mapRange(min, max);

            // std::cerr << "FlexiNoteLayer[" << this << "]::getScaleExtents: min = " << min << ", max = " << max << ", log = " << log << std::endl;

        }

    } else {

        getDisplayExtents(min, max);

        if (m_verticalScale == MIDIRangeScale) {
            min = Pitch::getFrequencyForPitch(0);
            max = Pitch::getFrequencyForPitch(70);
        } else if (shouldConvertMIDIToHz()) {
            min = Pitch::getFrequencyForPitch(lrintf(min));
            max = Pitch::getFrequencyForPitch(lrintf(max + 1));
        }

        if (m_verticalScale == LogScale || m_verticalScale == MIDIRangeScale) {
            LogRange::mapRange(min, max);
            log = true;
        }
    }

    if (max == min) max = min + 1.0;
}

int
FlexiNoteLayer::getYForValue(View *v, float val) const
{
    float min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->height();

    getScaleExtents(v, min, max, logarithmic);

//    std::cerr << "FlexiNoteLayer[" << this << "]::getYForValue(" << val << "): min = " << min << ", max = " << max << ", log = " << logarithmic << std::endl;

    if (shouldConvertMIDIToHz()) {
        val = Pitch::getFrequencyForPitch(lrintf(val),
                                          lrintf((val - lrintf(val)) * 100));
//        std::cerr << "shouldConvertMIDIToHz true, val now = " << val << std::endl;
    }

    if (logarithmic) {
        val = LogRange::map(val);
//        std::cerr << "logarithmic true, val now = " << val << std::endl;
    }

    int y = int(h - ((val - min) * h) / (max - min)) - 1;
//    std::cerr << "y = " << y << std::endl;
    return y;
}

float
FlexiNoteLayer::getValueForY(View *v, int y) const
{
    float min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->height();

    getScaleExtents(v, min, max, logarithmic);

    float val = min + (float(h - y) * float(max - min)) / h;

    if (logarithmic) {
        val = powf(10.f, val);
    }

    if (shouldConvertMIDIToHz()) {
        val = Pitch::getPitchForFrequency(val);
    }

    return val;
}

bool
FlexiNoteLayer::shouldAutoAlign() const
{
    if (!m_model) return false;
    return (m_verticalScale == AutoAlignScale);
}

void
FlexiNoteLayer::paint(View *v, QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) return;

    int sampleRate = m_model->getSampleRate();
    if (!sampleRate) return;

//    Profiler profiler("FlexiNoteLayer::paint", true);

    int x0 = rect.left(), x1 = rect.right();
    long frame0 = v->getFrameForX(x0);
    long frame1 = v->getFrameForX(x1);

    FlexiNoteModel::PointList points(m_model->getPoints(frame0, frame1));
    if (points.empty()) return;

    paint.setPen(getBaseQColor());

    QColor brushColour(getBaseQColor());
    brushColour.setAlpha(80);

//    SVDEBUG << "FlexiNoteLayer::paint: resolution is "
//        << m_model->getResolution() << " frames" << endl;

    float min = m_model->getValueMinimum();
    float max = m_model->getValueMaximum();
    if (max == min) max = min + 1.0;

    QPoint localPos;
    FlexiNoteModel::Point illuminatePoint(0);
    bool shouldIlluminate = false;

    if (v->shouldIlluminateLocalFeatures(this, localPos)) {
        shouldIlluminate = getPointToDrag(v, localPos.x(), localPos.y(),
                                          illuminatePoint);
    }

    paint.save();
    paint.setRenderHint(QPainter::Antialiasing, false);
    
    for (FlexiNoteModel::PointList::const_iterator i = points.begin();
     i != points.end(); ++i) {

    const FlexiNoteModel::Point &p(*i);

    int x = v->getXForFrame(p.frame);
    int y = getYForValue(v, p.value);
    int w = v->getXForFrame(p.frame + p.duration) - x;
    int h = NOTE_HEIGHT; //GF: larger notes
    
    if (m_model->getValueQuantization() != 0.0) {
        h = y - getYForValue(v, p.value + m_model->getValueQuantization());
        if (h < NOTE_HEIGHT) h = NOTE_HEIGHT; //GF: larger notes
    }

    if (w < 1) w = 1;
    paint.setPen(getBaseQColor());
    paint.setBrush(brushColour);

    // if (shouldIlluminate &&
    //         // "illuminatePoint == p"
    //         !FlexiNoteModel::Point::Comparator()(illuminatePoint, p) &&
    //         !FlexiNoteModel::Point::Comparator()(p, illuminatePoint)) {
    // 
    //         paint.setPen(v->getForeground());
    //         paint.setBrush(v->getForeground());
    // 
    //         QString vlabel = QString("%1%2").arg(p.value).arg(m_model->getScaleUnits());
    //         v->drawVisibleText(paint, 
    //                            x - paint.fontMetrics().width(vlabel) - 2,
    //                            y + paint.fontMetrics().height()/2
    //                              - paint.fontMetrics().descent(), 
    //                            vlabel, View::OutlinedText);
    // 
    //         QString hlabel = RealTime::frame2RealTime
    //             (p.frame, m_model->getSampleRate()).toText(true).c_str();
    //         v->drawVisibleText(paint, 
    //                            x,
    //                            y - h/2 - paint.fontMetrics().descent() - 2,
    //                            hlabel, View::OutlinedText);
    // }
    
    paint.drawRect(x, y - h/2, w, h);
    }

    paint.restore();
}

void
FlexiNoteLayer::drawStart(View *v, QMouseEvent *e)
{
//    SVDEBUG << "FlexiNoteLayer::drawStart(" << e->x() << "," << e->y() << ")" << endl;

    if (!m_model) return;

    long frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    float value = getValueForY(v, e->y());

    m_editingPoint = FlexiNoteModel::Point(frame, value, 0, 0.8, tr("New Point"));
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) finish(m_editingCommand);
    m_editingCommand = new FlexiNoteModel::EditCommand(m_model,
                          tr("Draw Point"));
    m_editingCommand->addPoint(m_editingPoint);

    m_editing = true;
}

void
FlexiNoteLayer::drawDrag(View *v, QMouseEvent *e)
{
//    SVDEBUG << "FlexiNoteLayer::drawDrag(" << e->x() << "," << e->y() << ")" << endl;

    if (!m_model || !m_editing) return;

    long frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    float newValue = getValueForY(v, e->y());

    long newFrame = m_editingPoint.frame;
    long newDuration = frame - newFrame;
    if (newDuration < 0) {
        newFrame = frame;
        newDuration = -newDuration;
    } else if (newDuration == 0) {
        newDuration = 1;
    }

    m_editingCommand->deletePoint(m_editingPoint);
    m_editingPoint.frame = newFrame;
    m_editingPoint.value = newValue;
    m_editingPoint.duration = newDuration;
    m_editingCommand->addPoint(m_editingPoint);
}

void
FlexiNoteLayer::drawEnd(View *, QMouseEvent *)
{
//    SVDEBUG << "FlexiNoteLayer::drawEnd(" << e->x() << "," << e->y() << ")" << endl;
    if (!m_model || !m_editing) return;
    finish(m_editingCommand);
    m_editingCommand = 0;
    m_editing = false;
}

void
FlexiNoteLayer::eraseStart(View *v, QMouseEvent *e)
{
    if (!m_model) return;

    if (!getPointToDrag(v, e->x(), e->y(), m_editingPoint)) return;

    if (m_editingCommand) {
    finish(m_editingCommand);
    m_editingCommand = 0;
    }

    m_editing = true;
}

void
FlexiNoteLayer::eraseDrag(View *v, QMouseEvent *e)
{
}

void
FlexiNoteLayer::eraseEnd(View *v, QMouseEvent *e)
{
    if (!m_model || !m_editing) return;

    m_editing = false;

    FlexiNoteModel::Point p(0);
    if (!getPointToDrag(v, e->x(), e->y(), p)) return;
    if (p.frame != m_editingPoint.frame || p.value != m_editingPoint.value) return;

    m_editingCommand = new FlexiNoteModel::EditCommand(m_model, tr("Erase Point"));

    m_editingCommand->deletePoint(m_editingPoint);

    finish(m_editingCommand);
    m_editingCommand = 0;
    m_editing = false;
}

void
FlexiNoteLayer::editStart(View *v, QMouseEvent *e)
{
//    SVDEBUG << "FlexiNoteLayer::editStart(" << e->x() << "," << e->y() << ")" << endl;
    std::cerr << "FlexiNoteLayer::editStart(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model) return;

    if (!getPointToDrag(v, e->x(), e->y(), m_editingPoint)) return;
    m_originalPoint = FlexiNote(m_editingPoint);
    
    if (m_editMode == RightBoundary) {
        m_dragPointX =   v->getXForFrame(m_editingPoint.frame + m_editingPoint.duration);
    } else {
        m_dragPointX = v->getXForFrame(m_editingPoint.frame);
    }
    m_dragPointY = getYForValue(v, m_editingPoint.value);

    if (m_editingCommand) {
    finish(m_editingCommand);
    m_editingCommand = 0;
    }

    m_editing = true;
    m_dragStartX = e->x();
    m_dragStartY = e->y();
    
    long onset = m_originalPoint.frame;
    long offset = m_originalPoint.frame + m_originalPoint.duration - 1;
    
    m_greatestLeftNeighbourFrame = -1;
    m_smallestRightNeighbourFrame = std::numeric_limits<long>::max();
    
    for (FlexiNoteModel::PointList::const_iterator i = m_model->getPoints().begin();
         i != m_model->getPoints().end(); ++i) {
        FlexiNote currentNote = *i;
        
        // left boundary
        if (currentNote.frame + currentNote.duration - 1 < onset) {
            m_greatestLeftNeighbourFrame = currentNote.frame + currentNote.duration - 1;
        }
        
        // right boundary
        if (currentNote.frame > offset) {
            m_smallestRightNeighbourFrame = currentNote.frame;
            break;
        }
    }
    std::cerr << "note frame: " << onset << ", left boundary: " << m_greatestLeftNeighbourFrame << ", right boundary: " << m_smallestRightNeighbourFrame << std::endl;
}

void
FlexiNoteLayer::editDrag(View *v, QMouseEvent *e)
{
//    SVDEBUG << "FlexiNoteLayer::editDrag(" << e->x() << "," << e->y() << ")" << endl;
    std::cerr << "FlexiNoteLayer::editDrag(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model || !m_editing) return;

    int xdist = e->x() - m_dragStartX;
    int ydist = e->y() - m_dragStartY;
    int newx = m_dragPointX + xdist;
    int newy = m_dragPointY + ydist;

    long frame = v->getFrameForX(newx);
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();
    
    float value = getValueForY(v, newy);

    if (!m_editingCommand) {
    m_editingCommand = new FlexiNoteModel::EditCommand(m_model,
                              tr("Drag Point"));
    }

    m_editingCommand->deletePoint(m_editingPoint);

    std::cerr << "edit mode: " << m_editMode << std::endl;
    switch (m_editMode) {
        case LeftBoundary : {
            if ((frame > m_greatestLeftNeighbourFrame) 
                 && (frame < m_originalPoint.frame + m_originalPoint.duration - 1)
                 && (frame < m_smallestRightNeighbourFrame)) {
                m_editingPoint.duration = m_editingPoint.frame + m_editingPoint.duration - frame + 1;
                m_editingPoint.frame = frame;
            }
            break;
        }
        case RightBoundary : {
            long tempDuration = frame - m_originalPoint.frame;
            if (tempDuration > 0 && m_originalPoint.frame + tempDuration - 1 < m_smallestRightNeighbourFrame) {
                m_editingPoint.duration = tempDuration;
            }
            break;
        }
        case DragNote : {
            if (frame <= m_smallestRightNeighbourFrame - m_editingPoint.duration
                && frame > m_greatestLeftNeighbourFrame) {
                m_editingPoint.frame = frame; // only move if it doesn't overlap with right note or left note
            }
            m_editingPoint.value = value;
            break;
        }
    }
<<<<<<< local
    if (m_editMode == dragNote) m_editingPoint.value = value;
=======
    
>>>>>>> other
    m_editingCommand->addPoint(m_editingPoint);
    std::cerr << "added new point(" << m_editingPoint.frame << "," << m_editingPoint.duration << ")" << std::endl;
    
}

void
FlexiNoteLayer::editEnd(View *v, QMouseEvent *e)
{
//    SVDEBUG << "FlexiNoteLayer::editEnd(" << e->x() << "," << e->y() << ")" << endl;
    std::cerr << "FlexiNoteLayer::editEnd(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model || !m_editing) return;

    if (m_editingCommand) {

    QString newName = m_editingCommand->getName();

    if (m_editingPoint.frame != m_originalPoint.frame) {
        if (m_editingPoint.value != m_originalPoint.value) {
        newName = tr("Edit Point");
        } else {
        newName = tr("Relocate Point");
        }
    } else {
        newName = tr("Change Point Value");
    }

    m_editingCommand->setName(newName);
    finish(m_editingCommand);
    }

    m_editingCommand = 0;
    m_editing = false;
}

void
FlexiNoteLayer::splitStart(View *v, QMouseEvent *e)
{
    // GF: note splitting starts (!! remove printing soon)
    std::cerr << "splitStart" << std::endl;
    if (!m_model) return;

    if (!getPointToDrag(v, e->x(), e->y(), m_editingPoint)) return;
    // m_originalPoint = m_editingPoint;
    // 
    // m_dragPointX = v->getXForFrame(m_editingPoint.frame);
    // m_dragPointY = getYForValue(v, m_editingPoint.value);

    if (m_editingCommand) {
    finish(m_editingCommand);
    m_editingCommand = 0;
    }

    m_editing = true;
    m_dragStartX = e->x();
    m_dragStartY = e->y();
    
}

void
FlexiNoteLayer::splitEnd(View *v, QMouseEvent *e)
{
    // GF: note splitting ends. (!! remove printing soon)
    std::cerr << "splitEnd" << std::endl;
    if (!m_model || !m_editing || m_editMode != SplitNote) return;

    int xdist = e->x() - m_dragStartX;
    int ydist = e->y() - m_dragStartY;
    if (xdist != 0 || ydist != 0) { 
        std::cerr << "mouse moved" << std::endl;    
        return; 
    }

    // MM: simpler declaration 
    FlexiNote note(0);
    if (!getPointToDrag(v, e->x(), e->y(), note)) return;

    long frame = v->getFrameForX(e->x());

    int gap = 0; // MM: I prefer a gap of 0, but we can decide later
    
    // MM: changed this a bit, to make it slightly clearer (// GF: nice changes!)
    FlexiNote newNote1(note.frame, note.value, 
                       frame - note.frame - gap, 
                       note.level, note.label);
    
    FlexiNote newNote2(frame, note.value, 
                       note.duration - newNote1.duration, 
                       note.level, note.label);

    FlexiNoteModel::EditCommand *command = new FlexiNoteModel::EditCommand
        (m_model, tr("Edit Point"));
    command->deletePoint(note);
    if ((e->modifiers() & Qt::ShiftModifier)) {
        finish(command);
        return;
    }
    command->addPoint(newNote1);
    command->addPoint(newNote2);
    finish(command);
    
}

void 
FlexiNoteLayer::mouseMoveEvent(View *v, QMouseEvent *e)
{
    // GF: context sensitive cursors
    // v->setCursor(Qt::ArrowCursor);
    FlexiNoteModel::Point note(0);
    if (!getNoteToEdit(v, e->x(), e->y(), note)) { 
        // v->setCursor(Qt::UpArrowCursor);
        return; 
    }

    bool closeToLeft = false, closeToRight = false, closeToTop = false, closeToBottom = false;
    getRelativeMousePosition(v, note, e->x(), e->y(), closeToLeft, closeToRight, closeToTop, closeToBottom);
    // if (!closeToLeft) return;
    // if (closeToTop) v->setCursor(Qt::SizeVerCursor);
    
    if (closeToLeft) { v->setCursor(Qt::SizeHorCursor); m_editMode = LeftBoundary; return; }
    if (closeToRight) { v->setCursor(Qt::SizeHorCursor); m_editMode = RightBoundary; return; }
    if (closeToTop) { v->setCursor(Qt::CrossCursor);  m_editMode = DragNote; return; }
    if (closeToBottom) { v->setCursor(Qt::UpArrowCursor); m_editMode = SplitNote; return; }

    v->setCursor(Qt::ArrowCursor);

    // std::cerr << "Mouse moved in edit mode over FlexiNoteLayer" << std::endl;
    // v->setCursor(Qt::SizeHorCursor);

}

void
FlexiNoteLayer::getRelativeMousePosition(View *v, FlexiNoteModel::Point &note, int x, int y, bool &closeToLeft, bool &closeToRight, bool &closeToTop, bool &closeToBottom) const
{
    // GF: TODO: consoloidate the tolerance values
    if (!m_model) return;

    int ctol = 0;
    int noteStartX = v->getXForFrame(note.frame);
    int noteEndX = v->getXForFrame(note.frame + note.duration);
    int noteValueY = getYForValue(v,note.value);
    int noteStartY = noteValueY - (NOTE_HEIGHT / 2);
    int noteEndY = noteValueY + (NOTE_HEIGHT / 2);
    
    bool closeToNote = false;
    
    if (y >= noteStartY-ctol && y <= noteEndY+ctol && x >= noteStartX-ctol && x <= noteEndX+ctol) closeToNote = true;
    if (!closeToNote) return;
    
    int tol = NOTE_HEIGHT / 2;
    
    if (x >= noteStartX - tol && x <= noteStartX + tol) closeToLeft = true;
    if (x >= noteEndX - tol && x <= noteEndX + tol) closeToRight = true;
    if (y >= noteStartY - tol && y <= noteStartY + tol) closeToTop = true;
    if (y >= noteEndY - tol && y <= noteEndY + tol) closeToBottom = true;
    
}


bool
FlexiNoteLayer::editOpen(View *v, QMouseEvent *e)
{
    std::cerr << "Opening note editor dialog" << std::endl;
    if (!m_model) return false;

    FlexiNoteModel::Point note(0);
    if (!getPointToDrag(v, e->x(), e->y(), note)) return false;

//    FlexiNoteModel::Point note = *points.begin();

    ItemEditDialog *dialog = new ItemEditDialog
        (m_model->getSampleRate(),
         ItemEditDialog::ShowTime |
         ItemEditDialog::ShowDuration |
         ItemEditDialog::ShowValue |
         ItemEditDialog::ShowText,
         m_model->getScaleUnits());

    dialog->setFrameTime(note.frame);
    dialog->setValue(note.value);
    dialog->setFrameDuration(note.duration);
    dialog->setText(note.label);

    if (dialog->exec() == QDialog::Accepted) {

        FlexiNoteModel::Point newNote = note;
        newNote.frame = dialog->getFrameTime();
        newNote.value = dialog->getValue();
        newNote.duration = dialog->getFrameDuration();
        newNote.label = dialog->getText();
        
        FlexiNoteModel::EditCommand *command = new FlexiNoteModel::EditCommand
            (m_model, tr("Edit Point"));
        command->deletePoint(note);
        command->addPoint(newNote);
        finish(command);
    }

    delete dialog;
    return true;
}

void
FlexiNoteLayer::moveSelection(Selection s, size_t newStartFrame)
{
    if (!m_model) return;

    FlexiNoteModel::EditCommand *command =
    new FlexiNoteModel::EditCommand(m_model, tr("Drag Selection"));

    FlexiNoteModel::PointList points =
    m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (FlexiNoteModel::PointList::iterator i = points.begin();
     i != points.end(); ++i) {

    if (s.contains(i->frame)) {
        FlexiNoteModel::Point newPoint(*i);
        newPoint.frame = i->frame + newStartFrame - s.getStartFrame();
        command->deletePoint(*i);
        command->addPoint(newPoint);
    }
    }

    finish(command);
}

void
FlexiNoteLayer::resizeSelection(Selection s, Selection newSize)
{
    if (!m_model) return;

    FlexiNoteModel::EditCommand *command =
    new FlexiNoteModel::EditCommand(m_model, tr("Resize Selection"));

    FlexiNoteModel::PointList points =
    m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    double ratio =
    double(newSize.getEndFrame() - newSize.getStartFrame()) /
    double(s.getEndFrame() - s.getStartFrame());

    for (FlexiNoteModel::PointList::iterator i = points.begin();
     i != points.end(); ++i) {

    if (s.contains(i->frame)) {

        double targetStart = i->frame;
        targetStart = newSize.getStartFrame() + 
        double(targetStart - s.getStartFrame()) * ratio;

        double targetEnd = i->frame + i->duration;
        targetEnd = newSize.getStartFrame() +
        double(targetEnd - s.getStartFrame()) * ratio;

        FlexiNoteModel::Point newPoint(*i);
        newPoint.frame = lrint(targetStart);
        newPoint.duration = lrint(targetEnd - targetStart);
        command->deletePoint(*i);
        command->addPoint(newPoint);
    }
    }

    finish(command);
}

void
FlexiNoteLayer::deleteSelection(Selection s)
{
    if (!m_model) return;

    FlexiNoteModel::EditCommand *command =
    new FlexiNoteModel::EditCommand(m_model, tr("Delete Selected Points"));

    FlexiNoteModel::PointList points =
    m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (FlexiNoteModel::PointList::iterator i = points.begin();
     i != points.end(); ++i) {

        if (s.contains(i->frame)) {
            command->deletePoint(*i);
        }
    }

    finish(command);
}    

void
FlexiNoteLayer::copy(View *v, Selection s, Clipboard &to)
{
    if (!m_model) return;

    FlexiNoteModel::PointList points =
    m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (FlexiNoteModel::PointList::iterator i = points.begin();
     i != points.end(); ++i) {
    if (s.contains(i->frame)) {
            Clipboard::Point point(i->frame, i->value, i->duration, i->level, i->label);
            point.setReferenceFrame(alignToReference(v, i->frame));
            to.addPoint(point);
        }
    }
}

bool
FlexiNoteLayer::paste(View *v, const Clipboard &from, int frameOffset, bool /* interactive */)
{
    if (!m_model) return false;

    const Clipboard::PointList &points = from.getPoints();

    bool realign = false;

    if (clipboardHasDifferentAlignment(v, from)) {

        QMessageBox::StandardButton button =
            QMessageBox::question(v, tr("Re-align pasted items?"),
                                  tr("The items you are pasting came from a layer with different source material from this one.  Do you want to re-align them in time, to match the source material for this layer?"),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                  QMessageBox::Yes);

        if (button == QMessageBox::Cancel) {
            return false;
        }

        if (button == QMessageBox::Yes) {
            realign = true;
        }
    }

    FlexiNoteModel::EditCommand *command =
    new FlexiNoteModel::EditCommand(m_model, tr("Paste"));

    for (Clipboard::PointList::const_iterator i = points.begin();
         i != points.end(); ++i) {
        
        if (!i->haveFrame()) continue;
        size_t frame = 0;

        if (!realign) {
            
            frame = i->getFrame();

        } else {

            if (i->haveReferenceFrame()) {
                frame = i->getReferenceFrame();
                frame = alignFromReference(v, frame);
            } else {
                frame = i->getFrame();
            }
        }

        FlexiNoteModel::Point newPoint(frame);
  
        if (i->haveLabel()) newPoint.label = i->getLabel();
        if (i->haveValue()) newPoint.value = i->getValue();
        else newPoint.value = (m_model->getValueMinimum() +
                               m_model->getValueMaximum()) / 2;
        if (i->haveLevel()) newPoint.level = i->getLevel();
        if (i->haveDuration()) newPoint.duration = i->getDuration();
        else {
            size_t nextFrame = frame;
            Clipboard::PointList::const_iterator j = i;
            for (; j != points.end(); ++j) {
                if (!j->haveFrame()) continue;
                if (j != i) break;
            }
            if (j != points.end()) {
                nextFrame = j->getFrame();
            }
            if (nextFrame == frame) {
                newPoint.duration = m_model->getResolution();
            } else {
                newPoint.duration = nextFrame - frame;
            }
        }
        
        command->addPoint(newPoint);
    }

    finish(command);
    return true;
}

void
FlexiNoteLayer::addNoteOn(long frame, int pitch, int velocity)
{
    m_pendingNoteOns.insert(FlexiNote(frame, pitch, 0, float(velocity) / 127.0, ""));
}

void
FlexiNoteLayer::addNoteOff(long frame, int pitch)
{
    for (FlexiNoteSet::iterator i = m_pendingNoteOns.begin();
         i != m_pendingNoteOns.end(); ++i) {
        if (lrintf((*i).value) == pitch) {
            FlexiNote note(*i);
            m_pendingNoteOns.erase(i);
            note.duration = frame - note.frame;
            if (m_model) {
                FlexiNoteModel::AddPointCommand *c = new FlexiNoteModel::AddPointCommand
                    (m_model, note, tr("Record FlexiNote"));
                // execute and bundle:
                CommandHistory::getInstance()->addCommand(c, true, true);
            }
            break;
        }
    }
}

void
FlexiNoteLayer::abandonNoteOns()
{
    m_pendingNoteOns.clear();
}

int
FlexiNoteLayer::getDefaultColourHint(bool darkbg, bool &impose)
{
    impose = false;
    return ColourDatabase::getInstance()->getColourIndex
        (QString(darkbg ? "White" : "Black"));
}

void
FlexiNoteLayer::toXml(QTextStream &stream,
                 QString indent, QString extraAttributes) const
{
    SingleColourLayer::toXml(stream, indent, extraAttributes +
                             QString(" verticalScale=\"%1\" scaleMinimum=\"%2\" scaleMaximum=\"%3\" ")
                             .arg(m_verticalScale)
                             .arg(m_scaleMinimum)
                             .arg(m_scaleMaximum));
}

void
FlexiNoteLayer::setProperties(const QXmlAttributes &attributes)
{
    SingleColourLayer::setProperties(attributes);

    bool ok, alsoOk;
    VerticalScale scale = (VerticalScale)
    attributes.value("verticalScale").toInt(&ok);
    if (ok) setVerticalScale(scale);

    float min = attributes.value("scaleMinimum").toFloat(&ok);
    float max = attributes.value("scaleMaximum").toFloat(&alsoOk);
    if (ok && alsoOk) setDisplayExtents(min, max);
}

void
FlexiNoteLayer::setVerticalRangeToNoteRange()
{
    float minf = std::numeric_limits<float>::max();;
    float maxf = 0;
    for (FlexiNoteModel::PointList::const_iterator i = m_model->getPoints().begin();
         i != m_model->getPoints().end(); ++i) {
             FlexiNote note = *i;
             if (note.value < minf) minf = note.value;
             else if (note.value > maxf) maxf = note.value;
             std::cerr << "min frequency:" << minf << ", max frequency: " << maxf << std::endl;
    }
    
	if (this) {
            setDisplayExtents(minf,maxf);
    }
}


