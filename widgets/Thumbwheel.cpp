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

#include "base/RangeMapper.h"
#include "base/Profiler.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QInputDialog>
#include <QPainter>
#include <QPainterPath>
#include <QMenu>

#include "MenuTitle.h"

#include <cmath>
#include <iostream>

namespace sv {

Thumbwheel::Thumbwheel(Qt::Orientation orientation,
                       QWidget *parent) :
    QWidget(parent),
    m_min(0),
    m_max(100),
    m_default(50),
    m_value(50),
    m_mappedValue(50),
    m_noMappedUpdate(false),
    m_rotation(0.5),
    m_orientation(orientation),
    m_speed(1.0),
    m_tracking(true),
    m_showScale(true),
    m_clicked(false),
    m_atDefault(true),
    m_clickRotation(m_rotation),
    m_showTooltip(true),
    m_provideContextMenu(true),
    m_lastContextMenu(nullptr),
    m_rangeMapper(nullptr)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(contextMenuRequested(const QPoint &)));
}

Thumbwheel::~Thumbwheel()
{
    delete m_rangeMapper;
    delete m_lastContextMenu;
}

void
Thumbwheel::contextMenuRequested(const QPoint &pos)
{
    if (!m_provideContextMenu) {
        return;
    }
    
    delete m_lastContextMenu;
    m_lastContextMenu = new QMenu;
    auto m = m_lastContextMenu;

    if (m_title == "") {
        MenuTitle::addTitle(m, tr("Thumbwheel"));
    } else {        
        MenuTitle::addTitle(m, m_title);
    }

    m->addAction(tr("&Edit..."), this, SLOT(edit()));
    m->addAction(tr("&Reset to Default"), this, SLOT(resetToDefault()));

    m->popup(mapToGlobal(pos));
}

void
Thumbwheel::setRangeMapper(RangeMapper *mapper)
{
    if (m_rangeMapper == mapper) return;

    if (!m_rangeMapper && mapper) {
        connect(this, SIGNAL(valueChanged(int)),
                this, SLOT(updateMappedValue(int)));
    }

    delete m_rangeMapper;
    m_rangeMapper = mapper;

    updateMappedValue(getValue());
}

void
Thumbwheel::setShowToolTip(bool show)
{
    m_showTooltip = show;
    m_noMappedUpdate = true;
    updateMappedValue(getValue());
    m_noMappedUpdate = false;
}

void
Thumbwheel::setProvideContextMenu(bool provide)
{
    m_provideContextMenu = provide;
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
        m_cache = QImage();
        emit valueChanged(getValue());
    }
}

void
Thumbwheel::setMappedValue(double mappedValue)
{
    if (m_rangeMapper) {
        int newValue = m_rangeMapper->getPositionForValue(mappedValue);
        bool changed = (m_mappedValue != mappedValue);
        m_mappedValue = mappedValue;
        m_noMappedUpdate = true;
//        SVDEBUG << "Thumbwheel::setMappedValue(" << mappedValue << "): new value is " << newValue << " (visible " << isVisible() << ")" << endl;
        if (newValue != getValue()) {
            setValue(newValue);
            changed = true;
            m_cache = QImage();
        }
        if (changed) emit valueChanged(newValue);
        m_noMappedUpdate = false;
    } else {
        int v = int(mappedValue);
        if (v != getValue()) {
            setValue(v);
            m_cache = QImage();
            emit valueChanged(v);
        }
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
//    SVDEBUG << "Thumbwheel::setValue(" << value << ") (from " << m_value
//              << ", rotation " << m_rotation << ")" << " (visible " << isVisible() << ")" << endl;

    if (m_value != value) {

        m_atDefault = false;

        if (value < m_min) value = m_min;
        if (value > m_max) value = m_max;
        m_value = value;
    }

    m_rotation = float(m_value - m_min) / float(m_max - m_min);
    m_cache = QImage();

    if (isVisible()) {
        update();
        updateTitle();
    }
}

void
Thumbwheel::resetToDefault()
{
    if (m_default == m_value) return;
    setValue(m_default);
    m_atDefault = true;
    m_cache = QImage();
    emit valueChanged(getValue());
}

int
Thumbwheel::getValue() const
{
    return m_value;
}

double
Thumbwheel::getMappedValue() const
{
    if (m_rangeMapper) {
//        SVDEBUG << "Thumbwheel::getMappedValue(): value = " << getValue() << ", mappedValue = " << m_mappedValue << endl;
        return m_mappedValue;
    }
    return getValue();
}

void
Thumbwheel::updateMappedValue(int value)
{
    if (!m_noMappedUpdate) {
        if (m_rangeMapper) {
            m_mappedValue = m_rangeMapper->getValueForPosition(value);
        } else {
            m_mappedValue = value;
        }
    }

    updateTitle();
}

void
Thumbwheel::updateTitle()
{    
    QString name = objectName();
    QString unit = "";
    QString text;
    double mappedValue = getMappedValue();

    if (m_rangeMapper) unit = m_rangeMapper->getUnit();
    if (name != "") {
        text = tr("%1: %2%3").arg(name).arg(mappedValue).arg(unit);
    } else {
        text = tr("%2%3").arg(mappedValue).arg(unit);
    }

    m_title = text;

    if (m_showTooltip) {
        setToolTip(text);
    } else {
        setToolTip("");
    }
}

void
Thumbwheel::scroll(bool up)
{
    int step = int(lrintf(m_speed));
    if (step == 0) step = 1;

    if (up) {
        setValue(m_value + step);
    } else {
        setValue(m_value - step);
    }
    
    emit valueChanged(getValue());
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
Thumbwheel::enterEvent(QEnterEvent *)
{
    emit mouseEntered();
}

void
Thumbwheel::leaveEvent(QEvent *)
{
    emit mouseLeft();
}

void
Thumbwheel::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton ||
        ((e->button() == Qt::LeftButton) &&
         (e->modifiers() & Qt::ControlModifier))) {
        resetToDefault();
    } else if (e->button() == Qt::LeftButton) {
        m_clicked = true;
        m_clickPos = e->pos();
        m_clickRotation = m_rotation;
    }
}

