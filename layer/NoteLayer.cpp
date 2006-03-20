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

#include "NoteLayer.h"

#include "base/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/Pitch.h"
#include "base/View.h"

#include "model/NoteModel.h"

#include "SpectrogramLayer.h" // for optional frequency alignment

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

#include <iostream>
#include <cmath>

NoteLayer::NoteLayer() :
    Layer(),
    m_model(0),
    m_editing(false),
    m_originalPoint(0, 0.0, 0, tr("New Point")),
    m_editingPoint(0, 0.0, 0, tr("New Point")),
    m_editingCommand(0),
    m_colour(Qt::black),
    m_verticalScale(MinMaxRangeScale)
{
    
}

void
NoteLayer::setModel(NoteModel *model)
{
    if (m_model == model) return;
    m_model = model;

    connect(m_model, SIGNAL(modelChanged()), this, SIGNAL(modelChanged()));
    connect(m_model, SIGNAL(modelChanged(size_t, size_t)),
	    this, SIGNAL(modelChanged(size_t, size_t)));

    connect(m_model, SIGNAL(completionChanged()),
	    this, SIGNAL(modelCompletionChanged()));

    std::cerr << "NoteLayer::setModel(" << model << ")" << std::endl;

    emit modelReplaced();
}

Layer::PropertyList
NoteLayer::getProperties() const
{
    PropertyList list;
    list.push_back(tr("Colour"));
    list.push_back(tr("Vertical Scale"));
    return list;
}

Layer::PropertyType
NoteLayer::getPropertyType(const PropertyName &) const
{
    return ValueProperty;
}

int
NoteLayer::getPropertyRangeAndValue(const PropertyName &name,
					 int *min, int *max) const
{
    //!!! factor this colour handling stuff out into a colour manager class

    int deft = 0;

    if (name == tr("Colour")) {

	if (min) *min = 0;
	if (max) *max = 5;

	if (m_colour == Qt::black) deft = 0;
	else if (m_colour == Qt::darkRed) deft = 1;
	else if (m_colour == Qt::darkBlue) deft = 2;
	else if (m_colour == Qt::darkGreen) deft = 3;
	else if (m_colour == QColor(200, 50, 255)) deft = 4;
	else if (m_colour == QColor(255, 150, 50)) deft = 5;

    } else if (name == tr("Vertical Scale")) {
	
	if (min) *min = 0;
	if (max) *max = 2;
	
	deft = int(m_verticalScale);

    } else {
	
	deft = Layer::getPropertyRangeAndValue(name, min, max);
    }

    return deft;
}

QString
NoteLayer::getPropertyValueLabel(const PropertyName &name,
				    int value) const
{
    if (name == tr("Colour")) {
	switch (value) {
	default:
	case 0: return tr("Black");
	case 1: return tr("Red");
	case 2: return tr("Blue");
	case 3: return tr("Green");
	case 4: return tr("Purple");
	case 5: return tr("Orange");
	}
    } else if (name == tr("Vertical Scale")) {
	switch (value) {
	default:
	case 0: return tr("Note Range In Use");
	case 1: return tr("MIDI Note Range");
	case 2: return tr("Frequency");
	}
    }
    return tr("<unknown>");
}

void
NoteLayer::setProperty(const PropertyName &name, int value)
{
    if (name == tr("Colour")) {
	switch (value) {
	default:
	case 0:	setBaseColour(Qt::black); break;
	case 1: setBaseColour(Qt::darkRed); break;
	case 2: setBaseColour(Qt::darkBlue); break;
	case 3: setBaseColour(Qt::darkGreen); break;
	case 4: setBaseColour(QColor(200, 50, 255)); break;
	case 5: setBaseColour(QColor(255, 150, 50)); break;
	}
    } else if (name == tr("Vertical Scale")) {
	setVerticalScale(VerticalScale(value));
    }
}

void
NoteLayer::setBaseColour(QColor colour)
{
    if (m_colour == colour) return;
    m_colour = colour;
    emit layerParametersChanged();
}

void
NoteLayer::setVerticalScale(VerticalScale scale)
{
    if (m_verticalScale == scale) return;
    m_verticalScale = scale;
    emit layerParametersChanged();
}

