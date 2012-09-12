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


#include "config.h"

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "StreamMediaPlayerPrivateGStreamer.h"

#include "CentralPipelineUnit.h"
#include "GStreamerGWorld.h"
#include "GraphicsContext.h"
#include "GraphicsTypes.h"
#include "ImageGStreamer.h"
#include "IntRect.h"
#include "KURL.h"
#include "Logging.h"
#include "MediaPlayer.h"
#include "MediaStream.h"
#include "MediaStreamRegistry.h"
#include "VideoSinkGStreamer.h"
#include "WebKitWebSourceGStreamer.h"

#include <gst/gst.h>
#include <gst/video/video.h>
#include <math.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/text/CString.h>


namespace WebCore {

void streamMediaPlayerPrivateVolumeChangedCallback(GObject* element, GParamSpec* pspec, gpointer data);
void streamMediaPlayerPrivateMuteChangedCallback(GObject* element, GParamSpec* pspec, gpointer data);
gboolean streamMediaPlayerPrivateVolumeChangeTimeoutCallback(StreamMediaPlayerPrivateGStreamer*);
gboolean streamMediaPlayerPrivateMuteChangeTimeoutCallback(StreamMediaPlayerPrivateGStreamer*);


static bool doGstInit();

static int greatestCommonDivisor(int a, int b)
{
    while (b) {
        int temp = a;
        a = b;
        b = temp % b;
    }

    return ABS(a);
}

void streamMediaPlayerPrivateVolumeChangedCallback(GObject *element, GParamSpec *pspec, gpointer data)
{
    StreamMediaPlayerPrivateGStreamer* mp = reinterpret_cast<StreamMediaPlayerPrivateGStreamer*>(data);
    mp->volumeChanged();
}

gboolean streamMediaPlayerPrivateVolumeChangeTimeoutCallback(StreamMediaPlayerPrivateGStreamer* player)
{
    player->notifyPlayerOfVolumeChange();
    return FALSE;
}

void streamMediaPlayerPrivateMuteChangedCallback(GObject *element, GParamSpec *pspec, gpointer data)
{
    StreamMediaPlayerPrivateGStreamer* mp = reinterpret_cast<StreamMediaPlayerPrivateGStreamer*>(data);
    mp->muteChanged();
}

gboolean streamMediaPlayerPrivateMuteChangeTimeoutCallback(StreamMediaPlayerPrivateGStreamer* player)
{
    player->notifyPlayerOfMute();
    return FALSE;
}


StreamMediaPlayerPrivateGStreamer::StreamMediaPlayerPrivateGStreamer(MediaPlayer* player)
    : m_player(player)
    , m_webkitVideoSink(0)
    , m_videoSinkBin(0)
    , m_audioSinkBin(0)
    , m_volume(0)
    , m_buffer(0)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_paused(true)
    , m_stopped(true)
    , m_volumeTimerHandler(0)
    , m_muteTimerHandler(0)
    , m_repaintCallbackHandlerId(0)
{
    // LOG(StreamAPI, "StreamAPI - Creating Stream media player");

    if (doGstInit()) {
        createGSTVideoSinkBin();
        createGSTAudioSinkBin();
    }
}

StreamMediaPlayerPrivateGStreamer::~StreamMediaPlayerPrivateGStreamer()
{
    LOG(MediaStream, "StreamMediaPlayerPrivateGStreamer destructor called.");

    g_signal_handler_disconnect(m_webkitVideoSink, m_repaintCallbackHandlerId);

    if (m_buffer)
        gst_buffer_unref(m_buffer);
    m_buffer = 0;

    stop();

    if (m_audioSinkBin) {
        gst_object_unref(m_audioSinkBin);
        m_audioSinkBin = 0;
    }

    if (m_videoSinkBin) {
        gst_object_unref(m_videoSinkBin);
        m_videoSinkBin = 0;
    }

    if (m_volume) {
        gst_object_unref(m_volume);
        m_volume = 0;
    }

    if (m_webkitVideoSink) {
        gst_object_unref(m_webkitVideoSink);
        m_webkitVideoSink = 0;
    }

    m_player = 0;

    if (m_muteTimerHandler)
        g_source_remove(m_muteTimerHandler);

    if (m_volumeTimerHandler)
        g_source_remove(m_volumeTimerHandler);
}

void StreamMediaPlayerPrivateGStreamer::play()
{
    LOG(MediaStream, "StreamMediaPlayerPrivateGStreamer::play() called.");

    if (!m_streamDescriptor || m_streamDescriptor->ended()) {
        m_readyState = MediaPlayer::HaveNothing;
        loadingFailed(MediaPlayer::Empty);
        return;
    }

    m_paused = false;
    internalLoad();
}

void StreamMediaPlayerPrivateGStreamer::pause()
{
    LOG(MediaStream, "StreamMediaPlayerPrivateGStreamer::pause() called.");
    m_paused = true;
    stop();
}


bool StreamMediaPlayerPrivateGStreamer::hasVideo() const
{
    return !m_videoSourceId.isEmpty();
}

bool StreamMediaPlayerPrivateGStreamer::hasAudio() const
{
    return !m_audioSourceId.isEmpty();
}

void StreamMediaPlayerPrivateGStreamer::setVolume(float volume)
{
    if (!m_volume)
        return;
    g_object_set(m_volume, "volume", static_cast<double>(volume), NULL);
}

void StreamMediaPlayerPrivateGStreamer::notifyPlayerOfVolumeChange()
{
    m_volumeTimerHandler = 0;

    if (!m_player || !m_volume)
        return;
    double volume;
    g_object_get(m_volume, "volume", &volume, NULL);
    m_player->volumeChanged(static_cast<float>(volume));
}

void StreamMediaPlayerPrivateGStreamer::volumeChanged()
{
    if (m_volumeTimerHandler)
        g_source_remove(m_volumeTimerHandler);
    m_volumeTimerHandler = g_timeout_add(0, reinterpret_cast<GSourceFunc>(streamMediaPlayerPrivateVolumeChangeTimeoutCallback), this);
}


bool StreamMediaPlayerPrivateGStreamer::supportsMuting() const
{
    return true;
}

void StreamMediaPlayerPrivateGStreamer::setMuted(bool muted)
{
    if (!m_volume)
        return;

    g_object_set(m_volume, "mute", muted, NULL);
}

void StreamMediaPlayerPrivateGStreamer::notifyPlayerOfMute()
{
    m_muteTimerHandler = 0;

    if (!m_player || !m_volume)
        return;

    gboolean muted;
    g_object_get(m_volume, "mute", &muted, NULL);
    m_player->muteChanged(static_cast<bool>(muted));
}

void StreamMediaPlayerPrivateGStreamer::muteChanged()
{
    if (m_muteTimerHandler)
        g_source_remove(m_muteTimerHandler);
    m_muteTimerHandler = g_timeout_add(0, reinterpret_cast<GSourceFunc>(streamMediaPlayerPrivateMuteChangeTimeoutCallback), this);
}


void StreamMediaPlayerPrivateGStreamer::load(const String &url)
{
    LOG(MediaStream, "StreamAPI - Stream media player - load %s", url.utf8().data());

    m_streamDescriptor = MediaStreamRegistry::registry().lookupMediaStreamDescriptor(url);
    if (!m_streamDescriptor || m_streamDescriptor->ended()) {
        loadingFailed(MediaPlayer::NetworkError);
        return;
    }

    m_readyState = MediaPlayer::HaveNothing;
    m_networkState = MediaPlayer::Empty;
    m_player->networkStateChanged();
    m_player->readyStateChanged();

    if (!internalLoad())
        return;

    if (hasVideo()) {
        // Wait for first video frame to set HaveEnoughData
        m_readyState = MediaPlayer::HaveNothing;
    } else {
        // go go go
        m_readyState = MediaPlayer::HaveEnoughData;
    }

    m_networkState = MediaPlayer::Loaded;
    m_player->networkStateChanged();
    m_player->readyStateChanged();

}

void StreamMediaPlayerPrivateGStreamer::loadingFailed(MediaPlayer::NetworkState error)
{

    if (m_networkState != error) {
        m_networkState = error;
        m_player->networkStateChanged();
    }
    if (m_readyState != MediaPlayer::HaveNothing) {
        m_readyState = MediaPlayer::HaveNothing;
        m_player->readyStateChanged();
    }
}

bool StreamMediaPlayerPrivateGStreamer::didLoadingProgress() const
{
    return true;
}

bool StreamMediaPlayerPrivateGStreamer::internalLoad()
{
    if (m_stopped) {
        m_stopped = false;
        if (!m_streamDescriptor || m_streamDescriptor->ended()) {
            loadingFailed(MediaPlayer::NetworkError);
            return false;
        }
        return connectToGSTLiveStream(m_streamDescriptor.get());
    }
    return false;
}

void StreamMediaPlayerPrivateGStreamer::stop()
{
    if (!m_stopped) {
        m_stopped = true;
        CentralPipelineUnit& cpu = centralPipelineUnit();
        if (!m_audioSourceId.isEmpty()) {
            LOG(MediaStream, "StreamMediaPlayerPrivateGStreamer stop: disconnecting audio");
            cpu.disconnectFromSource(m_audioSourceId, m_audioSinkBin);
        }
        if (!m_videoSourceId.isEmpty()) {
            LOG(MediaStream, "StreamMediaPlayerPrivateGStreamer stop: disconnecting video");
            cpu.disconnectFromSource(m_videoSourceId, m_videoSinkBin);
        }
        m_audioSourceId = "";
        m_videoSourceId = "";
    }
}


void mediaPlayerPrivateRepaintCallback(WebKitVideoSink*, GstBuffer *buffer, StreamMediaPlayerPrivateGStreamer* playerPrivate)
{
    g_return_if_fail(GST_IS_BUFFER(buffer));

    if (!playerPrivate->m_buffer) {
        playerPrivate->m_readyState = MediaPlayer::HaveEnoughData;
        playerPrivate->m_player->readyStateChanged();
    }

    if (!playerPrivate->m_paused || !playerPrivate->m_buffer) {
        gst_buffer_replace(&playerPrivate->m_buffer, buffer);

        IntSize size = playerPrivate->calculateSize();
        playerPrivate->updateCurrentSize(size);

        playerPrivate->repaint();
    }
}


PassOwnPtr<MediaPlayerPrivateInterface> StreamMediaPlayerPrivateGStreamer::create(MediaPlayer* player)
{
    return adoptPtr(new StreamMediaPlayerPrivateGStreamer(player));
}

void StreamMediaPlayerPrivateGStreamer::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
        registrar(create, getSupportedTypes, supportsType, 0, 0, 0);
}

