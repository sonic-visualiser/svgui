
/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#ifndef _PANESTACK_H_
#define _PANESTACK_H_

#include <QSplitter>

class QWidget;
class QLabel;
class View;
class Pane;
class Layer;
class ViewManager;
class PropertyContainer;
class PropertyStack;

class PaneStack : public QSplitter
{
    Q_OBJECT

public:
    PaneStack(QWidget *parent, ViewManager *viewManager);

    Pane *addPane(bool suppressPropertyBox = false); // I own the returned value
    void deletePane(Pane *pane); // Deletes the pane, but _not_ its layers

    int getPaneCount() const; // Returns only count of visible panes
    Pane *getPane(int n); // Of visible panes; I own the returned value

    void hidePane(Pane *pane); // Also removes pane from getPane/getPaneCount
    void showPane(Pane *pane); // Returns pane to getPane/getPaneCount

    int getHiddenPaneCount() const;
    Pane *getHiddenPane(int n); // I own the returned value

    void setCurrentPane(Pane *pane);
    void setCurrentLayer(Pane *pane, Layer *layer);
    Pane *getCurrentPane();

signals:
    void currentPaneChanged(Pane *pane);
    void currentLayerChanged(Pane *pane, Layer *layer);

public slots:
    void propertyContainerAdded(PropertyContainer *);
    void propertyContainerRemoved(PropertyContainer *);
    void propertyContainerSelected(View *client, PropertyContainer *);
    void paneInteractedWith();

protected:
    Pane *m_currentPane;

    struct PaneRec
    {
	Pane *pane;
	QWidget *propertyStack;
	QLabel *currentIndicator;
    };

    std::vector<PaneRec> m_panes;
    std::vector<PaneRec> m_hiddenPanes;

    ViewManager *m_viewManager; // I don't own this
    void sizePropertyStacks();
};

#endif

