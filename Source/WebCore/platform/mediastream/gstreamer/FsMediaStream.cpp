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

#include "FsMediaStream.h"

#include "../gstreamer/PeerConnectionHandlerConfiguration.h"
#include "../gstreamer/sdp/MediaDescription.h"
#include "CentralPipelineUnit.h"
#include "Logging.h"
#include "PeerConnectionHandlerPrivateFarstream.h"

#include <farstream/fs-candidate.h>
#include <farstream/fs-utils.h>
#include <wtf/MainThread.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/text/CString.h>

namespace WebCore {

static inline GQuark quarkFsMediaStreamComponent()
{
    static GQuark q;
    if (G_UNLIKELY(!q))
        q = g_quark_from_string("fsMediaStreamComponent");

    return q;
}

static String fsCodecToString(FsCodec* codec)
{
    String result;
    result += String::format("(id=%d, name=%s, type=%d, rate=%u, channels=%u)",
                             codec->id, codec->encoding_name, codec->media_type,
                             codec->clock_rate, codec->channels);

    return result;
}

static String fsCandidateToString(FsCandidate* candidate)
{
    String result;
    result += String::format("(component=%d, ip=%s, port=%d, proto=%d, priority=%d, ", candidate->component_id, candidate->ip, candidate->port, candidate->proto, candidate->priority);
    result += String::format("base ip:%s:%d, username=%s, password=%s)", candidate->base_ip, candidate->base_port, candidate->username, candidate->password);

    return result;
}

static void printCodecList(GList* codecs)
{
    GList* lst = codecs;
    for (; lst; lst = lst->next)
        LOG(MediaStream, "- codec: %s", fsCodecToString(reinterpret_cast<FsCodec*>(lst->data)).utf8().data());
}

static void onSessionNotifyCodecsHelper(GObject* object, GParamSpec* pspec, gpointer userData)
{
    FsMediaStreamComponent* component = reinterpret_cast<FsMediaStreamComponent*>(userData);
    FsSession* self = FS_SESSION(object);
    GList* codecs = 0;

    LOG(MediaStream, "Session %p 'codecs' property changed for component %p:%p", self, component->fsMediaStream(), component);
    g_object_get(self, "codecs", &codecs, NULL);

    printCodecList(codecs);
    fs_codec_list_destroy(codecs);
}

static void onStreamSrcPadAddedHelper(FsStream* stream, GstPad* pad, FsCodec* codec, gpointer data)
{
    FsMediaStreamComponent* component = static_cast<FsMediaStreamComponent*>(data);

    LOG(MediaStream, "stream %p 'src-pad-added' (component %p:%p)", stream, component->fsMediaStream(), component);
    component->onSrcPadAdded(pad, codec);
}

static void onStreamNotifyNegotiatedCodecsHelper(GObject* object, GParamSpec* pspec, gpointer userData)
{
    FsMediaStreamComponent* component = reinterpret_cast<FsMediaStreamComponent*>(userData);
    FsStream* self = FS_STREAM(object);
    GList* codecs = 0;

    LOG(MediaStream, "Stream %p 'negotiated-codecs' property changed (component %p:%p).", self, component->fsMediaStream(), component);
    g_object_get(self, "negotiated-codecs", &codecs, NULL);

    printCodecList(codecs);
    fs_codec_list_destroy(codecs);
}

static void onStreamNotifyCurrentRecvCodecsHelper(GObject* object, GParamSpec* pspec, gpointer userData)
{
    FsMediaStreamComponent* component = reinterpret_cast<FsMediaStreamComponent*>(userData);
    FsStream* self = FS_STREAM(object);
    GList* codecs = 0;

    LOG(MediaStream, "Stream %p 'current-recv-codecs' property changed (component %p:%p).", self, component->fsMediaStream(), component);
    g_object_get(self, "current-recv-codecs", &codecs, NULL);

    printCodecList(codecs);
    fs_codec_list_destroy(codecs);
}

static int compareCandidatePriority(const FsCandidate* a, const FsCandidate* b)
{
    return (a->priority < b->priority);
}

struct PadAddedParams {
    FsMediaStreamComponent* component;
    GRefPtr<GstPad> pad;
};

static void notifySrcPadAddedMain(gpointer data)
{
    ASSERT(isMainThread());
    PadAddedParams* params = reinterpret_cast<PadAddedParams*>(data);
    FsMediaStreamComponent* component = params->component;
    component->fsMediaStream()->client()->onSrcPadAdded(component, params->pad.get());

    delete params;
}

static void onBufferProbeCalledMain(gpointer data)
{
    ASSERT(isMainThread());
    FsMediaStreamComponent* self = reinterpret_cast<FsMediaStreamComponent*>(data);

    self->onBufferProbeCalled();
}

static gboolean onBufferProbeCalledProxy(GstPad* pad, GstMiniObject* buffer, gpointer userData)
{
    LOG(MediaStream, "%s: pad '%s' (%p)", G_STRFUNC, GST_OBJECT_NAME(pad), pad);
    callOnMainThread(onBufferProbeCalledMain, userData);
    return TRUE;
}


void FsMediaStreamComponent::onBufferProbeCalled()
{
    LOG(MediaStream, "%s", G_STRFUNC);
    m_hasReceivedData = true;

    gst_pad_remove_buffer_probe(m_srcPad.get(), m_bufferProbeHandlerId);
    m_bufferProbeHandlerId = 0;

    fsMediaStream()->onComponentReceivedData(this);
}

bool FsMediaStreamComponent::hasReceivedData() const
{
    return m_hasReceivedData;
}

static String generateFsStreamPadSourceId(const FsMediaStreamClient* clientPointerValue, String padname)
{
    String sourceId = "FsStream:";
    sourceId += String::number((gulong)clientPointerValue);
    sourceId += ":";
    sourceId += padname;
    return sourceId;
}

void FsMediaStreamComponent::onSrcPadAdded(GstPad* pad, FsCodec* codec)
{
    m_srcPad = pad;
    setSourceId(generateFsStreamPadSourceId(fsMediaStream()->client(), GST_OBJECT_NAME(pad)));
    // take another ref since the pipeline will take over the initial ref
    m_valve = gst_element_factory_make("valve", 0);
    GRefPtr<GstPad> valveSink = adoptGRef(gst_element_get_static_pad(m_valve.get(), "sink"));
    m_valveSrc = adoptGRef(gst_element_get_static_pad(m_valve.get(), "src"));
    gst_bin_add(GST_BIN(m_pipeline.get()), m_valve.get());
    gst_pad_link(pad, valveSink.get());

    m_bufferProbeHandlerId = gst_pad_add_buffer_probe(pad, G_CALLBACK(onBufferProbeCalledProxy), this);
    LOG(MediaStream, "added buffer probe %u to pad %s", m_bufferProbeHandlerId, GST_OBJECT_NAME(pad));

    // drop packets until we've been notified that the source has actually been
    // inserted into the pipeline.
    g_object_set(m_valve.get(), "drop", TRUE, NULL);

    PadAddedParams* params = new PadAddedParams();
    params->component = this;
    params->pad = m_valveSrc;
    callOnMainThread(notifySrcPadAddedMain, params);
}

void FsMediaStreamComponent::setSourceId(const String& sourceId)
{
    m_sourceId = sourceId;
    if (m_fsMediaStream->type() == FsMediaStream::Local)
        centralPipelineUnit().connectToSource(m_sourceId, m_sinkElement.get(), m_sinkPad.get());
}

String FsMediaStreamComponent::sourceId() const
{
    return m_sourceId;
}

GstPad* FsMediaStreamComponent::sourcePad() const
{
    return m_valveSrc.get();
}

GstElement* FsMediaStreamComponent::sourceElement() const
{
    return m_valve.get();
}

FsMediaStreamComponent::FsMediaStreamComponent(FsMediaStream* fsMediaStream, FsMediaType type)
    : m_type(type)
    , m_fsMediaStream(fsMediaStream)
    , m_localCandidatesPrepared(false)
    , m_insertedIntoPipeline(false)
    , m_hasReceivedData(false)
    , m_bufferProbeHandlerId(0)
    , m_codecs(0)
{
    GOwnPtr<GError> error;
    GList* codecPrefs = fs_utils_get_default_codec_preferences(GST_ELEMENT(m_fsMediaStream->conference()));
    m_session = adoptGRef(fs_conference_new_session(m_fsMediaStream->conference(), type, &error.outPtr()));
    fs_session_set_codec_preferences(m_session.get(), codecPrefs, 0);
    fs_codec_list_destroy(codecPrefs);
    if (error) {
        g_warning("Error creating farstream session: %s", error->message);
        error.clear();
        return;
    }
    g_signal_connect(m_session.get(), "notify::codecs", G_CALLBACK(onSessionNotifyCodecsHelper), this);

    g_object_set_qdata(G_OBJECT(m_session.get()), quarkFsMediaStreamComponent(), this);

    GstPad* sinkPadPtr = 0;

    g_object_get(m_session.get(), "sink-pad", &sinkPadPtr, NULL);

    m_sinkPad = adoptGRef(sinkPadPtr);
    FsStreamDirection dir = FS_DIRECTION_NONE;
    if (m_fsMediaStream->type() == FsMediaStream::Local) {
        m_sinkElement = adoptGRef(gst_pad_get_parent_element(m_sinkPad.get()));

        centralPipelineUnit().connectToSource(m_sourceId, m_sinkElement.get(), m_sinkPad.get());

        dir = FS_DIRECTION_SEND;
    } else if (m_fsMediaStream->type() == FsMediaStream::Remote)
        dir = FS_DIRECTION_RECV;
    else
       g_warning("Unknown media stream direction = %d", m_fsMediaStream->type());

    m_stream = adoptGRef(fs_session_new_stream(m_session.get(), fsMediaStream->participant(), dir, &error.outPtr()));
    if (error) {
        g_warning("Error creating farstream stream: %s", error->message);
        error.clear();
        return;
    }

    g_object_set_qdata(G_OBJECT(m_stream.get()), quarkFsMediaStreamComponent(), this);
    g_signal_connect(m_stream.get(), "src-pad-added", G_CALLBACK(onStreamSrcPadAddedHelper), this);
    g_signal_connect(m_stream.get(), "notify::current-recv-codecs", G_CALLBACK(onStreamNotifyCurrentRecvCodecsHelper), this);
    g_signal_connect(m_stream.get(), "notify::negotiated-codecs", G_CALLBACK(onStreamNotifyNegotiatedCodecsHelper), this);

    // listen on the bus
    m_pipeline = centralPipelineUnit().pipeline();
    if (!m_pipeline) {
        g_warning("Error accessing central pipeline");
        return;
    }

    m_bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get()));
    if (!m_bus) {
        g_warning("Error accessing pipeline bus");
        return;
    }

    gst_bus_add_signal_watch(m_bus);
    g_signal_connect(m_bus, "message", G_CALLBACK(onBusMessage), this);
}

