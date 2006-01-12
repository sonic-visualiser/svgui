/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#ifndef _PROPERTY_STACK_H_
#define _PROPERTY_STACK_H_

#include <QFrame>
#include <QTabWidget>
#include <vector>

class Layer;
class View;
class PropertyBox;
class PropertyContainer;

class PropertyStack : public QTabWidget
{
    Q_OBJECT

public:
    PropertyStack(QWidget *parent, View *client);

    bool containsContainer(PropertyContainer *container) const;

signals:
    void propertyContainerSelected(PropertyContainer *container);

public slots:
    void propertyContainerAdded(PropertyContainer *);
    void propertyContainerRemoved(PropertyContainer *);
    void propertyContainerPropertyChanged(PropertyContainer *);
    void propertyContainerNameChanged(PropertyContainer *);

protected slots:
    void selectedContainerChanged(int);

protected:
    View *m_client;
    std::vector<PropertyBox *> m_boxes;

    void repopulate();
    void updateValues(PropertyContainer *);
};

#endif
