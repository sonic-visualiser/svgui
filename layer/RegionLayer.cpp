/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2008 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "RegionLayer.h"

#include "data/model/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/LogRange.h"
#include "ColourDatabase.h"
#include "ColourMapper.h"
#include "view/View.h"

#include "data/model/RegionModel.h"

#include "widgets/ItemEditDialog.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QTextStream>
#include <QMessageBox>

#include <iostream>
#include <cmath>

RegionLayer::RegionLayer() :
    SingleColourLayer(),
    m_model(0),
    m_editing(false),
    m_originalPoint(0, 0.0, 0, tr("New Point")),
    m_editingPoint(0, 0.0, 0, tr("New Point")),
    m_editingCommand(0),
    m_verticalScale(AutoAlignScale),
    m_colourMap(0),
    m_plotStyle(PlotLines)
{
    
}

void
RegionLayer::setModel(RegionModel *model)
{
    if (m_model == model) return;
    m_model = model;

    connectSignals(m_model);

//    std::cerr << "RegionLayer::setModel(" << model << ")" << std::endl;

    emit modelReplaced();
}

Layer::PropertyList
RegionLayer::getProperties() const
{
    PropertyList list = SingleColourLayer::getProperties();
    list.push_back("Vertical Scale");
    list.push_back("Scale Units");
    list.push_back("Plot Type");
    return list;
}

QString
RegionLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Vertical Scale") return tr("Vertical Scale");
    if (name == "Scale Units") return tr("Scale Units");
    if (name == "Plot Type") return tr("Plot Type");
    return SingleColourLayer::getPropertyLabel(name);
}

Layer::PropertyType
RegionLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Scale Units") return UnitsProperty;
    if (name == "Vertical Scale") return ValueProperty;
    if (name == "Plot Type") return ValueProperty;
    if (name == "Colour" && m_plotStyle == PlotSegmentation) return ValueProperty;
    return SingleColourLayer::getPropertyType(name);
}

QString
RegionLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Vertical Scale" || name == "Scale Units") {
        return tr("Scale");
    }
    return SingleColourLayer::getPropertyGroupName(name);
}