FsMediaStreamComponent::~FsMediaStreamComponent()
{
    clearLocalCandidates();
    clearActiveCandidates();
    gst_bus_remove_signal_watch(m_bus);
    if (m_codecs)
        fs_codec_list_destroy(m_codecs);
    gst_object_unref(m_bus);
}

#if 0
void FsMediaStreamComponent::setLocalCryptoContext(unsigned char* key, unsigned int keyLength,
                                         unsigned char* salt, unsigned int saltLength,
                                         unsigned char* mki, unsigned int mkiLength)
{
    m_localKey = (unsigned char*)malloc(keyLength);
    memcpy(m_localKey, key, keyLength);
    m_localKeyLength = keyLength;

    m_localSalt = (unsigned char*)malloc(saltLength);
    memcpy(m_localSalt, salt, saltLength);
    m_localSaltLength = saltLength;

    if (!mki || !mkiLength) {
        m_localMki = 0;
        m_localMkiLength = 0;
    } else {
        m_localMki = (unsigned char*)malloc(mkiLength);
        memcpy(m_localMki, mki, mkiLength);
        m_localMkiLength = mkiLength;
    }
}

void FsMediaStreamComponent::setRemoteCryptoContext(unsigned char* key, unsigned int keyLength,
                                         unsigned char* salt, unsigned int saltLength,
                                         unsigned char* mki, unsigned int mkiLength)
{
    m_remoteKey = (unsigned char*)malloc(keyLength);
    memcpy(m_remoteKey, key, keyLength);
    m_remoteKeyLength = keyLength;

    m_remoteSalt = (unsigned char*)malloc(saltLength);
    memcpy(m_remoteSalt, salt, saltLength);
    m_remoteSaltLength = saltLength;

    if (!mki || !mkiLength) {
        m_remoteMki = 0;
        m_remoteMkiLength = 0;
    } else {
        m_remoteMki = (unsigned char*)malloc(mkiLength);
        memcpy(m_remoteMki, mki, mkiLength);
        m_remoteMkiLength = mkiLength;
    }
}


