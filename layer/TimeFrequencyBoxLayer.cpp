/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "TimeFrequencyBoxLayer.h"

#include "data/model/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/LogRange.h"

#include "ColourDatabase.h"
#include "ColourMapper.h"
#include "LinearNumericalScale.h"
#include "LogNumericalScale.h"
#include "PaintAssistant.h"

#include "view/View.h"

#include "data/model/TimeFrequencyBoxModel.h"

#include "widgets/ItemEditDialog.h"
#include "widgets/TextAbbrev.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QTextStream>
#include <QMessageBox>

#include <iostream>
#include <cmath>

TimeFrequencyBoxLayer::TimeFrequencyBoxLayer() :
    SingleColourLayer(),
    m_editing(false),
    m_dragPointX(0),
    m_dragPointY(0),
    m_dragStartX(0),
    m_dragStartY(0),
    m_originalPoint(0, 0.0, 0, tr("New Box")),
    m_editingPoint(0, 0.0, 0, tr("New Box")),
    m_editingCommand(nullptr)
{
    
}

int
TimeFrequencyBoxLayer::getCompletion(LayerGeometryProvider *) const
{
    auto model = ModelById::get(m_model);
    if (model) return model->getCompletion();
    else return 0;
}

void
TimeFrequencyBoxLayer::setModel(ModelId modelId)
{
    auto oldModel = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    auto newModel = ModelById::getAs<TimeFrequencyBoxModel>(modelId);
    
    if (!modelId.isNone() && !newModel) {
        throw std::logic_error("Not a TimeFrequencyBoxModel");
    }
    
    if (m_model == modelId) return;
    m_model = modelId;

    if (newModel) {
        connectSignals(m_model);
    }
    
    emit modelReplaced();
}

Layer::PropertyList
TimeFrequencyBoxLayer::getProperties() const
{
    PropertyList list = SingleColourLayer::getProperties();
    list.push_back("Vertical Scale");
    return list;
}

QString
TimeFrequencyBoxLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Vertical Scale") return tr("Vertical Scale");
    return SingleColourLayer::getPropertyLabel(name);
}

Layer::PropertyType
TimeFrequencyBoxLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Vertical Scale") return ValueProperty;
    return SingleColourLayer::getPropertyType(name);
}

QString
TimeFrequencyBoxLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Vertical Scale") {
        return tr("Scale");
    }
    return SingleColourLayer::getPropertyGroupName(name);
}

int
TimeFrequencyBoxLayer::getPropertyRangeAndValue(const PropertyName &name,
                                    int *min, int *max, int *deflt) const
{
    int val = 0;

    if (name == "Vertical Scale") {
        
        if (min) *min = 0;
        if (max) *max = 2;
        if (deflt) *deflt = int(LinearScale);
        
        val = int(m_verticalScale);

    } else {
        val = SingleColourLayer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
TimeFrequencyBoxLayer::getPropertyValueLabel(const PropertyName &name,
                                   int value) const
{
    if (name == "Vertical Scale") {
        switch (value) {
        default:
        case 0: return tr("Auto-Align");
        case 1: return tr("Linear");
        case 2: return tr("Log");
        }
    }
    return SingleColourLayer::getPropertyValueLabel(name, value);
}

void
TimeFrequencyBoxLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Vertical Scale") {
        setVerticalScale(VerticalScale(value));
    } else {
        return SingleColourLayer::setProperty(name, value);
    }
}

void
TimeFrequencyBoxLayer::setVerticalScale(VerticalScale scale)
{
    if (m_verticalScale == scale) return;
    m_verticalScale = scale;
    emit layerParametersChanged();
}

bool
TimeFrequencyBoxLayer::isLayerScrollable(const LayerGeometryProvider *v) const
{
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

bool
TimeFrequencyBoxLayer::getValueExtents(double &min, double &max,
                                       bool &logarithmic, QString &unit) const
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model) return false;
    min = model->getFrequencyMinimum();
    max = model->getFrequencyMaximum();
    unit = getScaleUnits();

    if (m_verticalScale == LogScale) logarithmic = true;

    return true;
}

