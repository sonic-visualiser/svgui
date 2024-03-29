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

#ifndef SV_VIEW_MANAGER_H
#define SV_VIEW_MANAGER_H

#include <QObject>
#include <QTimer>
#include <QPalette>

#include <map>

#include "base/ViewManagerBase.h"
#include "base/Selection.h"
#include "base/Command.h"
#include "base/Clipboard.h"
#include "base/BaseTypes.h"
#include "base/ZoomLevel.h"

#include "data/model/Model.h"

namespace sv {

class AudioPlaySource;
class AudioRecordTarget;
class Model;

enum PlaybackFollowMode {

    /**
     * View scrolls continuously during playback, keeping the playback
     * position at the centre.
     */
    PlaybackScrollContinuous,

    /**
     * View follows playback page-by-page, but dragging the view
     * relocates playback to the centre frame. This is the classic
     * Sonic Visualiser behaviour.
     */
    PlaybackScrollPageWithCentre,

    /**
     * View follows playback page-by-page, and the play head is moved
     * (by the user) separately from dragging the view. This is
     * roughly the behaviour of a typical DAW or audio editor.
     */
    PlaybackScrollPage,

    /**
     * View is detached from playback. It doesn't follow playback, and
     * dragging the view does not affect the play head.
     */
    PlaybackIgnore
};

class View;

/**
 * The ViewManager manages properties that may need to be synchronised
 * between separate Views.  For example, it handles signals associated
 * with changes to the global pan and zoom, and it handles selections.
 *
 * Views should be implemented in such a way as to work
 * correctly whether they are supplied with a ViewManager or not.
 */

class ViewManager : public ViewManagerBase
{
    Q_OBJECT

public:
    ViewManager();
    virtual ~ViewManager();

    void setAudioPlaySource(AudioPlaySource *source) override;
    void setAudioRecordTarget(AudioRecordTarget *target) override;

    bool isPlaying() const;
    bool isRecording() const;

    sv_frame_t getGlobalCentreFrame() const; // the set method is a slot
    ZoomLevel getGlobalZoom() const;

    sv_frame_t getPlaybackFrame() const; // the set method is a slot

    // Only meaningful in solo mode, and used for optional alignment feature
    ModelId getPlaybackModel() const;
    void setPlaybackModel(ModelId);

    sv_frame_t alignPlaybackFrameToReference(sv_frame_t) const override;
    sv_frame_t alignReferenceToPlaybackFrame(sv_frame_t) const override;

    bool haveInProgressSelection() const;
    const Selection &getInProgressSelection(bool &exclusive) const;
    void setInProgressSelection(const Selection &selection, bool exclusive);
    void clearInProgressSelection();

    MultiSelection getSelection() const override;
    MultiSelection::SelectionList getSelections() const override;
    void setSelection(const Selection &selection);
    void addSelection(const Selection &selection);
    void removeSelection(const Selection &selection);
    void clearSelections();
    sv_frame_t constrainFrameToSelection(sv_frame_t frame) const override;

    /**
     * Adding a selection normally emits the selectionChangedByUser
     * signal. Call this to add a selection without emitting that signal.
     * This is used in session file load, for example.
     */
    void addSelectionQuietly(const Selection &selection);

    /**
     * Return the selection that contains a given frame.
     * If defaultToFollowing is true, and if the frame is not in a
     * selected area, return the next selection after the given frame.
     * Return the empty selection if no appropriate selection is found.
     */
    Selection getContainingSelection(sv_frame_t frame, bool defaultToFollowing) const override;

    Clipboard &getClipboard() { return m_clipboard; }

    enum ToolMode {
        NavigateMode,
        SelectMode,
        EditMode,
        DrawMode,
        EraseMode,
        MeasureMode,
        NoteEditMode //GF: Tonioni: this tool mode will be context sensitive.
    };
    ToolMode getToolMode() const { return m_toolMode; }
    void setToolMode(ToolMode mode);

    /// Override the tool mode for a specific view 
    void setToolModeFor(const View *v, ToolMode mode);
    /// Return override mode if it exists for this view or global mode otherwise
    ToolMode getToolModeFor(const View *v) const;
    /// Clear all current view-specific overrides
    void clearToolModeOverrides();

