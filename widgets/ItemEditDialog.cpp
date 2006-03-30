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

#include "ItemEditDialog.h"

#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>


ItemEditDialog::ItemEditDialog(size_t sampleRate, int options, QWidget *parent) :
    QDialog(parent),
    m_sampleRate(sampleRate),
    m_frame(0),
    m_duration(0),
    m_value(0.0)
{
    QGridLayout *grid = new QGridLayout;
    setLayout(grid);
    
    int row = 0;
    QLineEdit *line = 0;

    if (options & ShowTime) {

        grid->addWidget(new QLabel(tr("Frame time:")), row, 0);

        line = new QLineEdit;
        line->setValidator(new QIntValidator(this));
        grid->addWidget(line, row, 1);
        connect(line, SIGNAL(textChanged(QString)),
                this, SLOT(frameTimeChanged(QString)));

        ++row;

        grid->addWidget(new QLabel(tr("Secs:")), row, 0);

        line = new QLineEdit;
        line->setValidator(new QIntValidator(this));
        grid->addWidget(line, row, 1);
        connect(line, SIGNAL(textChanged(QString)),
                this, SLOT(realTimeSecsChanged(QString)));

        ++row;

        grid->addWidget(new QLabel(tr("Nsecs:")), row, 0);

        line = new QLineEdit;
        line->setValidator(new QIntValidator(this));
        grid->addWidget(line, row, 1);
        connect(line, SIGNAL(textChanged(QString)),
                this, SLOT(realTimeSecsChanged(QString)));

        ++row;
    }

    if (options & ShowDuration) {

        grid->addWidget(new QLabel(tr("Frame duration:")), row, 0);

        line = new QLineEdit;
        line->setValidator(new QIntValidator(this));
        grid->addWidget(line, row, 1);
        connect(line, SIGNAL(textChanged(QString)),
                this, SLOT(frameDurationChanged(QString)));

        ++row;

        grid->addWidget(new QLabel(tr("Secs:")), row, 0);

        line = new QLineEdit;
        line->setValidator(new QIntValidator(this));
        grid->addWidget(line, row, 1);
        connect(line, SIGNAL(textChanged(QString)),
                this, SLOT(realDurationSecsChanged(QString)));

        ++row;

        grid->addWidget(new QLabel(tr("Nsecs:")), row, 0);

        line = new QLineEdit;
        line->setValidator(new QIntValidator(this));
        grid->addWidget(line, row, 1);
        connect(line, SIGNAL(textChanged(QString)),
                this, SLOT(realDurationSecsChanged(QString)));

        ++row;
    }

    if (options & ShowValue) {
        
        grid->addWidget(new QLabel(tr("Value:")), row, 0);

        QDoubleSpinBox *spinbox = new QDoubleSpinBox;
        grid->addWidget(spinbox, row, 1);
        connect(spinbox, SIGNAL(valueChanged(double)),
                this, SLOT(valueChanged(double)));

        ++row;
    }

    if (options & ShowText) {
        
        grid->addWidget(new QLabel(tr("Text:")), row, 0);

        line = new QLineEdit;
        grid->addWidget(line, row, 1);
        connect(line, SIGNAL(textChanged(QString)),
                this, SLOT(textChanged(QString)));

        ++row;
    }

    QHBoxLayout *hbox = new QHBoxLayout;
    grid->addLayout(hbox, row, 0, 1, 2);
    
    QPushButton *ok = new QPushButton(tr("OK"));
    QPushButton *cancel = new QPushButton(tr("Cancel"));
    hbox->addStretch(10);
    hbox->addWidget(ok);
    hbox->addWidget(cancel);
    connect(ok, SIGNAL(clicked()), this, SLOT(accept()));
    connect(cancel, SIGNAL(clicked()), this, SLOT(reject()));
}

void
ItemEditDialog::setFrameTime(long frame)
{
    m_frame = frame;
    //!!!
}

long
ItemEditDialog::getFrameTime() const
{
    return m_frame;
}

void
ItemEditDialog::setRealTime(RealTime rt)
{
    m_frame = RealTime::realTime2Frame(rt, m_sampleRate);
    //!!!
}

RealTime
ItemEditDialog::getRealTime() const
{
    return RealTime::frame2RealTime(m_frame, m_sampleRate);
}

void
ItemEditDialog::setFrameDuration(long duration)
{
    m_duration = duration;
    //!!!
}

long
ItemEditDialog::getFrameDuration() const
{
    return m_duration;
}

void
ItemEditDialog::setRealDuration(RealTime rt)
{
    m_duration = RealTime::realTime2Frame(rt, m_sampleRate);
}

RealTime
ItemEditDialog::getRealDuration() const
{
    return RealTime::frame2RealTime(m_duration, m_sampleRate);
}

void
ItemEditDialog::setValue(float v)
{
    m_value = v; 
    //!!!
}

float
ItemEditDialog::getValue() const
{
    return m_value;
}

void
ItemEditDialog::setText(QString text)
{
    m_text = text;
    //!!!
}

QString
ItemEditDialog::getText() const
{
    return m_text;
}

void
ItemEditDialog::frameTimeChanged(QString s)
{
    setFrameTime(s.toInt());
}

void
ItemEditDialog::realTimeSecsChanged(QString s)
{
    RealTime rt = getRealTime();
    rt.sec = s.toInt();
    setRealTime(rt);
}

void
ItemEditDialog::realTimeNSecsChanged(QString s)
{
    RealTime rt = getRealTime();
    rt.nsec = s.toInt();
    setRealTime(rt);
}

void
ItemEditDialog::frameDurationChanged(QString s)
{
    setFrameDuration(s.toInt());
}

void
ItemEditDialog::realDurationSecsChanged(QString s)
{
    RealTime rt = getRealDuration();
    rt.sec = s.toInt();
    setRealDuration(rt);
}

void
ItemEditDialog::realDurationNSecsChanged(QString s)
{
    RealTime rt = getRealDuration();
    rt.nsec = s.toInt();
    setRealDuration(rt);
}

void
ItemEditDialog::valueChanged(double v)
{
    setValue((float)v);
}

void
ItemEditDialog::textChanged(QString text)
{
    setText(text);
}

