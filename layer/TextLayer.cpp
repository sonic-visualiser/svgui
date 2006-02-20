/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#include "TextLayer.h"

#include "base/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/View.h"

#include "model/TextModel.h"

#include <QPainter>
#include <QMouseEvent>
#include <QInputDialog>

#include <iostream>
#include <cmath>

TextLayer::TextLayer(View *w) :
    Layer(w),
    m_model(0),
    m_editing(false),
    m_originalPoint(0, 0.0, tr("Empty Label")),
    m_editingPoint(0, 0.0, tr("Empty Label")),
    m_editingCommand(0),
    m_colour(255, 150, 50) // orange
{
    m_view->addLayer(this);
}

void
TextLayer::setModel(TextModel *model)
{
    if (m_model == model) return;
    m_model = model;

    connect(m_model, SIGNAL(modelChanged()), this, SIGNAL(modelChanged()));
    connect(m_model, SIGNAL(modelChanged(size_t, size_t)),
	    this, SIGNAL(modelChanged(size_t, size_t)));

    connect(m_model, SIGNAL(completionChanged()),
	    this, SIGNAL(modelCompletionChanged()));

//    std::cerr << "TextLayer::setModel(" << model << ")" << std::endl;

    emit modelReplaced();
}

Layer::PropertyList
TextLayer::getProperties() const
{
    PropertyList list;
    list.push_back(tr("Colour"));
    return list;
}

Layer::PropertyType
TextLayer::getPropertyType(const PropertyName &name) const
{
    return ValueProperty;
}

int
TextLayer::getPropertyRangeAndValue(const PropertyName &name,
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

    } else {
	
	deft = Layer::getPropertyRangeAndValue(name, min, max);
    }

    return deft;
}