void FsMediaStreamComponent::getLocalCryptoContext(unsigned char** key, unsigned int* keyLength,
                                         unsigned char** salt, unsigned int* saltLength,
                                         unsigned char** mki, unsigned int* mkiLength)
{
    *key = m_localKey;
    *keyLength = m_localKeyLength;
    *salt = m_localSalt;
    *saltLength = m_localSaltLength;
    *mki = m_localMki;
    *mkiLength = m_localMkiLength;
}

void FsMediaStreamComponent::getRemoteCryptoContext(unsigned char** key, unsigned int* keyLength,
                                         unsigned char** salt, unsigned int* saltLength,
                                         unsigned char** mki, unsigned int* mkiLength)
{
    *key = m_remoteKey;
    *keyLength = m_remoteKeyLength;
    *salt = m_remoteSalt;
    *saltLength = m_remoteSaltLength;
    *mki = m_remoteMki;
    *mkiLength = m_remoteMkiLength;
}
#endif

bool FsMediaStreamComponent::localCandidatesPrepared() const
{
    return m_localCandidatesPrepared;
}

static Payload* fromFsCodec(const FsCodec* codec)
{
    Payload* payload = new Payload();
    if (!payload)
        return 0;

    payload->setPayloadTypeNumber(codec->id);
    payload->setRate(codec->clock_rate);
    payload->setCodecName(codec->encoding_name);
    payload->setChannels(!codec->channels ? 1 : codec->channels);
    // TODO payload->setFormat(codec->);
    // TODO payload->setFramesizeWidth(unsigned int width);
    // TODO payload->setFramesizeHeight(unsigned int height);

    return payload;
}

