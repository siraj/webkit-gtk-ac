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

#include "Payload.h"

#include "Logging.h"
#include <glib.h>
#include <wtf/text/CString.h>

namespace WebCore {

static bool addPayloadAttribute(Payload*, const String& rtpmapSuffix);

Payload* createPayload(const String& rtpmapLine)
{
    String rtpmapSuffix = rtpmapLine.substring(9);
    LOG(MediaStream, "a=rtpmap: suffix is %s. whole line is %s\n", rtpmapSuffix.ascii().data(), rtpmapLine.ascii().data());

    Payload* payload = new Payload();
    bool succeed = addPayloadAttribute(payload, rtpmapSuffix);
    if (!succeed) {
        delete payload;
        payload = 0;
    }

    return payload;
}

static bool addPayloadAttribute(Payload* payload, const String& rtpmapSuffix)
{
    LOG(MediaStream, "Rtpmap suffix: %s\n", rtpmapSuffix.ascii().data());

    Vector<String> tokens;
    rtpmapSuffix.split(" ", tokens);

    Vector<String>::iterator tokenIt = tokens.begin();
    Vector<String>::iterator tokenItEnd = tokens.end();

    if (tokenIt == tokenItEnd)
        return false;

    bool wasInt = false;

    String& pltnStr = *tokenIt++;
    LOG(MediaStream, "Payloadtype number string is %s\n", pltnStr.ascii().data());
    unsigned int pltn = pltnStr.toUInt(&wasInt);
    if (!wasInt /*|| pltn > 128*/)
        return false;
    payload->setPayloadTypeNumber(pltn);

    if (tokenIt == tokenItEnd)
        return false;
    String& nameAndRate = *tokenIt++;
    Vector<String> nameRateVec;
    nameAndRate.split("/", nameRateVec);
    Vector<String>::iterator mapIt = nameRateVec.begin();
    if (mapIt == nameRateVec.end())
        return false;
    String& codecName = *mapIt;
    payload->setCodecName(codecName);

    mapIt++;
    if (mapIt == nameRateVec.end())
        return false;
    String& rateStr = *mapIt;
    unsigned int rate = rateStr.toUInt(&wasInt);
    if (!wasInt)
        return false;
    payload->setRate(rate);

    mapIt++;
    if (mapIt != nameRateVec.end()) {
        String& channelsStr = *mapIt;
        unsigned int channels = channelsStr.toUInt(&wasInt);
        if (!wasInt)
           return false;
        payload->setChannels(channels);
        LOG(MediaStream, "Channels set to %u", channels);
    }
    return true;
}

String formatPayload(Payload* payload)
{
    String plLines = "a=rtpmap:";
    plLines += String::number(payload->payloadTypeNumber());
    plLines += " ";
    plLines += payload->codecName();
    plLines += "/";
    plLines += String::number(payload->rate());
    if (payload->channels() != 1) {
        plLines += "/";
        plLines += String::number(payload->channels());
    }
    plLines += "\r\n";
    if (!(payload->format().isNull() || payload->format().isEmpty() || payload->format() == "")) {
        plLines += "a=fmtp:";
        plLines += String::number(payload->payloadTypeNumber());
        plLines += " ";
        plLines += payload->format();
        plLines += "\r\n";
    }
    unsigned int framesizeWidth = payload->framesizeWidth();
    unsigned int framesizeHeight = payload->framesizeHeight();
    if (framesizeWidth && framesizeHeight) {
        plLines += "a=framesize:";
        plLines += String::number(payload->payloadTypeNumber());
        plLines += " ";
        plLines += String::number(framesizeWidth);
        plLines += "-";
        plLines += String::number(framesizeHeight);
        plLines += "\r\n";
    }

    return plLines;
}

}

#endif
