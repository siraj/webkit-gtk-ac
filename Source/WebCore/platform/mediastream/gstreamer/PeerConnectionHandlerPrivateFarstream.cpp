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

#include "PeerConnectionHandlerPrivateFarstream.h"

#include "../gstreamer/sdp/Candidate.h"
#include "../gstreamer/sdp/MediaDescription.h"
#include "../gstreamer/sdp/SignalingMessage.h"
#include "CentralPipelineUnit.h"
// #include "DeprecatedPeerConnectionHandlerClient.h"
#include "FsMediaStream.h"
#include "Logging.h"
#include "SecurityOrigin.h"

#include <farstream/fs-candidate.h>
#include <farstream/fs-conference.h>
#include <farstream/fs-enumtypes.h>
#include <farstream/fs-session.h>
#include <farstream/fs-stream.h>
#include <farstream/fs-utils.h>
#include <gst/gst.h>

#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/Vector.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>


namespace WebCore {

static bool gstInitialized = false;
static GQuark quarkLabel = 0;

static bool doGstInit()
{
    // FIXME: We should pass the arguments from the command line
    if (!gstInitialized) {
        GOwnPtr<GError> error;
        gstInitialized = gst_init_check(0, 0, &error.outPtr());
        if (!gstInitialized)
            g_warning("Could not initialize GStreamer: %s", error ? error->message : "unknown error occurred");
        if (!quarkLabel)
            quarkLabel = g_quark_from_string("label");
    }
    return gstInitialized;
}

static GQuark quarkComponent()
{
    static GQuark q = 0;
    if (G_UNLIKELY(!q))
        q = g_quark_from_string("component");

    return q;
}

static void padBlockedHelper(GstPad *, gboolean blocked, gpointer user_data)
{

}

void PeerConnectionHandlerPrivateFarstream::onStreamReceivedData(FsMediaStream* stream)
{
    LOG(MediaStream, "%s", G_STRFUNC);

    if (stream->type() == FsMediaStream::Remote) {
        const Vector<FsMediaStreamComponent*>& fsComponents = stream->components();
        MediaStreamSourceVector audioSource;
        MediaStreamSourceVector videoSource;
        Vector<FsMediaStreamComponent*>::const_iterator componentIt = fsComponents.begin();
        for (; componentIt != fsComponents.end(); ++componentIt) {
            FsMediaStreamComponent* component = *componentIt;

            if (component->type() == FS_MEDIA_TYPE_AUDIO)
                audioSource.append(MediaStreamSource::create(component->sourceId(), MediaStreamSource::TypeAudio, "Remote audio"));
            else // if (component->type() == video)
                videoSource.append(MediaStreamSource::create(component->sourceId(), MediaStreamSource::TypeVideo, "Remote video"));
        }
        RefPtr<MediaStreamDescriptor> streamDescriptor = MediaStreamDescriptor::create(stream->label(), audioSource, videoSource);
        stream->setStreamDescriptor(streamDescriptor);
        m_client->didAddRemoteStream(streamDescriptor);
    }
}

void PeerConnectionHandlerPrivateFarstream::onStreamConnectionReady(FsMediaStream* stream)
{
    // if all streams are now ready, signal that ICE negotiation is complete
    Vector<FsMediaStream*>::iterator sIter = m_sessionStreams.begin();
    for (; sIter != m_sessionStreams.end(); ++sIter) {
        if (!(*sIter)->isConnectionReady())
            return;
    }

    changeIceState(ICE_STATE_COMPLETED);
}

void PeerConnectionHandlerPrivateFarstream::onLocalCandidatesPrepared(FsMediaStream* stream)
{
    if (m_sessionStreams.contains(stream)) {
        // we shouldn't ever be gathering candidates for a stream in
        // m_mediaStreamMap unless we need to send a response to a remote offer
        g_warn_if_fail(m_sdpState == SDP_STATE_PENDING_ANSWER);

        // only send the SDP when all streams are ready
        Vector<FsMediaStream*>::iterator iter;
        bool ready = false;
        for (iter = m_sessionStreams.begin(); iter != m_sessionStreams.end(); ++iter) {
            FsMediaStream* s = *iter;
            if (!(ready = s->localCandidatesPrepared()))
                break;
        }
        if (ready)
            sendMessage();
    } else {
        if (m_streamsToAdd.contains(stream)) {
            // check if the pending add streams have candidates. If all are ready, we
            // add them to our o
            bool ready = false;
            Vector<FsMediaStream*>::iterator iter;
            for (iter = m_streamsToAdd.begin(); iter != m_streamsToAdd.end(); ++iter) {
                FsMediaStream* s = *iter;
                if (!(ready = s->localCandidatesPrepared()))
                    break;
            }
            if (ready)
                scheduleOffer();
        }
    }
}

SignalingMessage* PeerConnectionHandlerPrivateFarstream::createSignalingMessageOffer()
{
    // increment the session version each time a new offer is generated, but
    // keep the same session ID
    SignalingMessage* message = new SignalingMessage(m_sessionId, m_sessionVersion++);
    if (!message)
        return 0;

    Vector<MediaDescription*> & md = message->mediaDescriptions();

    Vector<FsMediaStream*>::iterator sIter;
    for (sIter = m_sessionStreams.begin(); sIter != m_sessionStreams.end(); ++sIter) {
        FsMediaStream* stream = *sIter;
        if (!stream)
            continue;

        Vector<FsMediaStreamComponent*>::const_iterator componentIter = stream->components().begin();
        for (; componentIter != stream->components().end(); componentIter++) {
            FsMediaStreamComponent* component = *componentIter;
            if (!component)
                continue;

            MediaDescription* mediaDescription = component->createMediaDescription();

            md.append(mediaDescription);
        }
    }

    return message;
}

PassOwnPtr<PeerConnectionHandlerPrivateFarstream> PeerConnectionHandlerPrivateFarstream::create(DeprecatedPeerConnectionHandlerClient* client, const String& serverConfiguration, const String& username)
{
    return adoptPtr(new PeerConnectionHandlerPrivateFarstream(client, serverConfiguration, username));
}

// FIXME: remove when real implementations are available
// Empty implementations for ports that build with MEDIA_STREAM enabled by default.
PeerConnectionHandlerPrivateFarstream::PeerConnectionHandlerPrivateFarstream(DeprecatedPeerConnectionHandlerClient* client, const String& serverConfiguration, const String& username)
    : m_client(client)
    , m_initiator(false)
    , m_sessionId(g_get_real_time())
    , m_sessionVersion(m_sessionId)
    , m_iceState(ICE_STATE_NEW)
    , m_sdpState(SDP_STATE_NEW)
    , m_needResendOffer(false)
{
    if (!doGstInit())
        return;

    m_configuration = PeerConnectionHandlerConfiguration::create(serverConfiguration, username);

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

    // don't adopt the ref but take another ref, since adding it to the
    // pipeline will transfer ownership of the initial reference
    m_conference = FS_CONFERENCE(gst_element_factory_make("fsrtpconference", 0));
    if (!m_conference) {
        g_warning("Error creating fsrtpconference");
        return;
    }
    gst_bin_add(GST_BIN(m_pipeline.get()), GST_ELEMENT(m_conference.get()));

    GOwnPtr<GError> error;
    m_participant = adoptGRef(fs_conference_new_participant(m_conference.get(), &error.outPtr()));
    if (error) {
        g_warning("Error creating participant: %s", error->message);
        error.clear();
        return;
    }
}

PeerConnectionHandlerPrivateFarstream::~PeerConnectionHandlerPrivateFarstream()
{
    LOG(MediaStream, "%s", G_STRFUNC);

    gst_bus_remove_signal_watch(m_bus);
    gst_object_unref(m_bus);
}

void PeerConnectionHandlerPrivateFarstream::produceInitialOffer(const MediaStreamDescriptorVector& streams)
{
    m_initiator = true;
    // we're idle until we've gathered candidates and sent out the first offer
    changeSDPState(SDP_STATE_IDLE);
    LOG(MediaStream, "%s: %s %s:%u", G_STRFUNC,
        m_configuration->type == PeerConnectionHandlerConfiguration::TypeSTUN ? "STUN" : "TURN",
        m_configuration->host.utf8().data(), m_configuration->port);

    addStreams(streams);
    gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
}

void PeerConnectionHandlerPrivateFarstream::addStreams(const MediaStreamDescriptorVector& streams)
{
    LOG(MediaStream, "%s: %s %s:%u", G_STRFUNC,
        m_configuration->type == PeerConnectionHandlerConfiguration::TypeSTUN ? "STUN" : "TURN",
        m_configuration->host.utf8().data(), m_configuration->port);
    LOG(MediaStream, "%s: adding %zu streams", G_STRFUNC, streams.size());

    for (MediaStreamDescriptorVector::const_iterator iter = streams.begin(); iter != streams.end(); ++iter) {
        FsMediaStream* stream = new FsMediaStream(this, m_conference.get(), m_participant.get(), FsMediaStream::Local, (*iter)->label());
        if (stream) {
            stream->setStreamDescriptor(*iter);
            m_streamsToAdd.append(stream);

            for (unsigned i = 0; i < (*iter)->numberOfAudioComponents(); ++i) {
                MediaStreamSource* source = (*iter)->audioComponent(i)->source();
                FsMediaType mediaType = FS_MEDIA_TYPE_AUDIO;

                switch (source->type()) {
                case MediaStreamSource::TypeAudio:
                    mediaType = FS_MEDIA_TYPE_AUDIO;
                    break;
                case MediaStreamSource::TypeVideo:
                    mediaType = FS_MEDIA_TYPE_VIDEO;
                    break;
                default:
                    g_warning("Unknown source type %d", source->type());
                    break;
                }

                FsMediaStreamComponent* component = stream->addComponent(mediaType);
                if (!component) {
                    g_warning("Error creating FsMediaStreamComponent");
                    continue;
                }

                component->setSourceId(source->id());
                component->setServerConfiguration(m_configuration.get(), m_initiator);
            }

            for (unsigned i = 0; i < (*iter)->numberOfVideoComponents(); ++i) {
                MediaStreamSource* source = (*iter)->videoComponent(i)->source();
                FsMediaType mediaType = FS_MEDIA_TYPE_AUDIO;

                switch (source->type()) {
                case MediaStreamSource::TypeAudio:
                    mediaType = FS_MEDIA_TYPE_AUDIO;
                    break;
                case MediaStreamSource::TypeVideo:
                    mediaType = FS_MEDIA_TYPE_VIDEO;
                    break;
                default:
                    g_warning("Unknown source type %d", source->type());
                    break;
                }

                FsMediaStreamComponent* component = stream->addComponent(mediaType);
                if (!component) {
                    g_warning("Error creating FsMediaStreamComponent");
                    continue;
                }

                component->setSourceId(source->id());
                component->setServerConfiguration(m_configuration.get(), m_initiator);
            }
        }
    }
}

void PeerConnectionHandlerPrivateFarstream::shutdownAndRemoveMediaStream(FsMediaStream* fsMediaStream, bool fromClose)
{
    ASSERT(isMainThread());
    LOG(MediaStream, "%s", G_STRFUNC);
    // Go through all components and remove the sources / sinks from central pipeline unit
    LOG(MediaStream, "shutdownAndRemoveMediaStream called for stream with type: %d, label: %s", fsMediaStream->type(), fsMediaStream->label().ascii().data());

    if (fsMediaStream->type() == FsMediaStream::Remote && fsMediaStream->streamDescriptor())
        m_client->didRemoveRemoteStream(fsMediaStream->streamDescriptor());

    CentralPipelineUnit& cpu = centralPipelineUnit();
    const Vector<FsMediaStreamComponent*> &fsComponentVector = fsMediaStream->components();
    Vector<FsMediaStreamComponent*>::const_iterator fsComponentIt = fsComponentVector.begin();
    for (; fsComponentIt != fsComponentVector.end(); fsComponentIt++) {
        FsMediaStreamComponent* fsComponent = *fsComponentIt;

        if (fsMediaStream->type() == FsMediaStream::Local) {
            LOG(MediaStream, "removing sink for sourceId %s", fsComponent->sourceId().ascii().data());
            cpu.disconnectFromSource(fsComponent->sourceId(), fsComponent->sinkElement(), fsComponent->sinkPad());
        } else if (fsMediaStream->type() == FsMediaStream::Remote) {
            LOG(MediaStream, "removing source");
            cpu.removeSource(fsComponent->sourceId(), fromClose);
        }

        // TODO: unref components
        // fs_stream_destroy()??
    }
}

void PeerConnectionHandlerPrivateFarstream::addMediaDescriptionToSignalingMessage(SignalingMessage* signalingMessage, FsMediaStream* fsMediaStream)
{
    LOG(MediaStream, "%s", G_STRFUNC);
    LOG(MediaStream, "PeerConnection - Construct SignalingMessage");

    const Vector<FsMediaStreamComponent*>& fsComponents = fsMediaStream->components();
    Vector<MediaDescription*>& mediaDescriptions = signalingMessage->mediaDescriptions();

    Vector<FsMediaStreamComponent*>::const_iterator componentIt = fsComponents.begin();
    for (; componentIt != fsComponents.end(); componentIt++) {
        FsMediaStreamComponent* component = *componentIt;

        MediaDescription* md = component->createMediaDescription();
        mediaDescriptions.append(md);
    }
}


void PeerConnectionHandlerPrivateFarstream::removeRequestedStreams()
{
    LOG(MediaStream, "%s", G_STRFUNC);
    ASSERT(isMainThread());

    LOG(MediaStream, "m_streamsToRemove.size = %d", (int)m_streamsToRemove.size());
    Vector<FsMediaStream*>::iterator sIt = m_streamsToRemove.begin();
    for (; sIt != m_streamsToRemove.end(); sIt++) {
        LOG(MediaStream, "removing a media");
        FsMediaStream* fsMediaStream = *sIt;

        shutdownAndRemoveMediaStream(fsMediaStream, false);
    }
    m_streamsToRemove.clear();
}

void PeerConnectionHandlerPrivateFarstream::removeStreams(const MediaStreamDescriptorVector& streams)
{
    LOG(MediaStream, "%s: removing %zu streams", G_STRFUNC, streams.size());
    for (size_t i = 0; i < streams.size(); i++) {
        String label = streams[i]->label();
        if (!m_mediaStreamMap.contains(label)) {
            // FIXME, what do do?
            continue;
        }
        FsMediaStream* fsMediaStream = m_mediaStreamMap.get(label);
        fsMediaStream->setClosed();
        m_streamsToRemove.append(fsMediaStream);

    }

    if (!streams.isEmpty())
        scheduleOffer();
}

void PeerConnectionHandlerPrivateFarstream::handleInitialOffer(const String& sdp)
{
    LOG(MediaStream, "%s:\n", G_STRFUNC);
    g_warn_if_fail(!m_initiator);
    processSDP(sdp);
    gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
}

void PeerConnectionHandlerPrivateFarstream::processSDP(const String& sdp)
{
    LOG(MediaStream, "%s:\n%s", G_STRFUNC, sdp.utf8().data());

    switch (m_sdpState) {
    case SDP_STATE_NEW:
    case SDP_STATE_IDLE:
        changeSDPState(SDP_STATE_PENDING_ANSWER);
        break;
    case SDP_STATE_WAITING:
        changeSDPState(SDP_STATE_IDLE);
        break;
    default:
        g_warn_if_reached();
    }

    OwnPtr<SignalingMessage> signalingMessage = adoptPtr(createSignalingMessage(sdp));
    if (!signalingMessage)
        return;

    Vector<String> labels = signalingMessage->labels();
    for (size_t i = 0; i < labels.size(); i++) {
        Vector<MediaDescription*> mds = signalingMessage->mediaDescriptions(labels[i]);
        if (!m_mediaStreamMap.contains(labels[i])) {
            if (!(mds[0]->port())) {
                // invalid sdp or response to remove stream
                // IGNORE!
            } else {
                // Unknown label, must be a initial offer
                processStreamMediaDescriptionOffer(mds);
            }
        } else {
            processStreamMediaDescriptionAnswer(mds);
    // TODO
#if 0
            } else if (localGstMediaStream->m_negotiationState == GstMediaStream::UpdateSent) {
                processStreamMediaDescriptionUpdateAnswer(mds);
            } else if (localGstMediaStream->m_negotiationState == GstMediaStream::Idle)
                processStreamMediaDescriptionUpdate(mds);
#endif
        }
    }
}

