/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 QMUL.

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

Thumbwheel::Thumbwheel(Qt::Orientation orientation,
		       QWidget *parent) :
    QWidget(parent),
    m_min(0),
    m_max(100),
    m_default(50),
    m_value(50),
    m_rotation(0.5),
    m_orientation(orientation),
    m_speed(1.0),
    m_tracking(true),
    m_showScale(true),
    m_clicked(false),
    m_atDefault(true),
    m_clickRotation(m_rotation)
{
}

Thumbwheel::~Thumbwheel()
{
}

void
Thumbwheel::setMinimumValue(int min)
{
    if (m_min == min) return;

    m_min = min;
    if (m_max <= m_min) m_max = m_min + 1;
    if (m_value < m_min) m_value = m_min;
    if (m_value > m_max) m_value = m_max;

    m_rotation = float(m_value - m_min) / float(m_max - m_min);
    update();
}

int
Thumbwheel::getMinimumValue() const
{
    return m_min;
}

void
Thumbwheel::setMaximumValue(int max)
{
    if (m_max == max) return;

    m_max = max;
    if (m_min >= m_max) m_min = m_max - 1;
    if (m_value < m_min) m_value = m_min;
    if (m_value > m_max) m_value = m_max;

    m_rotation = float(m_value - m_min) / float(m_max - m_min);
    update();
}

int
Thumbwheel::getMaximumValue() const
{
    return m_max;
}

void
Thumbwheel::setDefaultValue(int deft)
{
    if (m_default == deft) return;

    m_default = deft;
    if (m_atDefault) {
        setValue(m_default);
        m_atDefault = true; // setValue unsets this
        emit valueChanged(getValue());
    }
}

int
Thumbwheel::getDefaultValue() const
{
    return m_default;
}

void
Thumbwheel::setValue(int value)
{
//    std::cerr << "Thumbwheel::setValue(" << value << ") (from " << m_value
//              << ", rotation " << m_rotation << ")" << std::endl;

    if (m_value != value) {

        m_atDefault = false;

        if (value < m_min) value = m_min;
        if (value > m_max) value = m_max;
        m_value = value;
    }

    m_rotation = float(m_value - m_min) / float(m_max - m_min);
    update();
}

void
Thumbwheel::resetToDefault()
{
    if (m_default == m_value) return;
    setValue(m_default);
    m_atDefault = true;
    emit valueChanged(getValue());
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
    if (e->button() == Qt::LeftButton) {
        m_clicked = true;
        m_clickPos = e->pos();
        m_clickRotation = m_rotation;
    } else if (e->button() == Qt::MidButton) {
        resetToDefault();
    }
}

void
Thumbwheel::mouseDoubleClickEvent(QMouseEvent *)
{
    resetToDefault();
}

void
Thumbwheel::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_clicked) return;
    int dist = 0;
    if (m_orientation == Qt::Horizontal) {
        dist = e->x() - m_clickPos.x();
    } else {
        dist = e->y() - m_clickPos.y();
    }

    float rotation = m_clickRotation + (m_speed * dist) / 100;
    if (rotation < 0.f) rotation = 0.f;
    if (rotation > 1.f) rotation = 1.f;
    int value = lrintf(m_min + (m_max - m_min) * m_rotation);
    if (value != m_value) {
        setValue(value);
        if (m_tracking) emit valueChanged(getValue());
        m_rotation = rotation;
    } else if (fabsf(rotation - m_rotation) > 0.001) {
        m_rotation = rotation;
        repaint();
    }
}

void
Thumbwheel::mouseReleaseEvent(QMouseEvent *e)
{
    if (!m_clicked) return;
    bool reallyTracking = m_tracking;
    m_tracking = true;
    mouseMoveEvent(e);
    m_tracking = reallyTracking;
    m_clicked = false;
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
    QPainter paint(this);
    paint.fillRect(rect(), palette().background().color());
    paint.setRenderHint(QPainter::Antialiasing, false);

    int bw = 3;

    for (int i = 0; i < bw; ++i) {
        int grey = (i + 1) * (256 / (bw + 1));
        QColor fc = QColor(grey, grey, grey);
        paint.setPen(fc);
        paint.drawRect(i, i, width() - i*2 - 1, height() - i*2 - 1);
    }

    paint.setClipRect(QRect(bw, bw, width() - bw*2, height() - bw*2));

    float radians = m_rotation * 1.5f * M_PI;

//    std::cerr << "value = " << m_value << ", min = " << m_min << ", max = " << m_max << ", rotation = " << rotation << std::endl;

    int w = (m_orientation == Qt::Horizontal ? width() : height()) - bw*2;

    // total number of notches on the entire wheel
    int notches = 25;
    
    // radius of the wheel including invisible part
    int radius = w / 2 + 2;

    paint.setRenderHint(QPainter::Antialiasing, true);

    for (int i = 0; i < notches; ++i) {

        float a0 = (2.f * M_PI * i) / notches + radians;
        float a1 = a0 + M_PI / (notches * 2);
        float a2 = (2.f * M_PI * (i + 1)) / notches + radians;

        float depth = cosf((a0 + a2) / 2);
        if (depth < 0) continue;

        float x0 = radius * sinf(a0) + w/2;
        float x1 = radius * sinf(a1) + w/2;
        float x2 = radius * sinf(a2) + w/2;
        if (x2 < 0 || x0 > w) continue;

        if (x0 < 0) x0 = 0;
        if (x2 > w) x2 = w;

        x0 += bw;
        x1 += bw;
        x2 += bw;

        int grey = lrintf(255 * depth);
        QColor fc = QColor(grey, grey, grey);
        QColor oc = palette().dark().color();

        paint.setPen(oc);
        paint.setBrush(fc);

        if (m_orientation == Qt::Horizontal) {
            paint.drawRect(QRectF(x1, bw, x2 - x1, height() - bw*2));
        } else {
            paint.drawRect(QRectF(bw, x1, width() - bw*2, x2 - x1));
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
                paint.drawRect(QRectF(x1, height() - (height() - bw*2) * prop - bw,
                                      x2 - x1, height() * prop));
            } else {
                paint.drawRect(QRectF(bw, x1, (width() - bw*2) * prop, x2 - x1));
            }
        }

        paint.setPen(oc);
        paint.setBrush(palette().background().color());

        if (m_orientation == Qt::Horizontal) {
            paint.drawRect(QRectF(x0, bw, x1 - x0, height() - bw*2));
        } else {
            paint.drawRect(QRectF(bw, x0, width() - bw*2, x1 - x0));
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

