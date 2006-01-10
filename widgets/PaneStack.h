
/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005
    
    This is experimental software.  Not for distribution.
*/

#ifndef _PANESTACK_H_
#define _PANESTACK_H_

#include <QSplitter>

class QWidget;
class QLabel;
class Pane;
class ViewManager;
class PropertyContainer;
class PropertyStack;

class PaneStack : public QSplitter
{
    Q_OBJECT

public:
    PaneStack(QWidget *parent, ViewManager *viewManager);

    Pane *addPane(bool suppressPropertyBox = false); // I own the returned value
    Pane *getPane(int n); // I own the returned value
    void deletePane(Pane *pane); // Deletes the pane and all its views
    int getPaneCount() const;

    void setCurrentPane(Pane *pane);
    Pane *getCurrentPane();

signals:
    void currentPaneChanged(Pane *pane);

public slots:
    void propertyContainerAdded(PropertyContainer *);
    void propertyContainerRemoved(PropertyContainer *);
    void propertyContainerSelected(PropertyContainer *);
    void paneInteractedWith();

protected:
    Pane *m_currentPane;
    //!!! should be a single vector of structs
    std::vector<Pane *> m_panes; // I own these
    std::vector<QWidget *> m_propertyStacks; // I own these
    std::vector<QLabel *> m_currentIndicators; // I own these
    ViewManager *m_viewManager; // I don't own this

    void sizePropertyStacks();
};

#endif

