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
//#include <SelectableLabel>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QApplication>

SelectableLabel::SelectableLabel(QWidget *parent) :
    QLabel(parent),
    m_selected(false)
{
    setStyleSheet("QLabel:hover { background: #e0e0e0; text: black; } QLabel:!hover { background: white; text: black } QLabel { padding: 4px }");

    setTextFormat(Qt::RichText);
    setLineWidth(2);
    setFixedWidth(420);
}

void
SelectableLabel::setUnselectedText(QString text)
{
    m_unselectedText = text;
    if (!m_selected) {
        setText(m_unselectedText);
        resize(sizeHint());
    }
}

void
SelectableLabel::setSelectedText(QString text)
{
    m_selectedText = text;
    if (m_selected) {
        setText(m_selectedText);
        resize(sizeHint());
    }
}

void
SelectableLabel::setSelected(bool s)
{
    if (m_selected == s) return;
    m_selected = s;
    if (m_selected) {
        setText(m_selectedText);
        setWordWrap(true);
//        setFrameStyle(QFrame::Box | QFrame::Plain);
    } else {
        setText(m_unselectedText);
        setWordWrap(false);
//        setFrameStyle(QFrame::NoFrame);
    }
//    resize(sizeHint());
    parentWidget()->resize(parentWidget()->sizeHint());
}

void
SelectableLabel::toggle()
{
    setSelected(!m_selected);
}

void
SelectableLabel::mousePressEvent(QMouseEvent *e)
{
    toggle();
    emit selectionChanged();
}

void
SelectableLabel::enterEvent(QEvent *)
{
//    std::cerr << "enterEvent" << std::endl;
//    QPalette palette = QApplication::palette();
//    palette.setColor(QPalette::Window, Qt::gray);
//    setStyleSheet("background: gray");
//    setPalette(palette);
}

void
SelectableLabel::leaveEvent(QEvent *)
{
//    std::cerr << "leaveEvent" << std::endl;
//    setStyleSheet("background: white");
//    QPalette palette = QApplication::palette();
//    palette.setColor(QPalette::Window, Qt::gray);
//    setPalette(palette);
}

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
//        resultsFrame->setFrameStyle(QFrame::Sunken | QFrame::Box);
        m_resultsLayout = new QVBoxLayout;
        m_resultsFrame->setLayout(m_resultsLayout);
        m_resultsScroll->setWidget(m_resultsFrame);
        m_resultsFrame->show();
    }

    resize(500, 400); //!!!
    m_resultsFrame->resize(480, 380);
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

    i = 0;
    int maxResults = 40;
    int height = 0;
    int width = 0;

    if (sorted.empty()) m_selectedTransform = "";

    for (std::set<TransformFactory::Match>::const_iterator j = sorted.end();
         j != sorted.begin(); ) {
        --j;

        TransformDescription desc =
            TransformFactory::getInstance()->getTransformDescription(j->transform);

        QString labelText;
        labelText += tr("%2<br><small>").arg(desc.name);
        labelText += "...";
        for (TransformFactory::Match::FragmentMap::const_iterator k =
                 j->fragments.begin();
             k != j->fragments.end(); ++k) {
            labelText += k->second;
            labelText += "... ";
        }
        labelText += tr("</small>");

        QString selectedText;
        selectedText += tr("<b>%1</b><br>").arg(desc.name);
        selectedText += tr("<small>%1</small>").arg(desc.longDescription);
/*
        for (TransformFactory::Match::FragmentMap::const_iterator k =
                 j->fragments.begin();
             k != j->fragments.end(); ++k) {
            selectedText += tr("<br><small>%1: %2</small>").arg(k->first).arg(k->second);
        }
*/

        selectedText += tr("<ul><small>");
        selectedText += tr("<li>Plugin type: %1</li>").arg(desc.type);
        selectedText += tr("<li>Category: %1</li>").arg(desc.category);
        selectedText += tr("<li>System identifier: %1</li>").arg(desc.identifier);
        selectedText += tr("</small></ul>");

        if (i >= m_labels.size()) {
            SelectableLabel *label = new SelectableLabel(m_resultsFrame);
//            m_resultsLayout->addWidget(label, i, 0);
            m_resultsLayout->addWidget(label);
            connect(label, SIGNAL(selectionChanged()), this,
                    SLOT(selectedLabelChanged()));
            QPalette palette = label->palette();
            label->setPalette(palette);
            m_labels.push_back(label);
        }

        m_labels[i]->setObjectName(desc.identifier);
        m_selectedTransform = desc.identifier;
        m_labels[i]->setUnselectedText(labelText);
        m_labels[i]->setSelectedText(selectedText);
        m_labels[i]->setSelected(i == 0);

/*
        QSize sh = m_labels[i]->sizeHint();
        std::cerr << "size hint for text \"" << labelText.toStdString() << "\" has height " << sh.height() << std::endl;
        height += sh.height();
        if (sh.width() > width) width = sh.width();
*/
//        m_labels[i]->resize(m_labels[i]->sizeHint());
//        m_labels[i]->updateGeometry();
        m_labels[i]->show();

        if (++i == maxResults) break;
    }

    std::cerr << "m_labels.size() = " << m_labels.size() << ", i = " << i << ", height = " << height << std::endl;

    for (int j = m_labels.size(); j > i; ) m_labels[--j]->hide();

    m_resultsFrame->resize(m_resultsFrame->sizeHint());
//    m_resultsFrame->resize(height, width);
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
    return "";
}

