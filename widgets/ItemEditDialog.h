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

#ifndef _ITEM_EDIT_DIALOG_H_
#define _ITEM_EDIT_DIALOG_H_

#include <QDialog>
#include <QString>

#include "base/RealTime.h"

class ItemEditDialog : public QDialog
{
    Q_OBJECT

public:
    enum {
        ShowTime       = 1 << 0,
        ShowDuration   = 1 << 1,
        ShowValue      = 1 << 2,
        ShowText       = 1 << 3
    };

    ItemEditDialog(size_t sampleRate, int options, QWidget *parent = 0);

    void setFrameTime(long frame);
    long getFrameTime() const;

    void setRealTime(RealTime rt);
    RealTime getRealTime() const;

    void setFrameDuration(long frame);
    long getFrameDuration() const;
    
    void setRealDuration(RealTime rt);
    RealTime getRealDuration() const;

    void setValue(float value);
    float getValue() const;
    
    void setText(QString text);
    QString getText() const;

protected slots:
    void frameTimeChanged(QString);
    void realTimeSecsChanged(QString);
    void realTimeNSecsChanged(QString);
    void frameDurationChanged(QString);
    void realDurationSecsChanged(QString);
    void realDurationNSecsChanged(QString);
    void valueChanged(double);
    void textChanged(QString);

protected:
    size_t m_sampleRate;
    long m_frame;
    long m_duration;
    double m_value;
    QString m_text;
};

#endif
