/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2008 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _MODEL_DATA_TABLE_DIALOG_H_
#define _MODEL_DATA_TABLE_DIALOG_H_

#include <QMainWindow>

class TabularModel;
class ModelDataTableModel;
class QTableView;
class QModelIndex;
class Command;

class ModelDataTableDialog : public QMainWindow
{
    Q_OBJECT
    
public:
    ModelDataTableDialog(TabularModel *model, QString title, QWidget *parent =0);
    ~ModelDataTableDialog();

signals:
    void scrollToFrame(unsigned long frame);

public slots:
    void userScrolledToFrame(unsigned long frame);
    void playbackScrolledToFrame(unsigned long frame);
    void addCommand(Command *);

protected slots:
    void viewClicked(const QModelIndex &);
    void viewPressed(const QModelIndex &);
    void currentChanged(const QModelIndex &, const QModelIndex &);
    void currentChangedThroughResort(const QModelIndex &);
    
    void insertRow();
    void deleteRows();
    void editRow();
    void togglePlayTracking();

protected:
    void makeCurrent(int row);
    ModelDataTableModel *m_table;
    QTableView *m_tableView;
    int m_currentRow;
    bool m_trackPlayback;
};

#endif