static Vector<Payload*> fromFsCodecList(const GList* fsCodecs, FsMediaType mediaType)
{
    Vector<Payload*> outPayloads;
    const GList* lst = fsCodecs;
    while (lst) {
        const FsCodec* codec = static_cast<FsCodec*> (lst->data);
        if (codec->media_type == mediaType && codec->id != FS_CODEC_ID_DISABLE) {
            Payload* payload = fromFsCodec(codec);
            if (payload)
                outPayloads.append(payload);
        }
        lst = g_list_next(lst);
    }
    return outPayloads;
}

static Candidate::CandidateType fromFsCandidateType(FsCandidateType type)
{
    switch (type) {
    case FS_CANDIDATE_TYPE_HOST:
        return Candidate::Host;
    case FS_CANDIDATE_TYPE_SRFLX:
        return Candidate::ServerReflexive;
    case FS_CANDIDATE_TYPE_PRFLX:
        return Candidate::PeerReflexive;
    case FS_CANDIDATE_TYPE_RELAY:
        return Candidate::Relayed;
    case FS_CANDIDATE_TYPE_MULTICAST:
        return Candidate::Multicast;
    default:
        g_warning("FsCandidate type=%d unknown", type);
    }

    return Candidate::Host;
}

static Candidate::Protocol fromFsProtocol(FsNetworkProtocol proto)
{
    if (proto == FS_NETWORK_PROTOCOL_TCP)
        return Candidate::TCP;

    return Candidate::UDP;
}

static Candidate* fromFsCandidate(FsCandidate* fsCandidate)
{
    Candidate* candidate = new Candidate();
    if (!candidate)
        return candidate;

    candidate->setFoundation(fsCandidate->foundation);
    candidate->setComponentId(fsCandidate->component_id);
    candidate->setCandidateType(fromFsCandidateType(fsCandidate->type));
    candidate->setAddress(fsCandidate->ip);
    candidate->setPort(fsCandidate->port);
    candidate->setPriority(fsCandidate->priority);
    candidate->setRelatedAddress(fsCandidate->foundation);
    candidate->setRelatedPort(fsCandidate->foundation);
    candidate->setProtocol(fromFsProtocol(fsCandidate->proto));

    return candidate;
}

Vector<Candidate*> FsMediaStreamComponent::localCandidates() const
{
    Vector<Candidate*> candidates;

    if (!m_localCandidatesPrepared)
        return candidates;

    Vector<FsCandidate*>::const_iterator iter;
    for (iter = m_localCandidates.begin();iter != m_localCandidates.end(); ++iter) {
        // FIXME: afaict, nobody has responsibility for freeing this.
        // ownership is unclear, and I believe it is always leaked
        Candidate* candidate = fromFsCandidate(*iter);
        candidates.append(candidate);
    }

    return candidates;
}

Vector<Candidate*> FsMediaStreamComponent::activeCandidates() const
{
    Vector<Candidate*> candidates;

    Vector<FsCandidate*>::const_iterator iter;
    for (iter = m_activeCandidates.begin(); iter != m_activeCandidates.end(); ++iter) {
        // FIXME: afaict, nobody has responsibility for freeing this.
        // ownership is unclear, and I believe it is always leaked
        Candidate* candidate = fromFsCandidate(*iter);
        candidates.append(candidate);
    }

    return candidates;
}

void FsMediaStreamComponent::clearLocalCandidates()
{
    Vector<FsCandidate*>::iterator iter;
    for (iter = m_localCandidates.begin();iter != m_localCandidates.end(); ++iter)
        fs_candidate_destroy(*iter);

    m_localCandidates.clear();
    m_localCandidatesPrepared = false;
}

void FsMediaStreamComponent::clearActiveCandidates()
{
    Vector<FsCandidate*>::iterator iter;
    for (iter = m_activeCandidates.begin();iter != m_activeCandidates.end(); ++iter)
        fs_candidate_destroy(*iter);

    m_activeCandidates.clear();
}

