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

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamCenterPrivateGStreamer.h"

#include "CentralPipelineUnit.h"
#include "Logging.h"
#include "MediaStreamCenter.h"
#include "MediaStreamDescriptor.h"
#include "MediaStreamSourcesQueryClient.h"
#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/interfaces/propertyprobe.h>
#include <wtf/text/CString.h>

namespace WebCore {

static GstElement* findVideoDeviceSource();
static GstElement* findAudioDeviceSource();
static GstElement* findDeviceSource(GstElement* autoSrc);
static void probeSource(GstElement* source, const String& key, MediaStreamSource::Type, MediaStreamSourceVector& sources);
static String filterAudioSourceName(const String& deviceId, const String& name);
static String getDeviceStringFromDeviceIdString(const String&);
static String getFactoryNameFromDeviceIdString(const String&);
static gchar* createUniqueName(const gchar* prefix);
static GstElement* createAudioSourceBin(GstElement* source);
static GstElement* createVideoSourceBin(GstElement* source);

void MediaStreamCenterPrivateGStreamer::queryMediaStreamSources(PassRefPtr<MediaStreamSourcesQueryClient> prpClient)
{
    RefPtr<MediaStreamSourcesQueryClient> client = prpClient;
    MediaStreamSourceVector audioSources;
    MediaStreamSourceVector videoSources;

    GOwnPtr<GError> error;
    if (!gst_init_check(0, 0, &error.outPtr())) {
        client->didCompleteQuery(audioSources, videoSources);
        return;
    }

    if (client->audio()) {
        GstElement* audioSrc = findAudioDeviceSource();
        if (audioSrc) {
            String key = storeElementFactoryForElement(audioSrc);
            probeSource(audioSrc, key, MediaStreamSource::TypeAudio, audioSources);
            gst_object_unref(audioSrc);
        }

#if 0
        GstElement* audioTestSrc = gst_element_factory_make("audiotestsrc", NULL);
        if (audioTestSrc) {
            String key = storeElementFactoryForElement(audioTestSrc);
            audioSources.append(MediaStreamSource::create(key + ";testsrc_audio", MediaStreamSource::TypeAudio, "audioTestSrc"));
            gst_object_unref(audioTestSrc);
        }
#endif
    }

    if (client->video()) {
        GstElement* videoSrc = findVideoDeviceSource();
        if (videoSrc) {
            String key = storeElementFactoryForElement(videoSrc);
            probeSource(videoSrc, key, MediaStreamSource::TypeVideo, videoSources);
            gst_object_unref(videoSrc);
        }

#if 0
        GstElement* videoTestSrc = gst_element_factory_make("videotestsrc", NULL);
        if (videoTestSrc) {
            String key = storeElementFactoryForElement(videoTestSrc);
            videoSources.append(MediaStreamSource::create(key + ";testsrc_video", MediaStreamSource::TypeVideo, "videoTestSrc"));
            gst_object_unref(videoTestSrc);
        }
#endif
    }

    registerSourceFactories(audioSources);
    registerSourceFactories(videoSources);

    client->didCompleteQuery(audioSources, videoSources);
}

void MediaStreamCenterPrivateGStreamer::didSetMediaStreamTrackEnabled(MediaStreamDescriptor* streamDescriptor, MediaStreamComponent* component)
{
    // FIXME
    LOG(MediaStream, "enabled: %d", component->enabled());
}

void MediaStreamCenterPrivateGStreamer::didStopLocalMediaStream(MediaStreamDescriptor* streamDescriptor)
{
    LOG(MediaStream, "didStopLocalMediaStream %s", streamDescriptor->label().utf8().data());

    for (unsigned i = 0; i < streamDescriptor->numberOfVideoComponents(); i++) {
        MediaStreamComponent *component = streamDescriptor->videoComponent(i);
        MediaStreamSource *source = component->source();

        component->setEnabled(false);
        source->setReadyState(MediaStreamSource::ReadyStateEnded);
    }

    for (unsigned i = 0; i < streamDescriptor->numberOfAudioComponents(); i++) {
        MediaStreamComponent *component = streamDescriptor->audioComponent(i);
        MediaStreamSource *source = component->source();

        component->setEnabled(false);
        source->setReadyState(MediaStreamSource::ReadyStateEnded);
    }
}

void MediaStreamCenterPrivateGStreamer::registerSourceFactories(const MediaStreamSourceVector& sources)
{
    CentralPipelineUnit& cpu = centralPipelineUnit();

    for (size_t i = 0; i < sources.size(); i++) {
        String id = sources[i]->id();
        String deviceString = getDeviceStringFromDeviceIdString(id);
        String factoryString = getFactoryNameFromDeviceIdString(id);

        String sourceId = storeSourceInfo(sources[i]->type(), factoryString, deviceString);
        LOG(MediaStream, "Registering source factory for source with id=%s", sourceId.ascii().data());
        cpu.registerSourceFactory(this, sourceId);
    }
}

String MediaStreamCenterPrivateGStreamer::storeElementFactoryForElement(GstElement* source)
{
    GstElementFactory* elementFactory = gst_element_get_factory(source);
    gchar* factoryName = gst_element_get_name(elementFactory);
    String strFactoryName(factoryName);
    g_free(factoryName);

    ElementFactoryMap::iterator elementFactoryIt = m_elementFactoryMap.find(strFactoryName);
    if (elementFactoryIt == m_elementFactoryMap.end())
        m_elementFactoryMap.add(strFactoryName, elementFactory);

    return strFactoryName;
}

GstElementFactory* MediaStreamCenterPrivateGStreamer::storedElementFactory(const String& key)
{
    GstElementFactory* elementFactory = 0;
    ElementFactoryMap::iterator elementFactoryIt = m_elementFactoryMap.find(key);
    if (elementFactoryIt != m_elementFactoryMap.end())
        elementFactory = (elementFactoryIt->value).get();
    return elementFactory;
}

String MediaStreamCenterPrivateGStreamer::storeSourceInfo(MediaStreamSource::Type type, const String& factoryKey, const String& device)
{
    String key = String(factoryKey);
    key.append(";");
    key.append(device);

    SourceInfoMap::iterator sourceInfoIt = m_sourceInfoMap.find(key);
    if (sourceInfoIt != m_sourceInfoMap.end())
        return sourceInfoIt->key;

    SourceInfo sourceInfo = {factoryKey, type, device};
    SourceInfoMap::AddResult result = m_sourceInfoMap.add(key, sourceInfo);

    return key;
}

GstElement* MediaStreamCenterPrivateGStreamer::createSource(const String& sourceId, GstPad** srcPad)
{
    LOG(MediaStream, "Creating source with id=%s", sourceId.ascii().data());

    SourceInfoMap::iterator sourceInfoIt = m_sourceInfoMap.find(sourceId);
    if (sourceInfoIt == m_sourceInfoMap.end())
        return 0;

    SourceInfo sourceInfo = sourceInfoIt->value;

    GstElementFactory* factory = storedElementFactory(sourceInfo.m_factoryKey);
    if (!factory)
        return 0;

    GstElement* source = gst_element_factory_create(factory, "device_source");
    if (!source)
        return 0;

    LOG(MediaStream, "sourceInfo.m_device=%s", sourceInfo.m_device.ascii().data());
    // do not set device for test sources
    if (sourceInfo.m_device != "testsrc_audio"
        && sourceInfo.m_device != "testsrc_video")
        g_object_set(source, "device", sourceInfo.m_device.ascii().data(), NULL);

    if (sourceInfo.m_type == MediaStreamSource::TypeAudio)
        source = createAudioSourceBin(source);
    else if (sourceInfo.m_type == MediaStreamSource::TypeVideo)
        source = createVideoSourceBin(source);

    if (srcPad)
        *srcPad = gst_element_get_static_pad(source, "src");

    return source;
}

static GstElement* findAudioDeviceSource()
{
    GstElement* autoSrc = gst_element_factory_make("autoaudiosrc", "autosrc");
    if (!autoSrc)
        return 0;
    GstElement* audioSrc = findDeviceSource(autoSrc);
    gst_object_unref(autoSrc);
    return audioSrc;
}

static GstElement* findVideoDeviceSource()
{
    GstElement* autoSrc = gst_element_factory_make("autovideosrc", "autosrc");
    if (!autoSrc)
        return 0;
    GstElement* videoSrc = findDeviceSource(autoSrc);
    gst_object_unref(autoSrc);
    return videoSrc;
}

static GstElement* findDeviceSource(GstElement* autoSrc)
{
    GstStateChangeReturn stateChangeResult = gst_element_set_state(autoSrc, GST_STATE_READY);
    if (stateChangeResult != GST_STATE_CHANGE_SUCCESS)
        return 0;

    GstChildProxy* childProxy = GST_CHILD_PROXY(autoSrc);
    if (!childProxy)
        return 0;

    GstElement* deviceSrc = 0;
    guint childCount = gst_child_proxy_get_children_count(childProxy);
    if (childCount) {
        GstObject* child = gst_child_proxy_get_child_by_index(childProxy, 0);
        deviceSrc = GST_ELEMENT(child);
    }

    gst_element_set_state(autoSrc, GST_STATE_NULL);
    return deviceSrc;
}

static void probeSource(GstElement* source, const String& key, MediaStreamSource::Type type, MediaStreamSourceVector& sources)
{
    GstPropertyProbe* probe = GST_PROPERTY_PROBE(source);
    if (!probe)
        return;

    const GParamSpec* pspec = gst_property_probe_get_property(probe, "device");
    if (!pspec)
        return;

    GValueArray* array = gst_property_probe_probe_and_get_values(probe, pspec);
    if (array) {
        GValue* value = 0;
        gchar* deviceName = 0;

        for (int i = 0; i < (int)array->n_values; i++) {
            value = g_value_array_get_nth(array, i);
            const gchar* device = g_value_get_string(value);
            g_object_set(source, "device", device, NULL);
            g_object_get(source, "device-name", &deviceName, NULL);
            String sourceName = String(deviceName);

            String deviceId = key;
            deviceId.append(";");
            deviceId.append(device);

            gchar* sourceElementName = gst_element_get_name(source);
            if (!g_strcmp0(sourceElementName, "autosrc-actual-src-puls"))
                sourceName = filterAudioSourceName(deviceId, sourceName);
            g_free(sourceElementName);

            if (!sourceName.isEmpty())
                sources.append(MediaStreamSource::create(deviceId, type, sourceName));
            g_free(deviceName);
        }
        g_value_array_free(array);
    }
}

static String filterAudioSourceName(const String& deviceId, const String& name)
{
    // autosrc-actual-src-puls returns output devices, and does not supply deviceName. This
    // removes the output and parses a somewhat readable display name from the "device id".
    if (deviceId.contains("alsa_output", false))
        return "";

    guint length = deviceId.length();
    size_t position = deviceId.find("alsa_input.");
    if (position != notFound)
        return deviceId.substring(position + 11, length);
    return name;
}

static String getDeviceStringFromDeviceIdString(const String& deviceId)
{
    size_t index = deviceId.find(";");
    String deviceStr = deviceId.substring(index+1, deviceId.length());
    return deviceStr;
}

static String getFactoryNameFromDeviceIdString(const String& deviceId)
{
    size_t index = deviceId.find(";");
    String deviceStr = deviceId.left(index);
    return deviceStr;
}

static gchar* createUniqueName(const gchar* prefix)
{
    static int count = 0;
    gchar* uniqueName = g_strdup_printf("%s%d", prefix, count++);
    return uniqueName;
}

static GstElement* createAudioSourceBin(GstElement* source)
{
    GstElement* audioconvert = gst_element_factory_make("audioconvert", "audioconvert");
    if (!audioconvert) {
        LOG(MediaStream, "ERROR, Got no audioconvert element for audio source pipeline");
        return 0;
    }

    GstCaps* audiocaps = gst_caps_new_simple("audio/x-raw-int", "channels", G_TYPE_INT, 1, NULL);
    if (!audiocaps) {
        LOG(MediaStream, "ERROR, Unable to create filter caps for audio source pipeline");
        gst_object_unref(audioconvert);
        return 0;
    }

    gchar* binName = createUniqueName("StreamDevice_AudioTestSourceBin");
    GstElement* audioSourceBin = gst_bin_new(binName);
    g_free(binName);

    gst_bin_add_many(GST_BIN(audioSourceBin), source, audioconvert, NULL);

    if (!gst_element_link_filtered(source, audioconvert, audiocaps)) {
        LOG(MediaStream, "ERROR, Cannot link audio source elements");
        gst_caps_unref(audiocaps);
        gst_object_unref(audioSourceBin);
        return 0;
    }
    gst_caps_unref(audiocaps);

    GstPad* srcPad = gst_element_get_static_pad(audioconvert, "src");
    gst_element_add_pad(audioSourceBin, gst_ghost_pad_new("src", srcPad));

    return audioSourceBin;
}

static GstElement* createVideoSourceBin(GstElement *source)
{
    GstElement* colorspace = gst_element_factory_make("ffmpegcolorspace", "colorspace");
    if (!colorspace) {
        LOG(MediaStream, "ERROR, Got no ffmpegcolorspace element for video source pipeline");
        return 0;
    }
    GstElement* videoscale = gst_element_factory_make("videoscale", "videoscale");
    if (!videoscale) {
        LOG(MediaStream, "ERROR, Got no videoscale element for video source pipeline");
        gst_object_unref(colorspace);
        return 0;
    }

    GstCaps* videocaps = gst_caps_new_simple("video/x-raw-yuv", "width", G_TYPE_INT, 320, "height", G_TYPE_INT, 240, NULL);
    if (!videocaps) {
        LOG(MediaStream, "ERROR, Unable to create filter caps for video source pipeline");
        gst_object_unref(colorspace);
        gst_object_unref(videoscale);
        return 0;
    }

    gchar* binName = createUniqueName("StreamDevice_VideoTestSourceBin");
    GstElement* videoSourceBin = gst_bin_new(binName);
    g_free(binName);

    gst_bin_add_many(GST_BIN(videoSourceBin), source, videoscale, colorspace, NULL);

    if (!gst_element_link_many(source, videoscale, NULL)
        || !gst_element_link_filtered(videoscale, colorspace, videocaps)) {
        LOG(MediaStream, "ERROR, Cannot link video source elements");
        gst_caps_unref(videocaps);
        gst_object_unref(videoSourceBin);
        return 0;
    }
    gst_caps_unref(videocaps);

    GstPad* srcPad = gst_element_get_static_pad(colorspace, "src");
    gst_element_add_pad(videoSourceBin, gst_ghost_pad_new("src", srcPad));

    return videoSourceBin;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
