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


#ifndef Crypto_h
#define Crypto_h

#if ENABLE(MEDIA_STREAM)

#include "PlatformString.h"

namespace WebCore {

class Crypto {
    friend String formatCrypto(Crypto*);
public:
    enum CryptoSuiteType {
        AES_CM_128_HMAC_SHA1_80,
        AES_CM_128_HMAC_SHA1_32,
        F8_128_HMAC_SHA1_80
    };

    Crypto() { m_useLifeTime = false; m_useMki = false; m_useMkiLength = false;m_useSessionParameters = false; }
    ~Crypto() { }

    unsigned int tag() const { return m_tag; }
    void setTag(unsigned int tag) { m_tag = tag; }

    CryptoSuiteType cryptoSuiteType() const { return m_cryptoSuiteType; }
    void setCryptoSuiteType(CryptoSuiteType cryptoSuiteType) { m_cryptoSuiteType = cryptoSuiteType; }

    String keyParameters();

    String sessionParameters() const { return m_sessionParameters; }
    void setSessionParameters(const String& sessionParameters) { m_sessionParameters = sessionParameters; }

    String masterKey() const { return m_mk; }
    void setMasterKey(const String&);

    unsigned long int lifeTime(bool& isSet) { isSet = m_useLifeTime; return m_lifeTime; }
    void setLifeTime(unsigned long int);

    unsigned int mki(bool& isSet) { isSet = m_useMki; return m_mki; }
    void setMki(unsigned int);

    unsigned int mkiLength(bool& isSet) { isSet = m_useMkiLength; return m_mkiLength; }
    void setMkiLength(unsigned int);

private:
    unsigned int m_tag;
    CryptoSuiteType m_cryptoSuiteType;

    String m_mk;
    unsigned long int m_lifeTime;
    unsigned int m_mki;
    unsigned int m_mkiLength;
    String m_sessionParameters;

    bool m_useLifeTime;
    bool m_useMki;
    bool m_useMkiLength;
    bool m_useSessionParameters;
};

Crypto* createCrypto(const String& candidateAttribute);

String formatCrypto(Crypto*);


} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // Crypto_h