void StreamMediaPlayerPrivateGStreamer::getSupportedTypes(HashSet<String>& types)
{
    types.add(String("video/x-raw-yuv"));
    types.add(String("audio/x-raw-int"));
}

MediaPlayer::SupportsType StreamMediaPlayerPrivateGStreamer::supportsType(const String& type, const String& codecs, const KURL& url)
{
    UNUSED_PARAM(type);
    UNUSED_PARAM(codecs);

    if (url.isNull() || url.isEmpty() || !url.isValid())
        return MediaPlayer::IsNotSupported;

    return MediaStreamRegistry::registry().lookupMediaStreamDescriptor(url.string()) ? MediaPlayer::IsSupported : MediaPlayer::IsNotSupported;
}

static bool gstInitialized = false;

static bool doGstInit()
{
    if (!gstInitialized) {
        GOwnPtr<GError> error;
        gstInitialized = gst_init_check(0, 0, &error.outPtr());
    }
    return gstInitialized;
}


bool StreamMediaPlayerPrivateGStreamer::isAvailable()
{

    if (!doGstInit())
        return false;

    return true;
}


void StreamMediaPlayerPrivateGStreamer::repaint()
{
    m_player->repaint();
}

void StreamMediaPlayerPrivateGStreamer::paint(GraphicsContext* context, const IntRect& rect)
{
    if (context->paintingDisabled())
        return;

    if (!m_player->visible())
        return;
    if (!m_buffer)
        return;

    GstCaps* caps = gst_buffer_get_caps(m_buffer);
    RefPtr<ImageGStreamer> gstImage = ImageGStreamer::createImage(m_buffer, caps);
    gst_caps_unref(caps);
    if (!gstImage)
        return;

    context->drawImage(reinterpret_cast<Image*>(gstImage->image().get()), ColorSpaceSRGB,
                       rect, CompositeCopy, WebCore::DoNotRespectImageOrientation, false);
}