void PeerConnectionHandlerPrivateFarstream::processStreamMediaDescriptionOffer(Vector<MediaDescription*>& mds)
{
    LOG(MediaStream, "Processing SDP offer");

    String label = mds[0]->label();
    // Check if remote stream already exists. If so, abort.
    if (m_mediaStreamMap.contains(label)) {
        LOG(MediaStream, "Offered media already exists");
        return;
    }
    FsMediaStream* fsMediaStream = new FsMediaStream(this, m_conference.get(), m_participant.get(), FsMediaStream::Remote, label);
    // fsMediaStream->m_gatheringState = GstMediaStream::GatheringAnswer;
    LOG(MediaStream, "Added new remote stream %p (%s)", fsMediaStream, label.utf8().data());
    m_mediaStreamMap.add(fsMediaStream->label(), fsMediaStream);
    m_sessionStreams.append(fsMediaStream);

    for (size_t i = 0; i < mds.size(); i++) {
        MediaDescription* md = mds[i];

        FsMediaStreamComponent* fsComponent = 0;
        if (md->mediaType() == MediaDescription::Audio)
            fsComponent = fsMediaStream->addComponent(FS_MEDIA_TYPE_AUDIO);
        else if (md->mediaType() == MediaDescription::Video)
            fsComponent = fsMediaStream->addComponent(FS_MEDIA_TYPE_VIDEO);
        else {
            g_warning("unkown media type");
            continue;
        }

        // FIXME: refactor to do these things in the addComponent() call so we
        // don't need to remember to call them every time?
        fsComponent->setServerConfiguration(m_configuration.get(), m_initiator);

#if 0
        // gstRtpStream->m_defaultRemotePort = md->port();
        // gstRtpStream->m_defaultRemoteAddress = md->connectionAddress();
        // gstRtpStream->setMaxBandwidth(md->maximumBandwidth());

        // FIXME: figure out what to do with this crypto stuff...
        if (m_useEncryption) {
            Vector<Crypto*>& cryptos = md->cryptos();
            if (cryptos.size() == 1) {
                Crypto* crypto = cryptos[0];
                String masterKey = crypto->masterKey();
                Vector<char> byteVec;
                bool decoded = base64Decode(masterKey, byteVec);
                if (!decoded)
                    LOG(MediaStream, "Failed to decode crypto key");
                else if (byteVec.size() != 30)
                    LOG(MediaStream, "Invalid key length: %u", byteVec.size());
                else {
                    unsigned char key[SRTP_MASTERKEY_LEN];
                    unsigned char salt[SRTP_MASTERSALT_LEN];
                    for (int i = 0; i < SRTP_MASTERKEY_LEN; i++)
                        key[i] = (unsigned char) byteVec[i];
                    for (int i = 0; i < SRTP_MASTERSALT_LEN; i++)
                        salt[i] = (unsigned char) byteVec[i + SRTP_MASTERKEY_LEN];
                    gstRtpStream->setRemoteCryptoContext(key, SRTP_MASTERKEY_LEN, salt, SRTP_MASTERSALT_LEN, 0, 0);
                }

            } else
                LOG(MediaStream, "%u keys cryptos found in media descriptor. Only 1 supported", cryptos.size());

            // Create local key
            unsigned char* key = 0;
            unsigned int keyLength = 0;
            unsigned char* salt = 0;
            unsigned int saltLength = 0;
            unsigned char* mki = 0;
            unsigned int mkiLength = 0;
            generateCryptoContext(&key, &keyLength, &salt, &saltLength, &mki, &mkiLength);
            gstRtpStream->setLocalCryptoContext(key, keyLength, salt, saltLength, mki, mkiLength);
        }
#endif

        fsComponent->setRemoteCodecs(md->payloads());
        fsComponent->addRemoteCandidates(md->candidates(), md->username(), md->password());

        // FIXME: determine whether we need these maps and what we would use for
        // the streamId
        // m_streamIdFsComponentMap.add(streamId, fsComponent);
        // LOG(MediaStream, "FsMediaStreamComponenet added for streamId %u", streamId);

        // m_rtpSessionGstRtpStreamMap.add(gstRtpStream->rtpSessionId(), gstRtpStream);
    }
}