QString
TextLayer::getPropertyValueLabel(const PropertyName &name,
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
TextLayer::setProperty(const PropertyName &name, int value)
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
TextLayer::setBaseColour(QColor colour)
{
    if (m_colour == colour) return;
    m_colour = colour;
    emit layerParametersChanged();
}

bool
TextLayer::isLayerScrollable() const
{
    QPoint discard;
    return !m_view->shouldIlluminateLocalFeatures(this, discard);
}


TextModel::PointList
TextLayer::getLocalPoints(int x, int y) const
{
    if (!m_model) return TextModel::PointList();

    long frame0 = getFrameForX(-150);
    long frame1 = getFrameForX(m_view->width() + 150);
    
    TextModel::PointList points(m_model->getPoints(frame0, frame1));

    TextModel::PointList rv;
    QFontMetrics metrics = QPainter().fontMetrics();

    for (TextModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

	const TextModel::Point &p(*i);

	int px = getXForFrame(p.frame);
	int py = getYForHeight(p.height);

	QString label = p.label;
	if (label == "") {
	    label = tr("<no text>");
	}

	QRect rect = metrics.boundingRect
	    (QRect(0, 0, 150, 200),
	     Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, label);

	if (py + rect.height() > m_view->height()) {
	    if (rect.height() > m_view->height()) py = 0;
	    else py = m_view->height() - rect.height() - 1;
	}

	if (x >= px && x < px + rect.width() &&
	    y >= py && y < py + rect.height()) {
	    rv.insert(p);
	}
    }

    return rv;
}

QString
TextLayer::getFeatureDescription(QPoint &pos) const
{
    int x = pos.x();

    if (!m_model || !m_model->getSampleRate()) return "";

    TextModel::PointList points = getLocalPoints(x, pos.y());

    if (points.empty()) {
	if (!m_model->isReady()) {
	    return tr("In progress");
	} else {
	    return "";
	}
    }

    long useFrame = points.begin()->frame;

    RealTime rt = RealTime::frame2RealTime(useFrame, m_model->getSampleRate());
    
    QString text;

    if (points.begin()->label == "") {
	text = QString(tr("Time:\t%1\nHeight:\t%2\nLabel:\t%3"))
	    .arg(rt.toText(true).c_str())
	    .arg(points.begin()->height)
	    .arg(points.begin()->label);
    }

    pos = QPoint(getXForFrame(useFrame), getYForHeight(points.begin()->height));
    return text;
}


//!!! too much overlap with TimeValueLayer/TimeInstantLayer

bool
TextLayer::snapToFeatureFrame(int &frame,
			      size_t &resolution,
			      SnapType snap) const
{
    if (!m_model) {
	return Layer::snapToFeatureFrame(frame, resolution, snap);
    }

    resolution = m_model->getResolution();
    TextModel::PointList points;

    if (snap == SnapNeighbouring) {
	
	points = getLocalPoints(getXForFrame(frame), -1);
	if (points.empty()) return false;
	frame = points.begin()->frame;
	return true;
    }    

    points = m_model->getPoints(frame, frame);
    int snapped = frame;
    bool found = false;

    for (TextModel::PointList::const_iterator i = points.begin();
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

	    TextModel::PointList::const_iterator j = i;
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
TextLayer::getYForHeight(float height) const
{
    int h = m_view->height();
    return h - int(height * h);
}

float
TextLayer::getHeightForY(int y) const
{
    int h = m_view->height();
    return float(h - y) / h;
}

void
TextLayer::paint(QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) return;

    int sampleRate = m_model->getSampleRate();
    if (!sampleRate) return;

//    Profiler profiler("TextLayer::paint", true);

    int x0 = rect.left(), x1 = rect.right();
    long frame0 = getFrameForX(x0);
    long frame1 = getFrameForX(x1);

    TextModel::PointList points(m_model->getPoints(frame0, frame1));
    if (points.empty()) return;

    QColor brushColour(m_colour);

    int h, s, v;
    brushColour.getHsv(&h, &s, &v);
    brushColour.setHsv(h, s, 255, 100);

    QColor penColour;
    if (m_view->hasLightBackground()) {
	penColour = Qt::black;
    } else {
	penColour = Qt::white;
    }

//    std::cerr << "TextLayer::paint: resolution is "
//	      << m_model->getResolution() << " frames" << std::endl;

    QPoint localPos;
    long illuminateFrame = -1;

    if (m_view->shouldIlluminateLocalFeatures(this, localPos)) {
	TextModel::PointList localPoints = getLocalPoints(localPos.x(),
							  localPos.y());
	if (!localPoints.empty()) illuminateFrame = localPoints.begin()->frame;
    }

    int boxMaxWidth = 150;
    int boxMaxHeight = 200;

    paint.save();
    paint.setClipRect(rect.x(), 0, rect.width() + boxMaxWidth, m_view->height());
    
    for (TextModel::PointList::const_iterator i = points.begin();
	 i != points.end(); ++i) {

	const TextModel::Point &p(*i);

	int x = getXForFrame(p.frame);
	int y = getYForHeight(p.height);

	if (illuminateFrame == p.frame) {
	    paint.setBrush(penColour);
	    if (m_view->hasLightBackground()) {
		paint.setPen(Qt::white);
	    } else {
		paint.setPen(Qt::black);
	    }
	} else {
	    paint.setPen(penColour);
	    paint.setBrush(brushColour);
	}

	QString label = p.label;
	if (label == "") {
	    label = tr("<no text>");
	}

	QRect boxRect = paint.fontMetrics().boundingRect
	    (QRect(0, 0, boxMaxWidth, boxMaxHeight),
	     Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, label);

	QRect textRect = QRect(3, 2, boxRect.width(), boxRect.height());
	boxRect = QRect(0, 0, boxRect.width() + 6, boxRect.height() + 2);

	if (y + boxRect.height() > m_view->height()) {
	    if (boxRect.height() > m_view->height()) y = 0;
	    else y = m_view->height() - boxRect.height() - 1;
	}

	boxRect = QRect(x, y, boxRect.width(), boxRect.height());
	textRect = QRect(x + 3, y + 2, textRect.width(), textRect.height());

//	boxRect = QRect(x, y, boxRect.width(), boxRect.height());
//	textRect = QRect(x + 3, y + 2, textRect.width(), textRect.height());

	paint.setRenderHint(QPainter::Antialiasing, false);
	paint.drawRect(boxRect);

	paint.setRenderHint(QPainter::Antialiasing, true);
	paint.drawText(textRect,
		       Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
		       label);

///	if (p.label != "") {
///	    paint.drawText(x + 5, y - paint.fontMetrics().height() + paint.fontMetrics().ascent(), p.label);
///	}
    }

    paint.restore();

    // looks like save/restore doesn't deal with this:
    paint.setRenderHint(QPainter::Antialiasing, false);
}

void
TextLayer::drawStart(QMouseEvent *e)
{
//    std::cerr << "TextLayer::drawStart(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model) {
	std::cerr << "TextLayer::drawStart: no model" << std::endl;
	return;
    }

    long frame = getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    float height = getHeightForY(e->y());

    m_editingPoint = TextModel::Point(frame, height, "");
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) m_editingCommand->finish();
    m_editingCommand = new TextModel::EditCommand(m_model, "Add Label");
    m_editingCommand->addPoint(m_editingPoint);

    m_editing = true;
}

