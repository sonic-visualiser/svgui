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

#include "TransformFinder.h"

#include "transform/TransformFactory.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QScrollArea>

TransformFinder::TransformFinder(QWidget *parent) :
    QDialog(parent),
    m_resultsFrame(0),
    m_resultsLayout(0)
{
    setWindowTitle(tr("Find a Transform"));
    
    QGridLayout *mainGrid = new QGridLayout;
    setLayout(mainGrid);

    mainGrid->addWidget(new QLabel(tr("Find:")), 0, 0);
    
    QLineEdit *searchField = new QLineEdit;
    mainGrid->addWidget(searchField, 0, 1);
    connect(searchField, SIGNAL(textChanged(const QString &)),
            this, SLOT(searchTextChanged(const QString &)));

    m_resultsScroll = new QScrollArea;
    mainGrid->addWidget(m_resultsScroll, 1, 0, 1, 2);
    mainGrid->setRowStretch(1, 10);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok |
                                                QDialogButtonBox::Cancel);
    mainGrid->addWidget(bb, 2, 0, 1, 2);
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));

    resize(500, 400); //!!!
}

TransformFinder::~TransformFinder()
{
}

void
TransformFinder::searchTextChanged(const QString &text)
{
    std::cerr << "text is " << text.toStdString() << std::endl;

    QStringList keywords = text.split(' ', QString::SkipEmptyParts);
    TransformFactory::SearchResults results =
        TransformFactory::getInstance()->search(keywords);
    
    std::cerr << results.size() << " result(s)..." << std::endl;

    std::set<TransformFactory::Match> sorted;
    for (TransformFactory::SearchResults::const_iterator j = results.begin();
         j != results.end(); ++j) {
        sorted.insert(j->second);
    }

    int i = 0;
/*
    for (std::set<TransformFactory::Match>::const_iterator j = sorted.begin();
         j != sorted.end(); ++j) {
        std::cerr << i++ << ": " << j->transform.toStdString() << ": ";
        for (TransformFactory::Match::FragmentMap::const_iterator k =
                 j->fragments.begin();
             k != j->fragments.end(); ++k) {
            std::cerr << k->first.toStdString() << ": "
                      << k->second.toStdString() << " ";
        }
        std::cerr << "(" << j->score << ")" << std::endl;
    }
*/

    if (!m_resultsLayout) {
        std::cerr << "creating frame & layout" << std::endl;
        m_resultsFrame = new QWidget;
//        resultsFrame->setFrameStyle(QFrame::Sunken | QFrame::Box);
        m_resultsLayout = new QVBoxLayout;
        m_resultsFrame->setLayout(m_resultsLayout);
        m_resultsScroll->setWidget(m_resultsFrame);
        m_resultsFrame->show();
    }

    i = 0;
    int maxResults = 40;
    int height = 0;
    int width = 0;

    for (std::set<TransformFactory::Match>::const_iterator j = sorted.end();
         j != sorted.begin(); ) {
        --j;

        QString labelText;
        TransformDescription desc =
            TransformFactory::getInstance()->getTransformDescription(j->transform);
        labelText += tr("%2<br><small>").arg(desc.name);
        labelText += "...";
        for (TransformFactory::Match::FragmentMap::const_iterator k =
                 j->fragments.begin();
             k != j->fragments.end(); ++k) {
            labelText += k->second;
            labelText += "... ";
        }
        labelText += tr("</small>");

        if (i >= m_labels.size()) {
            QLabel *label = new QLabel(m_resultsFrame);
            m_resultsLayout->addWidget(label);
            m_labels.push_back(label);
        }
        m_labels[i]->setText(labelText);
        QSize sh = m_labels[i]->sizeHint();
        std::cerr << "size hint for text \"" << labelText.toStdString() << "\" has height " << sh.height() << std::endl;
        height += sh.height();
        if (sh.width() > width) width = sh.width();
        m_labels[i]->show();

        if (++i == maxResults) break;
    }

    std::cerr << "m_labels.size() = " << m_labels.size() << ", i = " << i << ", height = " << height << std::endl;

    while (i < m_labels.size()) m_labels[i++]->hide();

    m_resultsFrame->resize(height, width);
}

TransformId
TransformFinder::getTransform() const
{
    return "";
}

