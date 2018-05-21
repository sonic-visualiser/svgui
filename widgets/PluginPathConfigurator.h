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

#ifndef SV_PLUGIN_PATH_CONFIGURATOR_H
#define SV_PLUGIN_PATH_CONFIGURATOR_H

#include <QFrame>
#include <QStringList>

class QLabel;
class QWidget;
class QListWidget;
class QPushButton;
class QGridLayout;
class QComboBox;

class PluginPathConfigurator : public QFrame
{
    Q_OBJECT

public:
    PluginPathConfigurator(QWidget *parent = 0);
    ~PluginPathConfigurator();

    // Text used to identify a plugin type to the user.
    // e.g. "LADSPA", "Vamp", or potentially transliterations thereof
    typedef QString PluginTypeLabel;

    struct PathConfig {
        QStringList directories;
        QString envVariable; // e.g. "LADSPA_PATH" etc
    };

    typedef std::map<PluginTypeLabel, PathConfig> Paths;
    
    void setPaths(Paths paths);
    Paths getPaths() const;

signals:
    void pathsChanged(const Paths &paths);

private slots:
    void upClicked();
    void downClicked();
    void deleteClicked();
    void currentTypeChanged(QString);
    void currentLocationChanged(int);
    
private:
    QGridLayout *m_layout;
    QLabel *m_header;
    QComboBox *m_pluginTypeSelector;
    QListWidget *m_list;
    QPushButton *m_up;
    QPushButton *m_down;
    QPushButton *m_delete;

    Paths m_paths;
    
    void populate();
    void populateFor(QString type, int makeCurrent);
};

#endif