void
TextLayer::drawDrag(QMouseEvent *e)
{
//    std::cerr << "TextLayer::drawDrag(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model || !m_editing) return;

    long frame = getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    float height = getHeightForY(e->y());

    m_editingCommand->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_editingPoint.height = height;
    m_editingCommand->addPoint(m_editingPoint);
}

void
TextLayer::drawEnd(QMouseEvent *e)
{
//    std::cerr << "TextLayer::drawEnd(" << e->x() << "," << e->y() << ")" << std::endl;
    if (!m_model || !m_editing) return;

    bool ok = false;
    QString label = QInputDialog::getText(m_view, tr("Enter label"),
					  tr("Please enter a new label:"),
					  QLineEdit::Normal, "", &ok);

    if (ok) {
	TextModel::RelabelCommand *command =
	    new TextModel::RelabelCommand(m_model, m_editingPoint, label);
	m_editingCommand->addCommand(command);
    }

    m_editingCommand->finish();
    m_editingCommand = 0;
    m_editing = false;
}

void
TextLayer::editStart(QMouseEvent *e)
{
//    std::cerr << "TextLayer::editStart(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model) return;

    TextModel::PointList points = getLocalPoints(e->x(), e->y());
    if (points.empty()) return;

    m_editOrigin = e->pos();
    m_editingPoint = *points.begin();
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) {
	m_editingCommand->finish();
	m_editingCommand = 0;
    }

    m_editing = true;
}

void
TextLayer::editDrag(QMouseEvent *e)
{
    if (!m_model || !m_editing) return;

    long frameDiff = getFrameForX(e->x()) - getFrameForX(m_editOrigin.x());
    float heightDiff = getHeightForY(e->y()) - getHeightForY(m_editOrigin.y());

    long frame = m_originalPoint.frame + frameDiff;
    float height = m_originalPoint.height + heightDiff;

//    long frame = getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = (frame / m_model->getResolution()) * m_model->getResolution();

//    float height = getHeightForY(e->y());

    if (!m_editingCommand) {
	m_editingCommand = new TextModel::EditCommand(m_model, tr("Drag Label"));
    }

    m_editingCommand->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_editingPoint.height = height;
    m_editingCommand->addPoint(m_editingPoint);
}

void
TextLayer::editEnd(QMouseEvent *e)
{
//    std::cerr << "TextLayer::editEnd(" << e->x() << "," << e->y() << ")" << std::endl;
    if (!m_model || !m_editing) return;

    if (m_editingCommand) {

	QString newName = m_editingCommand->getName();

	if (m_editingPoint.frame != m_originalPoint.frame) {
	    if (m_editingPoint.height != m_originalPoint.height) {
		newName = tr("Move Label");
	    } else {
		newName = tr("Move Label Horizontally");
	    }
	} else {
	    newName = tr("Move Label Vertically");
	}

	m_editingCommand->setName(newName);
	m_editingCommand->finish();
    }
    
    m_editingCommand = 0;
    m_editing = false;
}

void
TextLayer::editOpen(QMouseEvent *e)
{
    std::cerr << "TextLayer::editOpen" << std::endl;

    if (!m_model) return;

    TextModel::PointList points = getLocalPoints(e->x(), e->y());
    if (points.empty()) return;

    QString label = points.begin()->label;

    bool ok = false;
    label = QInputDialog::getText(m_view, tr("Enter label"),
				  tr("Please enter a new label:"),
				  QLineEdit::Normal, label, &ok);
    if (ok && label != points.begin()->label) {
	TextModel::RelabelCommand *command =
	    new TextModel::RelabelCommand(m_model, *points.begin(), label);
	CommandHistory::getInstance()->addCommand(command, true);
    }
}    

QString
TextLayer::toXmlString(QString indent, QString extraAttributes) const
{
    return Layer::toXmlString(indent, extraAttributes +
			      QString(" colour=\"%1\"")
			      .arg(encodeColour(m_colour)));
}

void
TextLayer::setProperties(const QXmlAttributes &attributes)
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
#include "TextLayer.moc.cpp"
#endif