bool FsMediaStreamComponent::getLocalParams(String& outIp, unsigned int& outPort, String& outUsername, String& outPassword) const
{
    outIp.truncate(0);
    outPort = 0;
    outUsername.truncate(0);
    outPassword.truncate(0);

    if (!m_localCandidatesPrepared)
        return false;

    Vector<FsCandidate*>::const_iterator iter = std::min_element(m_localCandidates.begin(), m_localCandidates.end(), compareCandidatePriority);
    if (*iter) {
        // favor HOST candidate type to return ip and port
        outIp = (*iter)->ip;
        outPort = (*iter)->port;
        outUsername = (*iter)->username;
        outPassword = (*iter)->password;
        return true;
    }

    if (fsMediaStream()->closed())
        outPort = 0;

    return false;
}

void FsMediaStreamComponent::addRemoteCandidates(const Vector<Candidate*>& candidates, const String& username, const String& password)
{
    GList* fscandidates = 0;

    if (candidates.isEmpty())
        return;

    Vector<Candidate*>::const_iterator canditer = candidates.begin();
    for (; canditer != candidates.end(); ++canditer) {
        Candidate* cand = *canditer;
        FsCandidateType fstype;
        switch (cand->candidateType()) {
        case Candidate::ServerReflexive:
            fstype = FS_CANDIDATE_TYPE_SRFLX;
            break;
        case Candidate::PeerReflexive:
            fstype = FS_CANDIDATE_TYPE_PRFLX;
            break;
        case Candidate::Relayed:
            fstype = FS_CANDIDATE_TYPE_RELAY;
            break;
        case Candidate::Host:
            fstype = FS_CANDIDATE_TYPE_HOST;
            break;
        case Candidate::Multicast:
            fstype = FS_CANDIDATE_TYPE_MULTICAST;
            break;
        default:
            g_warn_if_reached();
            return;
        }

        FsNetworkProtocol fsproto;
        if (cand->protocol() == Candidate::UDP) {
            fsproto = FS_NETWORK_PROTOCOL_UDP;
        } else if (cand->protocol() == Candidate::TCP)
            fsproto = FS_NETWORK_PROTOCOL_TCP;
        else {
            g_warn_if_reached();
            return;
        }

        FsCandidate* fscand = fs_candidate_new(cand->foundation().utf8().data(),
                                               cand->componentId(),
                                               fstype, fsproto,
                                               cand->address().utf8().data(),
                                               cand->port());
        fscand->priority = cand->priority();
        fscand->username = g_strdup(username.utf8().data());
        fscand->password = g_strdup(password.utf8().data());

        fscandidates = g_list_append(fscandidates, fscand);

        if (m_componentState.find(cand->componentId()) == m_componentState.end())
            m_componentState.set(cand->componentId(), FS_STREAM_STATE_DISCONNECTED);
    }

    GError* error = 0;
    if (!fs_stream_add_remote_candidates(m_stream.get(), fscandidates, &error)) {
        g_warning("Unable to add remote candidates: %s", error->message);
        g_error_free(error);
    }

    fs_candidate_list_destroy(fscandidates);
}

void FsMediaStreamComponent::setRemoteCodecs(const Vector<Payload*>& codecs)
{
    GList* fscodecs = 0;

    if (codecs.isEmpty())
        return;

    Vector<Payload*>::const_iterator iter = codecs.begin();
    for (; iter != codecs.end(); ++iter) {
        Payload* payload = *iter;
        FsCodec* fscodec = fs_codec_new(payload->payloadTypeNumber(),
                                        payload->codecName().utf8().data(),
                                        type(),
                                        payload->rate());
        fscodec->channels = payload->channels();
        // XXX: add frame size, etc with fs_codec_add_optional_parameter()?

        fscodecs = g_list_append(fscodecs, fscodec);
    }

    GError* error = 0;
    if (!fs_stream_set_remote_codecs(m_stream.get(), fscodecs, &error)) {
        g_warning("Couldn't set remote codecs: %s", error->message);
        g_error_free(error);
    }

    fs_codec_list_destroy(fscodecs);
}


void FsMediaStreamComponent::onNewLocalCandidate(FsCandidate* candidate)
{
    g_return_if_fail(candidate);
    LOG(MediaStream, "Adding candidate %p for stream %p (%p:%p)\n%s", candidate, m_stream.get(), m_fsMediaStream, this, fsCandidateToString(candidate).utf8().data());
    m_localCandidates.append(fs_candidate_copy(candidate));
}