bool
NoteLayer::isLayerScrollable(const View *v) const
{
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

NoteModel::PointList
NoteLayer::getLocalPoints(View *v, int x) const
{
    if (!m_model) return NoteModel::PointList();

    long frame = v->getFrameForX(x);

    NoteModel::PointList onPoints =
	m_model->getPoints(frame);

    if (!onPoints.empty()) {
	return onPoints;
    }

    NoteModel::PointList prevPoints =
	m_model->getPreviousPoints(frame);
    NoteModel::PointList nextPoints =
	m_model->getNextPoints(frame);

    NoteModel::PointList usePoints = prevPoints;

    if (prevPoints.empty()) {
	usePoints = nextPoints;
    } else if (prevPoints.begin()->frame < v->getStartFrame() &&
	       !(nextPoints.begin()->frame > v->getEndFrame())) {
	usePoints = nextPoints;
    } else if (nextPoints.begin()->frame - frame <
	       frame - prevPoints.begin()->frame) {
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

QString
NoteLayer::getFeatureDescription(View *v, QPoint &pos) const
{
    int x = pos.x();

    if (!m_model || !m_model->getSampleRate()) return "";

    NoteModel::PointList points = getLocalPoints(v, x);

    if (points.empty()) {
	if (!m_model->isReady()) {
	    return tr("In progress");
	} else {
	    return tr("No local points");
	}
    }

    Note note(0);
    NoteModel::PointList::iterator i;

    for (i = points.begin(); i != points.end(); ++i) {

	int y = getYForValue(v, i->value);
	int h = 3;

	if (m_model->getValueQuantization() != 0.0) {
	    h = y - getYForValue(v, i->value + m_model->getValueQuantization());
	    if (h < 3) h = 3;
	}

	if (pos.y() >= y - h && pos.y() <= y) {
	    note = *i;
	    break;
	}
    }

    if (i == points.end()) return tr("No local points");

    RealTime rt = RealTime::frame2RealTime(note.frame,
					   m_model->getSampleRate());
    RealTime rd = RealTime::frame2RealTime(note.duration,
					   m_model->getSampleRate());
    
    QString text;

    if (note.label == "") {
	text = QString(tr("Time:\t%1\nPitch:\t%2\nDuration:\t%3\nNo label"))
	    .arg(rt.toText(true).c_str())
	    .arg(note.value)
	    .arg(rd.toText(true).c_str());
    } else {
	text = QString(tr("Time:\t%1\nPitch:\t%2\nDuration:\t%3\nLabel:\t%4"))
	    .arg(rt.toText(true).c_str())
	    .arg(note.value)
	    .arg(rd.toText(true).c_str())
	    .arg(note.label);
    }

    pos = QPoint(v->getXForFrame(note.frame),
		 getYForValue(v, note.value));
    return text;
}

bool
NoteLayer::snapToFeatureFrame(View *v, int &frame,
			      size_t &resolution,
			      SnapType snap) const
{
    if (!m_model) {
	return Layer::snapToFeatureFrame(v, frame, resolution, snap);
    }

    resolution = m_model->getResolution();
    NoteModel::PointList points;

    if (snap == SnapNeighbouring) {
	
	points = getLocalPoints(v, v->getXForFrame(frame));
	if (points.empty()) return false;
	frame = points.begin()->frame;
	return true;
    }    

    points = m_model->getPoints(frame, frame);
    int snapped = frame;
    bool found = false;

    for (NoteModel::PointList::const_iterator i = points.begin();
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

	    NoteModel::PointList::const_iterator j = i;
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

int
NoteLayer::getYForValue(View *v, float value) const
{
    float min, max, h = v->height();

    switch (m_verticalScale) {

    case MIDIRangeScale:
	min = 0.0;
	max = 127.0;
	break;

    case MinMaxRangeScale:
	min = m_model->getValueMinimum();
	max = m_model->getValueMaximum();
	break;

    case FrequencyScale:
    
	value = Pitch::getFrequencyForPitch(lrintf(value),
					    value - lrintf(value));

	// If we have a spectrogram layer on the same view as us, align
	// ourselves with it...
	for (int i = 0; i < v->getLayerCount(); ++i) {
	    SpectrogramLayer *spectrogram = dynamic_cast<SpectrogramLayer *>
		(v->getLayer(i));
	    if (spectrogram) {
		return spectrogram->getYForFrequency(v, value);
	    }
	}

	// ...otherwise just interpolate
	std::cerr << "FrequencyScale: value in = " << value << std::endl;
	min = m_model->getValueMinimum();
	min = Pitch::getFrequencyForPitch(lrintf(min), min - lrintf(min));
	max = m_model->getValueMaximum();
	max = Pitch::getFrequencyForPitch(lrintf(max), max - lrintf(max));
	std::cerr << "FrequencyScale: min = " << min << ", max = " << max << ", value = " << value << std::endl;
	break;
    }

    if (max < min) max = min;
    max = max + 1.0;

    return int(h - ((value - min) * h) / (max - min)) - 1;
}

float
NoteLayer::getValueForY(View *v, int y) const
{
    float min = m_model->getValueMinimum();
    float max = m_model->getValueMaximum();
    if (max == min) max = min + 1.0;

    int h = v->height();

    return min + (float(h - y) * float(max - min)) / h;
}

void
NoteLayer::paint(View *v, QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) return;

    int sampleRate = m_model->getSampleRate();
    if (!sampleRate) return;

//    Profiler profiler("NoteLayer::paint", true);

    int x0 = rect.left(), x1 = rect.right();
    long frame0 = v->getFrameForX(x0);
    long frame1 = v->getFrameForX(x1);

    NoteModel::PointList points(m_model->getPoints(frame0, frame1));
    if (points.empty()) return;

    paint.setPen(m_colour);

    QColor brushColour(m_colour);
    brushColour.setAlpha(80);

//    std::cerr << "NoteLayer::paint: resolution is "
//	      << m_model->getResolution() << " frames" << std::endl;

    float min = m_model->getValueMinimum();
    float max = m_model->getValueMaximum();
    if (max == min) max = min + 1.0;

    int origin = int(nearbyint(v->height() -
			       (-min * v->height()) / (max - min)));

    QPoint localPos;
    long illuminateFrame = -1;

    if (v->shouldIlluminateLocalFeatures(this, localPos)) {
	NoteModel::PointList localPoints =
	    getLocalPoints(v, localPos.x());
	if (!localPoints.empty()) illuminateFrame = localPoints.begin()->frame;
    }

    paint.save();
    paint.setRenderHint(QPainter::Antialiasing, false);
    
    for (NoteModel::PointList::const_iterator i = points.begin();
	 i != points.end(); ++i) {

	const NoteModel::Point &p(*i);

	int x = v->getXForFrame(p.frame);
	int y = getYForValue(v, p.value);
	int w = v->getXForFrame(p.frame + p.duration) - x;
	int h = 3;
	
	if (m_model->getValueQuantization() != 0.0) {
	    h = y - getYForValue(v, p.value + m_model->getValueQuantization());
	    if (h < 3) h = 3;
	}

	if (w < 1) w = 1;
	paint.setPen(m_colour);
	paint.setBrush(brushColour);

	if (illuminateFrame == p.frame) {
	    if (localPos.y() >= y - h && localPos.y() < y) {
		paint.setPen(Qt::black);//!!!
		paint.setBrush(Qt::black);//!!!
	    }
	}
	
	paint.drawRect(x, y - h, w, h);

///	if (p.label != "") {
///	    paint.drawText(x + 5, y - paint.fontMetrics().height() + paint.fontMetrics().ascent(), p.label);
///	}
    }

    paint.restore();
}

void
NoteLayer::drawStart(View *v, QMouseEvent *e)
{
    std::cerr << "NoteLayer::drawStart(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model) return;

    long frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    float value = getValueForY(v, e->y());

    m_editingPoint = NoteModel::Point(frame, value, 0, tr("New Point"));
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) m_editingCommand->finish();
    m_editingCommand = new NoteModel::EditCommand(m_model,
						  tr("Draw Point"));
    m_editingCommand->addPoint(m_editingPoint);

    m_editing = true;
}

void
NoteLayer::drawDrag(View *v, QMouseEvent *e)
{
    std::cerr << "NoteLayer::drawDrag(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model || !m_editing) return;

    long frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    float value = getValueForY(v, e->y());

    m_editingCommand->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_editingPoint.value = value;
    m_editingCommand->addPoint(m_editingPoint);
}

void
NoteLayer::drawEnd(View *v, QMouseEvent *e)
{
    std::cerr << "NoteLayer::drawEnd(" << e->x() << "," << e->y() << ")" << std::endl;
    if (!m_model || !m_editing) return;
    m_editingCommand->finish();
    m_editingCommand = 0;
    m_editing = false;
}

void
NoteLayer::editStart(View *v, QMouseEvent *e)
{
    std::cerr << "NoteLayer::editStart(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model) return;

    NoteModel::PointList points = getLocalPoints(v, e->x());
    if (points.empty()) return;

    m_editingPoint = *points.begin();
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) {
	m_editingCommand->finish();
	m_editingCommand = 0;
    }

    m_editing = true;
}

void
NoteLayer::editDrag(View *v, QMouseEvent *e)
{
    std::cerr << "NoteLayer::editDrag(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model || !m_editing) return;

    long frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    float value = getValueForY(v, e->y());

    if (!m_editingCommand) {
	m_editingCommand = new NoteModel::EditCommand(m_model,
						      tr("Drag Point"));
    }

    m_editingCommand->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_editingPoint.value = value;
    m_editingCommand->addPoint(m_editingPoint);
}

void
NoteLayer::editEnd(View *v, QMouseEvent *e)
{
    std::cerr << "NoteLayer::editEnd(" << e->x() << "," << e->y() << ")" << std::endl;
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
	m_editingCommand->finish();
    }

    m_editingCommand = 0;
    m_editing = false;
}

void
NoteLayer::moveSelection(Selection s, size_t newStartFrame)
{
    NoteModel::EditCommand *command =
	new NoteModel::EditCommand(m_model, tr("Drag Selection"));

    NoteModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (NoteModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

	if (s.contains(i->frame)) {
	    NoteModel::Point newPoint(*i);
	    newPoint.frame = i->frame + newStartFrame - s.getStartFrame();
	    command->deletePoint(*i);
	    command->addPoint(newPoint);
	}
    }

    command->finish();
}

void
NoteLayer::resizeSelection(Selection s, Selection newSize)
{
    NoteModel::EditCommand *command =
	new NoteModel::EditCommand(m_model, tr("Resize Selection"));

    NoteModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    double ratio =
	double(newSize.getEndFrame() - newSize.getStartFrame()) /
	double(s.getEndFrame() - s.getStartFrame());

    for (NoteModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

	if (s.contains(i->frame)) {

	    double targetStart = i->frame;
	    targetStart = newSize.getStartFrame() + 
		double(targetStart - s.getStartFrame()) * ratio;

	    double targetEnd = i->frame + i->duration;
	    targetEnd = newSize.getStartFrame() +
		double(targetEnd - s.getStartFrame()) * ratio;

	    NoteModel::Point newPoint(*i);
	    newPoint.frame = lrint(targetStart);
	    newPoint.duration = lrint(targetEnd - targetStart);
	    command->deletePoint(*i);
	    command->addPoint(newPoint);
	}
    }

    command->finish();
}

QString
NoteLayer::toXmlString(QString indent, QString extraAttributes) const
{
    return Layer::toXmlString(indent, extraAttributes +
			      QString(" colour=\"%1\" verticalScale=\"%2\"")
			      .arg(encodeColour(m_colour)).arg(m_verticalScale));
}

void
NoteLayer::setProperties(const QXmlAttributes &attributes)
{
    QString colourSpec = attributes.value("colour");
    if (colourSpec != "") {
	QColor colour(colourSpec);
	if (colour.isValid()) {
	    setBaseColour(QColor(colourSpec));
	}
    }

    bool ok;
    VerticalScale scale = (VerticalScale)
	attributes.value("verticalScale").toInt(&ok);
    if (ok) setVerticalScale(scale);
}


