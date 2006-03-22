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

#include "PluginParameterBox.h"

#include "AudioDial.h"

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLayout>
#include <QLabel>

#include <iostream>
#include <string>

PluginParameterBox::PluginParameterBox(PluginInstance *plugin, QWidget *parent) :
    QFrame(parent),
    m_plugin(plugin)
{
    m_layout = new QGridLayout;
    setLayout(m_layout);
    populate();
}

PluginParameterBox::~PluginParameterBox()
{
}

void
PluginParameterBox::populate()
{
    PluginInstance::ParameterList params = m_plugin->getParameterDescriptors();

    m_params.clear();

    if (params.empty()) {
        m_layout->addWidget
            (new QLabel(tr("This plugin has no adjustable parameters.")),
             0, 0);
    }

    for (size_t i = 0; i < params.size(); ++i) {

        QString name = params[i].name.c_str();
        QString description = params[i].description.c_str();
        QString unit = params[i].unit.c_str();

        float min = params[i].minValue;
        float max = params[i].maxValue;
        float deft = params[i].defaultValue;
        float value = m_plugin->getParameter(params[i].name);

        float qtz = 0.0;
        if (params[i].isQuantized) qtz = params[i].quantizeStep;

        // construct an integer range

        int imin = 0, imax = 100;

        if (qtz > 0.0) {
            imax = int((max - min) / qtz);
        } else {
            qtz = (max - min) / 100.0;
        }

        //!!! would be nice to ensure the default value corresponds to
        // an integer!

        QLabel *label = new QLabel(description);
        m_layout->addWidget(label, i, 0);
        
        AudioDial *dial = new AudioDial;
        dial->setObjectName(name);
        dial->setMinimum(imin);
        dial->setMaximum(imax);
        dial->setPageStep(1);
        dial->setNotchesVisible((imax - imin) <= 12);
        dial->setDefaultValue(int((deft - min) / qtz));
        dial->setValue(int((value - min) / qtz));
	dial->setFixedWidth(32);
	dial->setFixedHeight(32);
        connect(dial, SIGNAL(valueChanged(int)),
                this, SLOT(dialChanged(int)));
        m_layout->addWidget(dial, i, 1);

        QDoubleSpinBox *spinbox = new QDoubleSpinBox;
        spinbox->setObjectName(name);
        spinbox->setMinimum(min);
        spinbox->setMaximum(max);
        spinbox->setSuffix(QString(" %1").arg(unit));
        spinbox->setSingleStep(qtz);
        spinbox->setValue(value);
        connect(spinbox, SIGNAL(valueChanged(double)),
                this, SLOT(spinBoxChanged(double)));
        m_layout->addWidget(spinbox, i, 2);

        ParamRec rec;
        rec.dial = dial;
        rec.spin = spinbox;
        rec.param = params[i];
        m_params[name] = rec;
    }
}

void
PluginParameterBox::dialChanged(int ival)
{
    QObject *obj = sender();
    QString name = obj->objectName();

    if (m_params.find(name) == m_params.end()) {
        std::cerr << "WARNING: PluginParameterBox::dialChanged: Unknown parameter \"" << name.toStdString() << "\"" << std::endl;
        return;
    }

    PluginInstance::ParameterDescriptor params = m_params[name].param;

    float min = params.minValue;
    float max = params.maxValue;

    float qtz = 0.0;
    if (params.isQuantized) qtz = params.quantizeStep;
    
    if (qtz == 0.0) {
        qtz = (max - min) / 100.0;
    }

    float newValue = min + ival * qtz;

    QDoubleSpinBox *spin = m_params[name].spin;
    spin->blockSignals(true);
    spin->setValue(newValue);
    spin->blockSignals(false);

    m_plugin->setParameter(name.toStdString(), newValue);
}

void
PluginParameterBox::spinBoxChanged(double value)
{
    QObject *obj = sender();
    QString name = obj->objectName();

    if (m_params.find(name) == m_params.end()) {
        std::cerr << "WARNING: PluginParameterBox::spinBoxChanged: Unknown parameter \"" << name.toStdString() << "\"" << std::endl;
        return;
    }

    PluginInstance::ParameterDescriptor params = m_params[name].param;

    float min = params.minValue;
    float max = params.maxValue;

    float qtz = 0.0;
    if (params.isQuantized) qtz = params.quantizeStep;
    
    if (qtz > 0.0) {
        int step = int((value - min) / qtz);
        value = min + step * qtz;
    }

    int imin = 0, imax = 100;
    
    if (qtz > 0.0) {
        imax = int((max - min) / qtz);
    } else {
        qtz = (max - min) / 100.0;
    }

    int ival = (value - min) / qtz;

    AudioDial *dial = m_params[name].dial;
    dial->blockSignals(true);
    dial->setValue(ival);
    dial->blockSignals(false);

    m_plugin->setParameter(name.toStdString(), value);
}


