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

#include "Thumbwheel.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QPainter>

#include <cmath>
#include <iostream>

Thumbwheel::Thumbwheel(int min, int max, int defaultValue,
                       Qt::Orientation orientation,
		       QWidget *parent) :
    QWidget(parent),
    m_min(min),
    m_max(max),
    m_default(defaultValue),
    m_value((min + max) / 2),
    m_orientation(orientation),
    m_tracking(true),
    m_showScale(true),
    m_clicked(false),
    m_clickValue(m_value)
{
    if (max <= min) max = min + 1;
    m_speed = float(max - min) / 300.f;
}

Thumbwheel::~Thumbwheel()
{
}

void
Thumbwheel::setValue(int value)
{
    if (value < m_min) value = m_min;
    if (value > m_max) value = m_max;
    m_value = value;
    update();
}

int
Thumbwheel::getValue() const
{
    return m_value;
}

void
Thumbwheel::setSpeed(float speed)
{
    m_speed = speed;
}

float
Thumbwheel::getSpeed() const
{
    return m_speed;
}

void
Thumbwheel::setTracking(bool tracking)
{
    m_tracking = tracking;
}

bool
Thumbwheel::getTracking() const
{
    return m_tracking;
}

void
Thumbwheel::setShowScale(bool showScale)
{
    m_showScale = showScale;
}

bool
Thumbwheel::getShowScale() const
{
    return m_showScale;
}

void
Thumbwheel::mousePressEvent(QMouseEvent *e)
{
    m_clicked = true;
    m_clickPos = e->pos();
    m_clickValue = m_value;
}

void
Thumbwheel::mouseDoubleClickEvent(QMouseEvent *)
{
    setValue(m_default);
    emit valueChanged(getValue());
}

void
Thumbwheel::mouseMoveEvent(QMouseEvent *e)
{
    int dist = 0;
    if (m_orientation == Qt::Horizontal) {
        dist = e->x() - m_clickPos.x();
    } else {
        dist = e->y() - m_clickPos.y();
    }
    int value = m_clickValue + lrintf(m_speed * dist);
    if (value < m_min) value = m_min;
    if (value > m_max) value = m_max;
    if (value != m_value) {
        setValue(value);
        if (m_tracking) emit valueChanged(getValue());
    }        
}

void
Thumbwheel::mouseReleaseEvent(QMouseEvent *e)
{
    bool reallyTracking = m_tracking;
    m_tracking = true;
    mouseMoveEvent(e);
    m_tracking = reallyTracking;
}

void
Thumbwheel::wheelEvent(QWheelEvent *e)
{
    int step = lrintf(m_speed);
    if (step == 0) step = 1;

    if (e->delta() > 0) {
	setValue(m_value + step);
    } else {
	setValue(m_value - step);
    }
    
    emit valueChanged(getValue());
}

void
Thumbwheel::paintEvent(QPaintEvent *)
{
    float distance = float(m_value - m_min) / float(m_max - m_min);
    float rotation = distance * 1.5f * M_PI;

//    std::cerr << "value = " << m_value << ", min = " << m_min << ", max = " << m_max << ", rotation = " << rotation << std::endl;

    int w = (m_orientation == Qt::Horizontal ? width() : height());

    // total number of notches on the entire wheel
    int notches = 25;
    
    // radius of the wheel including invisible part
    int radius = w / 2 + 2;

    QPainter paint(this);
    paint.fillRect(rect(), palette().background().color());
    paint.setRenderHint(QPainter::Antialiasing, true);

    for (int i = 0; i < notches; ++i) {

        float a0 = (2.f * M_PI * i) / notches + rotation;
        float a1 = a0 + M_PI / (notches * 2);
        float a2 = (2.f * M_PI * (i + 1)) / notches + rotation;

        float depth = cosf((a0 + a2) / 2);
        if (depth < 0) continue;

        float x0 = radius * sinf(a0) + w/2;
        float x1 = radius * sinf(a1) + w/2;
        float x2 = radius * sinf(a2) + w/2;
        if (x2 < 0 || x0 > w) continue;

        if (x0 < 0) x0 = 0;
        if (x2 > w) x2 = w;

        int grey = lrintf(255 * depth);
        QColor fc = QColor(grey, grey, grey);
        QColor oc = palette().dark().color();

        paint.setPen(oc);
        paint.setBrush(fc);

        if (m_orientation == Qt::Horizontal) {
            paint.drawRect(QRectF(x1, 0, x2 - x1, height()));
        } else {
            paint.drawRect(QRectF(0, x1, width(), x2 - x1));
        }

        if (m_showScale) {

            paint.setBrush(oc);

            float prop;
            if (i >= notches / 4) {
                prop = float(notches - (((i - float(notches) / 4.f) * 4.f) / 3.f))
                    / notches;
            } else {
                prop = 0.f;
            }
            
            if (m_orientation == Qt::Horizontal) {
                paint.drawRect(QRectF(x1, height() - height() * prop,
                                      x2 - x1, height() * prop));
            } else {
                paint.drawRect(QRectF(0, x1, width() * prop, x2 - x1));
            }
        }

        paint.setPen(oc);
        paint.setBrush(palette().background().color());

        if (m_orientation == Qt::Horizontal) {
            paint.drawRect(QRectF(x0, 0, x1 - x0, height()));
        } else {
            paint.drawRect(QRectF(0, x0, width(), x1 - x0));
        }
    }
}

QSize
Thumbwheel::sizeHint() const
{
    if (m_orientation == Qt::Horizontal) {
        return QSize(80, 12);
    } else {
        return QSize(12, 80);
    }
}

