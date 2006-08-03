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

#ifndef _THUMBWHEEL_H_
#define _THUMBWHEEL_H_

#include <QWidget>

class Thumbwheel : public QWidget
{
    Q_OBJECT

public:
    Thumbwheel(int min, int max, int defaultValue,
               Qt::Orientation orientation, QWidget *parent = 0);
    virtual ~Thumbwheel();

    void setSpeed(float speed);
    float getSpeed() const;

    void setTracking(bool tracking);
    bool getTracking() const;

    void setShowScale(bool show);
    bool getShowScale() const;

    void setValue(int value);
    int getValue() const;

    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseDoubleClickEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void wheelEvent(QWheelEvent *e);
    virtual void paintEvent(QPaintEvent *e);

    QSize sizeHint() const;

signals:
    void valueChanged(int);

private:
    int m_min;
    int m_max;
    int m_default;
    int m_value;
    Qt::Orientation m_orientation;
    float m_speed;
    bool m_tracking;
    bool m_showScale;
    bool m_clicked;
    QPoint m_clickPos;
    int m_clickValue;
};

#endif