bool
TimeFrequencyBoxLayer::getDisplayExtents(double &min, double &max) const
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model || m_verticalScale == AutoAlignScale) return false;

    min = model->getFrequencyMinimum();
    max = model->getFrequencyMaximum();

    return true;
}

EventVector
TimeFrequencyBoxLayer::getLocalPoints(LayerGeometryProvider *v, int x) const
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model) return EventVector();

    sv_frame_t frame = v->getFrameForX(x);

    EventVector local = model->getEventsCovering(frame);
    if (!local.empty()) return local;

    int fuzz = ViewManager::scalePixelSize(2);
    sv_frame_t start = v->getFrameForX(x - fuzz);
    sv_frame_t end = v->getFrameForX(x + fuzz);

    local = model->getEventsStartingWithin(frame, end - frame);
    if (!local.empty()) return local;

    local = model->getEventsSpanning(start, frame - start);
    if (!local.empty()) return local;

    return {};
}

bool
TimeFrequencyBoxLayer::getPointToDrag(LayerGeometryProvider *v, int x, int y,
                                      Event &point) const
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model) return false;

    sv_frame_t frame = v->getFrameForX(x);

    EventVector onPoints = model->getEventsCovering(frame);
    if (onPoints.empty()) return false;

    int nearestDistance = -1;
    for (const auto &p: onPoints) {
        int distance = getYForValue(v, p.getValue()) - y;
        if (distance < 0) distance = -distance;
        if (nearestDistance == -1 || distance < nearestDistance) {
            nearestDistance = distance;
            point = p;
        }
    }

    return true;
}

QString
TimeFrequencyBoxLayer::getLabelPreceding(sv_frame_t frame) const
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model) return "";
    EventVector points = model->getEventsStartingWithin
        (model->getStartFrame(), frame - model->getStartFrame());
    if (!points.empty()) {
        for (auto i = points.rbegin(); i != points.rend(); ++i) {
            if (i->getLabel() != QString()) {
                return i->getLabel();
            }
        }
    }
    return QString();
}

QString
TimeFrequencyBoxLayer::getFeatureDescription(LayerGeometryProvider *v,
                                             QPoint &pos) const
{
    int x = pos.x();

    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model || !model->getSampleRate()) return "";

    EventVector points = getLocalPoints(v, x);

    if (points.empty()) {
        if (!model->isReady()) {
            return tr("In progress");
        } else {
            return tr("No local points");
        }
    }

    Event box;
    EventVector::iterator i;

    for (i = points.begin(); i != points.end(); ++i) {

        int y0 = getYForValue(v, i->getValue());
        int y1 = getYForValue(v, i->getValue() + fabsf(i->getLevel()));

        if (pos.y() >= y0 && pos.y() <= y1) {
            box = *i;
            break;
        }
    }

    if (i == points.end()) return tr("No local points");

    RealTime rt = RealTime::frame2RealTime(box.getFrame(),
                                           model->getSampleRate());
    RealTime rd = RealTime::frame2RealTime(box.getDuration(),
                                           model->getSampleRate());
    
    QString rangeText;

    rangeText = tr("%1 %2 - %3 %4")
        .arg(box.getValue()).arg(getScaleUnits())
        .arg(box.getValue() + fabsf(box.getLevel())).arg(getScaleUnits());

    QString text;

    if (box.getLabel() == "") {
        text = QString(tr("Time:\t%1\nDuration:\t%2\nFrequency:\t%3\nNo label"))
            .arg(rt.toText(true).c_str())
            .arg(rd.toText(true).c_str())
            .arg(rangeText);
    } else {
        text = QString(tr("Time:\t%1\nDuration:\t%2\nFrequency:\t%3\nLabel:\t%4"))
            .arg(rt.toText(true).c_str())
            .arg(rd.toText(true).c_str())
            .arg(rangeText)
            .arg(box.getLabel());
    }

    pos = QPoint(v->getXForFrame(box.getFrame()),
                 getYForValue(v, box.getValue()));
    return text;
}