void
Thumbwheel::mouseDoubleClickEvent(QMouseEvent *mouseEvent)
{
    //!!! needs a common base class with AudioDial (and Panner?)

    if (mouseEvent->button() != Qt::LeftButton) {
        return;
    }

    edit();
}

void
Thumbwheel::edit()
{
    bool ok = false;

    if (m_rangeMapper) {
        
        double min = m_rangeMapper->getValueForPosition(m_min);
        double max = m_rangeMapper->getValueForPosition(m_max);
                
        if (min > max) { 
            double tmp = min;
            min = max;
            max = tmp;
        }

        QString unit = m_rangeMapper->getUnit();
        
        QString text;
        if (objectName() != "") {
            if (unit != "") {
                text = tr("New value for %1, from %2 to %3 %4:")
                    .arg(objectName()).arg(min).arg(max).arg(unit);
            } else {
                text = tr("New value for %1, from %2 to %3:")
                    .arg(objectName()).arg(min).arg(max);
            }
        } else {
            if (unit != "") {
                text = tr("Enter a new value from %1 to %2 %3:")
                    .arg(min).arg(max).arg(unit);
            } else {
                text = tr("Enter a new value from %1 to %2:")
                    .arg(min).arg(max);
            }
        }
        
        double newValue = QInputDialog::getDouble
            (this,
             tr("Enter new value"),
             text,
             m_mappedValue,
             min,
             max,
             4, 
             &ok);
        
        if (ok) {
            setMappedValue(newValue);
        }
        
    } else {
        
        int newValue = QInputDialog::getInt
            (this,
             tr("Enter new value"),
             tr("Enter a new value from %1 to %2:")
             .arg(m_min).arg(m_max),
             getValue(), m_min, m_max, 1, &ok);
        
        if (ok) {
            setValue(newValue);
        }
    }
}


void
Thumbwheel::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_clicked) return;
    int dist = 0;
    if (m_orientation == Qt::Horizontal) {
        dist = e->position().x() - m_clickPos.x();
    } else {
        dist = e->position().y() - m_clickPos.y();
    }

    float rotation = m_clickRotation + (m_speed * float(dist)) / 100;
    if (rotation < 0.f) rotation = 0.f;
    if (rotation > 1.f) rotation = 1.f;
    int value = int(lrintf(float(m_min) + float(m_max - m_min) * m_rotation));
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
    int delta = m_wheelCounter.count(e);

    if (delta == 0) {
        return;
    }

    setValue(m_value + delta);
    emit valueChanged(getValue());
}