void FsMediaStreamComponent::onLocalCandidatesPrepared()
{
    LOG(MediaStream, "localCandidatesPrepared for stream %p (%p:%p)", m_stream.get(), m_fsMediaStream, this);
    m_localCandidatesPrepared = true;

    // call the handler only if all components are prepared
    m_fsMediaStream->onLocalCandidatesPrepared(this);
}

void FsMediaStreamComponent::onNewActiveCandidatePair(FsCandidate* localCandidate, FsCandidate* remoteCandidate)
{
    LOG(MediaStream, "Component %p:%p got new active candidate pair:\nLocal:\n%s\nRemote:\n%s",
            m_fsMediaStream, this,
            fsCandidateToString(localCandidate).utf8().data(),
            fsCandidateToString(remoteCandidate).utf8().data());

    m_activeCandidates.append(fs_candidate_copy(localCandidate));
}

void FsMediaStreamComponent::onReceiveCodecsChanged(GList* codecs)
{
    LOG(MediaStream, "Receive codecs changed for stream %p (%p:%p)", m_stream.get(), m_fsMediaStream, this);
    printCodecList(codecs);
}

void FsMediaStreamComponent::onComponentStateChanged(guint component, FsStreamState state)
{
    LOG(MediaStream, "Stream %p#%u (%p:%p) changed to state %i", m_stream.get(), component, m_fsMediaStream, this, state);

    m_componentState.set(component, state);

    if (isConnectionReady()) {
        LOG(MediaStream, "All %u components are ready, notifying Stream", m_componentState.size());
        m_fsMediaStream->onComponentConnectionReady(this);
    }
}

bool FsMediaStreamComponent::isConnectionReady() const
{
    if (m_componentState.isEmpty())
        return false;

    HashMap<guint, FsStreamState>::const_iterator iter = m_componentState.begin();
    for (; iter != m_componentState.end(); ++iter) {
        if (iter->second != FS_STREAM_STATE_READY)
            return false;
    }

    return true;
}

void FsMediaStreamComponent::onSendCodecChanged(FsCodec *codec, GList* secondaryCodecs)
{
    LOG(MediaStream, "Send codec changed for component %p:%p: %s", m_fsMediaStream, this, fsCodecToString(codec).utf8().data());
    printCodecList(secondaryCodecs);
}

void FsMediaStreamComponent::onCodecsChanged()
{
    LOG(MediaStream, "%s", G_STRFUNC);
    GList* changedCodecs = 0;
    GList* newCodecs = 0;
    g_object_get(m_session.get(), "codecs", &newCodecs, NULL);

    printCodecList(newCodecs);
    changedCodecs =  fs_session_codecs_need_resend(m_session.get(), m_codecs, newCodecs);

    if (m_codecs)
        fs_codec_list_destroy(m_codecs);

    m_codecs = newCodecs;

    if (changedCodecs) {
        LOG(MediaStream, "%s: Need to resend codecs", G_STRFUNC);
        fsMediaStream()->client()->codecsNeedResend(this);
        fs_codec_list_destroy(changedCodecs);
    }
}

gboolean FsMediaStreamComponent::onBusMessage(GstBus* bus, GstMessage* message, gpointer data)
{
    FsMediaStreamComponent* self = static_cast<FsMediaStreamComponent*>(data);
    bool handled = true;
    GError* error = 0;
    gchar* errorMsg = 0;

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
        gst_message_parse_error(message, &error, &errorMsg);
        g_warning("An Error occurred: %s [%s]", error->message, errorMsg);
        g_error_free(error);
        g_free(errorMsg);
        break;
    case GST_MESSAGE_WARNING:
        gst_message_parse_warning(message, &error, &errorMsg);
        g_warning("A Warning occurred: %s [%s]", error->message, errorMsg);
        g_error_free(error);
        g_free(errorMsg);
        break;
    case GST_MESSAGE_ELEMENT:
        {
            FsCodec* codec = 0;
            GList* codecs = 0;
            FsCandidate* localCandidate = 0;
            FsCandidate* remoteCandidate = 0;
            guint component = 0;
            FsStreamState state = FS_STREAM_STATE_DISCONNECTED;

            if (fs_stream_parse_new_local_candidate(self->m_stream.get(), message, &localCandidate)) {
                self->onNewLocalCandidate(localCandidate);
            } else if (fs_stream_parse_local_candidates_prepared(self->m_stream.get(), message)) {
                self->onLocalCandidatesPrepared();
            } else if (fs_stream_parse_new_active_candidate_pair(self->m_stream.get(), message, &localCandidate, &remoteCandidate)) {
                self->onNewActiveCandidatePair(localCandidate, remoteCandidate);
            } else if (fs_stream_parse_recv_codecs_changed(self->m_stream.get(), message, &codecs)) {
                self->onReceiveCodecsChanged(codecs);
            } else if (fs_stream_parse_component_state_changed(self->m_stream.get(), message, &component, &state)) {
                self->onComponentStateChanged(component, state);
            } else if (fs_session_parse_send_codec_changed(self->m_session.get(), message, &codec, &codecs)) {
                self->onSendCodecChanged(codec, codecs);
            } else if (fs_session_parse_codecs_changed(self->m_session.get(), message))
                self->onCodecsChanged();
            else {
                handled = false;
                // FIXME: use fs_parse_error()?
                const GstStructure *structure = gst_message_get_structure(message);

                if (gst_structure_has_name(structure, "farstream-error")) {
                    gint error;
                    const gchar *error_msg = gst_structure_get_string(structure, "error-msg");
                    g_assert(gst_structure_get_enum(structure, "error-no", FS_TYPE_ERROR, &error));
                    g_warning("Farstream error: %d %s", error, error_msg);
                }
            }
        }
        break;
    default:
        handled = false;
    }

    // only print out the debug information for this message if we're supposed
    // to handle it, otherwise we'll get a lot of duplicate debug statement if
    // there are multiple listeners
    if (handled) {
        GOwnPtr<gchar> details(gst_structure_to_string(message->structure));
        if (message->type != GST_MESSAGE_STATE_CHANGED) {
            LOG(MediaStream, "%s: message type '%s' from %s (%p)\n\t-=[%s]=-", G_STRFUNC,
                    GST_MESSAGE_TYPE_NAME(message),
                    g_type_name(G_OBJECT_TYPE(message->src)),
                    message->src,
                    details.get());
        }
    }

    return TRUE;
}