bool
TimeFrequencyBoxLayer::snapToFeatureFrame(LayerGeometryProvider *v,
                                          sv_frame_t &frame,
                                          int &resolution,
                                          SnapType snap) const
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model) {
        return Layer::snapToFeatureFrame(v, frame, resolution, snap);
    }

    // SnapLeft / SnapRight: return frame of nearest feature in that
    // direction no matter how far away
    //
    // SnapNeighbouring: return frame of feature that would be used in
    // an editing operation, i.e. closest feature in either direction
    // but only if it is "close enough"

    resolution = model->getResolution();

    if (snap == SnapNeighbouring) {
        EventVector points = getLocalPoints(v, v->getXForFrame(frame));
        if (points.empty()) return false;
        frame = points.begin()->getFrame();
        return true;
    }    

    // Normally we snap to the start frame of whichever event we
    // find. However here, for SnapRight only, if the end frame of
    // whichever event we would have snapped to had we been snapping
    // left is closer than the start frame of the next event to the
    // right, then we snap to that frame instead. Clear?
    
    Event left;
    bool haveLeft = false;
    if (model->getNearestEventMatching
        (frame, [](Event) { return true; }, EventSeries::Backward, left)) {
        haveLeft = true;
    }

    if (snap == SnapLeft) {
        frame = left.getFrame();
        return haveLeft;
    }

    Event right;
    bool haveRight = false;
    if (model->getNearestEventMatching
        (frame, [](Event) { return true; }, EventSeries::Forward, right)) {
        haveRight = true;
    }

    if (haveLeft) {
        sv_frame_t leftEnd = left.getFrame() + left.getDuration();
        if (leftEnd > frame) {
            if (haveRight) {
                if (leftEnd - frame < right.getFrame() - frame) {
                    frame = leftEnd;
                } else {
                    frame = right.getFrame();
                }
            } else {
                frame = leftEnd;
            }
            return true;
        }
    }

    if (haveRight) {
        frame = right.getFrame();
        return true;
    }

    return false;
}

QString
TimeFrequencyBoxLayer::getScaleUnits() const
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (model) return model->getScaleUnits();
    else return "";
}

void
TimeFrequencyBoxLayer::getScaleExtents(LayerGeometryProvider *v,
                                       double &min, double &max,
                                       bool &log) const
{
    min = 0.0;
    max = 0.0;
    log = false;

    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model) return;

    QString queryUnits;
    queryUnits = getScaleUnits();

    if (m_verticalScale == AutoAlignScale) {

        if (!v->getValueExtents(queryUnits, min, max, log)) {

            min = model->getFrequencyMinimum();
            max = model->getFrequencyMaximum();

//            cerr << "TimeFrequencyBoxLayer[" << this << "]::getScaleExtents: min = " << min << ", max = " << max << ", log = " << log << endl;

        } else if (log) {

            LogRange::mapRange(min, max);

//            cerr << "TimeFrequencyBoxLayer[" << this << "]::getScaleExtents: min = " << min << ", max = " << max << ", log = " << log << endl;

        }

    } else {

        min = model->getFrequencyMinimum();
        max = model->getFrequencyMaximum();

        if (m_verticalScale == LogScale) {
            LogRange::mapRange(min, max);
            log = true;
        }
    }

    if (max == min) max = min + 1.0;
}

int
TimeFrequencyBoxLayer::getYForValue(LayerGeometryProvider *v, double val) const
{
    double min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->getPaintHeight();

    getScaleExtents(v, min, max, logarithmic);

//    cerr << "TimeFrequencyBoxLayer[" << this << "]::getYForValue(" << val << "): min = " << min << ", max = " << max << ", log = " << logarithmic << endl;
//    cerr << "h = " << h << ", margin = " << margin << endl;

    if (logarithmic) {
        val = LogRange::map(val);
    }

    return int(h - ((val - min) * h) / (max - min));
}

