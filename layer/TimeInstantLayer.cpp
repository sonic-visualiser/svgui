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

TimeInstantLayer::TimeInstantLayer(View *w) :
    Layer(w),
    m_model(0),
    m_editing(false),
    m_editingPoint(0, tr("New Point")),
    m_colour(QColor(200, 50, 255))
{
    m_view->addLayer(this);
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
    }
}

void
TimeInstantLayer::setBaseColour(QColor colour)
{
    if (m_colour == colour) return;
    m_colour = colour;
    emit layerParametersChanged();
}

bool
TimeInstantLayer::isLayerScrollable() const
{
    QPoint discard;
    return !m_view->shouldIlluminateLocalFeatures(this, discard);
}

QRect
TimeInstantLayer::getFeatureDescriptionRect(QPainter &paint, QPoint pos) const
{
    return QRect(0, 0,
		 std::max(100, paint.fontMetrics().width(tr("No local points"))),
		 50); //!!! cruddy
}

SparseOneDimensionalModel::PointList
TimeInstantLayer::getLocalPoints(int x) const
{
    if (!m_model) return SparseOneDimensionalModel::PointList();

    long frame = getFrameForX(x);

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
    } else if (prevPoints.begin()->frame < m_view->getStartFrame() &&
	       !(nextPoints.begin()->frame > m_view->getEndFrame())) {
	usePoints = nextPoints;
    } else if (nextPoints.begin()->frame - frame <
	       frame - prevPoints.begin()->frame) {
	usePoints = nextPoints;
    }

    return usePoints;
}

void
TimeInstantLayer::paintLocalFeatureDescription(QPainter &paint, QRect rect,
					       QPoint pos) const
{
    //!!! bleagh

    int x = pos.x();
    
    if (!m_model || !m_model->getSampleRate()) return;

    SparseOneDimensionalModel::PointList points = getLocalPoints(x);

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
    QString timeText = QString(tr("Time %1")).arg(rt.toText(true).c_str());

    int timewidth = metrics.width(timeText);
    int labelwidth = metrics.width(points.begin()->label);

    int boxheight = metrics.height() * 2 + 3;
    int boxwidth = std::max(timewidth, labelwidth);

    paint.drawRect(xbase, ybase, boxwidth + 10,
		   boxheight + 10 - metrics.descent() + 1);

    paint.drawText(xbase + 5, ybase + 5 + metrics.ascent(), timeText);
    paint.drawText(xbase + 5, ybase + 7 + metrics.ascent() + metrics.height(),
		   points.begin()->label);
}

int
TimeInstantLayer::getNearestFeatureFrame(int frame,
					 size_t &resolution,
					 bool snapRight) const
{
    if (!m_model) {
	return Layer::getNearestFeatureFrame(frame, resolution, snapRight);
    }

    resolution = m_model->getResolution();
    SparseOneDimensionalModel::PointList points(m_model->getPoints(frame, frame));

    int returnFrame = frame;

    for (SparseOneDimensionalModel::PointList::const_iterator i = points.begin();
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
TimeInstantLayer::paint(QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) return;

//    Profiler profiler("TimeInstantLayer::paint", true);

    int x0 = rect.left(), x1 = rect.right();

    long frame0 = getFrameForX(x0);
    long frame1 = getFrameForX(x1);

    SparseOneDimensionalModel::PointList points(m_model->getPoints
						(frame0, frame1));

    paint.setPen(m_colour);

    QColor brushColour(m_colour);
    brushColour.setAlpha(100);
    paint.setBrush(brushColour);

//    std::cerr << "TimeInstantLayer::paint: resolution is "
//	      << m_model->getResolution() << " frames" << std::endl;

    QPoint localPos;
    long illuminateFrame = -1;

    if (m_view->shouldIlluminateLocalFeatures(this, localPos)) {
	SparseOneDimensionalModel::PointList localPoints =
	    getLocalPoints(localPos.x());
	if (!localPoints.empty()) illuminateFrame = localPoints.begin()->frame;
    }
	
    for (SparseOneDimensionalModel::PointList::const_iterator i = points.begin();
	 i != points.end(); ++i) {

	const SparseOneDimensionalModel::Point &p(*i);
	SparseOneDimensionalModel::PointList::const_iterator j = i;
	++j;

	int x = getXForFrame(p.frame);
	int iw = getXForFrame(p.frame + m_model->getResolution()) - x;
	if (iw < 2) {
	    if (iw < 1) {
		iw = 2;
		if (j != points.end()) {
		    int nx = getXForFrame(j->frame);
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
	paint.drawRect(x, 0, iw - 1, m_view->height() - 1);
	paint.setPen(m_colour);
	
	if (p.label != "") {

	    // only draw if there's enough room from here to the next point

	    int lw = paint.fontMetrics().width(p.label);
	    bool good = true;

	    if (j != points.end()) {
		int nx = getXForFrame(j->frame);
		if (nx >= x && nx - x - iw - 3 <= lw) good = false;
	    }

	    if (good) {
		paint.drawText(x + iw + 2,
			       m_view->height() - paint.fontMetrics().height(),
			       p.label);
	    }
	}
    }
}

void
TimeInstantLayer::drawStart(QMouseEvent *e)
{
    std::cerr << "TimeInstantLayer::drawStart(" << e->x() << ")" << std::endl;

    if (!m_model) return;

    long frame = getFrameForX(e->x());
    if (frame < 0) frame = 0;
    m_editingPoint = SparseOneDimensionalModel::Point(frame, tr("New Point"));
    m_model->addPoint(m_editingPoint);
    m_editing = true;
}

void
TimeInstantLayer::drawDrag(QMouseEvent *e)
{
    std::cerr << "TimeInstantLayer::drawDrag(" << e->x() << ")" << std::endl;

    if (!m_model || !m_editing) return;

    long frame = getFrameForX(e->x());
    if (frame < 0) frame = 0;
    m_model->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_model->addPoint(m_editingPoint);
}

void
TimeInstantLayer::drawEnd(QMouseEvent *e)
{
    std::cerr << "TimeInstantLayer::drawEnd(" << e->x() << ")" << std::endl;
    if (!m_model || !m_editing) return;
    m_editing = false;
}

void
TimeInstantLayer::editStart(QMouseEvent *e)
{
    std::cerr << "TimeInstantLayer::editStart(" << e->x() << ")" << std::endl;

    if (!m_model) return;

    SparseOneDimensionalModel::PointList points = getLocalPoints(e->x());
    if (points.empty()) return;

    m_editingPoint = *points.begin();
    m_editing = true;
}

void
TimeInstantLayer::editDrag(QMouseEvent *e)
{
    std::cerr << "TimeInstantLayer::editDrag(" << e->x() << ")" << std::endl;

    if (!m_model || !m_editing) return;

    long frame = getFrameForX(e->x());
    if (frame < 0) frame = 0;
    m_model->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_model->addPoint(m_editingPoint);
}

void
TimeInstantLayer::editEnd(QMouseEvent *e)
{
    std::cerr << "TimeInstantLayer::editEnd(" << e->x() << ")" << std::endl;
    if (!m_model || !m_editing) return;
    m_editing = false;
}

QString
TimeInstantLayer::toXmlString(QString indent, QString extraAttributes) const
{
    return Layer::toXmlString(indent, extraAttributes +
			      QString(" colour=\"%1\"").arg(encodeColour(m_colour)));
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
}

#ifdef INCLUDE_MOCFILES
#include "TimeInstantLayer.moc.cpp"
#endif

