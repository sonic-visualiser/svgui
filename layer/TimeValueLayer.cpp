/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#include "TimeValueLayer.h"

#include "base/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/View.h"

#include "model/SparseTimeValueModel.h"

#include <QPainter>
#include <QPainterPath>

#include <iostream>
#include <cmath>

TimeValueLayer::TimeValueLayer(View *w) :
    Layer(w),
    m_model(0),
    m_colour(Qt::black),
    m_plotStyle(PlotConnectedPoints)
{
    m_view->addLayer(this);
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
	if (max) *max = 4;
	
	deft = int(m_plotStyle);

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

bool
TimeValueLayer::isLayerScrollable() const
{
    // We don't illuminate sections in the line or curve modes, so
    // they're always scrollable

    if (m_plotStyle == PlotLines ||
	m_plotStyle == PlotCurve) return true;

    QPoint discard;
    return !m_view->shouldIlluminateLocalFeatures(this, discard);
}

QRect
TimeValueLayer::getFeatureDescriptionRect(QPainter &paint, QPoint pos) const
{
    return QRect(0, 0,
		 std::max(100, paint.fontMetrics().width(tr("No local points"))),
		 70); //!!!
}

//!!! too much in common with TimeInstantLayer

SparseTimeValueModel::PointList
TimeValueLayer::getLocalPoints(int x) const
{
    if (!m_model) return SparseTimeValueModel::PointList();

    long startFrame = m_view->getStartFrame();
    long endFrame = m_view->getEndFrame();
    int zoomLevel = m_view->getZoomLevel();
    long frame = startFrame + x * zoomLevel;

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
    } else if (prevPoints.begin()->frame < startFrame &&
	       !(nextPoints.begin()->frame > endFrame)) {
	usePoints = nextPoints;
    } else if (nextPoints.begin()->frame - frame <
	       frame - prevPoints.begin()->frame) {
	usePoints = nextPoints;
    }

    return usePoints;
}

void
TimeValueLayer::paintLocalFeatureDescription(QPainter &paint, QRect rect,
					     QPoint pos) const
{
    //!!! bleagh

    int x = pos.x();
    
    if (!m_model || !m_model->getSampleRate()) return;

    SparseTimeValueModel::PointList points = getLocalPoints(x);

    QFontMetrics metrics = paint.fontMetrics();
    int xbase = rect.x() + 5;
    int ybase = rect.y() + 5;

    if (points.empty()) {
	QString label = tr("No local points");
	if (!m_model->isReady()) {
	    label = tr("In progress");
	}
	paint.drawText(xbase + 5, ybase + 5 + metrics.ascent(), label);
	return;
    }

    long useFrame = points.begin()->frame;

    RealTime rt = RealTime::frame2RealTime(useFrame, m_model->getSampleRate());
    QString timeText = QString("%1").arg(rt.toText(true).c_str());
    QString valueText = QString("%1").arg(points.begin()->value);

    int timewidth = metrics.width(timeText);
    int valuewidth = metrics.width(valueText);
    int labelwidth = metrics.width(points.begin()->label);

    int boxheight = metrics.height() * 3 + 4;
    int boxwidth = std::max(std::max(timewidth, labelwidth), valuewidth);
    
    paint.drawRect(xbase, ybase, boxwidth + 10,
		   boxheight + 10 - metrics.descent() + 1);

    paint.drawText(xbase + 5, ybase + 5 + metrics.ascent(), timeText);
    paint.drawText(xbase + 5, ybase + 7 + metrics.ascent() + metrics.height(),
		   valueText);
    paint.drawText(xbase + 5, ybase + 9 + metrics.ascent() + 2*metrics.height(),
		   points.begin()->label);
}

