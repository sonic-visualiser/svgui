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

#include "UnitConverter.h"

#include <QSpinBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QGridLayout>

#include "base/Debug.h"
#include "base/Pitch.h"

using namespace std;

UnitConverter::UnitConverter(QWidget *parent) :
    QDialog(parent)
{
    QGridLayout *grid = new QGridLayout;
    setLayout(grid);

    m_hz = new QDoubleSpinBox;
    m_hz->setSuffix(QString(" Hz"));
    m_hz->setDecimals(6);
    m_hz->setMinimum(1e-6);
    m_hz->setMaximum(1e6);
    m_hz->setValue(440);
    connect(m_hz, SIGNAL(valueChanged(double)),
	    this, SLOT(hzChanged(double)));

    m_midi = new QSpinBox;
    m_midi->setMinimum(0);
    m_midi->setMaximum(127);
    connect(m_midi, SIGNAL(valueChanged(int)),
	    this, SLOT(midiChanged(int)));

    m_note = new QComboBox;
    //!!!

    m_octave = new QSpinBox;
    //!!!

    m_cents = new QDoubleSpinBox;
    m_cents->setSuffix(tr(" cents"));
    m_cents->setDecimals(4);
    m_cents->setMinimum(-50);
    m_cents->setMaximum(50);
    connect(m_cents, SIGNAL(valueChanged(double)),
	    this, SLOT(centsChanged(double)));

    m_piano = new QSpinBox;
    //!!!
    
    grid->addWidget(m_hz, 1, 0);
    grid->addWidget(new QLabel(tr("=")), 1, 1);

    grid->addWidget(new QLabel(tr("MIDI note")), 1, 2, 1, 2);
    grid->addWidget(m_midi, 1, 4);

    grid->addWidget(new QLabel(tr("+")), 1, 5);
    grid->addWidget(m_cents, 1, 6);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    grid->addWidget(bb, 2, 0, 1, 7);
    connect(bb, SIGNAL(rejected()), this, SLOT(close()));

    updateAllFromHz();
}

UnitConverter::~UnitConverter()
{
}

void
UnitConverter::hzChanged(double hz)
{
    cerr << "hzChanged: " << hz << endl;
    updateAllFromHz();
}

void
UnitConverter::midiChanged(int midi)
{
    cerr << "midiChanged: " << midi << endl;
    m_hz->setValue(Pitch::getFrequencyForPitch(m_midi->value(), m_cents->value()));
    updateAllFromHz();
}

void
UnitConverter::noteChanged(int note)
{
    cerr << "noteChanged: " << note << endl;
}

void
UnitConverter::octaveChanged(int oct)
{
    cerr << "octaveChanged: " << oct << endl;
}

void
UnitConverter::centsChanged(double cents)
{
    cerr << "centsChanged: " << cents << endl;
    m_hz->setValue(Pitch::getFrequencyForPitch(m_midi->value(), m_cents->value()));
    updateAllFromHz();
}

void
UnitConverter::pianoChanged(int piano)
{
    cerr << "pianoChanged: " << piano << endl;
}

void
UnitConverter::updateAllFromHz()
{
    float cents = 0;
    int pitch = Pitch::getPitchForFrequency(m_hz->value(), &cents);

    m_midi->blockSignals(true);
    m_cents->blockSignals(true);
    
    m_midi->setValue(pitch);
    m_cents->setValue(cents);

    m_midi->blockSignals(false);
    m_cents->blockSignals(false);
}



 