void PeerConnectionHandlerPrivateFarstream::processStreamMediaDescriptionAnswer(Vector<MediaDescription*>& mds)
{
    String label = mds[0]->label();
    if (!m_mediaStreamMap.contains(label)) {
        LOG(MediaStream, "SDP RESPONSE INVALID! Label %s not in map.", label.ascii().data());
        return;
    }

    FsMediaStream* fsMediaStream = m_mediaStreamMap.get(label);

    // if we've closed this stream locally, don't process an answer for it.
    if (fsMediaStream->closed())
        return;

    /* FIXME: is this necessary?
    if (gstMediaStream->m_negotiationState == GstMediaStream::Idle) {
        LOG(MediaStream, "Received answer twice. Ignoring last.\n");
        return;
    }
    */

    // FIXME: What if only one port is 0???
    if (!(mds[0]->port())) {
        LOG(MediaStream, "Offered media rejected");
        fsMediaStream->setClosed();
        shutdownAndRemoveMediaStream(fsMediaStream, false);
        return;
    }

    if (mds.size() != fsMediaStream->components().size()) {
        LOG(MediaStream, "Number of components mismatch!");
        return;
    }

    for (size_t i = 0; i < mds.size(); i++) {
        MediaDescription* md = mds[i];
        FsMediaStreamComponent* fsComponent = fsMediaStream->components()[i];

        // fsComponent->setMaxBandwidth(mds[i]->maximumBandwidth());

        fsComponent->setRemoteCodecs(md->payloads());
        fsComponent->addRemoteCandidates(md->candidates(), md->username(), md->password());

#if 0
        if (m_useEncryption) {
            Vector<Crypto*>& cryptos = md->cryptos();
            if (cryptos.size() == 1) {
                Crypto* crypto = cryptos[0];
                String masterKey = crypto->masterKey();
                Vector<char> byteVec;
                bool decoded = base64Decode(masterKey, byteVec);
                if (!decoded)
                    LOG(MediaStream, "Failed to decode crypto key");
                else if (byteVec.size() != 30)
                    LOG(MediaStream, "Invalid key length: %u", byteVec.size());
                else {
                    unsigned char key[SRTP_MASTERKEY_LEN];
                    unsigned char salt[SRTP_MASTERSALT_LEN];
                    for (int i = 0; i < SRTP_MASTERKEY_LEN; i++)
                        key[i] = (unsigned char) byteVec[i];
                    for (int i = 0; i < SRTP_MASTERSALT_LEN; i++)
                        salt[i] = (unsigned char) byteVec[i + SRTP_MASTERKEY_LEN];
                    gstRtpStream->setRemoteCryptoContext(key, SRTP_MASTERKEY_LEN, salt, SRTP_MASTERSALT_LEN, 0, 0);
                }

            } else
                LOG(MediaStream, "%u keys cryptos found in media descriptor. Only 1 supported", cryptos.size());

        }
#endif
    }
}