    bool getPlayLoopMode() const override { return m_playLoopMode; }
    void setPlayLoopMode(bool on);

    bool getPlaySelectionMode() const override { return m_playSelectionMode; }
    void setPlaySelectionMode(bool on);

    bool getPlaySoloMode() const override { return m_playSoloMode; }
    void setPlaySoloMode(bool on);

    bool getAlignMode() const override { return m_alignMode; }
    void setAlignMode(bool on);

    void setIlluminateLocalFeatures(bool i) { m_illuminateLocalFeatures = i; }
    void setShowWorkTitle(bool show) { m_showWorkTitle = show; }
    void setShowDuration(bool show) { m_showDuration = show; }

    /**
     * The sample rate that is used for playback.  This is usually the
     * rate of the main model, but not always.  Models whose rates
     * differ from this will play back at the wrong speed -- there is
     * no per-model resampler.
     */
    sv_samplerate_t getPlaybackSampleRate() const;

    /**
     * The sample rate of the audio output device.  If the playback
     * sample rate differs from this, everything will be resampled at
     * the output stage (but not before).
     */
    sv_samplerate_t getDeviceSampleRate() const;

    /**
     * The sample rate of the current main model.  This may in theory
     * differ from the playback sample rate, in which case even the
     * main model will play at the wrong speed.
     */
    sv_samplerate_t getMainModelSampleRate() const { return m_mainModelSampleRate; }

    void setMainModelSampleRate(sv_samplerate_t sr) { m_mainModelSampleRate = sr; }

    /**
     * Take a "design pixel" size and scale it for the actual
     * display. This is relevant to hi-dpi systems that do not do
     * pixel doubling (i.e. Windows and Linux rather than OS/X).
     */
    static int scalePixelSize(int pixels);
    
    enum OverlayMode {
        NoOverlays,
        GlobalOverlays,
        StandardOverlays,
        AllOverlays
    };
    void setOverlayMode(OverlayMode mode);
    OverlayMode getOverlayMode() const { return m_overlayMode; }

    void setShowCentreLine(bool show);
    bool shouldShowCentreLine() const { return m_showCentreLine; }

    bool shouldShowDuration() const {
        return m_overlayMode != NoOverlays && m_showDuration;
    }
    bool shouldShowFrameCount() const {
        return m_showCentreLine && shouldShowDuration();
    }
    bool shouldShowVerticalScale() const {
        return m_overlayMode != NoOverlays;
    }
    bool shouldShowVerticalColourScale() const {
        return m_overlayMode == AllOverlays;
    }
    bool shouldShowHorizontalValueScale() const { // for layers where x != time
        return m_overlayMode != NoOverlays;
    }
    bool shouldShowSelectionExtents() const {
        return m_overlayMode != NoOverlays && m_overlayMode != GlobalOverlays;
    }
    bool shouldShowLayerNames() const {
        return m_overlayMode == AllOverlays;
    }
    bool shouldShowScaleGuides() const {
        return m_overlayMode != NoOverlays;
    }
    bool shouldShowWorkTitle() const {
        return m_showWorkTitle;
    }
    bool shouldIlluminateLocalFeatures() const {
        return m_illuminateLocalFeatures;
    }
    bool shouldShowFeatureLabels() const {
        return m_overlayMode != NoOverlays && m_overlayMode != GlobalOverlays;
    }

    void setZoomWheelsEnabled(bool enable);
    bool getZoomWheelsEnabled() const { return m_zoomWheelsEnabled; }

    /** 
     * Enable or disable opportunistic editing. This allows certain
     * edits while not in edit modes - e.g. double-click on an item
     * while in navigate mode to open an edit dialog. It is enabled by
     * default, but it may be undesirable if the application is
     * intended to be "read-only".
     *
     * This setting makes no difference to behaviour when actually in
     * editing modes.
     * 
     * Unlike some other options, this is considered to be
     * application-build-specific and is not restored from settings.
     */
    void setOpportunisticEditingEnabled(bool enable);
    bool getOpportunisticEditingEnabled() const {
        return m_opportunisticEditingEnabled;
    }