// Returns the size of the video
IntSize StreamMediaPlayerPrivateGStreamer::naturalSize() const
{
    return m_currentSize;
}

bool StreamMediaPlayerPrivateGStreamer::supportsFullscreen() const
{
    return false;
}

PlatformMedia StreamMediaPlayerPrivateGStreamer::platformMedia() const
{
    PlatformMedia p;
    p.type = PlatformMedia::GStreamerGWorldType;
    p.media.gstreamerGWorld = m_gstGWorld.get();
    return p;
}


void StreamMediaPlayerPrivateGStreamer::createGSTVideoSinkBin()
{
    LOG(MediaStream, "createGSTVideoSinkBin called");
    GstElement* pipeline = centralPipelineUnit().pipeline();
    m_gstGWorld = GStreamerGWorld::createGWorld(pipeline);
    m_webkitVideoSink = webkitVideoSinkNew(m_gstGWorld.get());

    gchar* name = gst_element_get_name(m_webkitVideoSink);
    LOG(MediaStream, "m_webkitVideoSink=%p name=%s", m_webkitVideoSink, name);

    m_repaintCallbackHandlerId = g_signal_connect(m_webkitVideoSink, "repaint-requested", G_CALLBACK(mediaPlayerPrivateRepaintCallback), this);

    m_videoSinkBin = gst_bin_new(0);
    GstElement* colorspace = gst_element_factory_make("ffmpegcolorspace", 0);
    GstElement* videoTee = gst_element_factory_make("tee", "videoTee");
    GstElement* queue = gst_element_factory_make("queue", 0);
    GstElement* identity = gst_element_factory_make("identity", "videoValve");

    // Take ownership.
    gst_object_ref_sink(m_videoSinkBin);

    // Build a new video sink consisting of a bin containing a tee
    // (meant to distribute data to multiple video sinks) and our
    // internal video sink. For fullscreen we create an autovideosink
    // and initially block the data flow towards it and configure it

    gst_bin_add_many(GST_BIN(m_videoSinkBin), colorspace, videoTee, queue, identity, m_webkitVideoSink, NULL);

    gst_element_link(colorspace, videoTee);
    // Link a new src pad from tee to queue1.
    GstPad* srcPad = gst_element_get_request_pad(videoTee, "src%d");
    GstPad* sinkPad = gst_element_get_static_pad(queue, "sink");
    gst_pad_link(srcPad, sinkPad);
    gst_object_unref(GST_OBJECT(srcPad));
    gst_object_unref(GST_OBJECT(sinkPad));
    gst_element_link_many(queue, identity, m_webkitVideoSink, NULL);

    g_object_set(m_webkitVideoSink, "async", FALSE, NULL);

    // Add a ghostpad to the bin so it can proxy to tee.
    GstPad* pad = gst_element_get_static_pad(colorspace, "sink");
    gst_element_add_pad(m_videoSinkBin, gst_ghost_pad_new("sink", pad));
    gst_object_unref(GST_OBJECT(pad));

}

