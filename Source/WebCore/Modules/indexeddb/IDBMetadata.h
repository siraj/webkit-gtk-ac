/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBMetadata_h
#define IDBMetadata_h

#include "IDBKeyPath.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

struct IDBObjectStoreMetadata;
struct IDBIndexMetadata;

struct IDBDatabaseMetadata {
    // FIXME: These can probably be collapsed into 0.
    enum {
        NoIntVersion = -1,
        DefaultIntVersion = 0
    };

    IDBDatabaseMetadata()
        : intVersion(NoIntVersion)
    {
    }
    IDBDatabaseMetadata(const String& name, const String& version, int64_t intVersion)
        : name(name)
        , version(version)
        , intVersion(intVersion)
    {
    }

    String name;
    String version;
    int64_t intVersion;

    typedef HashMap<String, IDBObjectStoreMetadata> ObjectStoreMap;
    ObjectStoreMap objectStores;
};

struct IDBObjectStoreMetadata {
    IDBObjectStoreMetadata() { }
    IDBObjectStoreMetadata(const String& name, const IDBKeyPath& keyPath, bool autoIncrement)
        : name(name)
        , keyPath(keyPath)
        , autoIncrement(autoIncrement) { }
    String name;
    IDBKeyPath keyPath;
    bool autoIncrement;

    typedef HashMap<String, IDBIndexMetadata> IndexMap;
    IndexMap indexes;
};

struct IDBIndexMetadata {
    IDBIndexMetadata() { }
    IDBIndexMetadata(const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry)
        : name(name)
        , keyPath(keyPath)
        , unique(unique)
        , multiEntry(multiEntry) { }
    String name;
    IDBKeyPath keyPath;
    bool unique;
    bool multiEntry;
};

}

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBMetadata_h