double
TimeFrequencyBoxLayer::getValueForY(LayerGeometryProvider *v, int y) const
{
    double min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->getPaintHeight();

    getScaleExtents(v, min, max, logarithmic);

    double val = min + (double(h - y) * double(max - min)) / h;

    if (logarithmic) {
        val = pow(10.0, val);
    }

    return val;
}

void
TimeFrequencyBoxLayer::paint(LayerGeometryProvider *v, QPainter &paint,
                             QRect rect) const
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model || !model->isOK()) return;

    sv_samplerate_t sampleRate = model->getSampleRate();
    if (!sampleRate) return;

//    Profiler profiler("TimeFrequencyBoxLayer::paint", true);

    int x0 = rect.left() - 40, x1 = rect.right();

    sv_frame_t wholeFrame0 = v->getFrameForX(0);
    sv_frame_t wholeFrame1 = v->getFrameForX(v->getPaintWidth());

    EventVector points(model->getEventsSpanning(wholeFrame0,
                                                wholeFrame1 - wholeFrame0));
    if (points.empty()) return;

    paint.setPen(getBaseQColor());

    QColor brushColour(getBaseQColor());
    brushColour.setAlpha(80);

//    SVDEBUG << "TimeFrequencyBoxLayer::paint: resolution is "
//              << model->getResolution() << " frames" << endl;

    double min = model->getFrequencyMinimum();
    double max = model->getFrequencyMaximum();
    if (max == min) max = min + 1.0;

    QPoint localPos;
    Event illuminatePoint(0);
    bool shouldIlluminate = false;

    if (v->shouldIlluminateLocalFeatures(this, localPos)) {
        shouldIlluminate = getPointToDrag(v, localPos.x(), localPos.y(),
                                          illuminatePoint);
    }

    paint.save();
    paint.setRenderHint(QPainter::Antialiasing, false);

    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {

        const Event &p(*i);

        int x = v->getXForFrame(p.getFrame());
        int w = v->getXForFrame(p.getFrame() + p.getDuration()) - x;
        int y = getYForValue(v, p.getValue());
        int h = getYForValue(v, p.getValue() + fabsf(p.getLevel()));
        int ex = x + w;

        int gap = v->scalePixelSize(2);
        
        EventVector::const_iterator j = i;
        ++j;

        if (j != points.end()) {
            const Event &q(*j);
            int nx = v->getXForFrame(q.getFrame());
            if (nx < ex) ex = nx;
        }

        if (w < 1) w = 1;
        if (h < 1) h = 1;

        paint.setPen(getBaseQColor());
        paint.setBrush(brushColour);

        if (shouldIlluminate && illuminatePoint == p) {

            paint.setPen(v->getForeground());
            paint.setBrush(v->getForeground());
                
            // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
            // replacement (horizontalAdvance) was only added in Qt 5.11
            // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

            QString vlabel =
                QString("%1%2 - %3%4")
                .arg(p.getValue()).arg(getScaleUnits())
                .arg(p.getValue() + fabsf(p.getLevel())).arg(getScaleUnits());
            PaintAssistant::drawVisibleText
                (v, paint, 
                 x - paint.fontMetrics().width(vlabel) - gap,
                 y + paint.fontMetrics().height()/2
                 - paint.fontMetrics().descent(), 
                 vlabel, PaintAssistant::OutlinedText);
                
            QString hlabel = RealTime::frame2RealTime
                (p.getFrame(), model->getSampleRate()).toText(true).c_str();
            PaintAssistant::drawVisibleText
                (v, paint, 
                 x,
                 y - h/2 - paint.fontMetrics().descent() - gap,
                 hlabel, PaintAssistant::OutlinedText);
        }

        paint.drawRect(x, y, w, h);
    }

    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {

        const Event &p(*i);

        int x = v->getXForFrame(p.getFrame());
        int w = v->getXForFrame(p.getFrame() + p.getDuration()) - x;
        int y = getYForValue(v, p.getValue());

        QString label = p.getLabel();
        if (label == "") {
            label =
                QString("%1%2 - %3%4")
                .arg(p.getValue()).arg(getScaleUnits())
                .arg(p.getValue() + fabsf(p.getLevel())).arg(getScaleUnits());
        }
        int labelWidth = paint.fontMetrics().width(label);

        int gap = v->scalePixelSize(2);

        if (x + w < x0 || x - labelWidth - gap > x1) {
            continue;
        }
        
        bool illuminated = false;

        if (shouldIlluminate && illuminatePoint == p) {
            illuminated = true;
        }

        if (!illuminated) {

            int labelX, labelY;

            labelX = x - labelWidth - gap;
            labelY = y + paint.fontMetrics().height()/2 
                - paint.fontMetrics().descent();

            PaintAssistant::drawVisibleText(v, paint, labelX, labelY, label,
                                            PaintAssistant::OutlinedText);
        }
    }

    paint.restore();
}

