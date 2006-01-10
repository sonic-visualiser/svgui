/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005
    
    This is experimental software.  Not for distribution.
*/

#ifndef _PROPERTY_BOX_H_
#define _PROPERTY_BOX_H_

#include "base/PropertyContainer.h"

#include <QFrame>
#include <map>

class QGridLayout;

class PropertyBox : public QFrame
{
    Q_OBJECT

public:
    PropertyBox(PropertyContainer *);
    ~PropertyBox();

    PropertyContainer *getContainer() { return m_container; }

public slots:
    void propertyContainerPropertyChanged(PropertyContainer *);

protected slots:
    void propertyControllerChanged(int);

protected:
    void updatePropertyEditor(PropertyContainer::PropertyName);

    QGridLayout *m_layout;
    PropertyContainer *m_container;
    std::map<QString, QLayout *> m_groupLayouts;
    std::map<QString, QWidget *> m_propertyControllers;
};

#endif
