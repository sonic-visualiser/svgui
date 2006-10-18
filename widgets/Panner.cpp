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

#include "Panner.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QPainter>

#include <iostream>

Panner::Panner(QWidget *parent) :
    QWidget(parent),
    m_rectX(0),
    m_rectY(0),
    m_rectWidth(1),
    m_rectHeight(1)
{
}

Panner::~Panner()
{
}

void
Panner::mousePressEvent(QMouseEvent *e)
{
}

void
Panner::mouseDoubleClickEvent(QMouseEvent *e)
{
}

void
Panner::mouseMoveEvent(QMouseEvent *e)
{
}

void
Panner::mouseReleaseEvent(QMouseEvent *e)
{
}

void
Panner::wheelEvent(QWheelEvent *e)
{
}

void
Panner::paintEvent(QPaintEvent *e)
{
    QPainter paint(this);
    paint.fillRect(rect(), palette().background().color());
    paint.setRenderHint(QPainter::Antialiasing, false);
    paint.setPen(palette().dark().color());
    paint.setBrush(palette().highlight().color());
    paint.drawRect(QRectF(width() * m_rectX, height() - height() * m_rectY,
                          width() * m_rectWidth, height() * m_rectHeight));
}

void
Panner::normalise()
{
    if (m_rectWidth > 1.0) m_rectWidth = 1.0;
    if (m_rectHeight > 1.0) m_rectHeight = 1.0;
    if (m_rectX + m_rectWidth > 1.0) m_rectX = 1.0 - m_rectWidth;
    if (m_rectX < 0) m_rectX = 0;
    if (m_rectY + m_rectHeight > 1.0) m_rectY = 1.0 - m_rectHeight;
    if (m_rectY < 0) m_rectY = 0;
}

void
Panner::emitAndUpdate()
{
    emit rectExtentsChanged(m_rectX, m_rectY, m_rectWidth, m_rectHeight);
    emit rectCentreMoved(m_rectX + (m_rectWidth/2), m_rectY + (m_rectHeight/2));
    update();
}  

void
Panner::setRectExtents(float x0, float y0, float width, float height)
{
    if (m_rectX == x0 &&
        m_rectY == y0 &&
        m_rectWidth == width &&
        m_rectHeight == height) {
        return;
    }
    m_rectX = x0;
    m_rectY = y0;
    m_rectWidth = width;
    m_rectHeight = height;
    normalise();
    emitAndUpdate();
}

void
Panner::setRectWidth(float width)
{
    if (m_rectWidth == width) return;
    m_rectWidth = width;
    normalise();
    emitAndUpdate();
}

void
Panner::setRectHeight(float height)
{
    if (m_rectHeight == height) return;
    m_rectHeight = height;
    normalise();
    emitAndUpdate();
}

void
Panner::setRectCentreX(float x)
{
    float x0 = x - m_rectWidth/2;
    if (x0 == m_rectX) return;
    m_rectX = x0;
    normalise();
    emitAndUpdate();
}

void
Panner::setRectCentreY(float y)
{
    float y0 = y - m_rectWidth/2;
    if (y0 == m_rectY) return;
    m_rectY = y0;
    normalise();
    emitAndUpdate();
}

QSize
Panner::sizeHint() const
{
    return QSize(30, 30);
}



