/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebInspectorProxy_h
#define WebInspectorProxy_h

#if ENABLE(INSPECTOR)

#include "APIObject.h"
#include "Connection.h"
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>

OBJC_CLASS NSWindow;
OBJC_CLASS WKWebInspectorProxyObjCAdapter;
OBJC_CLASS WKWebInspectorWKView;
#endif

#if PLATFORM(WIN)
#include <WebCore/WindowMessageListener.h>
#endif

#if PLATFORM(GTK)
#include "WebInspectorClientGtk.h"
#endif

#if PLATFORM(EFL)
#include <Ecore_Evas.h>
#include <Evas.h>
#endif

namespace WebKit {

class WebFrameProxy;
class WebPageGroup;
class WebPageProxy;
struct WebPageCreationParameters;

#if PLATFORM(WIN)
class WebView;
#endif

class WebInspectorProxy : public APIObject
#if PLATFORM(WIN)
    , public WebCore::WindowMessageListener
#endif
{
public:
    static const Type APIType = TypeInspector;

    static PassRefPtr<WebInspectorProxy> create(WebPageProxy* page)
    {
        return adoptRef(new WebInspectorProxy(page));
    }

    ~WebInspectorProxy();

    void invalidate();

    // Public APIs
    WebPageProxy* page() const { return m_page; }

    bool isVisible() const { return m_isVisible; }
    bool isFront();

    void show();
    void close();
    
#if PLATFORM(MAC)
    void createInspectorWindow();
    void updateInspectorWindowTitle() const;
    void inspectedViewFrameDidChange();
#endif

#if PLATFORM(GTK)
    GtkWidget* inspectorView() const { return m_inspectorView; };
    void initializeInspectorClientGtk(const WKInspectorClientGtk*);
#endif

    void showConsole();
    void showResources();
    void showMainResourceForFrame(WebFrameProxy*);

    bool isAttached() const { return m_isAttached; }
    void attach();
    void detach();
    void setAttachedWindowHeight(unsigned);

    bool isDebuggingJavaScript() const { return m_isDebuggingJavaScript; }
    void toggleJavaScriptDebugging();

    bool isProfilingJavaScript() const { return m_isProfilingJavaScript; }
    void toggleJavaScriptProfiling();

    bool isProfilingPage() const { return m_isProfilingPage; }
    void togglePageProfiling();

#if ENABLE(INSPECTOR)
    // Implemented in generated WebInspectorProxyMessageReceiver.cpp
    void didReceiveWebInspectorProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void didReceiveSyncWebInspectorProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, OwnPtr<CoreIPC::ArgumentEncoder>&);
#endif

    static bool isInspectorPage(WebPageProxy*);

    // Implemented the platform WebInspectorProxy file
    String inspectorPageURL() const;
    String inspectorBaseURL() const;

#if ENABLE(INSPECTOR_SERVER)
    void enableRemoteInspection();
    void remoteFrontendConnected();
    void remoteFrontendDisconnected();
    void dispatchMessageFromRemoteFrontend(const String& message);
    int remoteInspectionPageID() const { return m_remoteInspectionPageId; }
#endif

private:
    explicit WebInspectorProxy(WebPageProxy*);

    virtual Type type() const { return APIType; }

    WebPageProxy* platformCreateInspectorPage();
    void platformOpen();
    void platformDidClose();
    void platformBringToFront();
    bool platformIsFront();
    void platformInspectedURLChanged(const String&);
    unsigned platformInspectedWindowHeight();
    void platformAttach();
    void platformDetach();
    void platformSetAttachedWindowHeight(unsigned);

    // Called by WebInspectorProxy messages
    void createInspectorPage(uint64_t& inspectorPageID, WebPageCreationParameters&);
    void didLoadInspectorPage();
    void didClose();
    void bringToFront();
    void inspectedURLChanged(const String&);

#if ENABLE(INSPECTOR_SERVER)
    void sendMessageToRemoteFrontend(const String& message);
#endif

    bool canAttach();
    bool shouldOpenAttached();

    static WebPageGroup* inspectorPageGroup();

#if PLATFORM(WIN)
    static bool registerInspectorViewWindowClass();
    static LRESULT CALLBACK InspectorViewWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT onSizeEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onMinMaxInfoEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onSetFocusEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onCloseEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);

    void onWebViewWindowPosChangingEvent(WPARAM, LPARAM);
    virtual void windowReceivedMessage(HWND, UINT message, WPARAM, LPARAM);
#endif

#if PLATFORM(GTK)
    void createInspectorWindow();
#endif

    static const unsigned minimumWindowWidth;
    static const unsigned minimumWindowHeight;

    static const unsigned initialWindowWidth;
    static const unsigned initialWindowHeight;

    // Keep this in sync with the value in InspectorFrontendClientLocal.
    static const unsigned minimumAttachedHeight;

    WebPageProxy* m_page;

    bool m_isVisible;
    bool m_isAttached;
    bool m_isDebuggingJavaScript;
    bool m_isProfilingJavaScript;
    bool m_isProfilingPage;

#if PLATFORM(MAC)
    RetainPtr<WKWebInspectorWKView> m_inspectorView;
    RetainPtr<NSWindow> m_inspectorWindow;
    RetainPtr<WKWebInspectorProxyObjCAdapter> m_inspectorProxyObjCAdapter;
    String m_urlString;
#elif PLATFORM(WIN)
    HWND m_inspectorWindow;
    RefPtr<WebView> m_inspectorView;
#elif PLATFORM(GTK)
    WebInspectorClientGtk m_client;
    GtkWidget* m_inspectorView;
    GtkWidget* m_inspectorWindow;
#elif PLATFORM(EFL)
    Evas_Object* m_inspectorView;
    Ecore_Evas* m_inspectorWindow;
#endif
#if ENABLE(INSPECTOR_SERVER)
    int m_remoteInspectionPageId;
#endif
};

} // namespace WebKit

#endif // ENABLE(INSPECTOR)

#endif // WebInspectorProxy_h
