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

#ifndef _PLUGIN_PARAMETER_DIALOG_H_
#define _PLUGIN_PARAMETER_DIALOG_H_

#include <QDialog>

namespace Vamp { class PluginBase; }
class PluginParameterBox;

/**
 * A dialog for editing the parameters of a given plugin, using a
 * PluginParameterBox.  This dialog does not contain any mechanism for
 * selecting the plugin in the first place.  Note that the dialog
 * directly modifies the parameters of the plugin, so they will remain
 * modified even if the dialog is then cancelled.
 */

class PluginParameterDialog : public QDialog
{
    Q_OBJECT
    
public:
    PluginParameterDialog(Vamp::PluginBase *,
                          int sourceChannels,
                          int targetChannels,
                          int defaultChannel,
                          QWidget *parent = 0);
    ~PluginParameterDialog();

    Vamp::PluginBase *getPlugin() { return m_plugin; }

    int getChannel() const { return m_channel; }

signals:
    void pluginConfigurationChanged(QString);

protected slots:
    void channelComboChanged(int);

protected:
    Vamp::PluginBase *m_plugin;
    int m_channel;
    PluginParameterBox *m_parameterBox;
};

#endif

