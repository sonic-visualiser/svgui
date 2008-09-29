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

#include "base/XmlExportable.h"
#include "transform/TransformFactory.h"
#include "SelectableLabel.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QApplication>
#include <QDesktopWidget>
#include <QTimer>

TransformFinder::TransformFinder(QWidget *parent) :
    QDialog(parent),
    m_resultsFrame(0),
    m_resultsLayout(0)
{
    setWindowTitle(tr("Find a Transform"));
    
    QGridLayout *mainGrid = new QGridLayout;
    mainGrid->setVerticalSpacing(0);
    setLayout(mainGrid);

    mainGrid->addWidget(new QLabel(tr("Find:")), 0, 0);
    
    QLineEdit *searchField = new QLineEdit;
    mainGrid->addWidget(searchField, 0, 1);
    connect(searchField, SIGNAL(textChanged(const QString &)),
            this, SLOT(searchTextChanged(const QString &)));

    m_resultsScroll = new QScrollArea;
//    m_resultsScroll->setWidgetResizable(true);
    mainGrid->addWidget(m_resultsScroll, 1, 0, 1, 2);
    mainGrid->setRowStretch(1, 10);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok |
                                                QDialogButtonBox::Cancel);
    mainGrid->addWidget(bb, 2, 0, 1, 2);
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));
    if (!m_resultsLayout) {
        std::cerr << "creating frame & layout" << std::endl;
        m_resultsFrame = new QWidget;
        QPalette palette = m_resultsFrame->palette();
        palette.setColor(QPalette::Window, palette.color(QPalette::Base));
        m_resultsFrame->setPalette(palette);
        m_resultsScroll->setPalette(palette);
        m_resultsLayout = new QVBoxLayout;
        m_resultsLayout->setSpacing(0);
        m_resultsLayout->setContentsMargins(0, 0, 0, 0);
        m_resultsFrame->setLayout(m_resultsLayout);
        m_resultsScroll->setWidget(m_resultsFrame);
        m_resultsFrame->show();
    }

    QDesktopWidget *desktop = QApplication::desktop();
    QRect available = desktop->availableGeometry();

    int width = available.width() / 2;
    int height = available.height() / 2;
    if (height < 450) {
        if (available.height() > 500) height = 450;
    }
    if (width < 600) {
        if (available.width() > 650) width = 600;
    }

    resize(width, height);
    raise();

    m_upToDateCount = 0;
    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(timeout()));
    m_timer->start(0);
}

TransformFinder::~TransformFinder()
{
}

void
TransformFinder::searchTextChanged(const QString &text)
{
    std::cerr << "text is " << text.toStdString() << std::endl;
    m_newSearchText = text;
}

void
TransformFinder::timeout()
{
    int maxResults = 40;
    
    if (m_newSearchText != "") {

        QString text = m_newSearchText;
        m_newSearchText = "";

        QStringList keywords = text.split(' ', QString::SkipEmptyParts);
        TransformFactory::SearchResults results =
            TransformFactory::getInstance()->search(keywords);
        
        std::cerr << results.size() << " result(s)..." << std::endl;
        
        std::set<TransformFactory::Match> sorted;
        sorted.clear();
        for (TransformFactory::SearchResults::const_iterator j = results.begin();
             j != results.end(); ++j) {
            sorted.insert(j->second);
        }

        m_sortedResults.clear();
        for (std::set<TransformFactory::Match>::const_iterator j = sorted.end();
             j != sorted.begin(); ) {
            --j;
            m_sortedResults.push_back(*j);
            if (m_sortedResults.size() == maxResults) break;
        }

        if (m_sortedResults.empty()) m_selectedTransform = "";
        else m_selectedTransform = m_sortedResults.begin()->transform;

        m_upToDateCount = 0;

        for (int j = m_labels.size(); j > m_sortedResults.size(); ) {
            m_labels[--j]->hide();
        }

        return;
    }

    if (m_upToDateCount < m_sortedResults.size()) {

        int i = m_upToDateCount;

        std::cerr << "sorted size = " << m_sortedResults.size() << std::endl;

        TransformDescription desc =
            TransformFactory::getInstance()->getTransformDescription
            (m_sortedResults[i].transform);

        QString labelText;
        labelText += tr("%1: %2<br><small>")
            .arg(m_sortedResults[i].score)
            .arg(XmlExportable::encodeEntities(desc.name));

        labelText += "...";
        for (TransformFactory::Match::FragmentMap::const_iterator k =
                 m_sortedResults[i].fragments.begin();
             k != m_sortedResults[i].fragments.end(); ++k) {
            labelText += k->second;
            labelText += "... ";
        }
        labelText += tr("</small>");

        QString selectedText;
        selectedText += tr("<b>%1</b><br>")
            .arg(XmlExportable::encodeEntities(desc.name));
        selectedText += tr("<small>%1</small>")
            .arg(XmlExportable::encodeEntities(desc.longDescription));

        selectedText += tr("<ul><small>");
        selectedText += tr("<li>Plugin type: %1</li>")
            .arg(XmlExportable::encodeEntities(desc.type));
        selectedText += tr("<li>Category: %1</li>")
            .arg(XmlExportable::encodeEntities(desc.category));
        selectedText += tr("<li>System identifier: %1</li>")
            .arg(XmlExportable::encodeEntities(desc.identifier));
        selectedText += tr("</small></ul>");

        if (i >= m_labels.size()) {
            SelectableLabel *label = new SelectableLabel(m_resultsFrame);
            m_resultsLayout->addWidget(label);
            connect(label, SIGNAL(selectionChanged()), this,
                    SLOT(selectedLabelChanged()));
            connect(label, SIGNAL(doubleClicked()), this,
                    SLOT(accept()));
            QPalette palette = label->palette();
            label->setPalette(palette);
            m_labels.push_back(label);
        }

        m_labels[i]->setObjectName(desc.identifier);
        m_labels[i]->setFixedWidth(this->width() - 40);
        m_labels[i]->setUnselectedText(labelText);
        m_labels[i]->setSelectedText(selectedText);

        m_labels[i]->setSelected(m_selectedTransform == desc.identifier);
        m_labels[i]->show();

        ++m_upToDateCount;

//        if (m_upToDateCount == m_sortedResults.size()) {
            m_resultsFrame->resize(m_resultsFrame->sizeHint());
//        }
    }
}

void
TransformFinder::selectedLabelChanged()
{
    QObject *s = sender();
    m_selectedTransform = "";
    for (int i = 0; i < m_labels.size(); ++i) {
        if (!m_labels[i]->isVisible()) continue;
        if (m_labels[i] == s) {
            if (m_labels[i]->isSelected()) {
                m_selectedTransform = m_labels[i]->objectName();
            }
        } else {
            if (m_labels[i]->isSelected()) {
                m_labels[i]->setSelected(false);
            }
        }
    }
    std::cerr << "selectedLabelChanged: selected transform is now \""
              << m_selectedTransform.toStdString() << "\"" << std::endl;
}

TransformId
TransformFinder::getTransform() const
{
    return m_selectedTransform;
}

