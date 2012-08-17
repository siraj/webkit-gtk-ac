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

#include "MediaDescription.h"

#include "Logging.h"
#include <glib.h>
#include <wtf/text/CString.h>


namespace WebCore {

const char* MediaDescription::RtcpProfileSpecificParameterString[3] = {"ack", "nack", "ccm fir"};


void MediaDescription::addRtcpProfileSpecificParameter(RtcpProfileSpecificParameter parameter)
{
    Vector<RtcpProfileSpecificParameter>::iterator paramIt = m_rtcpProfileSpecificParameters.begin();
    while (paramIt != m_rtcpProfileSpecificParameters.end()) {
        if (*paramIt == parameter)
            return;
        paramIt++;
    }
    m_rtcpProfileSpecificParameters.append(parameter);
}

MediaDescription* createMediaDescription(const String& mLine)
{
    if (mLine[0] != 'm' || mLine[1] != '=') {
        LOG(MediaStream, "Not a valid m line: %s\n", mLine.ascii().data());
        return 0;
    }

    String mSuffix = mLine.substring(2);
    Vector<String> tokens;
    mSuffix.split(" ", tokens);

    if (tokens.size() < 3) {
        LOG(MediaStream, "Invalid number of tokens in line: %s\n", mLine.ascii().data());
        return 0;
    }

    Vector<String>::iterator tokenIt = tokens.begin();

    MediaDescription* md = new MediaDescription();

    String mediaType = *tokenIt++;
    if (mediaType == "text")
        md->setMediaType(MediaDescription::Text);
    else if (mediaType == "audio")
        md->setMediaType(MediaDescription::Audio);
    else if (mediaType == "video")
        md->setMediaType(MediaDescription::Video);
    else {
        LOG(MediaStream, "Invalid media type (%s) on line: %s\n", mediaType.ascii().data(), mLine.ascii().data());
        delete md;
        return 0;
    }

    bool succeeded = false;
    String portStr = *tokenIt++;
    unsigned int port = portStr.toUInt(&succeeded);
    if (!succeeded || port > 0xFFFF) {
        LOG(MediaStream, "Found invalid port (%s) on line: %s\n", portStr.ascii().data(), mLine.ascii().data());
        delete md;
        return 0;
    }
    md->setPort(port);

    String& transportType = *tokenIt++;

    if (transportType == "udp")
        md->setMediaTransportType(MediaDescription::UDP);
    else if (transportType == "RTP/AVP")
        md->setMediaTransportType(MediaDescription::RTP_AVP);
    else if (transportType == "RTP/AVPF")
        md->setMediaTransportType(MediaDescription::RTP_AVPF);
    else {
        LOG(MediaStream, "Invalid transport type (%s) on line: %s\n", transportType.ascii().data(), mLine.ascii().data());
        delete md;
        return 0;
    }

    if (port && (md->mediaTransportType() == MediaDescription::RTP_AVP || md->mediaTransportType() == MediaDescription::RTP_AVPF)) {
        if (tokens.size() < 4) {
            delete md;
            return 0;
        }
        String& pltn = *tokenIt++;
        bool isPayloadType = false;
        pltn.toUInt(&isPayloadType);
        if (port && !isPayloadType) {
            LOG(MediaStream, "Expected payload type number, found %s on line %s\n", pltn.ascii().data(), mLine.ascii().data());
            delete md;
            return 0;
        }
    }

    return md;
}


bool addAttribute(MediaDescription* mediaDescription, const String& aLine)
{
    LOG(MediaStream, "line = %s\n", aLine.ascii().data());
    if (aLine[0] != 'a' || aLine[1] != '=') {
        LOG(MediaStream, "Not a valid a line: %s\n", aLine.ascii().data());
        return false;
    }

    String aSuffix = aLine.substring(2);
    Vector<String> tokens;
    aSuffix.split(":", tokens);
    String& attribute = tokens[0];

    if (attribute == "label") {
        LOG(MediaStream, "Label found on line: %s\n", aLine.ascii().data());
        if (tokens.size() != 2) {
            LOG(MediaStream, "Failed to parse label attribute\n");
            return false;
        }
        String& label = tokens[1];
        mediaDescription->setLabel(label);
    } else if (attribute == "rtpmap") {
        Payload* payload = createPayload(aLine);
        if (payload)
            mediaDescription->payloads().append(payload);
        else {
            LOG(MediaStream, "Failed parsing payload line: %s\n", aLine.ascii().data());
            return false;
        }
    } else if (attribute == "fmtp") {
        String fmtpSuffix = aLine.substring(7);
        int firstSpaceIndex = fmtpSuffix.find(" ");
        String format = fmtpSuffix.left(firstSpaceIndex);
        String formatSpecificParameters = fmtpSuffix.substring(firstSpaceIndex+1);
        bool wasInt = false;
        unsigned int payloadTypeNumber = format.toUInt(&wasInt);
        if (!wasInt) {
            LOG(MediaStream, "Failed parsing fmtp line: %s\n", aLine.ascii().data());
            return false;
        }

        Vector<Payload*>& payloads = mediaDescription->payloads();
        Vector<Payload*>::iterator plIt = payloads.begin();
        while (plIt != payloads.end()) {
            Payload* payload = *plIt;
            plIt++;
            if (payload->payloadTypeNumber() == payloadTypeNumber) {
                payload->setFormat(formatSpecificParameters);
                break;
            }
        }


    } else if (attribute == "candidate") {
        LOG(MediaStream, "Found candidate attribute");
        Candidate* candidate = createCandidate(aLine);
        if (candidate)
            mediaDescription->candidates().append(candidate);
        else {
            LOG(MediaStream, "Failed parsing candidate line: %s\n", aLine.ascii().data());
            return false;
        }
    } else if (attribute == "crypto") {
        LOG(MediaStream, "Found crypto attribute");
        Crypto* crypto = createCrypto(aLine);
        if (crypto) {
            mediaDescription->cryptos().append(crypto);
            LOG(MediaStream, "master key=%s\n", crypto->masterKey().ascii().data());
        } else {
            LOG(MediaStream, "Failed parsing crypto line: %s\n", aLine.ascii().data());
            return false;
        }
    } else if (attribute == "ice-ufrag") {
        if (tokens.size() > 1) {
            String username = tokens[1];
            mediaDescription->setUsername(username);
        }
    } else if (attribute == "ice-pwd") {
        if (tokens.size() > 1) {
            String password = tokens[1];
            mediaDescription->setPassword(password);
        }
    } else if (attribute == "sendonly") {
        mediaDescription->setDirection(MediaDescription::SendOnly);
    } else if (attribute == "recvonly") {
        mediaDescription->setDirection(MediaDescription::RecvOnly);
    } else if (attribute == "sendrecv") {
        mediaDescription->setDirection(MediaDescription::SendRecv);
    } else if (attribute == "rtcp-fb") {
        LOG(MediaStream, "rtcp-fb NOT IMPLEMENTED.. Has not been needed.");
        // FIXME: Should store in the vector
    } else if (attribute == "framesize") {
        LOG(MediaStream, "Found framesize attribute");
        String framesizeSuffix = aLine.substring(12);
        int firstSpaceIndex = framesizeSuffix.find(" ");
        if (firstSpaceIndex == -1)
            return false;
        String pltn = framesizeSuffix.left(firstSpaceIndex);
        String sizes = framesizeSuffix.substring(firstSpaceIndex+1);
        bool wasInt = false;
        unsigned int payloadTypeNumber = pltn.toUInt(&wasInt);
        if (!wasInt)
            return false;
        int firstDashIndex = sizes.find("-");
        if (firstDashIndex == -1)
            return false;
        String widthStr = sizes.left(firstDashIndex);
        String heightStr = sizes.substring(firstDashIndex+1);
        unsigned int width = widthStr.toUInt(&wasInt);
        if (!wasInt)
            return false;
        unsigned int height = heightStr.toUInt(&wasInt);
        if (!wasInt)
            return false;

        Vector<Payload*>& payloads = mediaDescription->payloads();
        Vector<Payload*>::iterator plIt = payloads.begin();
        while (plIt != payloads.end()) {
            Payload* payload = *plIt;
            plIt++;
            if (payload->payloadTypeNumber() == payloadTypeNumber) {
                payload->setFramesizeWidth(width);
                payload->setFramesizeHeight(height);
                LOG(MediaStream, "Set width to %u and height to %u for payload %u", width, height, payloadTypeNumber);
                break;
            }
        }
    } else {
        LOG(MediaStream, "Unrecognized attribute (%s) on row: %s\n", attribute.ascii().data(), aLine.ascii().data());
        return true;
    }

    return true;
}

bool setMediaBandwidth(MediaDescription* mediaDescription, const String& aLine)
{
    LOG(MediaStream, "line = %s\n", aLine.ascii().data());
    if (aLine[0] != 'b' || aLine[1] != '=') {
        LOG(MediaStream, "Not a valid a line: %s\n", aLine.ascii().data());
        return false;
    }

    String aSuffix = aLine.substring(2);
    Vector<String> tokens;
    aSuffix.split(":", tokens);
    String& attribute = tokens[0];
    if (attribute == "TIAS") {
        bool isInt = false;
        unsigned int maxBw = (tokens[1].toUInt(&isInt));
        if (!isInt)
            return false;
        mediaDescription->setMaximumBandwidth(maxBw);
    }

    return true;
}

bool setConnection(MediaDescription* mediaDescription, const String& aLine)
{
    if (aLine[0] != 'c' || aLine[1] != '=') {
        LOG(MediaStream, "Not a valid a line: %s\n", aLine.ascii().data());
        return false;
    }
    String aSuffix = aLine.substring(2);
    Vector<String> tokens;
    aSuffix.split(" ", tokens);
    if (tokens.size() != 3)
        return false;

    mediaDescription->setConnectionAddress(tokens[2]);
    return true;
}

const char* getAddressType(const char* ip)
{
    g_return_val_if_fail(ip, 0);
    return (!(strchr(ip, ':'))) ?  "IP4" : "IP6";
}

String formatMediaDescription(MediaDescription* md)
{
    static const char* MediaTypeText[4] = {"audio", "video", "text", "invalid"};
    static const char* MediaTransportType[3] = {"udp", "RTP/AVP", "RTP/AVPF"};
    static const char* Direction[4] = {"unknown", "sendonly", "recvonly", "sendrecv"};

    // TODO: What do we do if mediaType is unknown?

    Vector<Payload*>& payloads = md->payloads();
    Vector<Candidate*>& candidates = md->candidates();
    Vector<Crypto*>& cryptos = md->cryptos();

    String mediaLines = "";
    mediaLines += "m=";
    mediaLines += MediaTypeText[md->mediaType()];
    mediaLines += " ";
    mediaLines += String::number(md->port());
    mediaLines += " ";
    mediaLines += MediaTransportType[md->mediaTransportType()];
    if (md->mediaType() == MediaDescription::Text)
        mediaLines += " nt\r\n";
    else {
        Vector<Payload*>::iterator plIt = payloads.begin();
        while (plIt != payloads.end()) {
            Payload* payload = *plIt;
            plIt++;
            mediaLines += " ";
            mediaLines += String::number(payload->payloadTypeNumber());
        }
        mediaLines += "\r\n";
    }

    if (md->maximumBandwidth()) {
        mediaLines += "b=TIAS:";
        mediaLines += String::number(md->maximumBandwidth());
        mediaLines += "\r\n";
    }

    if (md->port()) {
        if (md->connectionAddress() != "") {
            mediaLines += "c=IN ";
            mediaLines += getAddressType(md->connectionAddress().ascii().data());
            mediaLines += " ";
            mediaLines += md->connectionAddress();
            mediaLines += "\r\n";
        }

        Vector<Payload*>::iterator plIt = payloads.begin();
        while (plIt != payloads.end()) {
            Payload* payload = *plIt;
            plIt++;
            mediaLines += formatPayload(payload);
        }

        Vector<Candidate*>::iterator cIt = candidates.begin();
        while (cIt != candidates.end()) {
            Candidate* candidate = *cIt;
            cIt++;
            mediaLines += formatCandidate(candidate);
        }


        mediaLines += "a=";
        mediaLines += Direction[md->direction()];
        mediaLines += "\r\n";

        if (!md->username().isEmpty()) {
            mediaLines += "a=ice-ufrag:";
            mediaLines += md->username();
            mediaLines += "\r\n";
        }

        if (!md->password().isEmpty()) {
            mediaLines += "a=ice-pwd:";
            mediaLines += md->password();
            mediaLines += "\r\n";
        }
    }
    if (!md->label().isEmpty()) {
        mediaLines += "a=label:";
        mediaLines += md->label();
        mediaLines += "\r\n";
    }
    if (!md->rtcpProfileSpecificParameters().isEmpty()) {
        mediaLines += "a=rtcp-fb:*";
        const Vector<MediaDescription::RtcpProfileSpecificParameter> rtcpProfileSpecificParameters = md->rtcpProfileSpecificParameters();
        Vector<MediaDescription::RtcpProfileSpecificParameter>::const_iterator paramIt = rtcpProfileSpecificParameters.begin();
        while (paramIt != rtcpProfileSpecificParameters.end()) {
            mediaLines += " ";
            mediaLines += MediaDescription::RtcpProfileSpecificParameterString[*paramIt];
            paramIt++;
        }
        mediaLines += "\r\n";
    }
    Vector<Crypto*>::iterator cIt = cryptos.begin();
    while (cIt != cryptos.end()) {
        Crypto* crypto = *cIt;
        cIt++;
        mediaLines += formatCrypto(crypto);
    }


    return mediaLines;
}



}

#endif
