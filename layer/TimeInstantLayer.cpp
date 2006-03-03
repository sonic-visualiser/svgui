/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#include "TimeInstantLayer.h"

#include "base/Model.h"
#include "base/RealTime.h"
#include "base/View.h"
#include "base/Profiler.h"

#include "model/SparseOneDimensionalModel.h"

#include <QPainter>
#include <QMouseEvent>

#include <iostream>
#include <cmath>

TimeInstantLayer::TimeInstantLayer() :
    Layer(),
    m_model(0),
    m_editing(false),
    m_editingPoint(0, tr("New Point")),
    m_editingCommand(0),
    m_colour(QColor(200, 50, 255)),
    m_plotStyle(PlotInstants)
{
    
}

void
TimeInstantLayer::setModel(SparseOneDimensionalModel *model)
{
    if (m_model == model) return;
    m_model = model;

    connect(m_model, SIGNAL(modelChanged()), this, SIGNAL(modelChanged()));
    connect(m_model, SIGNAL(modelChanged(size_t, size_t)),
	    this, SIGNAL(modelChanged(size_t, size_t)));

    connect(m_model, SIGNAL(completionChanged()),
	    this, SIGNAL(modelCompletionChanged()));

    std::cerr << "TimeInstantLayer::setModel(" << model << ")" << std::endl;

    emit modelReplaced();
}

Layer::PropertyList
TimeInstantLayer::getProperties() const
{
    PropertyList list;
    list.push_back(tr("Colour"));
    list.push_back(tr("Plot Type"));
    return list;
}

Layer::PropertyType
TimeInstantLayer::getPropertyType(const PropertyName &) const
{
    return ValueProperty;
}

int
TimeInstantLayer::getPropertyRangeAndValue(const PropertyName &name,
					 int *min, int *max) const
{
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
	if (max) *max = 1;
	
	deft = int(m_plotStyle);

    } else {
	
	deft = Layer::getPropertyRangeAndValue(name, min, max);
    }

    return deft;
}

QString
TimeInstantLayer::getPropertyValueLabel(const PropertyName &name,
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
	case 0: return tr("Instants");
	case 1: return tr("Segmentation");
	}
    }
    return tr("<unknown>");
}

void
TimeInstantLayer::setProperty(const PropertyName &name, int value)
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
TimeInstantLayer::setBaseColour(QColor colour)
{
    if (m_colour == colour) return;
    m_colour = colour;
    emit layerParametersChanged();
}

void
TimeInstantLayer::setPlotStyle(PlotStyle style)
{
    if (m_plotStyle == style) return;
    m_plotStyle = style;
    emit layerParametersChanged();
}

