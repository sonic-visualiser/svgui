/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "ListInputDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStringList>
#include <QRadioButton>
#include <QPushButton>

ListInputDialog::ListInputDialog(QWidget *parent, const QString &title,
				 const QString &labelText, const QStringList &list,
				 int current, Qt::WFlags f) :
    QDialog(parent, f),
    m_strings(list)
{
    setWindowTitle(title);

    QVBoxLayout *vbox = new QVBoxLayout(this);
    
    QLabel *label = new QLabel(labelText, this);
    vbox->addWidget(label);
    vbox->addStretch(1);

    int count = 0;
    for (QStringList::const_iterator i = list.begin(); i != list.end(); ++i) {
        QRadioButton *radio = new QRadioButton(*i);
        if (current == count++) radio->setChecked(true);
        m_radioButtons.push_back(radio);
        vbox->addWidget(radio);
    }

    vbox->addStretch(1);

    QHBoxLayout *hbox = new QHBoxLayout;
    vbox->addLayout(hbox, Qt::AlignRight);

    QPushButton *ok = new QPushButton(tr("OK"), this);
    ok->setDefault(true);

    QPushButton *cancel = new QPushButton(tr("Cancel"), this);

    QSize bs = ok->sizeHint().expandedTo(cancel->sizeHint());
    ok->setFixedSize(bs);
    cancel->setFixedSize(bs);

    hbox->addStretch();
    hbox->addWidget(ok);
    hbox->addWidget(cancel);

    QObject::connect(ok, SIGNAL(clicked()), this, SLOT(accept()));
    QObject::connect(cancel, SIGNAL(clicked()), this, SLOT(reject()));
}

ListInputDialog::~ListInputDialog()
{
}

QString
ListInputDialog::getCurrentString() const
{
    for (int i = 0; i < m_radioButtons.size(); ++i) {
        if (m_radioButtons[i]->isChecked()) {
            return m_strings[i];
        }
    }
    return "";
}

QString
ListInputDialog::getItem(QWidget *parent, const QString &title,
                         const QString &label, const QStringList &list,
                         int current, bool *ok, Qt::WFlags f)
{
    ListInputDialog dialog(parent, title, label, list, current, f);
    
    bool accepted = (dialog.exec() == QDialog::Accepted);
    if (ok) *ok = accepted;

    return dialog.getCurrentString();
}

