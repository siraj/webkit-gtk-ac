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

#include "PeerConnectionHandlerConfiguration.h"

#include <wtf/text/CString.h>

namespace WebCore {

static bool isCarriageReturnOrLineFeed(UChar c)
{
    return c == '\r' || c == '\n';
}

PassRefPtr<PeerConnectionHandlerConfiguration> PeerConnectionHandlerConfiguration::create(const String& serverConfiguration, const String& username)
{
    size_t endOfLine = serverConfiguration.find(WebCore::isCarriageReturnOrLineFeed);
    String serverConfig = endOfLine == notFound ? serverConfiguration : serverConfiguration.left(endOfLine);

    Vector<String> configurationComponents;
    serverConfig.split(' ', configurationComponents);

    RefPtr<PeerConnectionHandlerConfiguration> configuration = PeerConnectionHandlerConfiguration::create();

    if (configurationComponents.size() < 2)
        return configuration;

    String& type = configurationComponents[0];
    if (type == "STUN" || type == "STUNS")
        configuration->type = PeerConnectionHandlerConfiguration::TypeSTUN;
    else if (type == "TURN" || type == "TURNS")
        configuration->type = PeerConnectionHandlerConfiguration::TypeTURN;
    else
        return configuration;

    configuration->secure = type == "STUNS" || type == "TURNS";

    String& hostAndPort = configurationComponents[1];
    size_t firstColon = hostAndPort.find(':');
    if (firstColon != notFound) {
        bool portOk;
        int port = hostAndPort.substring(firstColon + 1).toInt(&portOk);
        portOk &= 0 <= port && port < 65536;
        if (!portOk)
            return PeerConnectionHandlerConfiguration::create();

        configuration->host = hostAndPort.left(firstColon);
        configuration->port = port;
    } else
        configuration->host = hostAndPort;

    configuration->username = username;

    return configuration;
}

}

#endif