bool FsMediaStreamComponent::setServerConfiguration(const PeerConnectionHandlerConfiguration* config, bool initiator)
{
    guint nparams = 0;
    GParameter params[4] = {{0, G_VALUE_INIT}, {0, G_VALUE_INIT}};

    if (config->type == PeerConnectionHandlerConfiguration::TypeSTUN) {
        params[nparams].name = "stun-ip";
        g_value_init(&params[nparams].value, G_TYPE_STRING);
        g_value_set_string(&params[nparams].value, config->host.utf8().data());
        nparams++;

        params[nparams].name = "stun-port";
        g_value_init(&params[nparams].value, G_TYPE_UINT);
        g_value_set_uint(&params[nparams].value, config->port);
        nparams++;
    } else {
        GValueArray* relayInfo(g_value_array_new(1));
        GstStructure* structure =
            gst_structure_new("relay-info",
                              "ip", G_TYPE_STRING, config->host.utf8().data(),
                              "port", G_TYPE_UINT, config->port,
                              "username", G_TYPE_STRING, config->username.utf8().data(),
                              "password", G_TYPE_STRING, config->password.utf8().data(),
                              "relay-type", G_TYPE_STRING, "udp",
                              0);
        GValue relayValue = G_VALUE_INIT;
        g_value_init(&relayValue, GST_TYPE_STRUCTURE);
        gst_value_set_structure(&relayValue, structure);
        g_value_array_append(relayInfo, &relayValue);
        gst_structure_free(structure);

        params[nparams].name = "relay-info";
        g_value_init(&params[nparams].value, G_TYPE_VALUE_ARRAY);
        g_value_take_boxed(&params[nparams].value, relayInfo);
        nparams++;
    }

    params[nparams].name = "controlling-mode";
    g_value_init(&params[nparams].value, G_TYPE_BOOLEAN);
    g_value_set_boolean(&params[nparams].value, initiator);
    nparams++;

    if (m_stream.get()) {
        GOwnPtr<GError> error;
        fs_stream_set_transmitter(m_stream.get(), "nice", params, nparams, &error.outPtr());
        if (error) {
            g_warning("Error setting transmitter: %s", error->message);
            error.clear();
            return false;
        }
        return true;
    }

    return false;
}

Vector<Payload*> FsMediaStreamComponent::offerCodecs() const
{
    GList* codecs = 0;
    g_object_get(m_session.get(), "codecs-without-config", &codecs, NULL);
    Vector<Payload*> retCodecs = fromFsCodecList(codecs, type());
    fs_codec_list_destroy(codecs);
    return retCodecs;
}

Vector<Payload*> FsMediaStreamComponent::responseCodecs() const
{
    GList* codecs = 0;
    g_object_get(m_session.get(), "codecs", &codecs, NULL);
    Vector<Payload*> retCodecs = fromFsCodecList(codecs, type());
    fs_codec_list_destroy(codecs);
    return retCodecs;
}