void StreamMediaPlayerPrivateGStreamer::createGSTAudioSinkBin()
{

    m_audioSinkBin = gst_bin_new(0);
    gst_object_ref_sink(m_audioSinkBin);

    m_volume = gst_element_factory_make("volume", "volume");

    g_object_set(m_volume, "mute", m_player->muted(), "volume", m_player->volume(), NULL);

    g_signal_connect(m_volume, "notify::volume", G_CALLBACK(streamMediaPlayerPrivateVolumeChangedCallback), this);
    g_signal_connect(m_volume, "notify::mute", G_CALLBACK(streamMediaPlayerPrivateMuteChangedCallback), this);


    GstElement* audioSink = gst_element_factory_make("autoaudiosink", 0);
    gst_bin_add_many(GST_BIN(m_audioSinkBin), m_volume, audioSink, NULL);
    gst_element_link_many(m_volume, audioSink, NULL);

    GstPad* pad = gst_element_get_static_pad(m_volume, "sink");
    gst_element_add_pad(m_audioSinkBin, gst_ghost_pad_new("sink", pad));
    gst_object_unref(GST_OBJECT(pad));
}

void StreamMediaPlayerPrivateGStreamer::sourceChangedState()
{
    LOG(MediaStream, "sourceChangedState");

    CentralPipelineUnit& cpu = centralPipelineUnit();
    if (!m_streamDescriptor || m_streamDescriptor->ended())
        stop();

    // check if the source should be ended
    if (!m_audioSourceId.isEmpty()) {
        for (unsigned i = 0; i < m_streamDescriptor->numberOfAudioComponents(); i++) {
            MediaStreamComponent* component = m_streamDescriptor->audioComponent(i);
            if (!component->enabled() && component->source()->id() == m_audioSourceId) {
                cpu.disconnectFromSource(m_audioSourceId, m_audioSinkBin);
                m_audioSourceId = "";
                break;
            }
        }
    }

    // Same check for video
    if (!m_videoSourceId.isEmpty()) {
        for (unsigned i = 0; i < m_streamDescriptor->numberOfVideoComponents(); i++) {
            MediaStreamComponent* component = m_streamDescriptor->videoComponent(i);
            if (!component->enabled() && component->source()->id() == m_videoSourceId) {
                cpu.disconnectFromSource(m_videoSourceId, m_videoSinkBin);
                m_videoSourceId = "";
                break;
            }
        }
    }
}

