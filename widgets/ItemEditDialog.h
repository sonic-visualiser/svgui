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

#ifndef SV_ITEM_EDIT_DIALOG_H
#define SV_ITEM_EDIT_DIALOG_H

#include <QDialog>
#include <QString>

#include "base/RealTime.h"

class QSpinBox;
class QDoubleSpinBox;
class QLineEdit;

namespace sv {

class ItemEditDialog : public QDialog
{
    Q_OBJECT

public:
    enum {
        ShowTime       = 1 << 0,
        ShowDuration   = 1 << 1,
        ShowValue      = 1 << 2,
        ShowText       = 1 << 3,
        ShowLevel      = 1 << 4
    };

    struct LabelOptions {
        LabelOptions();
        QString valueLabel;
        QString levelLabel;
        QString valueUnits;
        QString levelUnits;
    };
    
    ItemEditDialog(sv_samplerate_t sampleRate,
                   int options,
                   LabelOptions labelOptions = {},
                   QWidget *parent = 0);
    
    ItemEditDialog(sv_samplerate_t sampleRate,
                   int options,
                   QString scaleUnits,
                   QWidget *parent = 0);

    void setFrameTime(sv_frame_t frame);
    sv_frame_t getFrameTime() const;

    void setRealTime(RealTime rt);
    RealTime getRealTime() const;

    void setFrameDuration(sv_frame_t frame);
    sv_frame_t getFrameDuration() const;
    
    void setRealDuration(RealTime rt);
    RealTime getRealDuration() const;

    void setValue(float value);
    float getValue() const;

    void setLevel(float level);
    float getLevel() const;

    void setText(QString text);
    QString getText() const;

protected slots:
    void frameTimeChanged(int); // must be int as invoked from int signal
    void realTimeSecsChanged(int);
    void realTimeUSecsChanged(int);
    void frameDurationChanged(int); // must be int as invoked from int signal
    void realDurationSecsChanged(int);
    void realDurationUSecsChanged(int);
    void valueChanged(double);
    void levelChanged(double);
    void textChanged(QString);
    void reset();

protected:
    sv_samplerate_t m_sampleRate;
    sv_frame_t m_defaultFrame;
    sv_frame_t m_defaultDuration;
    float m_defaultValue;
    float m_defaultLevel;
    QString m_defaultText;
    QSpinBox *m_frameTimeSpinBox;
    QSpinBox *m_realTimeSecsSpinBox;
    QSpinBox *m_realTimeUSecsSpinBox;
    QSpinBox *m_frameDurationSpinBox;
    QSpinBox *m_realDurationSecsSpinBox;
    QSpinBox *m_realDurationUSecsSpinBox;
    QDoubleSpinBox *m_valueSpinBox;
    QDoubleSpinBox *m_levelSpinBox;
    QLineEdit *m_textField;
    QPushButton *m_resetButton;
};

} // end namespace sv

#endif