void
Thumbwheel::paintEvent(QPaintEvent *)
{
    Profiler profiler("Thumbwheel::paintEvent");

    if (!m_cache.isNull()) {
        QPainter paint(this);
        paint.drawImage(rect(), m_cache, m_cache.rect());
        return;
    }

    Profiler profiler2("Thumbwheel::paintEvent (no cache)");

    QSize imageSize = size() * devicePixelRatio();
    m_cache = QImage(imageSize, QImage::Format_ARGB32);
    m_cache.fill(Qt::transparent);

    int w = m_cache.width();
    int h = m_cache.height();
    int bw = 3; // border width

    QRect subclip;
    if (m_orientation == Qt::Horizontal) {
        subclip = QRect(bw, bw+1, w - bw*2, h - bw*2 - 2);
    } else {
        subclip = QRect(bw+1, bw, w - bw*2 - 2, h - bw*2);
    }

    QPainter paint(&m_cache);
    paint.setClipRect(m_cache.rect());
    paint.fillRect(subclip, palette().window().color());

    paint.setRenderHint(QPainter::Antialiasing, true);

    double w0 = 0.5;
    double w1 = w - 0.5;

    double h0 = 0.5;
    double h1 = h - 0.5;

    for (int i = bw-1; i >= 0; --i) {

        int grey = (i + 1) * (256 / (bw + 1));
        QColor fc = QColor(grey, grey, grey);
        paint.setPen(fc);

        QPainterPath path;

        if (m_orientation == Qt::Horizontal) {
            path.moveTo(w0 + i, h0 + i + 2);
            path.quadTo(w/2, i * 1.25, w1 - i, h0 + i + 2);
            path.lineTo(w1 - i, h1 - i - 2);
            path.quadTo(w/2, h - i * 1.25, w0 + i, h1 - i - 2);
            path.closeSubpath();
        } else {
            path.moveTo(w0 + i + 2, h0 + i);
            path.quadTo(i * 1.25, h/2, w0 + i + 2, h1 - i);
            path.lineTo(w1 - i - 2, h1 - i);
            path.quadTo(w - i * 1.25, h/2, w1 - i - 2, h0 + i);
            path.closeSubpath();
        }

        paint.drawPath(path);
    }

    paint.setClipRect(subclip);

    double radians = m_rotation * 1.5f * M_PI;

//    cerr << "value = " << m_value << ", min = " << m_min << ", max = " << m_max << ", rotation = " << rotation << endl;

    int ww = (m_orientation == Qt::Horizontal ? w : h) - bw*2; // wheel width

    // total number of notches on the entire wheel
    int notches = 25;
    
    // radius of the wheel including invisible part
    int radius = int(ww / 2 + 2);

    for (int i = 0; i < notches; ++i) {

        double a0 = (2.0 * M_PI * i) / notches + radians;
        double a1 = a0 + M_PI / (notches * 2);
        double a2 = (2.0 * M_PI * (i + 1)) / notches + radians;

        double depth = cos((a0 + a2) / 2);
        if (depth < 0) continue;

        double x0 = radius * sin(a0) + ww/2;
        double x1 = radius * sin(a1) + ww/2;
        double x2 = radius * sin(a2) + ww/2;
        if (x2 < 0 || x0 > ww) continue;

        if (x0 < 0) x0 = 0;
        if (x2 > ww) x2 = ww;

        x0 += bw;
        x1 += bw;
        x2 += bw;

        int grey = int(lrint(120 * depth));

        QColor fc = QColor(grey, grey, grey);
        QColor oc = palette().highlight().color();

        paint.setPen(fc);

        if (m_showScale) {

            paint.setBrush(oc);

            double prop;
            if (i >= notches / 4) {
                prop = double(notches - (((i - double(notches) / 4.f) * 4.f) / 3.f))
                    / notches;
            } else {
                prop = 0.f;
            }
            
            if (m_orientation == Qt::Horizontal) {
                paint.drawRect(QRectF(x1, h - (h - bw*2) * prop - bw,
                                      x2 - x1, h * prop));
            } else {
                paint.drawRect(QRectF(bw, x1, (w - bw*2) * prop, x2 - x1));
            }
        }

        paint.setPen(fc);
        paint.setBrush(palette().window().color());

        if (m_orientation == Qt::Horizontal) {
            paint.drawRect(QRectF(x0, bw, x1 - x0, h - bw*2));
        } else {
            paint.drawRect(QRectF(bw, x0, w - bw*2, x1 - x0));
        }
    }

    QPainter paint2(this);
    paint2.drawImage(rect(), m_cache, m_cache.rect());
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

} // end namespace sv