void PeerConnectionHandlerPrivateFarstream::processPendingStreams(const MediaStreamDescriptorVector& pendingAddStreams, const MediaStreamDescriptorVector& pendingRemoveStreams)
{
    LOG(MediaStream, "%s: Adding %zu streams, removing %zu streams", G_STRFUNC,
            pendingAddStreams.size(), pendingRemoveStreams.size());
    addStreams(pendingAddStreams);
    removeStreams(pendingRemoveStreams);
}

void PeerConnectionHandlerPrivateFarstream::sendDataStreamMessage(const char*, size_t)
{
    LOG(MediaStream, "%s", G_STRFUNC);
}

void PeerConnectionHandlerPrivateFarstream::stop()
{
    LOG(MediaStream, "%s", G_STRFUNC);
    ASSERT(isMainThread());

    // CentralPipelineUnit& cpu = centralPipelineUnit();
    Vector<FsMediaStream*>::iterator streamIt = m_sessionStreams.begin();
    for (; streamIt != m_sessionStreams.end(); ++streamIt) {
        FsMediaStream* fsMediaStream = *streamIt;

        if (fsMediaStream->type() != FsMediaStream::Local && fsMediaStream->type() != FsMediaStream::Remote)
            continue;

        LOG(MediaStream, "Removing stream with label: %s, type: %d", fsMediaStream->label().ascii().data(), fsMediaStream->type());
        fsMediaStream->setClosed();
        shutdownAndRemoveMediaStream(fsMediaStream, true);

        // FIXME: m_sessionStreams should probably hold a ref to the stream so
        // that it will be freed automatically when we remove it from the stream
        // map.
        if (fsMediaStream->type() == FsMediaStream::Remote)
            delete fsMediaStream;
    }
}

