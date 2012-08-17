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

#include "Crypto.h"

#include "Logging.h"
#include "PlatformString.h"
#include <glib.h>
#include <math.h>
#include <wtf/text/CString.h>

namespace WebCore {

static bool addCryptoAttribute(Crypto*, const String& cryptoSuffix);

Crypto* createCrypto(const String& cryptoAttribute)
{
    String cryptoSuffix = cryptoAttribute.substring(9);
    LOG(MediaStream, "a=crypto: suffix is %s. whole line is %s\n", cryptoSuffix.ascii().data(), cryptoAttribute.ascii().data());

    Crypto* crypto = new Crypto();
    bool succeeded = addCryptoAttribute(crypto, cryptoSuffix);
    if (!succeeded) {
        delete crypto;
        crypto = 0;
    }

    return crypto;
}

static bool addCryptoAttribute(Crypto* crypto, const String& cryptoSuffix)
{
    // <tag> <crypto-suite> <key-params> [<session-params>]
    // <key-params> = "inline:" <key||salt> ["|" lifetime] ["|" MKI ":" length]
    Vector<String> tokens;
    cryptoSuffix.split(" ", tokens);

    Vector<String>::iterator tokenIt = tokens.begin();
    Vector<String>::iterator tokenItEnd = tokens.end();

    if (tokenIt == tokenItEnd)
        return false;

    bool wasInt = false;

    // Tag
    String& tagStr = *tokenIt++;
    unsigned int tag = tagStr.toUInt(&wasInt);
    if (!wasInt)
        return false;
    crypto->setTag(tag);
    if (tokenIt == tokenItEnd)
        return false;

    // CryptoSuite
    String& cryptoSuiteStr = *tokenIt++;
    if (cryptoSuiteStr == "AES_CM_128_HMAC_SHA1_80")
        crypto->setCryptoSuiteType(Crypto::AES_CM_128_HMAC_SHA1_80);
    else if (cryptoSuiteStr == "AES_CM_128_HMAC_SHA1_32")
         crypto->setCryptoSuiteType(Crypto::AES_CM_128_HMAC_SHA1_32);
    else if (cryptoSuiteStr == "F8_128_HMAC_SHA1_80")
         crypto->setCryptoSuiteType(Crypto::F8_128_HMAC_SHA1_80);
    else
        return false;

    if (tokenIt == tokenItEnd)
        return false;

    // Key-parameters
    String& keysParametersStr = *tokenIt++;

    Vector<String> keys;
    keysParametersStr.split(";", keys);
    Vector<String>::iterator keyIt = keys.begin();
    Vector<String>::iterator keyItEnd = keys.end();
    if (keyIt == keyItEnd)
        return false;
    String& keyParametersStr = *keyIt++;
    if (keyIt != keyItEnd) {
        LOG(MediaStream, "Only one key is supported in this implementation\n");
        return false;
    }

    // Method
    Vector<String> method;
    keyParametersStr.split(":", method);
    Vector<String>::iterator methodIt = method.begin();
    Vector<String>::iterator methodItEnd = method.end();
    if (methodIt == methodItEnd)
        return false;
    String& methodStr = *methodIt++;
    if (methodStr != "inline")
        return false;
    String keyParametersSuffixStr = keyParametersStr.substring(7);

    Vector<String> params;
    keyParametersSuffixStr.split("|", params);
    Vector<String>::iterator paramIt = params.begin();
    Vector<String>::iterator paramItEnd = params.end();

    if (paramIt == paramItEnd)
        return false;

    String& masterKeyAndSaltStr = *paramIt++;
    crypto->setMasterKey(masterKeyAndSaltStr);
    if (paramIt != paramItEnd) {
        String& lifeTimeStr = *paramIt++;
        Vector<String> numbers;
        lifeTimeStr.split("^", numbers);
        Vector<String>::iterator numIt = numbers.begin();
        Vector<String>::iterator numItEnd = numbers.end();
        if (numIt == numItEnd)
            return false;
        String& num1Str = *numIt++;
        unsigned long int lifeTime;
        if (numIt != numItEnd) {
            String& num2Str = *numIt++;
            unsigned long int num1 = num1Str.toUInt64(&wasInt);
            unsigned long int num2 = num2Str.toUInt64(&wasInt);
            lifeTime = (unsigned long int) pow(num1, num2);
        } else
            lifeTime = num1Str.toUInt64(&wasInt);

        if (!wasInt)
            return false;
        crypto->setLifeTime(lifeTime);

        if (paramIt != paramItEnd) {
            String& mkiAndLengthStr = *paramIt++;
            Vector<String> mkiParams;
            mkiAndLengthStr.split(":", mkiParams);
            Vector<String>::iterator mkiParamIt = mkiParams.begin();
            Vector<String>::iterator mkiParamItEnd = mkiParams.end();

            // mki
            if (mkiParamIt == mkiParamItEnd)
                return false;

            String& mkiStr = *mkiParamIt++;
            unsigned int mki = mkiStr.toUInt(&wasInt);
            if (!wasInt)
                return false;
            crypto->setMki(mki);

            // mkiLength
            if (mkiParamIt != mkiParamItEnd) {
                String& mkiLengthStr = *mkiParamIt++;
                unsigned int mkiLength = mkiLengthStr.toUInt(&wasInt);
                if (!wasInt)
                    return false;
                crypto->setMkiLength(mkiLength);
            }
        }

    }

    // Session parameters
    if (paramIt != paramItEnd) {
        String& sessionParametersStr = *paramIt++;
        crypto->setSessionParameters(sessionParametersStr);

        if (paramIt != paramItEnd)
            return false;
    }

    return true;
}

String Crypto::keyParameters()
{
    // "inline:" <key||salt> ["|" lifetime] ["|" MKI ":" length]
    String keyParameters = "inline:";
    keyParameters += m_mk;

    if (m_useLifeTime) {
        keyParameters += "|";
        keyParameters += String::number(m_lifeTime);
    }

    if (m_useMki && m_useMkiLength) {
        keyParameters += "|";
        keyParameters += String::number(m_mki);
        keyParameters += ":";
        keyParameters += String::number(m_mkiLength);
    }

    if (m_useSessionParameters) {
        keyParameters += " ";
        keyParameters += m_sessionParameters;
    }

    return keyParameters;
}

void Crypto::setMasterKey(const String& mk)
{
    m_mk = mk;
}

void Crypto::setLifeTime(unsigned long int lifeTime)
{
    m_useLifeTime = true;
    m_lifeTime = lifeTime;
}

void Crypto::setMki(unsigned int mki)
{
    m_useMki = true;
    m_mki = mki;
}

void Crypto::setMkiLength(unsigned int length)
{
    m_useMkiLength = true;
    m_mkiLength = length;
}


String formatCrypto(Crypto* crypto)
{
    static const char* CryptoType[3] = {"AES_CM_128_HMAC_SHA1_80", "AES_CM_128_HMAC_SHA1_32", "F8_128_HMAC_SHA1_80"};

    String cryptoLine = "a=crypto:";
    cryptoLine += String::number(crypto->tag());
    cryptoLine += " ";
    cryptoLine += CryptoType[crypto->cryptoSuiteType()];
    cryptoLine += " ";
    cryptoLine += crypto->keyParameters();
    if (crypto->m_useSessionParameters) {
        cryptoLine += " ";
        cryptoLine += crypto->sessionParameters();
    }
    cryptoLine += "\r\n";
    return cryptoLine;
}


}

#endif
