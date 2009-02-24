/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2009 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "ActivityLog.h"

#include <QListView>
#include <QGridLayout>
#include <QStringListModel>
#include <QTime>

#include <iostream>

ActivityLog::ActivityLog() : QDialog()
{
    m_model = new QStringListModel;
    m_listView = new QListView;
    QGridLayout *layout = new QGridLayout;
    layout->addWidget(m_listView, 0, 0);
    setLayout(layout);
    m_listView->setModel(m_model);
}

ActivityLog::~ActivityLog()
{
}

void
ActivityLog::activityHappened(QString name)
{
    name = name.replace("&", "");
    std::cerr << "ActivityLog::activityHappened(" << name.toStdString() << ")" << std::endl;
    if (name == m_prevName) {
        std::cerr << "(ignoring duplicate)" << std::endl;
        return;
    }
    m_prevName = name;
    int row = m_model->rowCount();
    name = tr("%1: %2").arg(QTime::currentTime().toString()).arg(name);
    m_model->insertRows(row, 1);
    QModelIndex ix = m_model->index(row, 0);
    m_model->setData(ix, name);
    if (m_listView->isVisible()) m_listView->scrollTo(ix);
}