typedef struct {
    GstElement* source;
    GstPad* pad;
    gpointer data;
} FsSourceInsertedParameters;

static void callFsSourceInsertedMT(void* pSourceInsertedParameters)
{
    ASSERT(isMainThread());
    FsSourceInsertedParameters* param = (FsSourceInsertedParameters*)pSourceInsertedParameters;
    PeerConnectionHandlerPrivateFarstream* self =
        reinterpret_cast<PeerConnectionHandlerPrivateFarstream*>(param->data);
    if (self)
        self->sourceInserted(param->source, param->pad);
    delete param;
}

static void onFsSourceInsertedHelper(GstElement* source, GstPad* pad, gpointer userData)
{
    LOG(MediaStream, "%s: %s:%s", G_STRFUNC, GST_OBJECT_NAME(source), GST_OBJECT_NAME(pad));
    FsMediaStreamComponent* component = reinterpret_cast<FsMediaStreamComponent*>(userData);
    component->onInsertedIntoPipeline();
    /*
    FsSourceInsertedParameters* param = new FsSourceInsertedParameters();
    param->source = source;
    param->pad = pad;
    param->data = userData;
    callOnMainThread(callFsSourceInsertedMT, (void*)param);
    */
}

void PeerConnectionHandlerPrivateFarstream::onSrcPadAdded(FsMediaStreamComponent* component, GstPad* pad)
{
    LOG(MediaStream, "%s", G_STRFUNC);
    ASSERT(isMainThread());

    GRefPtr<GstElement> element = adoptGRef(gst_pad_get_parent_element(pad));
    GOwnPtr<gchar> elementname(gst_element_get_name(element.get()));
    GOwnPtr<gchar> padname(gst_pad_get_name(pad));
    LOG(MediaStream, "%s: element name = %s, padname = %s", G_STRFUNC, elementname.get(), padname.get());

    if (component) {
        // FIXME: how to handle this with the new FS types?
        // gstRtpStream->m_rtpRecvSrcGhostPadUnlinkedSignalHandler = m_callbackProxy->connectPadUnlinkedSignal(ghostSrcPad);

        g_object_set_qdata(G_OBJECT(pad), quarkComponent(), component);
        centralPipelineUnit().insertSource(element.get(), pad,
                                           component->sourceId(),
                                           onFsSourceInsertedHelper, component);
    }
}

