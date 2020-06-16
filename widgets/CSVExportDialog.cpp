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

#include "CSVExportDialog.h"

#include "view/ViewManager.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>

#include <vector>

using namespace std;

//!!! todo: remember & re-apply last set of options chosen for this layer type

CSVExportDialog::CSVExportDialog(Configuration config, QWidget *parent) :
    QDialog(parent),
    m_config(config)
{
    setWindowTitle(tr("Export Layer"));

    QString intro = tr("Exporting layer \"%1\" to %2 file.")
        .arg(config.layerName)
        .arg(config.fileExtension.toUpper());

    QVBoxLayout *vbox = new QVBoxLayout;
    
    QLabel *label = new QLabel(intro);
    label->setWordWrap(true);
    vbox->addWidget(label);

    int space = ViewManager::scalePixelSize(2);

    vbox->addSpacing(space);
    
    QGroupBox *rowColGroup = new QGroupBox(tr("Row and column options:"));
    QGridLayout *rowColLayout = new QGridLayout;
    rowColGroup->setLayout(rowColLayout);

    vector<pair<QString, QChar>> separators {
        { tr("Comma"), ',' },
        { tr("Tab"), '\t' },
        { tr("Space"), ' ' },
        { tr("Pipe"), '|' },
        { tr("Slash"), '/' },
        { tr("Colon"), ':' }
    };
    
    QChar defaultSeparator = ',';
    if (m_config.fileExtension != "csv") {
        defaultSeparator = '\t';
    }
    
    rowColLayout->addWidget(new QLabel(tr("Column separator:")));
    m_separatorCombo = new QComboBox;
    for (auto p: separators) {
        if (p.second == '\t' || p.second == ' ') {
            m_separatorCombo->addItem(p.first, p.second);
        } else {
            m_separatorCombo->addItem(tr("%1 '%2'").arg(p.first).arg(p.second),
                                      p.second);
        }
        if (p.second == defaultSeparator) {
            m_separatorCombo->setCurrentIndex(m_separatorCombo->count()-1);
        }
    }
    m_separatorCombo->setEditable(false);
    rowColLayout->addWidget(m_separatorCombo, 0, 1);
    rowColLayout->setColumnStretch(2, 10);

    m_header = new QCheckBox
        (tr("Include a header row before the data rows"));
    m_timestamps = new QCheckBox
        (tr("Include a timestamp column before the data columns"));
    rowColLayout->addWidget(m_header, 1, 0, 1, 3);
    rowColLayout->addWidget(m_timestamps, 2, 0, 1, 3);
    
    if (!m_config.isDense) {
        m_timestamps->setChecked(true);
        m_timestamps->setEnabled(false);
    }

    vbox->addWidget(rowColGroup);
    
    vbox->addSpacing(space);
    
    QGroupBox *framesGroup = new QGroupBox
        (tr("Timing format:"));

    m_seconds = new QRadioButton
        (tr("Write times in seconds"));
    m_frames = new QRadioButton
        (tr("Write times in audio sample frames"));
    m_seconds->setChecked(true);

    QVBoxLayout *framesLayout = new QVBoxLayout;
    framesLayout->addWidget(m_seconds);
    framesLayout->addWidget(m_frames);
    framesGroup->setLayout(framesLayout);
    vbox->addWidget(framesGroup);
    
    vbox->addSpacing(space);

    if (m_config.isDense) {
        m_seconds->setEnabled(false);
        m_frames->setEnabled(false);
    }
    
    QGroupBox *rangeGroup = new QGroupBox
        (tr("Range to export:"));

    QButtonGroup *selectionGroup = new QButtonGroup(rangeGroup);
    QButtonGroup *viewGroup = new QButtonGroup(rangeGroup);
    
    m_selectionOnly = new QRadioButton
        (tr("Export only the current selection"));
    QRadioButton *fullDuration = new QRadioButton
        (tr("Export the full duration of the layer"));

    selectionGroup->addButton(m_selectionOnly);
    selectionGroup->addButton(fullDuration);

    if (m_config.haveSelection) {
        m_selectionOnly->setChecked(true);
    } else {
        m_selectionOnly->setEnabled(false);
        fullDuration->setEnabled(false);
        fullDuration->setChecked(true);
    }

    QVBoxLayout *rangeLayout = new QVBoxLayout;
    rangeLayout->addWidget(m_selectionOnly);
    rangeLayout->addWidget(fullDuration);

    if (m_config.haveView && m_config.isDense) {

        m_viewOnly = new QRadioButton
            (tr("Export only the height of the visible view"));
        QRadioButton *fullHeight = new QRadioButton
            (tr("Export the full height of the layer"));

        viewGroup->addButton(m_viewOnly);
        viewGroup->addButton(fullHeight);

        m_viewOnly->setChecked(true);
    
        rangeLayout->addSpacing(space);
    
        rangeLayout->addWidget(m_viewOnly);
        rangeLayout->addWidget(fullHeight);

    } else {
        m_viewOnly = nullptr;
    }

    rangeGroup->setLayout(rangeLayout);
    vbox->addWidget(rangeGroup);
    
    vbox->addSpacing(space);
    
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok |
                                                QDialogButtonBox::Cancel);
    vbox->addWidget(bb);
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));

    connect(m_timestamps, SIGNAL(toggled(bool)),
            this, SLOT(timestampsToggled(bool)));

    setLayout(vbox);
}

void
CSVExportDialog::timestampsToggled(bool on)
{
    m_seconds->setEnabled(on);
    m_frames->setEnabled(on);
}

QString
CSVExportDialog::getDelimiter() const
{
    return m_separatorCombo->currentData().toChar();
}

bool
CSVExportDialog::shouldIncludeHeader() const
{
    return m_header && m_header->isChecked();
}

bool
CSVExportDialog::shouldIncludeTimestamps() const
{
    return m_timestamps && m_timestamps->isChecked();
}

bool
CSVExportDialog::shouldWriteTimeInFrames() const
{
    return shouldIncludeTimestamps() && m_frames && m_frames->isChecked();
}

bool
CSVExportDialog::shouldConstrainToViewHeight() const
{
    return m_viewOnly && m_viewOnly->isChecked();
}

bool
CSVExportDialog::shouldConstrainToSelection() const
{
    return m_selectionOnly && m_selectionOnly->isChecked();
}