bool
TimeInstantLayer::isLayerScrollable(const View *v) const
{
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

SparseOneDimensionalModel::PointList
TimeInstantLayer::getLocalPoints(View *v, int x) const
{
    // Return a set of points that all have the same frame number, the
    // nearest to the given x coordinate, and that are within a
    // certain fuzz distance of that x coordinate.

    if (!m_model) return SparseOneDimensionalModel::PointList();

    long frame = v->getFrameForX(x);

    SparseOneDimensionalModel::PointList onPoints =
	m_model->getPoints(frame);

    if (!onPoints.empty()) {
	return onPoints;
    }

    SparseOneDimensionalModel::PointList prevPoints =
	m_model->getPreviousPoints(frame);
    SparseOneDimensionalModel::PointList nextPoints =
	m_model->getNextPoints(frame);

    SparseOneDimensionalModel::PointList usePoints = prevPoints;

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
TimeInstantLayer::getFeatureDescription(View *v, QPoint &pos) const
{
    int x = pos.x();

    if (!m_model || !m_model->getSampleRate()) return "";

    SparseOneDimensionalModel::PointList points = getLocalPoints(v, x);

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
	text = QString(tr("Time:\t%1\nNo label"))
	    .arg(rt.toText(true).c_str());
    } else {
	text = QString(tr("Time:\t%1\nLabel:\t%2"))
	    .arg(rt.toText(true).c_str())
	    .arg(points.begin()->label);
    }

    pos = QPoint(v->getXForFrame(useFrame), pos.y());
    return text;
}

bool
TimeInstantLayer::snapToFeatureFrame(View *v, int &frame,
				     size_t &resolution,
				     SnapType snap) const
{
    if (!m_model) {
	return Layer::snapToFeatureFrame(v, frame, resolution, snap);
    }

    resolution = m_model->getResolution();
    SparseOneDimensionalModel::PointList points;

    if (snap == SnapNeighbouring) {
	
	points = getLocalPoints(v, v->getXForFrame(frame));
	if (points.empty()) return false;
	frame = points.begin()->frame;
	return true;
    }    

    points = m_model->getPoints(frame, frame);
    int snapped = frame;
    bool found = false;

    for (SparseOneDimensionalModel::PointList::const_iterator i = points.begin();
	 i != points.end(); ++i) {

	if (snap == SnapRight) {

	    if (i->frame >= frame) {
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

	    SparseOneDimensionalModel::PointList::const_iterator j = i;
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
TimeInstantLayer::paint(View *v, QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) return;

//    Profiler profiler("TimeInstantLayer::paint", true);

    int x0 = rect.left(), x1 = rect.right();

    long frame0 = v->getFrameForX(x0);
    long frame1 = v->getFrameForX(x1);

    SparseOneDimensionalModel::PointList points(m_model->getPoints
						(frame0, frame1));

    bool odd = false;
    if (m_plotStyle == PlotSegmentation && !points.empty()) {
	int index = m_model->getIndexOf(*points.begin());
	odd = ((index % 2) == 1);
    }

    paint.setPen(m_colour);

    QColor brushColour(m_colour);
    brushColour.setAlpha(100);
    paint.setBrush(brushColour);

    QColor oddBrushColour(brushColour);
    if (m_plotStyle == PlotSegmentation) {
	if (m_colour == Qt::black) {
	    oddBrushColour = Qt::gray;
	} else if (m_colour == Qt::darkRed) {
	    oddBrushColour = Qt::red;
	} else if (m_colour == Qt::darkBlue) {
	    oddBrushColour = Qt::blue;
	} else if (m_colour == Qt::darkGreen) {
	    oddBrushColour = Qt::green;
	} else {
	    oddBrushColour = oddBrushColour.light(150);
	}
	oddBrushColour.setAlpha(100);
    }

//    std::cerr << "TimeInstantLayer::paint: resolution is "
//	      << m_model->getResolution() << " frames" << std::endl;

    QPoint localPos;
    long illuminateFrame = -1;

    if (v->shouldIlluminateLocalFeatures(this, localPos)) {
	SparseOneDimensionalModel::PointList localPoints =
	    getLocalPoints(v, localPos.x());
	if (!localPoints.empty()) illuminateFrame = localPoints.begin()->frame;
    }
	
    int prevX = -1;

    for (SparseOneDimensionalModel::PointList::const_iterator i = points.begin();
	 i != points.end(); ++i) {

	const SparseOneDimensionalModel::Point &p(*i);
	SparseOneDimensionalModel::PointList::const_iterator j = i;
	++j;

	int x = v->getXForFrame(p.frame);
	if (x == prevX && p.frame != illuminateFrame) continue;

	int iw = v->getXForFrame(p.frame + m_model->getResolution()) - x;
	if (iw < 2) {
	    if (iw < 1) {
		iw = 2;
		if (j != points.end()) {
		    int nx = v->getXForFrame(j->frame);
		    if (nx < x + 3) iw = 1;
		}
	    } else {
		iw = 2;
	    }
	}
		
	if (p.frame == illuminateFrame) {
	    paint.setPen(Qt::black); //!!!
	} else {
	    paint.setPen(brushColour);
	}

	if (m_plotStyle == PlotInstants) {
	    if (iw > 1) {
		paint.drawRect(x, 0, iw - 1, v->height() - 1);
	    } else {
		paint.drawLine(x, 0, x, v->height() - 1);
	    }
	} else {

	    if (odd) paint.setBrush(oddBrushColour);
	    else paint.setBrush(brushColour);
	    
	    int nx;
	    
	    if (j != points.end()) {
		const SparseOneDimensionalModel::Point &q(*j);
		nx = v->getXForFrame(q.frame);
	    } else {
		nx = v->getXForFrame(m_model->getEndFrame());
	    }

	    if (nx >= x) {
		
		if (illuminateFrame != p.frame &&
		    (nx < x + 5 || x >= v->width() - 1)) {
		    paint.setPen(Qt::NoPen);
		}

		paint.drawRect(x, -1, nx - x, v->height() + 1);
	    }

	    odd = !odd;
	}

	paint.setPen(m_colour);
	
	if (p.label != "") {

	    // only draw if there's enough room from here to the next point

	    int lw = paint.fontMetrics().width(p.label);
	    bool good = true;

	    if (j != points.end()) {
		int nx = v->getXForFrame(j->frame);
		if (nx >= x && nx - x - iw - 3 <= lw) good = false;
	    }

	    if (good) {
		paint.drawText(x + iw + 2,
			       v->height() - paint.fontMetrics().height(),
			       p.label);
	    }
	}

	prevX = x;
    }
}

void
TimeInstantLayer::drawStart(View *v, QMouseEvent *e)
{
    std::cerr << "TimeInstantLayer::drawStart(" << e->x() << ")" << std::endl;

    if (!m_model) return;

    long frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    m_editingPoint = SparseOneDimensionalModel::Point(frame, tr("New Point"));

    if (m_editingCommand) m_editingCommand->finish();
    m_editingCommand = new SparseOneDimensionalModel::EditCommand(m_model,
								  tr("Draw Point"));
    m_editingCommand->addPoint(m_editingPoint);

    m_editing = true;
}

void
TimeInstantLayer::drawDrag(View *v, QMouseEvent *e)
{
    std::cerr << "TimeInstantLayer::drawDrag(" << e->x() << ")" << std::endl;

    if (!m_model || !m_editing) return;

    long frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();
    m_editingCommand->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_editingCommand->addPoint(m_editingPoint);
}

void
TimeInstantLayer::drawEnd(View *v, QMouseEvent *e)
{
    std::cerr << "TimeInstantLayer::drawEnd(" << e->x() << ")" << std::endl;
    if (!m_model || !m_editing) return;
    QString newName = tr("Add Point at %1 s")
	.arg(RealTime::frame2RealTime(m_editingPoint.frame,
				      m_model->getSampleRate())
	     .toText(false).c_str());
    m_editingCommand->setName(newName);
    m_editingCommand->finish();
    m_editingCommand = 0;
    m_editing = false;
}

void
TimeInstantLayer::editStart(View *v, QMouseEvent *e)
{
    std::cerr << "TimeInstantLayer::editStart(" << e->x() << ")" << std::endl;

    if (!m_model) return;

    SparseOneDimensionalModel::PointList points = getLocalPoints(v, e->x());
    if (points.empty()) return;

    m_editingPoint = *points.begin();

    if (m_editingCommand) {
	m_editingCommand->finish();
	m_editingCommand = 0;
    }

    m_editing = true;
}

void
TimeInstantLayer::editDrag(View *v, QMouseEvent *e)
{
    std::cerr << "TimeInstantLayer::editDrag(" << e->x() << ")" << std::endl;

    if (!m_model || !m_editing) return;

    long frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    if (!m_editingCommand) {
	m_editingCommand = new SparseOneDimensionalModel::EditCommand(m_model,
								      tr("Drag Point"));
    }

    m_editingCommand->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_editingCommand->addPoint(m_editingPoint);
}

void
TimeInstantLayer::editEnd(View *v, QMouseEvent *e)
{
    std::cerr << "TimeInstantLayer::editEnd(" << e->x() << ")" << std::endl;
    if (!m_model || !m_editing) return;
    if (m_editingCommand) {
	QString newName = tr("Move Point to %1 s")
	    .arg(RealTime::frame2RealTime(m_editingPoint.frame,
					  m_model->getSampleRate())
		 .toText(false).c_str());
	m_editingCommand->setName(newName);
	m_editingCommand->finish();
    }
    m_editingCommand = 0;
    m_editing = false;
}

void
TimeInstantLayer::moveSelection(Selection s, size_t newStartFrame)
{
    SparseOneDimensionalModel::EditCommand *command =
	new SparseOneDimensionalModel::EditCommand(m_model,
						   tr("Drag Selection"));

    SparseOneDimensionalModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (SparseOneDimensionalModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

	if (s.contains(i->frame)) {
	    SparseOneDimensionalModel::Point newPoint(*i);
	    newPoint.frame = i->frame + newStartFrame - s.getStartFrame();
	    command->deletePoint(*i);
	    command->addPoint(newPoint);
	}
    }

    command->finish();
}

void
TimeInstantLayer::resizeSelection(Selection s, Selection newSize)
{
    SparseOneDimensionalModel::EditCommand *command =
	new SparseOneDimensionalModel::EditCommand(m_model,
						   tr("Resize Selection"));

    SparseOneDimensionalModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    double ratio =
	double(newSize.getEndFrame() - newSize.getStartFrame()) /
	double(s.getEndFrame() - s.getStartFrame());

    for (SparseOneDimensionalModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

	if (s.contains(i->frame)) {

	    double target = i->frame;
	    target = newSize.getStartFrame() + 
		double(target - s.getStartFrame()) * ratio;

	    SparseOneDimensionalModel::Point newPoint(*i);
	    newPoint.frame = lrint(target);
	    command->deletePoint(*i);
	    command->addPoint(newPoint);
	}
    }

    command->finish();
}

void
TimeInstantLayer::deleteSelection(Selection s)
{
    SparseOneDimensionalModel::EditCommand *command =
	new SparseOneDimensionalModel::EditCommand(m_model,
						   tr("Delete Selection"));

    SparseOneDimensionalModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (SparseOneDimensionalModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {
	if (s.contains(i->frame)) command->deletePoint(*i);
    }

    command->finish();
}
  

QString
TimeInstantLayer::toXmlString(QString indent, QString extraAttributes) const
{
    return Layer::toXmlString(indent, extraAttributes +
			      QString(" colour=\"%1\" plotStyle=\"%2\"")
			      .arg(encodeColour(m_colour)).arg(m_plotStyle));
}

void
TimeInstantLayer::setProperties(const QXmlAttributes &attributes)
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
#include "TimeInstantLayer.moc.cpp"
#endif

