/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

/*
    This is a modified version of a source file from the KDE
    libraries.  Copyright (c) 1998-2004 Jörg Habenicht, Richard J
    Moore and others, distributed under the GNU Lesser General Public
    License.

    Ported to Qt4 by Chris Cannam.

    The original KDE widget comes in round and rectangular and flat,
    raised, and sunken variants.  This version retains only the round
    sunken variant.  This version also implements a simple button API.
*/

#ifndef _LED_BUTTON_H_
#define _LED_BUTTON_H_

#include <QWidget>

class QColor;

class LEDButton : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)
    Q_PROPERTY(int darkFactor READ darkFactor WRITE setDarkFactor)

public:
    LEDButton(QWidget *parent = 0);
    LEDButton(const QColor &col, QWidget *parent = 0);
    LEDButton(const QColor& col, bool state, QWidget *parent = 0);
    ~LEDButton();

    bool state() const;
    QColor color() const;
    int darkFactor() const;

    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const;

signals:
    void stateChanged(bool);

public slots:
    void toggle();
    void on();
    void off();

    void setState(bool);
    void toggleState();
    void setColor(const QColor& color);
    void setDarkFactor(int darkfactor);

protected:
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent *);

private:
    bool led_state;
    QColor led_color;

private:
    class LEDButtonPrivate;
    LEDButtonPrivate *d;
};

#endif