int
TimeFrequencyBoxLayer::getVerticalScaleWidth(LayerGeometryProvider *v,
                                             bool, QPainter &paint) const
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model || m_verticalScale == AutoAlignScale) {
        return 0;
    } else {
        if (m_verticalScale == LogScale) {
            return LogNumericalScale().getWidth(v, paint);
        } else {
            return LinearNumericalScale().getWidth(v, paint);
        }
    }
}

void
TimeFrequencyBoxLayer::paintVerticalScale(LayerGeometryProvider *v,
                                          bool, QPainter &paint, QRect) const
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model || model->isEmpty()) return;

    QString unit;
    double min, max;
    bool logarithmic;

    int w = getVerticalScaleWidth(v, false, paint);

    getScaleExtents(v, min, max, logarithmic);

    if (logarithmic) {
        LogNumericalScale().paintVertical(v, this, paint, 0, min, max);
    } else {
        LinearNumericalScale().paintVertical(v, this, paint, 0, min, max);
    }
        
    if (getScaleUnits() != "") {
        int mw = w - 5;
        paint.drawText(5,
                       5 + paint.fontMetrics().ascent(),
                       TextAbbrev::abbreviate(getScaleUnits(),
                                              paint.fontMetrics(),
                                              mw));
    }
}

//!!! All of the editing operations still need to be updated for
//!!! vertical frequency range instead of just value

void
TimeFrequencyBoxLayer::drawStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double value = getValueForY(v, e->y());

    m_editingPoint = Event(frame, float(value), 0, "");
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) finish(m_editingCommand);
    m_editingCommand = new ChangeEventsCommand(m_model.untyped,
                                               tr("Draw Time-Frequency Box"));
    m_editingCommand->add(m_editingPoint);

    m_editing = true;
}

void
TimeFrequencyBoxLayer::drawDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model || !m_editing) return;

    sv_frame_t frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double newValue = getValueForY(v, e->y());

    sv_frame_t newFrame = m_editingPoint.getFrame();
    sv_frame_t newDuration = frame - newFrame;
    if (newDuration < 0) {
        newFrame = frame;
        newDuration = -newDuration;
    } else if (newDuration == 0) {
        newDuration = 1;
    }

    m_editingCommand->remove(m_editingPoint);
    m_editingPoint = m_editingPoint
        .withFrame(newFrame)
        .withValue(float(newValue))
        .withDuration(newDuration);
    m_editingCommand->add(m_editingPoint);
}

void
TimeFrequencyBoxLayer::drawEnd(LayerGeometryProvider *, QMouseEvent *)
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model || !m_editing) return;
    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;
}

void
TimeFrequencyBoxLayer::eraseStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model) return;

    if (!getPointToDrag(v, e->x(), e->y(), m_editingPoint)) return;

    if (m_editingCommand) {
        finish(m_editingCommand);
        m_editingCommand = nullptr;
    }

    m_editing = true;
}

void
TimeFrequencyBoxLayer::eraseDrag(LayerGeometryProvider *, QMouseEvent *)
{
}