bool StreamMediaPlayerPrivateGStreamer::connectToGSTLiveStream(MediaStreamDescriptor* streamDescriptor)
{
    LOG(MediaStream, "connectToGSTLiveStream called");
    if (!streamDescriptor)
        return false;

    // FIXME: Error handling.. this could fail.. and this method never returns false.

    CentralPipelineUnit& cpu = centralPipelineUnit();

    if (!m_audioSourceId.isEmpty()) {
        cpu.disconnectFromSource(m_audioSourceId, m_audioSinkBin);
        m_audioSourceId = "";
    }

    if (!m_videoSourceId.isEmpty()) {
        cpu.disconnectFromSource(m_videoSourceId, m_videoSinkBin);
        m_videoSourceId = "";
    }

    for (unsigned i = 0; i < streamDescriptor->numberOfAudioComponents(); i++) {
        MediaStreamComponent* component = streamDescriptor->audioComponent(i);
        if (!component->enabled())
            continue;

        MediaStreamSource* source = component->source();
        if (source->type() == MediaStreamSource::TypeAudio) {
            if (cpu.connectToSource(source->id(), m_audioSinkBin)) {
                m_audioSourceId = source->id();
                source->addObserver(this);
                break;
            }
        }
    }

    for (unsigned i = 0; i < streamDescriptor->numberOfVideoComponents(); i++) {
        MediaStreamComponent* component = streamDescriptor->videoComponent(i);
        if (!component->enabled())
            continue;

        MediaStreamSource* source = component->source();
        if (source->type() == MediaStreamSource::TypeVideo) {
            if (cpu.connectToSource(source->id(), m_videoSinkBin)) {
                m_videoSourceId = source->id();
                source->addObserver(this);
                break;
            }
        }
    }

    return true;
}

