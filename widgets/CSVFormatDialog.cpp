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


CSVFormatDialog::CSVFormatDialog(QWidget *parent, CSVFormat format,
				 size_t defaultSampleRate) :
    QDialog(parent),
    m_modelType(CSVFormat::OneDimensionalModel),
    m_timingType(CSVFormat::ExplicitTiming),
    m_timeUnits(CSVFormat::TimeAudioFrames),
    m_separator(""),
    m_behaviour(QString::KeepEmptyParts)
{
    setModal(true);
    setWindowTitle(tr("Select Data Format"));

    m_modelType = format.getModelType();
    m_timingType = format.getTimingType();
    m_timeUnits = format.getTimeUnits();
    m_separator = format.getSeparator();
    m_sampleRate = format.getSampleRate();
    m_windowSize = format.getWindowSize();
    m_behaviour = format.getSplitBehaviour();
    m_example = format.getExample();
    m_maxExampleCols = format.getMaxExampleCols();

    QGridLayout *layout = new QGridLayout;

    layout->addWidget(new QLabel(tr("<b>Select Data Format</b><p>Please select the correct data format for this file.")),
		      0, 0, 1, 4);

    layout->addWidget(new QLabel(tr("Each row specifies:")), 1, 0);

    m_modelTypeCombo = new QComboBox;
    m_modelTypeCombo->addItem(tr("A point in time"));
    m_modelTypeCombo->addItem(tr("A value at a time"));
    m_modelTypeCombo->addItem(tr("A set of values"));
    layout->addWidget(m_modelTypeCombo, 1, 1, 1, 2);
    connect(m_modelTypeCombo, SIGNAL(activated(int)),
	    this, SLOT(modelTypeChanged(int)));
    m_modelTypeCombo->setCurrentIndex(int(m_modelType));

    layout->addWidget(new QLabel(tr("The first column contains:")), 2, 0);
    
    m_timingTypeCombo = new QComboBox;
    m_timingTypeCombo->addItem(tr("Time, in seconds"));
    m_timingTypeCombo->addItem(tr("Time, in audio sample frames"));
    m_timingTypeCombo->addItem(tr("Data (rows are consecutive in time)"));
    layout->addWidget(m_timingTypeCombo, 2, 1, 1, 2);
    connect(m_timingTypeCombo, SIGNAL(activated(int)),
	    this, SLOT(timingTypeChanged(int)));
    m_timingTypeCombo->setCurrentIndex(m_timingType == CSVFormat::ExplicitTiming ?
                                       m_timeUnits == CSVFormat::TimeSeconds ? 0 : 1 : 2);

    m_sampleRateLabel = new QLabel(tr("Audio sample rate (Hz):"));
    layout->addWidget(m_sampleRateLabel, 3, 0);
    
    size_t sampleRates[] = {
	8000, 11025, 12000, 22050, 24000, 32000,
	44100, 48000, 88200, 96000, 176400, 192000
    };

    m_sampleRateCombo = new QComboBox;
    m_sampleRate = defaultSampleRate;
    for (size_t i = 0; i < sizeof(sampleRates) / sizeof(sampleRates[0]); ++i) {
	m_sampleRateCombo->addItem(QString("%1").arg(sampleRates[i]));
	if (sampleRates[i] == m_sampleRate) m_sampleRateCombo->setCurrentIndex(i);
    }
    m_sampleRateCombo->setEditable(true);

    layout->addWidget(m_sampleRateCombo, 3, 1);
    connect(m_sampleRateCombo, SIGNAL(activated(QString)),
	    this, SLOT(sampleRateChanged(QString)));
    connect(m_sampleRateCombo, SIGNAL(editTextChanged(QString)),
	    this, SLOT(sampleRateChanged(QString)));

    m_windowSizeLabel = new QLabel(tr("Frame increment between rows:"));
    layout->addWidget(m_windowSizeLabel, 4, 0);

    m_windowSizeCombo = new QComboBox;
    m_windowSize = 1024;
    for (int i = 0; i <= 16; ++i) {
	int value = 1 << i;
	m_windowSizeCombo->addItem(QString("%1").arg(value));
	if (value == int(m_windowSize)) m_windowSizeCombo->setCurrentIndex(i);
    }
    m_windowSizeCombo->setEditable(true);

    layout->addWidget(m_windowSizeCombo, 4, 1);
    connect(m_windowSizeCombo, SIGNAL(activated(QString)),
	    this, SLOT(windowSizeChanged(QString)));
    connect(m_windowSizeCombo, SIGNAL(editTextChanged(QString)),
	    this, SLOT(windowSizeChanged(QString)));

    layout->addWidget(new QLabel(tr("\nExample data from file:")), 5, 0, 1, 4);

    m_exampleWidget = new QTableWidget
	(std::min(10, m_example.size()), m_maxExampleCols);

    layout->addWidget(m_exampleWidget, 6, 0, 1, 4);
    layout->setColumnStretch(3, 10);
    layout->setRowStretch(4, 10);

    QPushButton *ok = new QPushButton(tr("OK"));
    connect(ok, SIGNAL(clicked()), this, SLOT(accept()));
    ok->setDefault(true);

    QPushButton *cancel = new QPushButton(tr("Cancel"));
    connect(cancel, SIGNAL(clicked()), this, SLOT(reject()));

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(ok);
    buttonLayout->addWidget(cancel);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(layout);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
    
    timingTypeChanged(m_timingTypeCombo->currentIndex());
}

