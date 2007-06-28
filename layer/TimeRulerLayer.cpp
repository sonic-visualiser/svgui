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

#include "TimeRulerLayer.h"

#include "data/model/Model.h"
#include "base/RealTime.h"
#include "view/View.h"

#include <QPainter>

#include <iostream>
#include <cmath>

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
    list.push_back("Colour");
    return list;
}

QString
TimeRulerLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Colour") return tr("Colour");
    return "";
}

Layer::PropertyType
TimeRulerLayer::getPropertyType(const PropertyName &) const
{
    return ValueProperty;
}

int
TimeRulerLayer::getPropertyRangeAndValue(const PropertyName &name,
					 int *min, int *max, int *deflt) const
{
    int val = 0;

    if (name == "Colour") {

	if (min) *min = 0;
	if (max) *max = 5;
        if (deflt) *deflt = 0;

	if (m_colour == Qt::black) val = 0;
	else if (m_colour == Qt::darkRed) val = 1;
	else if (m_colour == Qt::darkBlue) val = 2;
	else if (m_colour == Qt::darkGreen) val = 3;
	else if (m_colour == QColor(200, 50, 255)) val = 4;
	else if (m_colour == QColor(255, 150, 50)) val = 5;

    } else {
	
	val = Layer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
TimeRulerLayer::getPropertyValueLabel(const PropertyName &name,
				    int value) const
{
    if (name == "Colour") {
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
    if (name == "Colour") {
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

bool
TimeRulerLayer::snapToFeatureFrame(View *v, int &frame,
                                   size_t &resolution, SnapType snap) const
{
    if (!m_model) {
        resolution = 1;
        return false;
    }

    bool q;
    int tick = getMajorTickSpacing(v, q);
    RealTime rtick = RealTime::fromMilliseconds(tick);
    int rate = m_model->getSampleRate();
    
    RealTime rt = RealTime::frame2RealTime(frame, rate);
    double ratio = rt / rtick;

    int rounded = lrint(ratio);
    RealTime rdrt = rtick * rounded;

    int left = RealTime::realTime2Frame(rdrt, rate);
    resolution = RealTime::realTime2Frame(rtick, rate);
    int right = left + resolution;

    switch (snap) {

    case SnapLeft:
        frame = left;
        break;

    case SnapRight:
        frame = right;
        break;
        
    case SnapNearest:
    {
        if (abs(frame - left) > abs(right - frame)) {
            frame = right;
        } else {
            frame = left;
        }
        break;
    }

    case SnapNeighbouring:
    {
        int dl = -1, dr = -1;
        int x = v->getXForFrame(frame);

        if (left > v->getStartFrame() &&
            left < v->getEndFrame()) {
            dl = abs(v->getXForFrame(left) - x);
        }

        if (right > v->getStartFrame() &&
            right < v->getEndFrame()) {
            dr = abs(v->getXForFrame(right) - x);
        }

        int fuzz = 2;

        if (dl >= 0 && dr >= 0) {
            if (dl < dr) {
                if (dl <= fuzz) {
                    frame = left;
                }
            } else {
                if (dr < fuzz) {
                    frame = right;
                }
            }
        } else if (dl >= 0) {
            if (dl <= fuzz) {
                frame = left;
            }
        } else if (dr >= 0) {
            if (dr <= fuzz) {
                frame = right;
            }
        }
    }
    }

    return true;
}

int
TimeRulerLayer::getMajorTickSpacing(View *v, bool &quarterTicks) const
{
    // return value is in milliseconds

    if (!m_model || !v) return 1000;

    int sampleRate = m_model->getSampleRate();
    if (!sampleRate) return 1000;

    long startFrame = v->getStartFrame();
    long endFrame = v->getEndFrame();

    int minPixelSpacing = 50;

    RealTime rtStart = RealTime::frame2RealTime(startFrame, sampleRate);
    RealTime rtEnd = RealTime::frame2RealTime(endFrame, sampleRate);

    int count = v->width() / minPixelSpacing;
    if (count < 1) count = 1;
    RealTime rtGap = (rtEnd - rtStart) / count;

    int incms;
    quarterTicks = false;

    if (rtGap.sec > 0) {
	incms = 1000;
	int s = rtGap.sec;
	if (s > 0) { incms *= 5; s /= 5; }
	if (s > 0) { incms *= 2; s /= 2; }
	if (s > 0) { incms *= 6; s /= 6; quarterTicks = true; }
	if (s > 0) { incms *= 5; s /= 5; quarterTicks = false; }
	if (s > 0) { incms *= 2; s /= 2; }
	if (s > 0) { incms *= 6; s /= 6; quarterTicks = true; }
	while (s > 0) {
	    incms *= 10;
	    s /= 10;
	    quarterTicks = false;
	}
    } else {
	incms = 1;
	int ms = rtGap.msec();
	if (ms > 0) { incms *= 10; ms /= 10; }
	if (ms > 0) { incms *= 10; ms /= 10; }
	if (ms > 0) { incms *= 5; ms /= 5; }
	if (ms > 0) { incms *= 2; ms /= 2; }
    }

    return incms;
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

//    std::cerr << "TimeRulerLayer::paint: calling paint.save()" << std::endl;
    paint.save();
//!!!    paint.setClipRect(v->rect());

    int minPixelSpacing = 50;

    bool quarter = false;
    int incms = getMajorTickSpacing(v, quarter);

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
    paint.setClipRect(rect);

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

        if (v->getViewManager() && v->getViewManager()->getOverlayMode() !=
            ViewManager::NoOverlays) {

            if (v->getLayer(0) == this) {
                // backmost layer, don't worry about outlining the text
                paint.drawText(x+2 - tw/2, y, text);
            } else {
                v->drawVisibleText(paint, x+2 - tw/2, y, text, View::OutlinedText);
            }
        }

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

