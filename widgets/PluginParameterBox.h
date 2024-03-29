/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_PLUGIN_PARAMETER_BOX_H
#define SV_PLUGIN_PARAMETER_BOX_H

#include <vamp-hostsdk/PluginBase.h>

#include <QFrame>
#include <map>
#include <memory>

class QDoubleSpinBox;
class QCheckBox;
class QGridLayout;
class QComboBox;

namespace sv {

class AudioDial;

class PluginParameterBox : public QFrame
{
    Q_OBJECT
    
public:
    PluginParameterBox(std::shared_ptr<Vamp::PluginBase>,
                       QWidget *parent = 0);
    ~PluginParameterBox();

    std::shared_ptr<Vamp::PluginBase> getPlugin() { return m_plugin; }

signals:
    void pluginConfigurationChanged(QString);

protected slots:
    void dialChanged(int);
    void spinBoxChanged(double);
    void checkBoxChanged(int);
    void programComboChanged(const QString &);

protected:
    void populate();
    void updateProgramCombo();

    QGridLayout *m_layout;
    std::shared_ptr<Vamp::PluginBase> m_plugin;

    struct ParamRec {
        AudioDial *dial;
        QDoubleSpinBox *spin;
        QCheckBox *check;
        QComboBox *combo;
        Vamp::PluginBase::ParameterDescriptor param;
    };

    QComboBox *m_programCombo;

    std::map<QString, ParamRec> m_params;
    std::map<QString, QString> m_nameMap;
    Vamp::PluginBase::ProgramList m_programs;
};

} // end namespace sv

#endif

