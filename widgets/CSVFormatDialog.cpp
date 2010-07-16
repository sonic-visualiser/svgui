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

#include "CSVFormatDialog.h"

#include <QFrame>
#include <QGridLayout>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include <QDialogButtonBox>

#include <iostream>

CSVFormatDialog::CSVFormatDialog(QWidget *parent, CSVFormat format) :
    QDialog(parent),
    m_format(format)
{
    setModal(true);
    setWindowTitle(tr("Select Data Format"));

    QGridLayout *layout = new QGridLayout;

    int row = 0;

    layout->addWidget(new QLabel(tr("Please select the correct data format for this file.")),
		      row++, 0, 1, 4);

    QFrame *exampleFrame = new QFrame;
    exampleFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    exampleFrame->setLineWidth(2);
    QGridLayout *exampleLayout = new QGridLayout;
    exampleFrame->setLayout(exampleLayout);

    QPalette palette = exampleFrame->palette();
    palette.setColor(QPalette::Window, palette.color(QPalette::Base));
    exampleFrame->setPalette(palette);

    QFont fp;
    fp.setFixedPitch(true);
    fp.setStyleHint(QFont::TypeWriter);
    fp.setFamily("Monospaced");
    
    int columns = format.getColumnCount();
    QList<QStringList> example = m_format.getExample();

    for (int i = 0; i < columns; ++i) {
        
        QComboBox *cpc = new QComboBox;
        m_columnPurposeCombos.push_back(cpc);
        exampleLayout->addWidget(cpc, 0, i);

        // NB must be in the same order as the CSVFormat::ColumnPurpose enum
        cpc->addItem(tr("<ignore>")); // ColumnUnknown
        cpc->addItem(tr("Time"));     // ColumnStartTime
        cpc->addItem(tr("End time")); // ColumnEndTime
        cpc->addItem(tr("Duration")); // ColumnDuration
        cpc->addItem(tr("Value"));    // ColumnValue
        cpc->addItem(tr("Label"));    // ColumnLabel
        cpc->setCurrentIndex(int(m_format.getColumnPurpose(i)));
        connect(cpc, SIGNAL(activated(int)), this, SLOT(columnPurposeChanged(int)));

        if (i < m_format.getMaxExampleCols()) {
            for (int j = 0; j < example.size() && j < 6; ++j) {
                QLabel *label = new QLabel;
                label->setTextFormat(Qt::PlainText);
                label->setText(example[j][i]);
                label->setFont(fp);
                label->setPalette(palette);
                exampleLayout->addWidget(label, j+1, i);
            }
        }
    }

    layout->addWidget(exampleFrame, row, 0, 1, 4);
    layout->setColumnStretch(3, 10);
    layout->setRowStretch(row++, 10);

    layout->addWidget(new QLabel(tr("Each row describes:")), row, 0);

    m_modelTypeCombo = new QComboBox;
    m_modelTypeCombo->addItem(tr("A point in time"));
    m_modelTypeCombo->addItem(tr("A value at a time"));
    m_modelTypeCombo->addItem(tr("A value across a time range"));
    m_modelTypeCombo->addItem(tr("A set of values"));
    layout->addWidget(m_modelTypeCombo, row++, 1, 1, 2);
    connect(m_modelTypeCombo, SIGNAL(activated(int)),
	    this, SLOT(modelTypeChanged(int)));
    m_modelTypeCombo->setCurrentIndex(int(m_format.getModelType()));

    layout->addWidget(new QLabel(tr("Timing is specified:")), row, 0);
    
    m_timingTypeCombo = new QComboBox;
    m_timingTypeCombo->addItem(tr("Explicitly, in seconds"));
    m_timingTypeCombo->addItem(tr("Explicitly, in audio sample frames"));
    m_timingTypeCombo->addItem(tr("Implicitly: rows are equally spaced in time"));
    layout->addWidget(m_timingTypeCombo, row++, 1, 1, 2);
    connect(m_timingTypeCombo, SIGNAL(activated(int)),
	    this, SLOT(timingTypeChanged(int)));
    m_timingTypeCombo->setCurrentIndex
        (m_format.getTimingType() == CSVFormat::ExplicitTiming ?
         m_format.getTimeUnits() == CSVFormat::TimeSeconds ? 0 : 1 : 2);

    m_sampleRateLabel = new QLabel(tr("Audio sample rate (Hz):"));
    layout->addWidget(m_sampleRateLabel, row, 0);
    
    size_t sampleRates[] = {
	8000, 11025, 12000, 22050, 24000, 32000,
	44100, 48000, 88200, 96000, 176400, 192000
    };

    m_sampleRateCombo = new QComboBox;
    for (size_t i = 0; i < sizeof(sampleRates) / sizeof(sampleRates[0]); ++i) {
	m_sampleRateCombo->addItem(QString("%1").arg(sampleRates[i]));
	if (sampleRates[i] == m_format.getSampleRate()) {
            m_sampleRateCombo->setCurrentIndex(i);
        }
    }
    m_sampleRateCombo->setEditable(true);

    layout->addWidget(m_sampleRateCombo, row++, 1);
    connect(m_sampleRateCombo, SIGNAL(activated(QString)),
	    this, SLOT(sampleRateChanged(QString)));
    connect(m_sampleRateCombo, SIGNAL(editTextChanged(QString)),
	    this, SLOT(sampleRateChanged(QString)));

    m_windowSizeLabel = new QLabel(tr("Frame increment between rows:"));
    layout->addWidget(m_windowSizeLabel, row, 0);

    m_windowSizeCombo = new QComboBox;
    for (int i = 0; i <= 16; ++i) {
	int value = 1 << i;
	m_windowSizeCombo->addItem(QString("%1").arg(value));
	if (value == int(m_format.getWindowSize())) {
            m_windowSizeCombo->setCurrentIndex(i);
        }
    }
    m_windowSizeCombo->setEditable(true);

    layout->addWidget(m_windowSizeCombo, row++, 1);
    connect(m_windowSizeCombo, SIGNAL(activated(QString)),
	    this, SLOT(windowSizeChanged(QString)));
    connect(m_windowSizeCombo, SIGNAL(editTextChanged(QString)),
	    this, SLOT(windowSizeChanged(QString)));

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok |
                                                QDialogButtonBox::Cancel);
    layout->addWidget(bb, row++, 0, 1, 4);
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));

    setLayout(layout);

    modelTypeChanged(m_modelTypeCombo->currentIndex());
    timingTypeChanged(m_timingTypeCombo->currentIndex());
}

