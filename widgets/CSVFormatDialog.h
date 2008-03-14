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

#ifndef _CSV_FORMAT_DIALOG_H_
#define _CSV_FORMAT_DIALOG_H_

#include "data/fileio/CSVFormat.h"

class QTableWidget;
class QComboBox;
class QLabel;
    
#include <QDialog>

class CSVFormatDialog : public QDialog
{
    Q_OBJECT
    
public:
    CSVFormatDialog(QWidget *parent, CSVFormat initialFormat,
                    size_t defaultSampleRate);
    ~CSVFormatDialog();

    CSVFormat getFormat() const;
    
protected slots:
    void modelTypeChanged(int type);
    void timingTypeChanged(int type);
    void sampleRateChanged(QString);
    void windowSizeChanged(QString);

protected:
    CSVFormat::ModelType  m_modelType;
    CSVFormat::TimingType m_timingType;
    CSVFormat::TimeUnits  m_timeUnits;

    QString    m_separator;
    size_t     m_sampleRate;
    size_t     m_windowSize;

    QString::SplitBehavior m_behaviour;
    
    QList<QStringList> m_example;
    int m_maxExampleCols;
    QTableWidget *m_exampleWidget;
    
    QComboBox *m_modelTypeCombo;
    QComboBox *m_timingTypeCombo;
    QLabel *m_sampleRateLabel;
    QComboBox *m_sampleRateCombo;
    QLabel *m_windowSizeLabel;
    QComboBox *m_windowSizeCombo;

    void populateExample();
};

#endif
