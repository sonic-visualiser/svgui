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

#ifndef SV_PANESTACK_H
#define SV_PANESTACK_H

#include <QFrame>

#include <map>

#include "base/BaseTypes.h"

class QWidget;
class QLabel;
class QStackedWidget;
class QVBoxLayout;
class QSplitter;
class QGridLayout;
class QPushButton;
class View;
class Pane;
class Layer;
class ViewManager;
class PropertyContainer;
class PropertyStack;
class AlignmentView;

class PaneStack : public QFrame
{
    Q_OBJECT

public:
    PaneStack(QWidget *parent,
              ViewManager *viewManager);

    Pane *addPane(bool suppressPropertyBox = false); // I own the returned value
    void deletePane(Pane *pane); // Deletes the pane, but _not_ its layers

    int getPaneCount() const; // Returns only count of visible panes
    Pane *getPane(int n); // Of visible panes; I own the returned value
    int getPaneIndex(Pane *pane); // so getPane(index)==pane; -1 if absent

    void hidePane(Pane *pane); // Also removes pane from getPane/getPaneCount
    void showPane(Pane *pane); // Returns pane to getPane/getPaneCount

    int getHiddenPaneCount() const;
    Pane *getHiddenPane(int n); // I own the returned value

    void setCurrentPane(Pane *pane);
    void setCurrentLayer(Pane *pane, Layer *layer);
    Pane *getCurrentPane();

    enum LayoutStyle {
        NoPropertyStacks = 0,
        SinglePropertyStackLayout = 1,
        PropertyStackPerPaneLayout = 2
    };

    LayoutStyle getLayoutStyle() const { return m_layoutStyle; }
    void setLayoutStyle(LayoutStyle style);

    enum ResizeMode {
        UserResizeable = 0,
        AutoResizeOnly = 1
    };

    ResizeMode getResizeMode() const { return m_resizeMode; }
    void setResizeMode(ResizeMode);

    // Set whether the current-pane indicators and close buttons are
    // shown. The default is true.
    void setShowPaneAccessories(bool show);

    // Set whether a close button is shown on the first pane as well
    // as others. (It may be reasonable to omit the close button from
    // what is presumably the main pane in some applications.)  The
    // default is true.
    void setShowCloseButtonOnFirstPane(bool);

    void setPropertyStackMinWidth(int mw);

    void setShowAlignmentViews(bool show);

    void sizePanesEqually();

signals:
    void currentPaneChanged(Pane *pane);
    void currentLayerChanged(Pane *pane, Layer *layer);
    void rightButtonMenuRequested(Pane *pane, QPoint position);
    void propertyStacksResized(int width);
    void propertyStacksResized();
    void contextHelpChanged(const QString &);

    void paneAdded(Pane *pane);
    void paneAdded();
    void paneHidden(Pane *pane);
    void paneHidden();
    void paneAboutToBeDeleted(Pane *pane);
    void paneDeleted();

    void dropAccepted(Pane *pane, QStringList uriList);
    void dropAccepted(Pane *pane, QString text);

    void paneDeleteButtonClicked(Pane *pane);

    void doubleClickSelectInvoked(sv_frame_t frame);

public slots:
    void propertyContainerAdded(PropertyContainer *);
    void propertyContainerRemoved(PropertyContainer *);
    void propertyContainerSelected(View *client, PropertyContainer *);
    void viewSelected(View *v);
    void paneInteractedWith();
    void rightButtonMenuRequested(QPoint);
    void paneDropAccepted(QStringList);
    void paneDropAccepted(QString);
    void paneDeleteButtonClicked();
    void indicatorClicked();

protected:
    Pane *m_currentPane;

    struct PaneRec
    {
        Pane          *pane;
        QWidget       *propertyStack;
        QPushButton   *xButton;
        QLabel        *currentIndicator;
        QFrame        *frame;
        QGridLayout   *layout;
        AlignmentView *alignmentView;
    };

    std::vector<PaneRec> m_panes;
    std::vector<PaneRec> m_hiddenPanes;

    bool m_showAccessories;
    bool m_showCloseButtonOnFirstPane;
    bool m_showAlignmentViews;

    QSplitter *m_splitter; // constitutes the stack in UserResizeable mode
    QWidget *m_autoResizeStack; // constitutes the stack in AutoResizeOnly mode
    QVBoxLayout *m_autoResizeLayout;

    QStackedWidget *m_propertyStackStack;

    ViewManager *m_viewManager; // I don't own this
    int m_propertyStackMinWidth;
    void sizePropertyStacks();

    void showOrHidePaneAccessories();

    void unlinkAlignmentViews();
    void relinkAlignmentViews();

    LayoutStyle m_layoutStyle;
    ResizeMode m_resizeMode;
};

#endif

