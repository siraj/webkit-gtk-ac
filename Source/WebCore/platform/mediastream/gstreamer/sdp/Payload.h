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

#ifndef Payload_h
#define Payload_h

#if ENABLE(MEDIA_STREAM)

#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Payload {
public:

    Payload()
    :   m_rate(0)
    ,   m_payloadTypeNumber(0)
    ,   m_codecName("")
    ,   m_format("")
    ,   m_framesizeWidth(0)
    ,   m_framesizeHeight(0)
    ,   m_channels(1)
    { };
    ~Payload() { };

    unsigned int rate() const { return m_rate; }
    void setRate(unsigned int rate) { m_rate = rate; }

    unsigned char payloadTypeNumber() const { return m_payloadTypeNumber; }
    void setPayloadTypeNumber(unsigned char pltn) {m_payloadTypeNumber = pltn; }

    String codecName() const { return m_codecName; }
    void setCodecName(const String& codecName) { m_codecName = codecName; }

    String format() const { return m_format; }
    void setFormat(const String& format) { m_format = format; }


    void setFramesizeWidth(unsigned int width) { m_framesizeWidth = width; }
    void setFramesizeHeight(unsigned int height) { m_framesizeHeight = height; }
    unsigned int framesizeWidth() const { return m_framesizeWidth; }
    unsigned int framesizeHeight() const { return m_framesizeHeight; }

    void setChannels(unsigned int channels) { m_channels = channels; }
    unsigned int channels() const { return m_channels; }

private:
    unsigned int m_rate;
    unsigned char m_payloadTypeNumber;
    String m_codecName;
    String m_format;
    unsigned int m_framesizeWidth;
    unsigned int m_framesizeHeight;
    unsigned int m_channels;
};

Payload* createPayload(const String& rtpmapLine);

String formatPayload(Payload*);

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // SignalingMessage_h
