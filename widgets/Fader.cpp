/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

/**
 * Horizontal audio fader and meter widget.
 *
 * Based on the vertical fader and meter widget from the Hydrogen drum
 * machine.  (Any poor taste that has crept in during the
 * modifications for this application is entirely my own, however.)
 * The following copyright notice applies to code from this file, and
 * also to the files in icons/fader_*.png (also modified by me). --cc
 */

/**
 * Hydrogen
 * Copyright(c) 2002-2005 by Alex >Comix< Cominu [comix@users.sourceforge.net]
 *
 * http://www.hydrogen-music.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "Fader.h"

#include "base/AudioLevel.h"

#include <QMouseEvent>
#include <QPixmap>
#include <QWheelEvent>
#include <QPaintEvent>
#include <QPainter>

Fader::Fader(QWidget *parent, bool withoutKnob) :
    QWidget(parent),
    m_withoutKnob(withoutKnob),
    m_value(1.0),
    m_peakLeft(0.0),
    m_peakRight(0.0)
{
    setMinimumSize(116, 23);
    setMaximumSize(116, 23);
    resize(116, 23);

    QString background_path = ":/icons/fader_background.png";
    bool ok = m_back.load(background_path);
    if (ok == false) {
	std::cerr << "Fader: Error loading pixmap" << std::endl;
    }

    QString leds_path = ":/icons/fader_leds.png";
    ok = m_leds.load(leds_path);
    if (ok == false) {
	std::cerr <<  "Error loading pixmap" << std::endl;
    }

    QString knob_path = ":/icons/fader_knob.png";
    ok = m_knob.load(knob_path);
    if (ok == false) {
	std::cerr <<  "Error loading pixmap" << std::endl;
    }

    QString clip_path = ":/icons/fader_knob_red.png";
    ok = m_clip.load(clip_path);
    if (ok == false) {
	std::cerr <<  "Error loading pixmap" << std::endl;
    }
}

Fader::~Fader()
{

}

void
Fader::mouseMoveEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::MidButton) {
        ev->accept();
        return;
    }

    int x = ev->x() - 6;
    const int max_x = 116 - 12;

    int value = x;

    if (value > max_x) {
	value = max_x;
    } else if (value < 0) {
	value = 0;
    }

//    float fval = float(value) / float(max_x);
    float fval = AudioLevel::fader_to_multiplier
	(value, max_x, AudioLevel::LongFader);

    setValue(fval);
    emit valueChanged(fval);

    update();
}


void
Fader::mouseReleaseEvent(QMouseEvent *ev)
{
    mouseMoveEvent(ev);
}


void
Fader::mouseDoubleClickEvent(QMouseEvent *)
{
    setValue(1.0);
    emit valueChanged(1.0);
    update();
}

void
Fader::mousePressEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::MidButton) {
        setValue(1.0);
        emit valueChanged(1.0);
        update();
        return;
    }

    int x = ev->x() - 6;
    const int max_x = 116 - 12;

    int value = x;

    if (value > max_x) {
	value = max_x;
    } else if (value < 0) {
	value = 0;
    }

    float fval = AudioLevel::fader_to_multiplier
	(value, max_x, AudioLevel::LongFader);

    setValue(fval);
    emit valueChanged(fval);

    update();
}


void
Fader::wheelEvent(QWheelEvent *ev)
{
    ev->accept();

    //!!! needs improvement

    if (ev->delta() > 0) {
	setValue(m_value * 1.1);
    } else {
	setValue(m_value / 1.1);
    }

    update();
    emit valueChanged(getValue());
}


void
Fader::setValue(float v)
{
    float max = AudioLevel::dB_to_multiplier(10.0);

    if (v > max) {
	v = max;
    } else if (v < 0.0) {
	v = 0.0;
    }

    if (m_value != v) {
	m_value = v;
	float db = AudioLevel::multiplier_to_dB(m_value);
	if (db <= AudioLevel::DB_FLOOR) {
	    setToolTip(tr("Level: Off"));
	} else {
	    setToolTip(tr("Level: %1%2.%3%4 dB")
		       .arg(db < 0.0 ? "-" : "")
		       .arg(abs(int(db)))
		       .arg(abs(int(db * 10.0) % 10))
		       .arg(abs(int(db * 100.0) % 10)));
	}
	update();
    }
}


float
Fader::getValue()
{
    return m_value;
}



void
Fader::setPeakLeft(float peak)
{
    if (this->m_peakLeft != peak) {
	this->m_peakLeft = peak;
	update();
    }
}


void
Fader::setPeakRight(float peak) 
{
    if (this->m_peakRight != peak) {
	this->m_peakRight = peak;
	update();
    }
}


void
Fader::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    // background
    painter.drawPixmap(rect(), m_back, QRect(0, 0, 116, 23));

    int offset_L = AudioLevel::multiplier_to_fader(m_peakLeft, 116,
						   AudioLevel::IEC268LongMeter);

    painter.drawPixmap(QRect(0, 0, offset_L, 11), m_leds,
		       QRect(0, 0, offset_L, 11));

    int offset_R = AudioLevel::multiplier_to_fader(m_peakRight, 116,
						   AudioLevel::IEC268LongMeter);

    painter.drawPixmap(QRect(0, 11, offset_R, 11), m_leds,
		       QRect(0, 11, offset_R, 11));

    if (m_withoutKnob == false) {

	static const uint knob_width = 29;
	static const uint knob_height = 9;

	int x = AudioLevel::multiplier_to_fader(m_value, 116 - knob_width,
						AudioLevel::LongFader);

	bool clipping = (m_peakLeft > 1.0 || m_peakRight > 1.0);

	painter.drawPixmap(QRect(x, 7, knob_width, knob_height),
			   clipping ? m_clip : m_knob,
			   QRect(0, 0, knob_width, knob_height));
    }
}


