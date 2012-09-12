/*
 *  Copyright (C) 2012 Collabora Ltd. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef StreamMediaPlayerPrivateGStreamer_h
#define StreamMediaPlayerPrivateGStreamer_h

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "MediaPlayerPrivate.h"
#include "MediaStream.h"
#include "TimeRanges.h"
#include "Timer.h"


#include <glib.h>
#include <gst/gst.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

typedef struct _WebKitVideoSink WebKitVideoSink;

namespace WebCore {

class KURL;
class StreamComponent;
class MediaStreamDescriptor;
class GStreamerGWorld;

class StreamMediaPlayerPrivateGStreamer : public MediaPlayerPrivateInterface, private MediaStreamSource::Observer{
    friend void mediaPlayerPrivateRepaintCallback(WebKitVideoSink*, GstBuffer*, StreamMediaPlayerPrivateGStreamer*);
public:
    ~StreamMediaPlayerPrivateGStreamer();
    static void registerMediaEngine(MediaEngineRegistrar);


    virtual void load(const String&);
    virtual void cancelLoad() { }

    virtual void prepareToPlay() { }
    void play();
    void pause();

    IntSize naturalSize() const;

    bool hasVideo() const;
    bool hasAudio() const;

    virtual void setVisible(bool) { }

    virtual float duration() const { return 0; }

    virtual float currentTime() const { return 0; }
    virtual void seek(float) { }
    virtual bool seeking() const { return false; }

    virtual void setRate(float) { }
    virtual void setPreservesPitch(bool) { }
    virtual bool paused() const { return m_paused; }

    void setVolume(float);
    void volumeChanged();
    void notifyPlayerOfVolumeChange();

    bool supportsMuting() const;
    void setMuted(bool);
    void muteChanged();
    void notifyPlayerOfMute();


    virtual bool hasClosedCaptions() const { return false; }
    virtual void setClosedCaptionsVisible(bool) { };

    virtual MediaPlayer::NetworkState networkState() const { return m_networkState; }
    virtual MediaPlayer::ReadyState readyState() const { return m_readyState; }

    virtual float maxTimeSeekable() const { return 0; }
    virtual PassRefPtr<TimeRanges> buffered() const { return TimeRanges::create(); }
    bool didLoadingProgress() const;

    virtual unsigned totalBytes() const { return 0; }
    virtual unsigned bytesLoaded() const { return 0; }

    virtual void setSize(const IntSize&) { }

    void repaint();
    virtual void paint(GraphicsContext*, const IntRect&);

    virtual bool canLoadPoster() const { return false; }
    virtual void setPoster(const String&) { }

    virtual bool hasSingleSecurityOrigin() const { return true; }

    bool supportsFullscreen() const;
    PlatformMedia platformMedia() const;

    void sourceChangedState();

private:
    StreamMediaPlayerPrivateGStreamer(MediaPlayer*);

    static PassOwnPtr<MediaPlayerPrivateInterface> create(MediaPlayer*);

    static void getSupportedTypes(HashSet<String>&);
    static MediaPlayer::SupportsType supportsType(const String& type, const String& codecs, const KURL&);
    static bool isAvailable();
    void createGSTAudioSinkBin();
    void createGSTVideoSinkBin();
    bool connectToGSTLiveStream(MediaStreamDescriptor*);
    void loadingFailed(MediaPlayer::NetworkState error);
    bool internalLoad();
    void stop();
    IntSize calculateSize();
    void updateCurrentSize(IntSize&);

private:
     MediaPlayer* m_player;
     GstElement* m_webkitVideoSink;
     GstElement* m_videoSinkBin;
     GstElement* m_audioSinkBin;
     GstElement* m_volume;
     GstBuffer* m_buffer;
     MediaPlayer::NetworkState m_networkState;
     MediaPlayer::ReadyState m_readyState;
     bool m_paused;
     bool m_stopped;
     RefPtr<GStreamerGWorld> m_gstGWorld;
     guint m_volumeTimerHandler;
     guint m_muteTimerHandler;
     String m_videoSourceId;
     String m_audioSourceId;
     RefPtr<MediaStreamDescriptor> m_streamDescriptor;
     IntSize m_currentSize;
     gulong m_repaintCallbackHandlerId;
};



} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#endif // StreamMediaPlayerPrivateGStreamer_h