void PeerConnectionHandlerPrivateFarstream::codecsNeedResend(FsMediaStreamComponent* component)
{
    LOG(MediaStream, "%s", G_STRFUNC);
    scheduleOffer();
}

void PeerConnectionHandlerPrivateFarstream::sourceInserted(GstElement* source, GstPad* pad)
{
}

void PeerConnectionHandlerPrivateFarstream::changeIceState(IceState newState)
{
    LOG(MediaStream, "%s", G_STRFUNC);
    if (m_iceState == newState)
        return;

    LOG(MediaStream, "Changing ICE state from %i to %i", m_iceState, newState);

    m_iceState = newState;

    // trigger actions as necessary
    switch (newState) {
    case ICE_STATE_COMPLETED:
        m_client->didCompleteICEProcessing();
        break;
    default:
        break;
    }
}

void PeerConnectionHandlerPrivateFarstream::changeSDPState(SDPState newState)
{
    LOG(MediaStream, "%s", G_STRFUNC);
    if (m_sdpState == newState)
        return;

    LOG(MediaStream, "Changing SDP state from %i to %i", m_sdpState, newState);

    m_sdpState = newState;

    if (newState == SDP_STATE_IDLE) {
        if (m_needResendOffer) {
            Vector<FsMediaStream*>::iterator it;
            for (it = m_streamsToAdd.begin(); it != m_streamsToAdd.end(); ++it) {
                FsMediaStream* stream = *it;
                m_mediaStreamMap.set(stream->label(), stream);
                m_sessionStreams.append(stream);
                g_warn_if_fail(stream->localCandidatesPrepared());
            }
            sendMessage();
            m_needResendOffer = false;
        }
    }
}

