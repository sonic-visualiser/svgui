/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
   
    This is experimental software.  Not for distribution.
*/

#include "TimeRulerLayer.h"

#include "base/Model.h"
#include "base/RealTime.h"
#include "base/View.h"

#include <QPainter>

#include <iostream>

using std::cerr;
using std::endl;

TimeRulerLayer::TimeRulerLayer() :
    Layer(),
    m_model(0),
    m_colour(Qt::black),
    m_labelHeight(LabelTop)
{
    
}

void
TimeRulerLayer::setModel(Model *model)
{
    if (m_model == model) return;
    m_model = model;
    emit modelReplaced();
}

void
TimeRulerLayer::setBaseColour(QColor colour)
{
    if (m_colour == colour) return;
    m_colour = colour;
    emit layerParametersChanged();
}

Layer::PropertyList
TimeRulerLayer::getProperties() const
{
    PropertyList list;
    list.push_back(tr("Colour"));
    return list;
}

Layer::PropertyType
TimeRulerLayer::getPropertyType(const PropertyName &name) const
{
    return ValueProperty;
}

int
TimeRulerLayer::getPropertyRangeAndValue(const PropertyName &name,
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
TimeRulerLayer::getPropertyValueLabel(const PropertyName &name,
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
TimeRulerLayer::setProperty(const PropertyName &name, int value)
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
TimeRulerLayer::paint(View *v, QPainter &paint, QRect rect) const
{
//    std::cerr << "TimeRulerLayer::paint (" << rect.x() << "," << rect.y()
//	      << ") [" << rect.width() << "x" << rect.height() << "]" << std::endl;
    
    if (!m_model || !m_model->isOK()) return;

    int sampleRate = m_model->getSampleRate();
    if (!sampleRate) return;

    long startFrame = v->getStartFrame();
    long endFrame = v->getEndFrame();

    int zoomLevel = v->getZoomLevel();

    long rectStart = startFrame + (rect.x() - 100) * zoomLevel;
    long rectEnd = startFrame + (rect.x() + rect.width() + 100) * zoomLevel;
    if (rectStart < startFrame) rectStart = startFrame;
    if (rectEnd > endFrame) rectEnd = endFrame;

//    std::cerr << "TimeRulerLayer::paint: calling paint.save()" << std::endl;
    paint.save();
//!!!    paint.setClipRect(v->rect());

    int minPixelSpacing = 50;

    RealTime rtStart = RealTime::frame2RealTime(startFrame, sampleRate);
    RealTime rtEnd = RealTime::frame2RealTime(endFrame, sampleRate);
//    cerr << "startFrame " << startFrame << ", endFrame " << v->getEndFrame() << ", rtStart " << rtStart << ", rtEnd " << rtEnd << endl;
    int count = v->width() / minPixelSpacing;
    if (count < 1) count = 1;
    RealTime rtGap = (rtEnd - rtStart) / count;
//    cerr << "rtGap is " << rtGap << endl;

    int incms;
    bool quarter = false;

    if (rtGap.sec > 0) {
	incms = 1000;
	int s = rtGap.sec;
	if (s > 0) { incms *= 5; s /= 5; }
	if (s > 0) { incms *= 2; s /= 2; }
	if (s > 0) { incms *= 6; s /= 6; quarter = true; }
	if (s > 0) { incms *= 5; s /= 5; quarter = false; }
	if (s > 0) { incms *= 2; s /= 2; }
	if (s > 0) { incms *= 6; s /= 6; quarter = true; }
	while (s > 0) {
	    incms *= 10;
	    s /= 10;
	    quarter = false;
	}
    } else {
	incms = 1;
	int ms = rtGap.msec();
	if (ms > 0) { incms *= 10; ms /= 10; }
	if (ms > 0) { incms *= 10; ms /= 10; }
	if (ms > 0) { incms *= 5; ms /= 5; }
	if (ms > 0) { incms *= 2; ms /= 2; }
    }
//    cerr << "incms is " << incms << endl;

    RealTime rt = RealTime::frame2RealTime(rectStart, sampleRate);
    long ms = rt.sec * 1000 + rt.msec();
    ms = (ms / incms) * incms - incms;

    RealTime incRt = RealTime::fromMilliseconds(incms);
    long incFrame = RealTime::realTime2Frame(incRt, sampleRate);
    int incX = incFrame / zoomLevel;
    int ticks = 10;
    if (incX < minPixelSpacing * 2) {
	ticks = quarter ? 4 : 5;
    }

    QRect oldClipRect = rect;
    QRect newClipRect(oldClipRect.x() - 25, oldClipRect.y(),
		      oldClipRect.width() + 50, oldClipRect.height());
    paint.setClipRect(newClipRect);

    QColor greyColour(m_colour);
    if (m_colour == Qt::black) {
	greyColour = QColor(200,200,200);
    } else {
	greyColour = m_colour.light(150);
    }

    while (1) {

	rt = RealTime::fromMilliseconds(ms);
	ms += incms;

	long frame = RealTime::realTime2Frame(rt, sampleRate);
	if (frame >= rectEnd) break;

	int x = (frame - startFrame) / zoomLevel;
	if (x < rect.x() || x >= rect.x() + rect.width()) continue;

	paint.setPen(greyColour);
	paint.drawLine(x, 0, x, v->height());

	paint.setPen(m_colour);
	paint.drawLine(x, 0, x, 5);
	paint.drawLine(x, v->height() - 6, x, v->height() - 1);

	QString text(QString::fromStdString(rt.toText()));

	int y;
	QFontMetrics metrics = paint.fontMetrics();
	switch (m_labelHeight) {
	default:
	case LabelTop:
	    y = 6 + metrics.ascent();
	    break;
	case LabelMiddle:
	    y = v->height() / 2 - metrics.height() / 2 + metrics.ascent();
	    break;
	case LabelBottom:
	    y = v->height() - metrics.height() + metrics.ascent() - 6;
	}

	int tw = metrics.width(text);

	paint.setPen(v->palette().background().color());

	//!!! simple drawing function for this please
	//!!! and need getContrastingColour() in widget, or use the
	//palette properly -- get the base class able to draw text
	//using the proper colour (or this technique) automatically
	for (int dx = -1; dx <= 1; ++dx) {
	    for (int dy = -1; dy <= 1; ++dy) {
		if ((dx && dy) || !(dx || dy)) continue;
		paint.drawText(x + 2 - tw / 2 + dx, y + dy, text);
	    }
	}
	
	paint.setPen(m_colour);
	paint.drawText(x + 2 - tw / 2, y, text);
		
	paint.setPen(greyColour);

	for (int i = 1; i < ticks; ++i) {
	    rt = rt + (incRt / ticks);
	    frame = RealTime::realTime2Frame(rt, sampleRate);
	    x = (frame - startFrame) / zoomLevel;
	    int sz = 5;
	    if (ticks == 10) {
		if ((i % 2) == 1) {
		    if (i == 5) {
			paint.drawLine(x, 0, x, v->height());
		    } else sz = 3;
		} else {
		    sz = 7;
		}
	    }
	    paint.drawLine(x, 0, x, sz);
	    paint.drawLine(x, v->height() - sz - 1, x, v->height() - 1);
	}
    }

    paint.restore();
}
    
QString
TimeRulerLayer::toXmlString(QString indent, QString extraAttributes) const
{
    return Layer::toXmlString(indent, extraAttributes +
			      QString(" colour=\"%1\"").arg(encodeColour(m_colour)));
}

void
TimeRulerLayer::setProperties(const QXmlAttributes &attributes)
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
#include "TimeRulerLayer.moc.cpp"
#endif

