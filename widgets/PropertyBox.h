/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#ifndef _PROPERTY_BOX_H_
#define _PROPERTY_BOX_H_

#include "base/PropertyContainer.h"

#include <QFrame>
#include <map>

class QLayout;
class QWidget;
class QGridLayout;
class QVBoxLayout;

class PropertyBox : public QFrame
{
    Q_OBJECT

public:
    PropertyBox(PropertyContainer *);
    ~PropertyBox();

    PropertyContainer *getContainer() { return m_container; }

signals:
    void changePlayGain(float);
    void changePlayGainDial(int);
    void changePlayPan(float);
    void changePlayPanDial(int);
    void showLayer(bool);

public slots:
    void propertyContainerPropertyChanged(PropertyContainer *);

protected slots:
    void propertyControllerChanged(int);

    void playGainChanged(float);
    void playGainDialChanged(int);
    void playPanChanged(float);
    void playPanDialChanged(int);

    void populateViewPlayFrame();

protected:
    void updatePropertyEditor(PropertyContainer::PropertyName);

    QWidget *m_mainWidget;
    QGridLayout *m_layout;
    PropertyContainer *m_container;
    QFrame *m_viewPlayFrame;
    QVBoxLayout *m_mainBox;
    std::map<QString, QLayout *> m_groupLayouts;
    std::map<QString, QWidget *> m_propertyControllers;
};

#endif