void
TimeFrequencyBoxLayer::eraseEnd(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model || !m_editing) return;

    m_editing = false;

    Event p(0);
    if (!getPointToDrag(v, e->x(), e->y(), p)) return;
    if (p.getFrame() != m_editingPoint.getFrame() ||
        p.getValue() != m_editingPoint.getValue()) return;

    m_editingCommand = new ChangeEventsCommand
        (m_model.untyped, tr("Erase Time-Frequency Box"));

    m_editingCommand->remove(m_editingPoint);

    finish(m_editingCommand);
    m_editingCommand = nullptr;
    m_editing = false;
}

void
TimeFrequencyBoxLayer::editStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model) return;

    if (!getPointToDrag(v, e->x(), e->y(), m_editingPoint)) {
        return;
    }

    m_dragPointX = v->getXForFrame(m_editingPoint.getFrame());
    m_dragPointY = getYForValue(v, m_editingPoint.getValue());

    m_originalPoint = m_editingPoint;

    if (m_editingCommand) {
        finish(m_editingCommand);
        m_editingCommand = nullptr;
    }

    m_editing = true;
    m_dragStartX = e->x();
    m_dragStartY = e->y();
}

void
TimeFrequencyBoxLayer::editDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model || !m_editing) return;

    int xdist = e->x() - m_dragStartX;
    int ydist = e->y() - m_dragStartY;
    int newx = m_dragPointX + xdist;
    int newy = m_dragPointY + ydist;

    sv_frame_t frame = v->getFrameForX(newx);
    if (frame < 0) frame = 0;
    frame = frame / model->getResolution() * model->getResolution();

    double value = getValueForY(v, newy);

    if (!m_editingCommand) {
        m_editingCommand = new ChangeEventsCommand
            (m_model.untyped,
             tr("Drag Time-Frequency Box"));
    }

    m_editingCommand->remove(m_editingPoint);
    m_editingPoint = m_editingPoint
        .withFrame(frame)
        .withValue(float(value));
    m_editingCommand->add(m_editingPoint);
}

void
TimeFrequencyBoxLayer::editEnd(LayerGeometryProvider *, QMouseEvent *)
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model || !m_editing) return;

    if (m_editingCommand) {

        QString newName = m_editingCommand->getName();

        if (m_editingPoint.getFrame() != m_originalPoint.getFrame()) {
            if (m_editingPoint.getValue() != m_originalPoint.getValue()) {
                newName = tr("Edit Time-Frequency Box");
            } else {
                newName = tr("Relocate Time-Frequency Box");
            }
        } else {
            newName = tr("Change Point Value");
        }

        m_editingCommand->setName(newName);
        finish(m_editingCommand);
    }

    m_editingCommand = nullptr;
    m_editing = false;
}

bool
TimeFrequencyBoxLayer::editOpen(LayerGeometryProvider *v, QMouseEvent *e)
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model) return false;

    Event region(0);
    if (!getPointToDrag(v, e->x(), e->y(), region)) return false;

    ItemEditDialog *dialog = new ItemEditDialog
        (model->getSampleRate(),
         ItemEditDialog::ShowTime |
         ItemEditDialog::ShowDuration |
         ItemEditDialog::ShowValue |
         ItemEditDialog::ShowText,
         getScaleUnits());

    dialog->setFrameTime(region.getFrame());
    dialog->setValue(region.getValue());
    dialog->setFrameDuration(region.getDuration());
    dialog->setText(region.getLabel());

    if (dialog->exec() == QDialog::Accepted) {

        Event newTimeFrequencyBox = region
            .withFrame(dialog->getFrameTime())
            .withValue(dialog->getValue())
            .withDuration(dialog->getFrameDuration())
            .withLabel(dialog->getText());
        
        ChangeEventsCommand *command = new ChangeEventsCommand
            (m_model.untyped, tr("Edit Time-Frequency Box"));
        command->remove(region);
        command->add(newTimeFrequencyBox);
        finish(command);
    }

    delete dialog;
    return true;
}