int
TimeValueLayer::getNearestFeatureFrame(int frame,
				       size_t &resolution,
				       bool snapRight) const
{
    if (!m_model) {
	return Layer::getNearestFeatureFrame(frame, resolution, snapRight);
    }

    resolution = m_model->getResolution();
    SparseTimeValueModel::PointList points(m_model->getPoints(frame, frame));

    int returnFrame = frame;

    for (SparseTimeValueModel::PointList::const_iterator i = points.begin();
	 i != points.end(); ++i) {

	if (snapRight) {
	    if (i->frame > frame) {
		returnFrame = i->frame;
		break;
	    }
	} else {
	    if (i->frame <= frame) {
		returnFrame = i->frame;
	    }
	}
    }

    return returnFrame;
}

void
TimeValueLayer::paint(QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) return;

    int sampleRate = m_model->getSampleRate();
    if (!sampleRate) return;

//    Profiler profiler("TimeValueLayer::paint", true);

    long startFrame = m_view->getStartFrame();
    int zoomLevel = m_view->getZoomLevel();

    int x0 = rect.left(), x1 = rect.right();
    long frame0 = startFrame + x0 * zoomLevel;
    long frame1 = startFrame + x1 * zoomLevel;

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

    int origin = int(nearbyint(m_view->height() -
			       (-min * m_view->height()) / (max - min)));

    QPoint localPos;
    long illuminateFrame = -1;

    if (m_view->shouldIlluminateLocalFeatures(this, localPos)) {
	SparseTimeValueModel::PointList localPoints =
	    getLocalPoints(localPos.x());
	if (!localPoints.empty()) illuminateFrame = localPoints.begin()->frame;
    }

    int w = m_model->getResolution() / zoomLevel;

    paint.save();

    if (w > 1 &&
	(m_plotStyle == PlotLines ||
	 m_plotStyle == PlotCurve)) {
	paint.setRenderHint(QPainter::Antialiasing, true);
    }
    QPainterPath path;
    
    for (SparseTimeValueModel::PointList::const_iterator i = points.begin();
	 i != points.end(); ++i) {

	const SparseTimeValueModel::Point &p(*i);

	int x = (p.frame - startFrame) / zoomLevel;
	int y = int(nearbyint(m_view->height() -
			      ((p.value - min) * m_view->height()) /
			      (max - min)));

	if (w < 1) w = 1;

	if (m_plotStyle == PlotLines ||
	    m_plotStyle == PlotCurve) {
	    paint.setPen(m_colour);
	    paint.setBrush(Qt::NoBrush);
	} else {
	    paint.setPen(m_colour);
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
		paint.setBrush(Qt::black);//!!!
	    }	    
	}

	if (m_plotStyle != PlotLines &&
	    m_plotStyle != PlotCurve) {
	    paint.drawRect(x, y - 1, w, 2);
	}

	if (m_plotStyle == PlotConnectedPoints ||
	    m_plotStyle == PlotLines ||
	    m_plotStyle == PlotCurve) {

	    SparseTimeValueModel::PointList::const_iterator j = i;
	    ++j;

	    if (j != points.end()) {

		const SparseTimeValueModel::Point &q(*j);
		int nx = (q.frame - startFrame) / zoomLevel;
		int ny = int(nearbyint(m_view->height() -
				       ((q.value - min) * m_view->height()) /
				       (max - min)));

		if (m_plotStyle == PlotConnectedPoints) {

		    paint.setPen(brushColour);
		    paint.drawLine(x + w, y, nx, ny);

		} else if (m_plotStyle == PlotLines) {

		    paint.drawLine(x + w/2, y, nx + w/2, ny);

		} else {

		    if (path.isEmpty()) {
			path.moveTo(x + w/2, y);
		    }

		    if (nx - x > 5) {
			path.cubicTo(x + w, y, nx, ny, nx + w/2, ny);
		    } else {
			path.lineTo(nx + w/2, ny);
		    }
		}
	    }
	}

///	if (p.label != "") {
///	    paint.drawText(x + 5, y - paint.fontMetrics().height() + paint.fontMetrics().ascent(), p.label);
///	}
    }

    if (m_plotStyle == PlotCurve && !path.isEmpty()) {
	paint.drawPath(path);
    }

    paint.restore();

    // looks like save/restore doesn't deal with this:
    paint.setRenderHint(QPainter::Antialiasing, false);
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