void PeerConnectionHandlerPrivateFarstream::sendMessage()
{
    LOG(MediaStream, "%s", G_STRFUNC);
    g_warn_if_fail(m_sdpState != SDP_STATE_WAITING);

    OwnPtr<SignalingMessage> message = adoptPtr(createSignalingMessageOffer());

    String sdp = formatSdp(message.get());
    LOG(MediaStream, "%s:\n%s", G_STRFUNC, sdp.utf8().data());
    m_client->didGenerateSDP(sdp);

    switch (m_sdpState) {
    case SDP_STATE_NEW:
    case SDP_STATE_IDLE:
        changeSDPState(SDP_STATE_WAITING);
        break;
    case SDP_STATE_PENDING_ANSWER:
        changeSDPState(SDP_STATE_IDLE);
        break;
    default:
        g_warn_if_reached();
    }
}

static bool streamIsNotReady(FsMediaStream* stream)
{
    return !stream->localCandidatesPrepared();
}

void PeerConnectionHandlerPrivateFarstream::scheduleOffer()
{
    LOG(MediaStream, "%s", G_STRFUNC);
    switch (m_sdpState) {
    case SDP_STATE_NEW:
    case SDP_STATE_IDLE:
        {
            bool changed = false;
            if (std::find_if(m_streamsToAdd.begin(), m_streamsToAdd.end(),
                             streamIsNotReady) == m_streamsToAdd.end()) {
                changed = true;
                Vector<FsMediaStream*>::iterator it;
                for (it = m_streamsToAdd.begin(); it != m_streamsToAdd.end(); ++it) {
                    FsMediaStream* stream = *it;
                    m_mediaStreamMap.set(stream->label(), stream);
                    m_sessionStreams.append(stream);
                }
            }
            if (!m_streamsToRemove.isEmpty()) {
                changed = true;
                removeRequestedStreams();
            }
            if (changed)
                sendMessage();
        }
        break;
    default:
        m_needResendOffer = true;
        break;
    }
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