    void setGlobalDarkBackground(bool dark);
    bool getGlobalDarkBackground() const;

signals:
    /** Emitted when user causes the global centre frame to change. */
    void globalCentreFrameChanged(sv_frame_t frame);

    /** Emitted when user scrolls a view, but doesn't affect global centre. */
    void viewCentreFrameChanged(View *v, sv_frame_t frame);

    /** Emitted when a view zooms. */
    void viewZoomLevelChanged(View *v, ZoomLevel zoom, bool locked);

    /** Emitted when the playback frame changes. */
    void playbackFrameChanged(sv_frame_t frame);

    /** Emitted when the output or record levels change. Values in range 0.0 -> 1.0. */
    void monitoringLevelsChanged(float left, float right);

    /** Emitted whenever the selection has changed. */
    void selectionChanged();

    /** Emitted when the selection has been changed through an
     * explicit selection-editing action. *Not* emitted when the
     * selection has been changed through undo or redo. */
    void selectionChangedByUser();

    /** Emitted when the in-progress (rubberbanding) selection has changed. */
    void inProgressSelectionChanged();

    /** Emitted when the tool mode has been changed. */
    void toolModeChanged();

    /** Emitted when the play loop mode has been changed. */
    void playLoopModeChanged();
    void playLoopModeChanged(bool);

    /** Emitted when the play selection mode has been changed. */
    void playSelectionModeChanged();
    void playSelectionModeChanged(bool);

    /** Emitted when the play solo mode has been changed. */
    void playSoloModeChanged();
    void playSoloModeChanged(bool);

    /** Emitted when the alignment mode has been changed. */
    void alignModeChanged();
    void alignModeChanged(bool);

    /** Emitted when the overlay mode has been changed. */
    void overlayModeChanged();

    /** Emitted when the centre line visibility has been changed. */
    void showCentreLineChanged();

    /** Emitted when the zoom wheels have been toggled. */
    void zoomWheelsEnabledChanged();

    /** Emitted when editing-enabled has been toggled. */
    void opportunisticEditingEnabledChanged();
    
    /** Emitted when any loggable activity has occurred. */
    void activity(QString);

public slots:
    void viewCentreFrameChanged(sv_frame_t, bool, PlaybackFollowMode);
    void viewZoomLevelChanged(ZoomLevel, bool);
    void setGlobalCentreFrame(sv_frame_t);
    void setPlaybackFrame(sv_frame_t);
    void playStatusChanged(bool playing);
    void recordStatusChanged(bool recording);

protected slots:
    void checkPlayStatus();
    void seek(sv_frame_t);
//!!!    void considerZoomChange(void *, int, bool);

protected:
    AudioPlaySource *m_playSource;
    AudioRecordTarget *m_recordTarget;
    
    sv_frame_t m_globalCentreFrame;
    ZoomLevel m_globalZoom;
    mutable sv_frame_t m_playbackFrame;
    ModelId m_playbackModel;
    sv_samplerate_t m_mainModelSampleRate;

    float m_lastLeft;
    float m_lastRight;

    MultiSelection m_selections;
    Selection m_inProgressSelection;
    bool m_inProgressExclusive;

    Clipboard m_clipboard;

    ToolMode m_toolMode;
    std::map<const View *, ToolMode> m_toolModeOverrides;

    bool m_playLoopMode;
    bool m_playSelectionMode;
    bool m_playSoloMode;
    bool m_alignMode;

    void setSelections(const MultiSelection &ms, bool quietly = false);
    void signalSelectionChange();

    class SetSelectionCommand : public Command
    {
    public:
        SetSelectionCommand(ViewManager *vm, const MultiSelection &ms);
        virtual ~SetSelectionCommand();
        void execute() override;
        void unexecute() override;
        QString getName() const override;

    protected:
        ViewManager *m_vm;
        MultiSelection m_oldSelection;
        MultiSelection m_newSelection;
    };

    OverlayMode m_overlayMode;
    bool m_zoomWheelsEnabled;
    bool m_opportunisticEditingEnabled;
    bool m_showCentreLine;
    bool m_illuminateLocalFeatures;
    bool m_showWorkTitle;
    bool m_showDuration;

    QPalette m_lightPalette;
    QPalette m_darkPalette;
};

} // end namespace sv

#endif

