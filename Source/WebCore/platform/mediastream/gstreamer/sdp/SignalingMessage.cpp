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

#include "SignalingMessage.h"

#include "Logging.h"
#include <glib.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>


namespace WebCore {

SignalingMessage::SignalingMessage(long sessionId, long sessionVersion)
    : m_username(g_get_user_name())
    , m_sessionId(sessionId)
    , m_sessionVersion(sessionVersion)
    , m_networkType("IN")
{
}

Vector<String> SignalingMessage::labels()
{
    Vector<String> labels;
    for (size_t i = 0; i < m_mediaDescriptions.size(); i++) {
        if (!labels.contains(m_mediaDescriptions[i]->label()))
            labels.append(m_mediaDescriptions[i]->label());
    }
    return labels;
}

Vector<MediaDescription*> SignalingMessage::mediaDescriptions(String label)
{
    Vector<MediaDescription*> mds;
    for (size_t i = 0; i < m_mediaDescriptions.size(); i++) {
        if (label == m_mediaDescriptions[i]->label())
            mds.append(m_mediaDescriptions[i]);
    }
    return mds;
}

void SignalingMessage::address(String& addrType, String& address) const
{
    if (!m_addrType.isEmpty() && !m_address.isEmpty()) {
        addrType = m_addrType;
        address = m_address;
        return;
    }

    if (m_mediaDescriptions.isEmpty())
        return;

    address = m_mediaDescriptions.first()->connectionAddress();
    addrType = (!(strchr(address.utf8().data(), ':'))) ?  "IP4" : "IP6";
}

static bool matchCR(UChar ch)
{
    if (ch == 0x00D)
        return true;
    return false;
}

bool SignalingMessage::setOrigin(const String& line)
{
    if (line[0] != 'o' || line[1] != '=') {
        g_warning("Not a valid a line: %s\n", line.ascii().data());
        return false;
    }
    String suffix = line.substring(2);
    Vector<String> tokens;
    suffix.split(" ", tokens);
    if (tokens.size() != 6) {
        g_warning("Wrong number of fields for origin: %zi <%s>", tokens.size(), suffix.ascii().data());
        return false;
    }
    m_username = tokens[0];
    bool res;
    m_sessionId = tokens[1].toInt64(&res);
    if (!res)
        g_warning("invalid session id: %s", tokens[1].ascii().data());

    m_sessionVersion = tokens[2].toInt64(&res);
    if (!res)
        g_warning("invalid session version: %s", tokens[2].ascii().data());

    // network type should always be "IN"
    g_warn_if_fail(tokens[3] == "IN");
    m_addrType = tokens[4];
    m_address = tokens[5];

    return true;
}

SignalingMessage* createSignalingMessage(const String& msg)
{
    // FIXME: temporary?
    String sdp = msg.removeCharacters(matchCR);
    String cLine = "";

    SignalingMessage* signalingMessage = new SignalingMessage();
    MediaDescription* mediaDescription = 0;
    bool succeeded = true;
    Vector<String> lines;
    // sdp.split("\r\n", lines);
    sdp.split("\n", lines);
    LOG(MediaStream, "Number of lines in SDP: %d\n", (int)lines.size());
    if (!(lines.size()))
        succeeded = false;

    Vector<String>::iterator lineIt = lines.begin();
    Vector<String>::iterator linesEndIt = lines.end();

    for (; lineIt != linesEndIt && succeeded; ++lineIt) {
        String& line = *lineIt;
        if (line[1] != '=') {
            LOG(MediaStream, "Failed parsing line in sdp: %s\n", line.ascii().data());
            succeeded = false;
            break;
        }
        UChar token = line[0];
        switch (token) {
        case 'm':
            mediaDescription = createMediaDescription(line);
            if (mediaDescription) {
                signalingMessage->mediaDescriptions().append(mediaDescription);
                if (cLine != "")
                    setConnection(mediaDescription, cLine);
            } else
                succeeded = false;
            break;
        case 'a':
            if (!mediaDescription)
                continue;
            succeeded = addAttribute(mediaDescription, line);
            break;
        case 'c':
            if (!mediaDescription) {
                cLine = line;
                continue;
            }
            succeeded = setConnection(mediaDescription, line);
            break;
        case 'b':
            if (!mediaDescription) // FIXME: actually we should set if b=CT here..
                continue;
            succeeded = setMediaBandwidth(mediaDescription, line);
            break;
        case 'o':
            signalingMessage->setOrigin(line);
            break;
        default:
            LOG(MediaStream, "Found unrecognized token. Ignoring\n");
            // IGNORE
            break;
        }

        if (!succeeded)
            break;
    }

    if (!succeeded) {
        LOG(MediaStream, "FAILED PARSING\n");
        if (lines.size())
            LOG(MediaStream, "Failed parsing line in sdp: %s\n", (*lineIt).ascii().data());
        if (mediaDescription)
            delete mediaDescription;
        return 0;
    }

    // FIXME: cleanup when failing..

    return signalingMessage;
}

String formatSdp(SignalingMessage* signalingMessage)
{

    String sdp;
    sdp += "v=0\r\n";

    String addr, addrType;
    signalingMessage->address(addrType, addr);
    sdp.append(String::format("o=%s %li %li IN %s %s\n", signalingMessage->username().ascii().data(), signalingMessage->sessionId(), signalingMessage->sessionVersion(), addrType.ascii().data(), addr.ascii().data()));
    sdp.append("s= \n");
    sdp.append(String::format("c=IN %s %s\n", addrType.ascii().data(), addr.ascii().data()));
    sdp.append("t=0 0\n");

    Vector<MediaDescription*>::iterator mdIt = signalingMessage->mediaDescriptions().begin();
    while (mdIt != signalingMessage->mediaDescriptions().end()) {
        MediaDescription* md = *mdIt;
        mdIt++;

        sdp += formatMediaDescription(md);
    }

    LOG(MediaStream, "Created sdp:\n%s", sdp.ascii().data());

    return sdp;
}


}

#endif
