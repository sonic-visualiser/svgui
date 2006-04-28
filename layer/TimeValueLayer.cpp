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

#include "TimeValueLayer.h"

#include "base/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/View.h"

#include "model/SparseTimeValueModel.h"

#include "widgets/ItemEditDialog.h"

#include "SpectrogramLayer.h" // for optional frequency alignment

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

#include <iostream>
#include <cmath>

TimeValueLayer::TimeValueLayer() :
    Layer(),
    m_model(0),
    m_editing(false),
    m_originalPoint(0, 0.0, tr("New Point")),
    m_editingPoint(0, 0.0, tr("New Point")),
    m_editingCommand(0),
    m_colour(Qt::darkGreen),
    m_plotStyle(PlotConnectedPoints),
    m_verticalScale(LinearScale)
{
    
}

void
TimeValueLayer::setModel(SparseTimeValueModel *model)
{
    if (m_model == model) return;
    m_model = model;

    connect(m_model, SIGNAL(modelChanged()), this, SIGNAL(modelChanged()));
    connect(m_model, SIGNAL(modelChanged(size_t, size_t)),
	    this, SIGNAL(modelChanged(size_t, size_t)));

    connect(m_model, SIGNAL(completionChanged()),
	    this, SIGNAL(modelCompletionChanged()));

    std::cerr << "TimeValueLayer::setModel(" << model << ")" << std::endl;

    emit modelReplaced();
}

Layer::PropertyList
TimeValueLayer::getProperties() const
{
    PropertyList list;
    list.push_back(tr("Colour"));
    list.push_back(tr("Plot Type"));
    list.push_back(tr("Vertical Scale"));
    return list;
}

Layer::PropertyType
TimeValueLayer::getPropertyType(const PropertyName &name) const
{
    return ValueProperty;
}

int
TimeValueLayer::getPropertyRangeAndValue(const PropertyName &name,
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

    } else if (name == tr("Plot Type")) {
	
	if (min) *min = 0;
	if (max) *max = 5;
	
	deft = int(m_plotStyle);

    } else if (name == tr("Vertical Scale")) {
	
	if (min) *min = 0;
	if (max) *max = 3;
	
	deft = int(m_verticalScale);

    } else {
	
	deft = Layer::getPropertyRangeAndValue(name, min, max);
    }

    return deft;
}

QString
TimeValueLayer::getPropertyValueLabel(const PropertyName &name,
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
    } else if (name == tr("Plot Type")) {
	switch (value) {
	default:
	case 0: return tr("Points");
	case 1: return tr("Stems");
	case 2: return tr("Connected Points");
	case 3: return tr("Lines");
	case 4: return tr("Curve");
	case 5: return tr("Segmentation");
	}
    } else if (name == tr("Vertical Scale")) {
	switch (value) {
	default:
	case 0: return tr("Linear Scale");
	case 1: return tr("Log Scale");
	case 2: return tr("+/-1 Scale");
	case 3: return tr("Frequency Scale");
	}
    }
    return tr("<unknown>");
}

void
TimeValueLayer::setProperty(const PropertyName &name, int value)
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
    } else if (name == tr("Plot Type")) {
	setPlotStyle(PlotStyle(value));
    } else if (name == tr("Vertical Scale")) {
	setVerticalScale(VerticalScale(value));
    }
}

void
TimeValueLayer::setBaseColour(QColor colour)
{
    if (m_colour == colour) return;
    m_colour = colour;
    emit layerParametersChanged();
}

void
TimeValueLayer::setPlotStyle(PlotStyle style)
{
    if (m_plotStyle == style) return;
    m_plotStyle = style;
    emit layerParametersChanged();
}

void
TimeValueLayer::setVerticalScale(VerticalScale scale)
{
    if (m_verticalScale == scale) return;
    m_verticalScale = scale;
    emit layerParametersChanged();
}

