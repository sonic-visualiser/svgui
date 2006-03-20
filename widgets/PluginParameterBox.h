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

#ifndef _PLUGIN_PARAMETER_BOX_H_
#define _PLUGIN_PARAMETER_BOX_H_

#include "plugin/PluginInstance.h"

#include <QFrame>
#include <map>

class AudioDial;
class QDoubleSpinBox;
class QGridLayout;

class PluginParameterBox : public QFrame
{
    Q_OBJECT
    
public:
    PluginParameterBox(PluginInstance *);
    ~PluginParameterBox();

    PluginInstance *getPlugin() { return m_plugin; }

protected slots:
    void dialChanged(int);
    void spinBoxChanged(double);

protected:
    void populate();

    QGridLayout *m_layout;
    PluginInstance *m_plugin;

    struct ParamRec {
        AudioDial *dial;
        QDoubleSpinBox *spin;
        PluginInstance::ParameterDescriptor param;
    };

    std::map<QString, ParamRec> m_params;
};

#endif