CSVFormatDialog::~CSVFormatDialog()
{
}

CSVFormat
CSVFormatDialog::getFormat() const
{
    return m_format;
}

void
CSVFormatDialog::modelTypeChanged(int type)
{
    m_format.setModelType((CSVFormat::ModelType)type);
}

void
CSVFormatDialog::timingTypeChanged(int type)
{
    switch (type) {

    case 0:
	m_format.setTimingType(CSVFormat::ExplicitTiming);
	m_format.setTimeUnits(CSVFormat::TimeSeconds);
	m_sampleRateCombo->setEnabled(false);
	m_sampleRateLabel->setEnabled(false);
	m_windowSizeCombo->setEnabled(false);
	m_windowSizeLabel->setEnabled(false);
	break;

    case 1:
	m_format.setTimingType(CSVFormat::ExplicitTiming);
	m_format.setTimeUnits(CSVFormat::TimeAudioFrames);
	m_sampleRateCombo->setEnabled(true);
	m_sampleRateLabel->setEnabled(true);
	m_windowSizeCombo->setEnabled(false);
	m_windowSizeLabel->setEnabled(false);
	break;

    case 2:
	m_format.setTimingType(CSVFormat::ImplicitTiming);
	m_format.setTimeUnits(CSVFormat::TimeWindows);
	m_sampleRateCombo->setEnabled(true);
	m_sampleRateLabel->setEnabled(true);
	m_windowSizeCombo->setEnabled(true);
	m_windowSizeLabel->setEnabled(true);
	break;
    }
}

void
CSVFormatDialog::sampleRateChanged(QString rateString)
{
    bool ok = false;
    int sampleRate = rateString.toInt(&ok);
    if (ok) m_format.setSampleRate(sampleRate);
}

void
CSVFormatDialog::windowSizeChanged(QString sizeString)
{
    bool ok = false;
    int size = sizeString.toInt(&ok);
    if (ok) m_format.setWindowSize(size);
}

void
CSVFormatDialog::columnPurposeChanged(int purpose)
{
    QObject *o = sender();
    QComboBox *cb = qobject_cast<QComboBox *>(o);
    if (!cb) return;
    int changedCol = -1;
    for (int i = 0; i < m_columnPurposeCombos.size(); ++i) {
        if (cb == m_columnPurposeCombos[i]) {
            changedCol = i;
            break;
        }
    }
    if (changedCol < 0) {
        std::cerr << "Hm, some problem here -- column combo changed, but cannot locate column" << std::endl;
        return;
    }
    
}