bool
TimeValueLayer::isLayerScrollable(const View *v) const
{
    // We don't illuminate sections in the line or curve modes, so
    // they're always scrollable

    if (m_plotStyle == PlotLines ||
	m_plotStyle == PlotCurve) return true;

    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

bool
TimeValueLayer::getValueExtents(float &min, float &max, QString &unit) const
{
    min = m_model->getValueMinimum();
    max = m_model->getValueMaximum();
    unit = m_model->getScaleUnits();
    return true;
}

SparseTimeValueModel::PointList
TimeValueLayer::getLocalPoints(View *v, int x) const
{
    if (!m_model) return SparseTimeValueModel::PointList();

    long frame = v->getFrameForX(x);

    SparseTimeValueModel::PointList onPoints =
	m_model->getPoints(frame);

    if (!onPoints.empty()) {
	return onPoints;
    }

    SparseTimeValueModel::PointList prevPoints =
	m_model->getPreviousPoints(frame);
    SparseTimeValueModel::PointList nextPoints =
	m_model->getNextPoints(frame);

    SparseTimeValueModel::PointList usePoints = prevPoints;

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
TimeValueLayer::getFeatureDescription(View *v, QPoint &pos) const
{
    int x = pos.x();

    if (!m_model || !m_model->getSampleRate()) return "";

    SparseTimeValueModel::PointList points = getLocalPoints(v, x);

    if (points.empty()) {
	if (!m_model->isReady()) {
	    return tr("In progress");
	} else {
	    return tr("No local points");
	}
    }

    long useFrame = points.begin()->frame;

    RealTime rt = RealTime::frame2RealTime(useFrame, m_model->getSampleRate());
    
    QString text;

    if (points.begin()->label == "") {
	text = QString(tr("Time:\t%1\nValue:\t%2\nNo label"))
	    .arg(rt.toText(true).c_str())
	    .arg(points.begin()->value);
    } else {
	text = QString(tr("Time:\t%1\nValue:\t%2\nLabel:\t%3"))
	    .arg(rt.toText(true).c_str())
	    .arg(points.begin()->value)
	    .arg(points.begin()->label);
    }

    pos = QPoint(v->getXForFrame(useFrame),
		 getYForValue(v, points.begin()->value));
    return text;
}

bool
TimeValueLayer::snapToFeatureFrame(View *v, int &frame,
				   size_t &resolution,
				   SnapType snap) const
{
    if (!m_model) {
	return Layer::snapToFeatureFrame(v, frame, resolution, snap);
    }

    resolution = m_model->getResolution();
    SparseTimeValueModel::PointList points;

    if (snap == SnapNeighbouring) {
	
	points = getLocalPoints(v, v->getXForFrame(frame));
	if (points.empty()) return false;
	frame = points.begin()->frame;
	return true;
    }    

    points = m_model->getPoints(frame, frame);
    int snapped = frame;
    bool found = false;

    for (SparseTimeValueModel::PointList::const_iterator i = points.begin();
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

	    SparseTimeValueModel::PointList::const_iterator j = i;
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
TimeValueLayer::getYForValue(View *v, float val) const
{
    float min = 0.0, max = 0.0;
    int h = v->height();

    if (!v->getValueExtents(m_model->getScaleUnits(), min, max)) {
        min = m_model->getValueMinimum();
        max = m_model->getValueMaximum();
    }

    if (max == min) max = min + 1.0;

/*!!!
    float min = m_model->getValueMinimum();
    float max = m_model->getValueMaximum();
    if (max == min) max = min + 1.0;

    int h = v->height();

    if (m_verticalScale == FrequencyScale || m_verticalScale == LogScale) {
        
        if (m_verticalScale == FrequencyScale) {
            // If we have a spectrogram layer on the same view as us, align
            // ourselves with it...
            for (int i = 0; i < v->getLayerCount(); ++i) {
                SpectrogramLayer *spectrogram = dynamic_cast<SpectrogramLayer *>
                    (v->getLayer(i));
                if (spectrogram) {
                    return spectrogram->getYForFrequency(v, val);
                }
            }
        }

        min = (min < 0.0) ? -log10(-min) : (min == 0.0) ? 0.0 : log10(min);
        max = (max < 0.0) ? -log10(-max) : (max == 0.0) ? 0.0 : log10(max);
        val = (val < 0.0) ? -log10(-val) : (val == 0.0) ? 0.0 : log10(val);

    } else if (m_verticalScale == PlusMinusOneScale) {
        min = -1.0;
        max = 1.0;
    }
*/

    return int(h - ((val - min) * h) / (max - min));
}

float
TimeValueLayer::getValueForY(View *v, int y) const
{
    //!!!

    float min = m_model->getValueMinimum();
    float max = m_model->getValueMaximum();
    if (max == min) max = min + 1.0;

    int h = v->height();

    return min + (float(h - y) * float(max - min)) / h;
}

QColor
TimeValueLayer::getColourForValue(float val) const
{
    float min = m_model->getValueMinimum();
    float max = m_model->getValueMaximum();
    if (max == min) max = min + 1.0;

    if (m_verticalScale == FrequencyScale || m_verticalScale == LogScale) {
        min = (min < 0.0) ? -log10(-min) : (min == 0.0) ? 0.0 : log10(min);
        max = (max < 0.0) ? -log10(-max) : (max == 0.0) ? 0.0 : log10(max);
        val = (val < 0.0) ? -log10(-val) : (val == 0.0) ? 0.0 : log10(val);
    } else if (m_verticalScale == PlusMinusOneScale) {
        min = -1.0;
        max = 1.0;
    }

    int iv = ((val - min) / (max - min)) * 255.999;

    QColor colour = QColor::fromHsv(256 - iv, iv / 2 + 128, iv);
    return QColor(colour.red(), colour.green(), colour.blue(), 120);
}

void
TimeValueLayer::paint(View *v, QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) return;

    int sampleRate = m_model->getSampleRate();
    if (!sampleRate) return;

//    Profiler profiler("TimeValueLayer::paint", true);

    int x0 = rect.left(), x1 = rect.right();
    long frame0 = v->getFrameForX(x0);
    long frame1 = v->getFrameForX(x1);

    SparseTimeValueModel::PointList points(m_model->getPoints
					   (frame0, frame1));
    if (points.empty()) return;

    paint.setPen(m_colour);

    QColor brushColour(m_colour);
    brushColour.setAlpha(80);
    paint.setBrush(brushColour);

//    std::cerr << "TimeValueLayer::paint: resolution is "
//	      << m_model->getResolution() << " frames" << std::endl;

    float min = m_model->getValueMinimum();
    float max = m_model->getValueMaximum();
    if (max == min) max = min + 1.0;

    int origin = int(nearbyint(v->height() -
			       (-min * v->height()) / (max - min)));

    QPoint localPos;
    long illuminateFrame = -1;

    if (v->shouldIlluminateLocalFeatures(this, localPos)) {
	SparseTimeValueModel::PointList localPoints =
	    getLocalPoints(v, localPos.x());
	if (!localPoints.empty()) illuminateFrame = localPoints.begin()->frame;
    }

    int w =
	v->getXForFrame(frame0 + m_model->getResolution()) -
	v->getXForFrame(frame0);

    paint.save();

    QPainterPath path;
    int pointCount = 0;

    int textY = 0;
    if (m_plotStyle == PlotSegmentation) {
        textY = v->getTextLabelHeight(this, paint);
    }
    
    for (SparseTimeValueModel::PointList::const_iterator i = points.begin();
	 i != points.end(); ++i) {

	const SparseTimeValueModel::Point &p(*i);

	int x = v->getXForFrame(p.frame);
	int y = getYForValue(v, p.value);

        if (m_plotStyle != PlotSegmentation) {
            textY = y - paint.fontMetrics().height()
                      + paint.fontMetrics().ascent();
        }

	bool haveNext = false;
	int nx = v->getXForFrame(v->getModelsEndFrame());
// m_model->getEndFrame());
	int ny = y;

	SparseTimeValueModel::PointList::const_iterator j = i;
	++j;

	if (j != points.end()) {
	    const SparseTimeValueModel::Point &q(*j);
	    nx = v->getXForFrame(q.frame);
	    ny = getYForValue(v, q.value);
	    haveNext = true;
        }

//        std::cout << "frame = " << p.frame << ", x = " << x << ", haveNext = " << haveNext 
//                  << ", nx = " << nx << std::endl;

	int labelY = y;

	if (w < 1) w = 1;
	paint.setPen(m_colour);

	if (m_plotStyle == PlotSegmentation) {
            paint.setBrush(getColourForValue(p.value));
	    labelY = v->height();
	} else if (m_plotStyle == PlotLines ||
		   m_plotStyle == PlotCurve) {
	    paint.setBrush(Qt::NoBrush);
	} else {
	    paint.setBrush(brushColour);
	}	    

	if (m_plotStyle == PlotStems) {
	    paint.setPen(brushColour);
	    if (y < origin - 1) {
		paint.drawRect(x + w/2, y + 1, 1, origin - y);
	    } else if (y > origin + 1) {
		paint.drawRect(x + w/2, origin, 1, y - origin - 1);
	    }
	    paint.setPen(m_colour);
	}

	if (illuminateFrame == p.frame) {

	    //!!! aside from the problem of choosing a colour, it'd be
	    //better to save the highlighted rects and draw them at
	    //the end perhaps

	    //!!! not equipped to illuminate the right section in line
	    //or curve mode

	    if (m_plotStyle != PlotCurve &&
		m_plotStyle != PlotLines) {
		paint.setPen(Qt::black);//!!!
		if (m_plotStyle != PlotSegmentation) {
		    paint.setBrush(Qt::black);//!!!
		}
	    }	    
	}

	if (m_plotStyle != PlotLines &&
	    m_plotStyle != PlotCurve &&
	    m_plotStyle != PlotSegmentation) {
	    paint.drawRect(x, y - 1, w, 2);
	}

	if (m_plotStyle == PlotConnectedPoints ||
	    m_plotStyle == PlotLines ||
	    m_plotStyle == PlotCurve) {

	    if (haveNext) {

		if (m_plotStyle == PlotConnectedPoints) {
		    
                    paint.save();
		    paint.setPen(brushColour);
		    paint.drawLine(x + w, y, nx, ny);
                    paint.restore();

		} else if (m_plotStyle == PlotLines) {

		    paint.drawLine(x + w/2, y, nx + w/2, ny);

		} else {

		    float x0 = x + float(w)/2;
		    float x1 = nx + float(w)/2;
		    
		    float y0 = y;
		    float y1 = ny;

		    if (pointCount == 0) {
			path.moveTo((x0 + x1) / 2, (y0 + y1) / 2);
		    }
		    ++pointCount;

		    if (nx - x > 5) {
			path.cubicTo(x0, y0,
				     x0, y0,
				     (x0 + x1) / 2, (y0 + y1) / 2);

			// // or
			// path.quadTo(x0, y0, (x0 + x1) / 2, (y0 + y1) / 2);

		    } else {
			path.lineTo((x0 + x1) / 2, (y0 + y1) / 2);
		    }
		}
	    }
	}

	if (m_plotStyle == PlotSegmentation) {

//            std::cerr << "drawing rect" << std::endl;
	    
	    if (nx <= x) continue;

	    if (illuminateFrame != p.frame &&
		(nx < x + 5 || x >= v->width() - 1)) {
		paint.setPen(Qt::NoPen);
	    }

	    paint.drawRect(x, -1, nx - x, v->height() + 1);
	}

	if (p.label != "") {
            if (!haveNext || nx > x + 6 + paint.fontMetrics().width(p.label)) {
                paint.drawText(x + 5, textY, p.label);
            }
	}
    }

    if (m_plotStyle == PlotCurve && !path.isEmpty()) {
	paint.setRenderHint(QPainter::Antialiasing, pointCount <= v->width());
	paint.drawPath(path);
    }

    paint.restore();

    // looks like save/restore doesn't deal with this:
    paint.setRenderHint(QPainter::Antialiasing, false);
}

int
TimeValueLayer::getVerticalScaleWidth(View *v, QPainter &paint) const
{
    int w = paint.fontMetrics().width("-000.000");
    if (m_plotStyle == PlotSegmentation) return w + 20;
    else return w + 10;
}

void
TimeValueLayer::paintVerticalScale(View *v, QPainter &paint, QRect rect) const
{
    if (!m_model) return;

    int h = v->height();

    int n = 10;

    float max = m_model->getValueMaximum();
    float min = m_model->getValueMinimum();
    float val = min;
    float inc = (max - val) / n;

    char buffer[40];

    int w = getVerticalScaleWidth(v, paint);

    int tx = 5;

    int boxx = 5, boxy = 5;
    if (m_model->getScaleUnits() != "") {
        boxy += paint.fontMetrics().height();
    }
    int boxw = 10, boxh = h - boxy - 5;

    if (m_plotStyle == PlotSegmentation) {
        tx += boxx + boxw;
        paint.drawRect(boxx, boxy, boxw, boxh);
    }

    if (m_plotStyle == PlotSegmentation) {
        paint.save();
        for (int y = 0; y < boxh; ++y) {
            float val = ((boxh - y) * (max - min)) / boxh + min;
            paint.setPen(getColourForValue(val));
            paint.drawLine(boxx + 1, y + boxy + 1, boxx + boxw, y + boxy + 1);
        }
        paint.restore();
    }

    for (int i = 0; i < n; ++i) {

	int y, ty;
        bool drawText = true;

        if (m_plotStyle == PlotSegmentation) {
            y = boxy + int(boxh - ((val - min) * boxh) / (max - min));
            ty = y;
        } else {
            if (i == n-1) {
                if (m_model->getScaleUnits() != "") drawText = false;
            }
            y = getYForValue(v, val);
            ty = y - paint.fontMetrics().height() +
                     paint.fontMetrics().ascent();
        }

	sprintf(buffer, "%.3f", val);
	QString label = QString(buffer);

        if (m_plotStyle != PlotSegmentation) {
            paint.drawLine(w - 5, y, w, y);
        } else {
            paint.drawLine(boxx + boxw - boxw/3, y, boxx + boxw, y);
        }

        if (drawText) paint.drawText(tx, ty, label);
	val += inc;
    }
    
    if (m_model->getScaleUnits() != "") {
        paint.drawText(5, 5 + paint.fontMetrics().ascent(),
                       m_model->getScaleUnits());
    }
}

void
TimeValueLayer::drawStart(View *v, QMouseEvent *e)
{
    std::cerr << "TimeValueLayer::drawStart(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model) return;

    long frame = v->getFrameForX(e->x());
    long resolution = m_model->getResolution();
    if (frame < 0) frame = 0;
    frame = (frame / resolution) * resolution;

    float value = getValueForY(v, e->y());

    bool havePoint = false;

    SparseTimeValueModel::PointList points = getLocalPoints(v, e->x());
    if (!points.empty()) {
        for (SparseTimeValueModel::PointList::iterator i = points.begin();
             i != points.end(); ++i) {
            if (((i->frame / resolution) * resolution) != frame) {
                std::cerr << "ignoring out-of-range frame at " << i->frame << std::endl;
                continue;
            }
            m_editingPoint = *i;
            havePoint = true;
        }
    }

    if (!havePoint) {
        m_editingPoint = SparseTimeValueModel::Point
            (frame, value, tr("New Point"));
    }

    m_originalPoint = m_editingPoint;

    if (m_editingCommand) m_editingCommand->finish();
    m_editingCommand = new SparseTimeValueModel::EditCommand(m_model,
							     tr("Draw Point"));
    if (!havePoint) {
        m_editingCommand->addPoint(m_editingPoint);
    }

    m_editing = true;
}

void
TimeValueLayer::drawDrag(View *v, QMouseEvent *e)
{
    std::cerr << "TimeValueLayer::drawDrag(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model || !m_editing) return;

    long frame = v->getFrameForX(e->x());
    long resolution = m_model->getResolution();
    if (frame < 0) frame = 0;
    frame = (frame / resolution) * resolution;

    float value = getValueForY(v, e->y());

    SparseTimeValueModel::PointList points = getLocalPoints(v, e->x());

    std::cerr << points.size() << " points" << std::endl;

    bool havePoint = false;

    if (!points.empty()) {
        for (SparseTimeValueModel::PointList::iterator i = points.begin();
             i != points.end(); ++i) {
            if (i->frame == m_editingPoint.frame &&
                i->value == m_editingPoint.value) {
                std::cerr << "ignoring current editing point at " << i->frame << ", " << i->value << std::endl;
                continue;
            }
            if (((i->frame / resolution) * resolution) != frame) {
                std::cerr << "ignoring out-of-range frame at " << i->frame << std::endl;
                continue;
            }
            std::cerr << "adjusting to new point at " << i->frame << ", " << i->value << std::endl;
            m_editingPoint = *i;
            m_originalPoint = m_editingPoint;
            m_editingCommand->deletePoint(m_editingPoint);
            havePoint = true;
        }
    }

    if (!havePoint) {
        if (frame == m_editingPoint.frame) {
            m_editingCommand->deletePoint(m_editingPoint);
        }
    }

//    m_editingCommand->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_editingPoint.value = value;
    m_editingCommand->addPoint(m_editingPoint);
}

void
TimeValueLayer::drawEnd(View *v, QMouseEvent *e)
{
    std::cerr << "TimeValueLayer::drawEnd(" << e->x() << "," << e->y() << ")" << std::endl;
    if (!m_model || !m_editing) return;
    m_editingCommand->finish();
    m_editingCommand = 0;
    m_editing = false;
}

void
TimeValueLayer::editStart(View *v, QMouseEvent *e)
{
    std::cerr << "TimeValueLayer::editStart(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model) return;

    SparseTimeValueModel::PointList points = getLocalPoints(v, e->x());
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
TimeValueLayer::editDrag(View *v, QMouseEvent *e)
{
    std::cerr << "TimeValueLayer::editDrag(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model || !m_editing) return;

    long frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    float value = getValueForY(v, e->y());

    if (!m_editingCommand) {
	m_editingCommand = new SparseTimeValueModel::EditCommand(m_model,
								 tr("Drag Point"));
    }

    m_editingCommand->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_editingPoint.value = value;
    m_editingCommand->addPoint(m_editingPoint);
}

void
TimeValueLayer::editEnd(View *v, QMouseEvent *e)
{
    std::cerr << "TimeValueLayer::editEnd(" << e->x() << "," << e->y() << ")" << std::endl;
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
TimeValueLayer::editOpen(View *v, QMouseEvent *e)
{
    if (!m_model) return;

    SparseTimeValueModel::PointList points = getLocalPoints(v, e->x());
    if (points.empty()) return;

    SparseTimeValueModel::Point point = *points.begin();

    ItemEditDialog *dialog = new ItemEditDialog
        (m_model->getSampleRate(),
         ItemEditDialog::ShowTime |
         ItemEditDialog::ShowValue |
         ItemEditDialog::ShowText,
         m_model->getScaleUnits());

    dialog->setFrameTime(point.frame);
    dialog->setValue(point.value);
    dialog->setText(point.label);

    if (dialog->exec() == QDialog::Accepted) {

        SparseTimeValueModel::Point newPoint = point;
        newPoint.frame = dialog->getFrameTime();
        newPoint.value = dialog->getValue();
        newPoint.label = dialog->getText();
        
        SparseTimeValueModel::EditCommand *command =
            new SparseTimeValueModel::EditCommand(m_model, tr("Edit Point"));
        command->deletePoint(point);
        command->addPoint(newPoint);
        command->finish();
    }

    delete dialog;
}

void
TimeValueLayer::moveSelection(Selection s, size_t newStartFrame)
{
    SparseTimeValueModel::EditCommand *command =
	new SparseTimeValueModel::EditCommand(m_model,
					      tr("Drag Selection"));

    SparseTimeValueModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (SparseTimeValueModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

	if (s.contains(i->frame)) {
	    SparseTimeValueModel::Point newPoint(*i);
	    newPoint.frame = i->frame + newStartFrame - s.getStartFrame();
	    command->deletePoint(*i);
	    command->addPoint(newPoint);
	}
    }

    command->finish();
}

void
TimeValueLayer::resizeSelection(Selection s, Selection newSize)
{
    SparseTimeValueModel::EditCommand *command =
	new SparseTimeValueModel::EditCommand(m_model,
					      tr("Resize Selection"));

    SparseTimeValueModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    double ratio =
	double(newSize.getEndFrame() - newSize.getStartFrame()) /
	double(s.getEndFrame() - s.getStartFrame());

    for (SparseTimeValueModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

	if (s.contains(i->frame)) {

	    double target = i->frame;
	    target = newSize.getStartFrame() + 
		double(target - s.getStartFrame()) * ratio;

	    SparseTimeValueModel::Point newPoint(*i);
	    newPoint.frame = lrint(target);
	    command->deletePoint(*i);
	    command->addPoint(newPoint);
	}
    }

    command->finish();
}

void
TimeValueLayer::deleteSelection(Selection s)
{
    SparseTimeValueModel::EditCommand *command =
	new SparseTimeValueModel::EditCommand(m_model,
					      tr("Delete Selected Points"));

    SparseTimeValueModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (SparseTimeValueModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

        if (s.contains(i->frame)) {
            command->deletePoint(*i);
        }
    }

    command->finish();
}    

void
TimeValueLayer::copy(Selection s, Clipboard &to)
{
    SparseTimeValueModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (SparseTimeValueModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {
	if (s.contains(i->frame)) {
            Clipboard::Point point(i->frame, i->value, i->label);
            to.addPoint(point);
        }
    }
}

void
TimeValueLayer::paste(const Clipboard &from, int frameOffset)
{
    const Clipboard::PointList &points = from.getPoints();

    SparseTimeValueModel::EditCommand *command =
	new SparseTimeValueModel::EditCommand(m_model, tr("Paste"));

    for (Clipboard::PointList::const_iterator i = points.begin();
         i != points.end(); ++i) {
        
        if (!i->haveFrame()) continue;
        size_t frame = 0;
        if (frameOffset > 0 || -frameOffset < i->getFrame()) {
            frame = i->getFrame() + frameOffset;
        }
        SparseTimeValueModel::Point newPoint(frame);
  
        if (i->haveLabel()) newPoint.label = i->getLabel();
        if (i->haveValue()) newPoint.value = i->getValue();
        else newPoint.value = (m_model->getValueMinimum() +
                               m_model->getValueMaximum()) / 2;
        
        command->addPoint(newPoint);
    }

    command->finish();
}

QString
TimeValueLayer::toXmlString(QString indent, QString extraAttributes) const
{
    return Layer::toXmlString(indent, extraAttributes +
			      QString(" colour=\"%1\" plotStyle=\"%2\"")
			      .arg(encodeColour(m_colour)).arg(m_plotStyle));
}

void
TimeValueLayer::setProperties(const QXmlAttributes &attributes)
{
    QString colourSpec = attributes.value("colour");
    if (colourSpec != "") {
	QColor colour(colourSpec);
	if (colour.isValid()) {
	    setBaseColour(QColor(colourSpec));
	}
    }

    bool ok;
    PlotStyle style = (PlotStyle)
	attributes.value("plotStyle").toInt(&ok);
    if (ok) setPlotStyle(style);
}


#ifdef INCLUDE_MOCFILES
#include "TimeValueLayer.moc.cpp"
#endif

