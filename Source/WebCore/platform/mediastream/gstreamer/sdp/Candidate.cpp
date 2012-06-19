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

#include "Candidate.h"

#include "Logging.h"
#include <glib.h>
#include <wtf/text/CString.h>

namespace WebCore {

static bool addCandidateAttribute(Candidate*, const String& candidateSuffix);

Candidate::Candidate() :
    m_componentId(0),
    m_candidateType(Host),
    m_port(0),
    m_prio(0),
    m_relAddress(false),
    m_relPort(false),
    m_protocol(Candidate::UDP)
{
}

Candidate* createCandidate(const String& candidateAttribute)
{
    String candidateSuffix = candidateAttribute.substring(12);
    LOG(MediaStream, "a=candidate: suffix is %s. whole line is %s\n", candidateSuffix.ascii().data(), candidateAttribute.ascii().data());

    Candidate* candidate = new Candidate();
    bool succeeded = addCandidateAttribute(candidate, candidateSuffix);
    if (!succeeded) {
        delete candidate;
        candidate = 0;
    }

    return candidate;
}


static bool addCandidateAttribute(Candidate* candidate, const String& candidateSuffix)
{
    LOG(MediaStream, "candidateSuffix is %s\n", candidateSuffix.ascii().data());
    Vector<String> tokens;
    candidateSuffix.split(" ", tokens);

    Vector<String>::iterator tokenIt = tokens.begin();
    Vector<String>::iterator tokenItEnd = tokens.end();

    if (tokenIt == tokenItEnd)
        return false;

    bool wasInt = false;

    String& foundationStr = *tokenIt++;
    candidate->setFoundation(foundationStr);

    if (tokenIt == tokenItEnd)
        return false;
    String& componentIdStr = *tokenIt++;
    unsigned int componentId = componentIdStr.toUInt(&wasInt);
    if (!wasInt)
        return false;
    candidate->setComponentId(componentId);

    if (tokenIt == tokenItEnd)
        return false;
    String& transport = *tokenIt++;
    if (transport == "UDP")
        candidate->setProtocol(Candidate::UDP);
    else if (transport == "TCP")
        candidate->setProtocol(Candidate::TCP);
    else
        return false;
    // FIXME: Save this?

    if (tokenIt == tokenItEnd)
        return false;
    String& priorityStr = *tokenIt++;
    unsigned int priority = priorityStr.toUInt(&wasInt);
    if (!wasInt)
        return false;
    candidate->setPriority(priority);

    if (tokenIt == tokenItEnd)
        return false;
    String& connectionAddress = *tokenIt++;
    candidate->setAddress(connectionAddress);
    if (tokenIt == tokenItEnd)
        return false;

    if (tokenIt == tokenItEnd)
        return false;
    String& portStr = *tokenIt++;
    unsigned int port = portStr.toUInt(&wasInt);
    if (!wasInt || port > 65535)
        return false;
    candidate->setPort(static_cast<unsigned short>(port));

    if (tokenIt == tokenItEnd)
            return false;
    String& typStr = *tokenIt++;
    if (typStr != "typ")
        return false;

    if (tokenIt == tokenItEnd)
            return false;
    String& candType = *tokenIt++;
    if (candType == "host")
        candidate->setCandidateType(Candidate::Host);
    else if (candType == "srflx")
        candidate->setCandidateType(Candidate::ServerReflexive);
    else if (candType == "prflx")
        candidate->setCandidateType(Candidate::PeerReflexive);
    else if (candType == "relay")
        candidate->setCandidateType(Candidate::Relayed);
    else
        return false;

    if (tokenIt != tokenItEnd) {
        String& rel = *tokenIt++;
        if (rel == "raddr")
            candidate->setRelatedAddress(true);
            // TODO missing parsing the raddr IP
        else if (rel == "rport")
            candidate->setRelatedPort(true);
            // TODO missing parsing the rport port number
    }

    if (tokenIt != tokenItEnd) {
        String& rel = *tokenIt++;
        if (rel == "raddr")
            candidate->setRelatedAddress(true);
            // TODO missing parsing the raddr IP
        else if (rel == "rport")
            candidate->setRelatedPort(true);
            // TODO missing parsing the rport port number
    }

     return true;
}


String formatCandidate(Candidate* candidate)
{
    static const char* CandidateType[5] = {"srflx", "prflx", "relay", "host", "multicast"};

    String candLine = "a=candidate:";
    candLine += candidate->foundation();
    candLine += " ";
    candLine += String::number(candidate->componentId());
    candLine += candidate->protocol() == Candidate::UDP ? " UDP " : " TCP ";
    candLine += String::number(candidate->priority());
    candLine += " ";
    candLine += candidate->address();
    candLine += " ";
    candLine += String::number(candidate->port());
    candLine += " typ ";
    candLine += CandidateType[candidate->candidateType()];
    candLine += "\r\n";
    return candLine;
}



}

#endif