CSVFormatDialog::~CSVFormatDialog()
{
}

CSVFormat
CSVFormatDialog::getFormat() const
{
    CSVFormat format;
    format.setModelType(m_modelType);
    format.setTimingType(m_timingType);
    format.setTimeUnits(m_timeUnits);
    format.setSeparator(m_separator);
    format.setSampleRate(m_sampleRate);
    format.setWindowSize(m_windowSize);
    format.setSplitBehaviour(m_behaviour);
    return format;
}

void
CSVFormatDialog::populateExample()
{
    m_exampleWidget->setColumnCount
        (m_timingType == CSVFormat::ExplicitTiming ?
	 m_maxExampleCols - 1 : m_maxExampleCols);

    m_exampleWidget->setHorizontalHeaderLabels(QStringList());

    for (int i = 0; i < m_example.size(); ++i) {
	for (int j = 0; j < m_example[i].size(); ++j) {

	    QTableWidgetItem *item = new QTableWidgetItem(m_example[i][j]);

	    if (j == 0) {
		if (m_timingType == CSVFormat::ExplicitTiming) {
		    m_exampleWidget->setVerticalHeaderItem(i, item);
		    continue;
		} else {
		    QTableWidgetItem *header =
			new QTableWidgetItem(QString("%1").arg(i));
		    header->setFlags(Qt::ItemIsEnabled);
		    m_exampleWidget->setVerticalHeaderItem(i, header);
		}
	    }
	    int index = j;
	    if (m_timingType == CSVFormat::ExplicitTiming) --index;
	    item->setFlags(Qt::ItemIsEnabled);
	    m_exampleWidget->setItem(i, index, item);
	}
    }
}

void
CSVFormatDialog::modelTypeChanged(int type)
{
    m_modelType = (CSVFormat::ModelType)type;

    if (m_modelType == CSVFormat::ThreeDimensionalModel) {
        // We can't load 3d models with explicit timing, because the 3d
        // model is dense so we need a fixed sample increment
        m_timingTypeCombo->setCurrentIndex(2);
        timingTypeChanged(2);
    }
}

void
CSVFormatDialog::timingTypeChanged(int type)
{
    switch (type) {

    case 0:
	m_timingType = CSVFormat::ExplicitTiming;
	m_timeUnits = CSVFormat::TimeSeconds;
	m_sampleRateCombo->setEnabled(false);
	m_sampleRateLabel->setEnabled(false);
	m_windowSizeCombo->setEnabled(false);
	m_windowSizeLabel->setEnabled(false);
        if (m_modelType == CSVFormat::ThreeDimensionalModel) {
            m_modelTypeCombo->setCurrentIndex(1);
            modelTypeChanged(1);
        }
	break;

    case 1:
	m_timingType = CSVFormat::ExplicitTiming;
	m_timeUnits = CSVFormat::TimeAudioFrames;
	m_sampleRateCombo->setEnabled(true);
	m_sampleRateLabel->setEnabled(true);
	m_windowSizeCombo->setEnabled(false);
	m_windowSizeLabel->setEnabled(false);
        if (m_modelType == CSVFormat::ThreeDimensionalModel) {
            m_modelTypeCombo->setCurrentIndex(1);
            modelTypeChanged(1);
        }
	break;

    case 2:
	m_timingType = CSVFormat::ImplicitTiming;
	m_timeUnits = CSVFormat::TimeWindows;
	m_sampleRateCombo->setEnabled(true);
	m_sampleRateLabel->setEnabled(true);
	m_windowSizeCombo->setEnabled(true);
	m_windowSizeLabel->setEnabled(true);
	break;
    }

    populateExample();
}

void
CSVFormatDialog::sampleRateChanged(QString rateString)
{
    bool ok = false;
    int sampleRate = rateString.toInt(&ok);
    if (ok) m_sampleRate = sampleRate;
}

void
CSVFormatDialog::windowSizeChanged(QString sizeString)
{
    bool ok = false;
    int size = sizeString.toInt(&ok);
    if (ok) m_windowSize = size;
}