int
RegionLayer::getPropertyRangeAndValue(const PropertyName &name,
                                    int *min, int *max, int *deflt) const
{
    int val = 0;

    if (name == "Colour" && m_plotStyle == PlotSegmentation) {
            
        if (min) *min = 0;
        if (max) *max = ColourMapper::getColourMapCount() - 1;
        if (deflt) *deflt = 0;
        
        val = m_colourMap;

    } else if (name == "Plot Type") {
	
	if (min) *min = 0;
	if (max) *max = 1;
        if (deflt) *deflt = 0;
	
	val = int(m_plotStyle);

    } else if (name == "Vertical Scale") {
	
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
RegionLayer::getPropertyValueLabel(const PropertyName &name,
                                   int value) const
{
    if (name == "Colour" && m_plotStyle == PlotSegmentation) {
        return ColourMapper::getColourMapName(value);
    } else if (name == "Plot Type") {

	switch (value) {
	default:
	case 0: return tr("Bars");
	case 1: return tr("Segmentation");
	}

    } else if (name == "Vertical Scale") {
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
RegionLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Colour" && m_plotStyle == PlotSegmentation) {
        setFillColourMap(value);
    } else if (name == "Plot Type") {
	setPlotStyle(PlotStyle(value));
    } else if (name == "Vertical Scale") {
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
RegionLayer::setFillColourMap(int map)
{
    if (m_colourMap == map) return;
    m_colourMap = map;
    emit layerParametersChanged();
}

void
RegionLayer::setPlotStyle(PlotStyle style)
{
    if (m_plotStyle == style) return;
    bool colourTypeChanged = (style == PlotSegmentation ||
                              m_plotStyle == PlotSegmentation);
    m_plotStyle = style;
    if (colourTypeChanged) {
        emit layerParameterRangesChanged();
    }
    emit layerParametersChanged();
}

void
RegionLayer::setVerticalScale(VerticalScale scale)
{
    if (m_verticalScale == scale) return;
    m_verticalScale = scale;
    emit layerParametersChanged();
}

bool
RegionLayer::isLayerScrollable(const View *v) const
{
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

bool
RegionLayer::getValueExtents(float &min, float &max,
                           bool &logarithmic, QString &unit) const
{
    if (!m_model) return false;
    min = m_model->getValueMinimum();
    max = m_model->getValueMaximum();
    unit = m_model->getScaleUnits();

    if (m_verticalScale == LogScale) logarithmic = true;

    return true;
}

bool
RegionLayer::getDisplayExtents(float &min, float &max) const
{
    if (!m_model || m_verticalScale == AutoAlignScale) return false;

    min = m_model->getValueMinimum();
    max = m_model->getValueMaximum();

    return true;
}

RegionModel::PointList
RegionLayer::getLocalPoints(View *v, int x) const
{
    if (!m_model) return RegionModel::PointList();

    long frame = v->getFrameForX(x);

    RegionModel::PointList onPoints =
	m_model->getPoints(frame);

    if (!onPoints.empty()) {
	return onPoints;
    }

    RegionModel::PointList prevPoints =
	m_model->getPreviousPoints(frame);
    RegionModel::PointList nextPoints =
	m_model->getNextPoints(frame);

    RegionModel::PointList usePoints = prevPoints;

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

QString
RegionLayer::getFeatureDescription(View *v, QPoint &pos) const
{
    int x = pos.x();

    if (!m_model || !m_model->getSampleRate()) return "";

    RegionModel::PointList points = getLocalPoints(v, x);

    if (points.empty()) {
	if (!m_model->isReady()) {
	    return tr("In progress");
	} else {
	    return tr("No local points");
	}
    }

    RegionRec region(0);
    RegionModel::PointList::iterator i;

    //!!! harmonise with whatever decision is made about point y
    //!!! coords in paint method

    for (i = points.begin(); i != points.end(); ++i) {

	int y = getYForValue(v, i->value);
	int h = 3;

	if (m_model->getValueQuantization() != 0.0) {
	    h = y - getYForValue(v, i->value + m_model->getValueQuantization());
	    if (h < 3) h = 3;
	}

	if (pos.y() >= y - h && pos.y() <= y) {
	    region = *i;
	    break;
	}
    }

    if (i == points.end()) return tr("No local points");

    RealTime rt = RealTime::frame2RealTime(region.frame,
					   m_model->getSampleRate());
    RealTime rd = RealTime::frame2RealTime(region.duration,
					   m_model->getSampleRate());
    
    QString valueText;

    valueText = tr("%1 %2").arg(region.value).arg(m_model->getScaleUnits());

    QString text;

    if (region.label == "") {
	text = QString(tr("Time:\t%1\nValue:\t%2\nDuration:\t%3\nNo label"))
	    .arg(rt.toText(true).c_str())
	    .arg(valueText)
	    .arg(rd.toText(true).c_str());
    } else {
	text = QString(tr("Time:\t%1\nValue:\t%2\nDuration:\t%3\nLabel:\t%4"))
	    .arg(rt.toText(true).c_str())
	    .arg(valueText)
	    .arg(rd.toText(true).c_str())
	    .arg(region.label);
    }

    pos = QPoint(v->getXForFrame(region.frame),
		 getYForValue(v, region.value));
    return text;
}

bool
RegionLayer::snapToFeatureFrame(View *v, int &frame,
                                size_t &resolution,
                                SnapType snap) const
{
    if (!m_model) {
	return Layer::snapToFeatureFrame(v, frame, resolution, snap);
    }

    resolution = m_model->getResolution();
    RegionModel::PointList points;

    if (snap == SnapNeighbouring) {
	
	points = getLocalPoints(v, v->getXForFrame(frame));
	if (points.empty()) return false;
	frame = points.begin()->frame;
	return true;
    }    

    points = m_model->getPoints(frame, frame);
    int snapped = frame;
    bool found = false;

    for (RegionModel::PointList::const_iterator i = points.begin();
	 i != points.end(); ++i) {

	if (snap == SnapRight) {

            // The best frame to snap to is the end frame of whichever
            // feature we would have snapped to the start frame of if
            // we had been snapping left.

	    if (i->frame <= frame) {
                if (i->frame + i->duration > frame) {
                    snapped = i->frame + i->duration;
                    found = true; // don't break, as the next may be better
                }
            } else {
                if (!found) {
                    snapped = i->frame;
                    found = true;
                }
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

	    RegionModel::PointList::const_iterator j = i;
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
RegionLayer::getScaleExtents(View *v, float &min, float &max, bool &log) const
{
    min = 0.0;
    max = 0.0;
    log = false;

    QString queryUnits;
    queryUnits = m_model->getScaleUnits();

    if (m_verticalScale == AutoAlignScale) {

        if (!v->getValueExtents(queryUnits, min, max, log)) {

            min = m_model->getValueMinimum();
            max = m_model->getValueMaximum();

            std::cerr << "RegionLayer[" << this << "]::getScaleExtents: min = " << min << ", max = " << max << ", log = " << log << std::endl;

        } else if (log) {

            LogRange::mapRange(min, max);

            std::cerr << "RegionLayer[" << this << "]::getScaleExtents: min = " << min << ", max = " << max << ", log = " << log << std::endl;

        }

    } else {

        min = m_model->getValueMinimum();
        max = m_model->getValueMaximum();

        if (m_verticalScale == LogScale) {
            LogRange::mapRange(min, max);
            log = true;
        }
    }

    if (max == min) max = min + 1.0;
}

int
RegionLayer::getYForValue(View *v, float val) const
{
    float min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->height();
    int margin = 8;
    if (h < margin * 8) margin = h / 8;

    getScaleExtents(v, min, max, logarithmic);

//    std::cerr << "RegionLayer[" << this << "]::getYForValue(" << val << "): min = " << min << ", max = " << max << ", log = " << logarithmic << std::endl;
//    std::cerr << "h = " << h << ", margin = " << margin << std::endl;

    if (logarithmic) {
        val = LogRange::map(val);
        std::cerr << "logarithmic true, val now = " << val << std::endl;
    }

    h -= margin * 2;
    int y = margin + int(h - ((val - min) * h) / (max - min)) - 1;
//    std::cerr << "y = " << y << std::endl;
    return y;
}

QColor
RegionLayer::getColourForValue(View *v, float val) const
{
    float min, max;
    bool log;
    getScaleExtents(v, min, max, log);

    if (min > max) std::swap(min, max);
    if (max == min) max = min + 1;

    if (log) {
        LogRange::mapRange(min, max);
        val = LogRange::map(val);
    }

//    std::cerr << "RegionLayer::getColourForValue: min " << min << ", max "
//              << max << ", log " << log << ", value " << val << std::endl;

    QColor solid = ColourMapper(m_colourMap, min, max).map(val);
    return QColor(solid.red(), solid.green(), solid.blue(), 120);
}

int
RegionLayer::getDefaultColourHint(bool darkbg, bool &impose)
{
    impose = false;
    return ColourDatabase::getInstance()->getColourIndex
        (QString(darkbg ? "Bright Blue" : "Blue"));
}

float
RegionLayer::getValueForY(View *v, int y) const
{
    float min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->height();

    getScaleExtents(v, min, max, logarithmic);

    float val = min + (float(h - y) * float(max - min)) / h;

    if (logarithmic) {
        val = powf(10.f, val);
    }

    return val;
}

void
RegionLayer::paint(View *v, QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) return;

    int sampleRate = m_model->getSampleRate();
    if (!sampleRate) return;

//    Profiler profiler("RegionLayer::paint", true);

    int x0 = rect.left(), x1 = rect.right();
    long frame0 = v->getFrameForX(x0);
    long frame1 = v->getFrameForX(x1);

    RegionModel::PointList points(m_model->getPoints(frame0, frame1));
    if (points.empty()) return;

    paint.setPen(getBaseQColor());

    QColor brushColour(getBaseQColor());
    brushColour.setAlpha(80);

//    std::cerr << "RegionLayer::paint: resolution is "
//	      << m_model->getResolution() << " frames" << std::endl;

    float min = m_model->getValueMinimum();
    float max = m_model->getValueMaximum();
    if (max == min) max = min + 1.0;

    QPoint localPos;
    long illuminateFrame = -1;

    if (v->shouldIlluminateLocalFeatures(this, localPos)) {
	RegionModel::PointList localPoints =
	    getLocalPoints(v, localPos.x());
	if (!localPoints.empty()) illuminateFrame = localPoints.begin()->frame;
    }

    paint.save();
    paint.setRenderHint(QPainter::Antialiasing, false);
    
    //!!! point y coords if model does not haveDistinctValues() should
    //!!! be assigned to avoid overlaps

    //!!! if it does have distinct values, we should still ensure y
    //!!! coord is never completely flat on the top or bottom

    int textY = 0;
    if (m_plotStyle == PlotSegmentation) {
        textY = v->getTextLabelHeight(this, paint);
    }
    
    for (RegionModel::PointList::const_iterator i = points.begin();
	 i != points.end(); ++i) {

	const RegionModel::Point &p(*i);

	int x = v->getXForFrame(p.frame);
	int y = getYForValue(v, p.value);
	int w = v->getXForFrame(p.frame + p.duration) - x;
	int h = 9;
	
	bool haveNext = false;
	int nx = v->getXForFrame(v->getModelsEndFrame());

        RegionModel::PointList::const_iterator j = i;
	++j;

	if (j != points.end()) {
	    const RegionModel::Point &q(*j);
	    nx = v->getXForFrame(q.frame);
	    haveNext = true;
        }

        if (m_plotStyle != PlotSegmentation) {
            textY = y - paint.fontMetrics().height()
                      + paint.fontMetrics().ascent();
            if (textY < paint.fontMetrics().ascent() + 1) {
                textY = paint.fontMetrics().ascent() + 1;
            }
        }

	if (m_model->getValueQuantization() != 0.0) {
	    h = y - getYForValue(v, p.value + m_model->getValueQuantization());
	    if (h < 3) h = 3;
	}

	if (w < 1) w = 1;

	if (m_plotStyle == PlotSegmentation) {
            paint.setPen(getForegroundQColor(v));
            paint.setBrush(getColourForValue(v, p.value));
        } else {
            paint.setPen(getBaseQColor());
            paint.setBrush(brushColour);
        }

	if (m_plotStyle == PlotSegmentation) {

	    if (nx <= x) continue;

	    if (illuminateFrame != p.frame &&
		(nx < x + 5 || x >= v->width() - 1)) {
		paint.setPen(Qt::NoPen);
	    }

	    paint.drawRect(x, -1, nx - x, v->height() + 1);

	} else {

            if (illuminateFrame == p.frame) {
                if (localPos.y() >= y - h && localPos.y() < y) {
                    paint.setPen(v->getForeground());
                    paint.setBrush(v->getForeground());
                }
            }
	
            paint.drawLine(x, y-1, x + w, y-1);
            paint.drawLine(x, y+1, x + w, y+1);
            paint.drawLine(x, y - h/2, x, y + h/2);
            paint.drawLine(x+w, y - h/2, x + w, y + h/2);
        }

	if (p.label != "") {
            if (!haveNext || nx > x + 6 + paint.fontMetrics().width(p.label)) {
                paint.drawText(x + 5, textY, p.label);
            }
	}
    }

    paint.restore();
}

void
RegionLayer::drawStart(View *v, QMouseEvent *e)
{
//    std::cerr << "RegionLayer::drawStart(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model) return;

    long frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    float value = getValueForY(v, e->y());

    m_editingPoint = RegionModel::Point(frame, value, 0, tr("New Point"));
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) finish(m_editingCommand);
    m_editingCommand = new RegionModel::EditCommand(m_model,
                                                    tr("Draw Point"));
    m_editingCommand->addPoint(m_editingPoint);

    m_editing = true;
}

void
RegionLayer::drawDrag(View *v, QMouseEvent *e)
{
//    std::cerr << "RegionLayer::drawDrag(" << e->x() << "," << e->y() << ")" << std::endl;

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
RegionLayer::drawEnd(View *, QMouseEvent *)
{
//    std::cerr << "RegionLayer::drawEnd(" << e->x() << "," << e->y() << ")" << std::endl;
    if (!m_model || !m_editing) return;
    finish(m_editingCommand);
    m_editingCommand = 0;
    m_editing = false;
}

void
RegionLayer::eraseStart(View *v, QMouseEvent *e)
{
    if (!m_model) return;

    RegionModel::PointList points = getLocalPoints(v, e->x());
    if (points.empty()) return;

    m_editingPoint = *points.begin();

    if (m_editingCommand) {
	finish(m_editingCommand);
	m_editingCommand = 0;
    }

    m_editing = true;
}

void
RegionLayer::eraseDrag(View *v, QMouseEvent *e)
{
}

void
RegionLayer::eraseEnd(View *v, QMouseEvent *e)
{
    if (!m_model || !m_editing) return;

    m_editing = false;

    RegionModel::PointList points = getLocalPoints(v, e->x());
    if (points.empty()) return;
    if (points.begin()->frame != m_editingPoint.frame ||
        points.begin()->value != m_editingPoint.value) return;

    m_editingCommand = new RegionModel::EditCommand
        (m_model, tr("Erase Point"));

    m_editingCommand->deletePoint(m_editingPoint);

    finish(m_editingCommand);
    m_editingCommand = 0;
    m_editing = false;
}

void
RegionLayer::editStart(View *v, QMouseEvent *e)
{
//    std::cerr << "RegionLayer::editStart(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model) return;

    RegionModel::PointList points = getLocalPoints(v, e->x());
    if (points.empty()) return;

    m_editingPoint = *points.begin();
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) {
	finish(m_editingCommand);
	m_editingCommand = 0;
    }

    m_editing = true;
}

void
RegionLayer::editDrag(View *v, QMouseEvent *e)
{
//    std::cerr << "RegionLayer::editDrag(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model || !m_editing) return;

    long frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    float value = getValueForY(v, e->y());

    if (!m_editingCommand) {
	m_editingCommand = new RegionModel::EditCommand(m_model,
						      tr("Drag Point"));
    }

    m_editingCommand->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_editingPoint.value = value;
    m_editingCommand->addPoint(m_editingPoint);
}

void
RegionLayer::editEnd(View *, QMouseEvent *)
{
//    std::cerr << "RegionLayer::editEnd(" << e->x() << "," << e->y() << ")" << std::endl;
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

bool
RegionLayer::editOpen(View *v, QMouseEvent *e)
{
    if (!m_model) return false;

    RegionModel::PointList points = getLocalPoints(v, e->x());
    if (points.empty()) return false;

    RegionModel::Point region = *points.begin();

    ItemEditDialog *dialog = new ItemEditDialog
        (m_model->getSampleRate(),
         ItemEditDialog::ShowTime |
         ItemEditDialog::ShowDuration |
         ItemEditDialog::ShowValue |
         ItemEditDialog::ShowText,
         m_model->getScaleUnits());

    dialog->setFrameTime(region.frame);
    dialog->setValue(region.value);
    dialog->setFrameDuration(region.duration);
    dialog->setText(region.label);

    if (dialog->exec() == QDialog::Accepted) {

        RegionModel::Point newRegion = region;
        newRegion.frame = dialog->getFrameTime();
        newRegion.value = dialog->getValue();
        newRegion.duration = dialog->getFrameDuration();
        newRegion.label = dialog->getText();
        
        RegionModel::EditCommand *command = new RegionModel::EditCommand
            (m_model, tr("Edit Point"));
        command->deletePoint(region);
        command->addPoint(newRegion);
        finish(command);
    }

    delete dialog;
    return true;
}

void
RegionLayer::moveSelection(Selection s, size_t newStartFrame)
{
    if (!m_model) return;

    RegionModel::EditCommand *command =
	new RegionModel::EditCommand(m_model, tr("Drag Selection"));

    RegionModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (RegionModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

	if (s.contains(i->frame)) {
	    RegionModel::Point newPoint(*i);
	    newPoint.frame = i->frame + newStartFrame - s.getStartFrame();
	    command->deletePoint(*i);
	    command->addPoint(newPoint);
	}
    }

    finish(command);
}

void
RegionLayer::resizeSelection(Selection s, Selection newSize)
{
    if (!m_model) return;

    RegionModel::EditCommand *command =
	new RegionModel::EditCommand(m_model, tr("Resize Selection"));

    RegionModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    double ratio =
	double(newSize.getEndFrame() - newSize.getStartFrame()) /
	double(s.getEndFrame() - s.getStartFrame());

    for (RegionModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

	if (s.contains(i->frame)) {

	    double targetStart = i->frame;
	    targetStart = newSize.getStartFrame() + 
		double(targetStart - s.getStartFrame()) * ratio;

	    double targetEnd = i->frame + i->duration;
	    targetEnd = newSize.getStartFrame() +
		double(targetEnd - s.getStartFrame()) * ratio;

	    RegionModel::Point newPoint(*i);
	    newPoint.frame = lrint(targetStart);
	    newPoint.duration = lrint(targetEnd - targetStart);
	    command->deletePoint(*i);
	    command->addPoint(newPoint);
	}
    }

    finish(command);
}

void
RegionLayer::deleteSelection(Selection s)
{
    if (!m_model) return;

    RegionModel::EditCommand *command =
	new RegionModel::EditCommand(m_model, tr("Delete Selected Points"));

    RegionModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (RegionModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

        if (s.contains(i->frame)) {
            command->deletePoint(*i);
        }
    }

    finish(command);
}    

void
RegionLayer::copy(View *v, Selection s, Clipboard &to)
{
    if (!m_model) return;

    RegionModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (RegionModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {
	if (s.contains(i->frame)) {
            Clipboard::Point point(i->frame, i->value, i->duration, i->label);
            point.setReferenceFrame(alignToReference(v, i->frame));
            to.addPoint(point);
        }
    }
}

bool
RegionLayer::paste(View *v, const Clipboard &from, int frameOffset, bool /* interactive */)
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

    RegionModel::EditCommand *command =
	new RegionModel::EditCommand(m_model, tr("Paste"));

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

        RegionModel::Point newPoint(frame);
  
        if (i->haveLabel()) newPoint.label = i->getLabel();
        if (i->haveValue()) newPoint.value = i->getValue();
        else newPoint.value = (m_model->getValueMinimum() +
                               m_model->getValueMaximum()) / 2;
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
RegionLayer::toXml(QTextStream &stream,
                 QString indent, QString extraAttributes) const
{
    SingleColourLayer::toXml(stream, indent, extraAttributes +
                             QString(" verticalScale=\"%1\" plotStyle=\"%2\"")
                             .arg(m_verticalScale)
                             .arg(m_plotStyle));
}

void
RegionLayer::setProperties(const QXmlAttributes &attributes)
{
    SingleColourLayer::setProperties(attributes);

    bool ok;
    VerticalScale scale = (VerticalScale)
	attributes.value("verticalScale").toInt(&ok);
    if (ok) setVerticalScale(scale);
    PlotStyle style = (PlotStyle)
	attributes.value("plotStyle").toInt(&ok);
    if (ok) setPlotStyle(style);
}


