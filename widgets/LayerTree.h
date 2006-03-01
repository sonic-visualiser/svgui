
/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#ifndef _LAYER_TREE_H_
#define _LAYER_TREE_H_

#include <QAbstractItemModel>

class PaneStack;

class LayerTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    LayerTreeModel(PaneStack *stack, QObject *parent = 0);
    virtual ~LayerTreeModel();

    QVariant data(const QModelIndex &index, int role) const;

//    Qt::ItemFlags flags(const QModelIndex &index) const;

//    QVariant headerData(int section, Qt::Orientation orientation,
//                        int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const;

    QModelIndex parent(const QModelIndex &index) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

protected:
    PaneStack *m_stack;
};

#endif