MediaDescription* FsMediaStreamComponent::createMediaDescription()
{
    MediaDescription* md = new MediaDescription();

    Vector<Candidate*>& candidates = md->candidates();
    Vector<Payload*>& payloads = md->payloads();

    switch (fsMediaStream()->type()) {
    case FsMediaStream::Local:
        md->setDirection(MediaDescription::SendOnly);
        break;
    case FsMediaStream::Remote:
        md->setDirection(MediaDescription::RecvOnly);
        break;
    default:
        g_warning("Unknown FsMediaStream type %d", fsMediaStream()->type());
        break;
    }
    if (isConnectionReady()) {
        payloads = responseCodecs();
        candidates = activeCandidates();
    } else {
        payloads = offerCodecs();
        candidates = localCandidates();
    }

    md->setLabel(fsMediaStream()->label());

    switch (type()) {
    case FS_MEDIA_TYPE_AUDIO:
        md->setMediaType(MediaDescription::Audio);
        break;
    case FS_MEDIA_TYPE_VIDEO:
        md->setMediaType(MediaDescription::Video);
        break;
    default:
        g_warning("Unknown FsMediaStreamComponent type %d", type());
        break;
    }

    md->setMediaTransportType(MediaDescription::RTP_AVP);

    String ip, username, password;
    unsigned int port;
    if (getLocalParams(ip, port, username, password)) {
        md->setConnectionAddress(ip);
        md->setPort(port);
        md->setUsername(username);
        md->setPassword(password);
    }


    // TODO Vector<Crypto*>& cryptos = mediaDescription->cryptos();

    // FIXME: Could add this for each payload as well.
    // TODO mediaDescription->addRtcpProfileSpecificParameter(RtcpProfileSpecificParameter parameter);

    // TODO mediaDescription->setMaximumBandwidth();

    return md;
}

FsMediaStreamComponent* FsMediaStream::addComponent(FsMediaType type)
{
    FsMediaStreamComponent* component = new FsMediaStreamComponent(this, type);
    if (!component) {
        g_warning("Error creating FsMediaStreamComponent");
        return 0;
    }

    LOG(MediaStream, "Adding component %p of type %i to stream %p", component, type, this);

    m_components.append(component);
    return component;
}

void FsMediaStream::onComponentConnectionReady(FsMediaStreamComponent* component)
{
    if (!isConnectionReady())
        return;

    LOG(MediaStream, "%s: %s stream %p is ready", G_STRFUNC, type() == FsMediaStream::Remote ? "Remote" : "Local", this);

    // all components are ready, so notify the client
    m_client->onStreamConnectionReady(this);
}

bool FsMediaStream::isConnectionReady() const
{
    Vector<FsMediaStreamComponent*>::const_iterator iter = components().begin();
    for (; iter != components().end(); ++iter) {
        if (!(*iter)->isConnectionReady())
            return false;
    }
    return true;
}

void FsMediaStream::onComponentReceivedData(FsMediaStreamComponent* component)
{
    if (!hasReceivedData())
        return;

    LOG(MediaStream, "All components have received data, so notify the client");
    m_client->onStreamReceivedData(this);
}

bool FsMediaStream::hasReceivedData() const
{
    Vector<FsMediaStreamComponent*>::const_iterator iter = components().begin();
    for (; iter != components().end(); ++iter) {
        if (!(*iter)->hasReceivedData())
            return false;
    }
    return true;
}

void FsMediaStreamComponent::onInsertedIntoPipeline()
{
    LOG(MediaStream, "%s: %p", G_STRFUNC, this);
    g_object_set(m_valve.get(), "drop", FALSE, NULL);

    m_insertedIntoPipeline = true;
}

bool FsMediaStreamComponent::isInsertedIntoPipeline() const
{
    return m_insertedIntoPipeline;
}

void FsMediaStream::onLocalCandidatesPrepared(FsMediaStreamComponent* component)
{
    if (!localCandidatesPrepared())
        return;

    // local candidates for all components prepared, notify client
    m_client->onLocalCandidatesPrepared(this);
}

bool FsMediaStream::localCandidatesPrepared() const
{
    Vector<FsMediaStreamComponent*>::const_iterator iter;
    for (iter = m_components.begin();iter != m_components.end();iter++) {
        if (!(*iter)->localCandidatesPrepared())
            return false;
    }
    return true;
}

bool FsMediaStream::componentSourceInserted(FsMediaStreamComponent* component)
{
    component->onInsertedIntoPipeline();

    return allComponentsInserted();
}

bool FsMediaStream::allComponentsInserted() const
{
    Vector<FsMediaStreamComponent*>::const_iterator iter;
    for (iter = m_components.begin();iter != m_components.end();iter++) {
        if (!(*iter)->isInsertedIntoPipeline())
            return false;
    }
    return true;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