void
TimeFrequencyBoxLayer::moveSelection(Selection s, sv_frame_t newStartFrame)
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Drag Selection"));

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (EventVector::iterator i = points.begin();
         i != points.end(); ++i) {

        Event newPoint = (*i)
            .withFrame(i->getFrame() + newStartFrame - s.getStartFrame());
        command->remove(*i);
        command->add(newPoint);
    }

    finish(command);
}

void
TimeFrequencyBoxLayer::resizeSelection(Selection s, Selection newSize)
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model || !s.getDuration()) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Resize Selection"));

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    double ratio = double(newSize.getDuration()) / double(s.getDuration());
    double oldStart = double(s.getStartFrame());
    double newStart = double(newSize.getStartFrame());
    
    for (Event p: points) {

        double newFrame = (double(p.getFrame()) - oldStart) * ratio + newStart;
        double newDuration = double(p.getDuration()) * ratio;

        Event newPoint = p
            .withFrame(lrint(newFrame))
            .withDuration(lrint(newDuration));
        command->remove(p);
        command->add(newPoint);
    }

    finish(command);
}

void
TimeFrequencyBoxLayer::deleteSelection(Selection s)
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model) return;

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Delete Selected Points"));

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (EventVector::iterator i = points.begin();
         i != points.end(); ++i) {

        if (s.contains(i->getFrame())) {
            command->remove(*i);
        }
    }

    finish(command);
}    

void
TimeFrequencyBoxLayer::copy(LayerGeometryProvider *v, Selection s, Clipboard &to)
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model) return;

    EventVector points =
        model->getEventsStartingWithin(s.getStartFrame(), s.getDuration());

    for (Event p: points) {
        to.addPoint(p.withReferenceFrame(alignToReference(v, p.getFrame())));
    }
}

bool
TimeFrequencyBoxLayer::paste(LayerGeometryProvider *v, const Clipboard &from, sv_frame_t /* frameOffset */, bool /* interactive */)
{
    auto model = ModelById::getAs<TimeFrequencyBoxModel>(m_model);
    if (!model) return false;

    const EventVector &points = from.getPoints();

    bool realign = false;

    if (clipboardHasDifferentAlignment(v, from)) {

        QMessageBox::StandardButton button =
            QMessageBox::question(v->getView(), tr("Re-align pasted items?"),
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

    ChangeEventsCommand *command =
        new ChangeEventsCommand(m_model.untyped, tr("Paste"));

    for (EventVector::const_iterator i = points.begin();
         i != points.end(); ++i) {
        
        sv_frame_t frame = 0;

        if (!realign) {
            
            frame = i->getFrame();

        } else {

            if (i->hasReferenceFrame()) {
                frame = i->getReferenceFrame();
                frame = alignFromReference(v, frame);
            } else {
                frame = i->getFrame();
            }
        }

        Event p = *i;
        Event newPoint = p;
        if (!p.hasValue()) {
            newPoint = newPoint.withValue((model->getFrequencyMinimum() +
                                           model->getFrequencyMaximum()) / 2);
        }
        if (!p.hasDuration()) {
            sv_frame_t nextFrame = frame;
            EventVector::const_iterator j = i;
            for (; j != points.end(); ++j) {
                if (j != i) break;
            }
            if (j != points.end()) {
                nextFrame = j->getFrame();
            }
            if (nextFrame == frame) {
                newPoint = newPoint.withDuration(model->getResolution());
            } else {
                newPoint = newPoint.withDuration(nextFrame - frame);
            }
        }
        
        command->add(newPoint);
    }

    finish(command);
    return true;
}

void
TimeFrequencyBoxLayer::toXml(QTextStream &stream,
                 QString indent, QString extraAttributes) const
{
    QString s;

    s += QString("verticalScale=\"%1\" ")
        .arg(m_verticalScale);
    
    SingleColourLayer::toXml(stream, indent, extraAttributes + " " + s);
}

void
TimeFrequencyBoxLayer::setProperties(const QXmlAttributes &attributes)
{
    SingleColourLayer::setProperties(attributes);

    bool ok;
    VerticalScale scale = (VerticalScale)
        attributes.value("verticalScale").toInt(&ok);
    if (ok) setVerticalScale(scale);
}