IntSize StreamMediaPlayerPrivateGStreamer::calculateSize()
{

    if (!hasVideo())
        return IntSize();

    GstPad* pad = gst_element_get_static_pad(m_webkitVideoSink, "sink");
    if (!pad)
        return IntSize();

    guint64 width = 0, height = 0;
    GstCaps* caps = GST_PAD_CAPS(pad);
    int pixelAspectRatioNumerator, pixelAspectRatioDenominator;
    int displayWidth, displayHeight, displayAspectRatioGCD;
    int originalWidth = 0, originalHeight = 0;

    // TODO: handle possible clean aperture data. See
    // https://bugzilla.gnome.org/show_bug.cgi?id=596571
    // TODO: handle possible transformation matrix. See
    // https://bugzilla.gnome.org/show_bug.cgi?id=596326

    // Get the video PAR and original size.
    if (!GST_IS_CAPS(caps) || !gst_caps_is_fixed(caps)
        || !gst_video_format_parse_caps(caps, 0, &originalWidth, &originalHeight)
        || !gst_video_parse_caps_pixel_aspect_ratio(caps, &pixelAspectRatioNumerator,
                                                    &pixelAspectRatioDenominator)) {
        gst_object_unref(GST_OBJECT(pad));
        return IntSize();
    }

    gst_object_unref(GST_OBJECT(pad));

    // LOG_VERBOSE(Media, "Original video size: %dx%d", originalWidth, originalHeight);
    // LOG_VERBOSE(Media, "Pixel aspect ratio: %d/%d", pixelAspectRatioNumerator, pixelAspectRatioDenominator);

    // Calculate DAR based on PAR and video size.
    displayWidth = originalWidth * pixelAspectRatioNumerator;
    displayHeight = originalHeight * pixelAspectRatioDenominator;

    // Divide display width and height by their GCD to avoid possible overflows.
    displayAspectRatioGCD = greatestCommonDivisor(displayWidth, displayHeight);
    displayWidth /= displayAspectRatioGCD;
    displayHeight /= displayAspectRatioGCD;

    // Apply DAR to original video size. This is the same behavior as in xvimagesink's setcaps function.
    if (!(originalHeight % displayHeight)) {
        // LOG_VERBOSE(Media, "Keeping video original height");
        width = gst_util_uint64_scale_int(originalHeight, displayWidth, displayHeight);
        height = static_cast<guint64>(originalHeight);
    } else if (!(originalWidth % displayWidth)) {
        // LOG_VERBOSE(Media, "Keeping video original width");
        height = gst_util_uint64_scale_int(originalWidth, displayHeight, displayWidth);
        width = static_cast<guint64>(originalWidth);
    } else {
        // LOG_VERBOSE(Media, "Approximating while keeping original video height");
        width = gst_util_uint64_scale_int(originalHeight, displayWidth, displayHeight);
        height = static_cast<guint64>(originalHeight);
    }

    // LOG_VERBOSE(Media, "Natural size: %" G_GUINT64_FORMAT "x%" G_GUINT64_FORMAT, width, height);
    return IntSize(static_cast<int>(width), static_cast<int>(height));
}

void StreamMediaPlayerPrivateGStreamer::updateCurrentSize(IntSize& size)
{
    m_currentSize.setWidth(size.width());
    m_currentSize.setHeight(size.height());
    m_player->sizeChanged();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
