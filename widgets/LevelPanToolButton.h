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

#ifndef LEVEL_PAN_TOOLBUTTON_H
#define LEVEL_PAN_TOOLBUTTON_H

#include <QToolButton>

class QMenu;

namespace sv {

class LevelPanWidget;

class LevelPanToolButton : public QToolButton
{
    Q_OBJECT

public:
    LevelPanToolButton(QWidget *parent = 0);
    ~LevelPanToolButton();
    
    /// Return level as a gain value in the range [0,1]
    float getLevel() const; 
    
    /// Return pan as a value in the range [-1,1]
    float getPan() const;

    /// Discover whether the level range includes muting or not
    bool includesMute() const;

    void setImageSize(int pixels);

    /// Specify whether a right-click context menu is provided
    void setProvideContextMenu(bool);
                        
    void setBigImageSize(int pixels);
                        
public slots:
    /// Set level in the range [0,1] -- will be rounded
    void setLevel(float);

    /// Set pan in the range [-1,1] -- will be rounded
    void setPan(float);

    /// Set left and right peak monitoring levels in the range [0,1]
    void setMonitoringLevels(float, float);
    
    /// Specify whether the level range should include muting or not
    void setIncludeMute(bool);

    void setEnabled(bool enabled);
    
protected slots:
    void contextMenuRequested(const QPoint &);

signals:
    void levelChanged(float);
    void panChanged(float);

    void mouseEntered();
    void mouseLeft();

private slots:
    void selfLevelChanged(float);
    void selfClicked();
    
protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *e) override;
    
    LevelPanWidget *m_lpw;
    int m_pixels;
    int m_pixelsBig;
    bool m_muted;
    float m_savedLevel;
    bool m_provideContextMenu;
    QMenu *m_lastContextMenu;
};

} // end namespace sv

#endif
