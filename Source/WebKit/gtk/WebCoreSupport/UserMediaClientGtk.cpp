/*
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */


#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "UserMediaClientGtk.h"

#include "webkitwebusermedialistprivate.h"
#include "webkitwebusermediarequestprivate.h"
#include "webkitwebviewprivate.h"

using namespace WebCore;

void addUserMediaSupport(WebKitWebView*);

namespace WebKit {

UserMediaClientGtk::UserMediaClientGtk(WebKitWebView *webView)
    : m_webView(webView)
{
    addUserMediaSupport(webView);
}

UserMediaClientGtk::~UserMediaClientGtk()
{
}

void UserMediaClientGtk::pageDestroyed()
{
    delete this;
}

void UserMediaClientGtk::requestUserMedia(PassRefPtr<UserMediaRequest> prpRequest, const WebCore::MediaStreamSourceVector& audioSources, const WebCore::MediaStreamSourceVector& videoSources)
{
    RefPtr<UserMediaRequest> request = prpRequest;
    m_requestSet.add(request);

    g_signal_emit_by_name(m_webView, "user-media-requested", kitNew(request.get()), kitNew(audioSources, videoSources));
}

void UserMediaClientGtk::cancelUserMediaRequest(UserMediaRequest* request)
{
    ASSERT(m_requestSet.contains(request));
    g_signal_emit_by_name(m_webView, "user-media-request-cancelled", 0);
}

void UserMediaClientGtk::userMediaRequestSucceeded(UserMediaRequest* request, const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources)
{
    request->succeed(audioSources, videoSources);
    m_requestSet.remove(request);
}

void UserMediaClientGtk::userMediaRequestFailed(UserMediaRequest* request)
{
    request->fail();
    m_requestSet.remove(request);
}

}

//==============================================================================
// TEMPORARY
// The code below this point is moved from the browser to the client to be able
// to use same UI code in both GtkLauncher and Epiphany.
//==============================================================================

typedef struct {
    WebKitWebView* webView;
    WebKitWebUserMediaRequest* request;
    WebKitWebUserMediaList* userMediaList;
} UserMediaSelectorData;

static void userMediaRequestCancelledCb(WebKitWebView *webView, WebKitWebUserMediaRequest* request, GtkWidget* dialog)
{
    g_signal_emit_by_name(dialog, "response", GTK_RESPONSE_CANCEL);
}

static void runUserMediaSelector(WebKitWebView *webView, WebKitWebUserMediaRequest* request, WebKitWebUserMediaList *userMediaList)
{

#if 0
    gint audioListLength = webkit_web_user_media_list_get_audio_length(userMediaList);
    gint videoListLength = webkit_web_user_media_list_get_video_length(userMediaList);

    if (audioListLength)
        webkit_web_user_media_list_select_audio_item(userMediaList, 0);

    if (videoListLength)
        webkit_web_user_media_list_select_video_item(userMediaList, 0);

    webkit_web_view_succeed_user_media_request(webView, request, userMediaList);

#endif

// TODO: the selector must be removed.
// should an an option on webkit settings
// so here we're selecting the firsts available devices, for testing propouses
#if 1
    GtkWidget* dialog;
    GtkWidget* contentArea;
    GtkWidget* actionArea;
    GtkWidget* frame;
    GtkWidget* vbox;
    GtkWidget** audioCheckButtons;
    GtkWidget** videoCheckButtons;
    GtkWidget* audioMessage;
    GtkWidget* videoMessage = 0;
    gboolean wantsAudio = webkit_web_user_media_request_wants_audio(request);
    gboolean wantsVideo = webkit_web_user_media_request_wants_video(request);
    gboolean hasAudio = false;
    gboolean hasVideo = false;
    gint audioListLength = webkit_web_user_media_list_get_audio_length(userMediaList);
    gint videoListLength = webkit_web_user_media_list_get_video_length(userMediaList);
    gint i;

    dialog = gtk_dialog_new_with_buttons("User Media Selector",
                                         GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(webView))),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL,
                                         GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK,
                                         GTK_RESPONSE_OK,
                                         0);

    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);

    contentArea = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(contentArea), 2);

    actionArea = gtk_dialog_get_action_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(actionArea), 5);
    gtk_box_set_spacing(GTK_BOX(actionArea), 6);

    /*
    for (i = 0; i < listLength && !hasVideo; i++) {
        if (webkit_web_user_media_list_get_item_type(userMediaList, i) == WEBKIT_USER_MEDIA_TYPE_AUDIO)
            hasAudio = true;
        else
            hasVideo = true;

        if (hasAudio && hasVideo)
            break;
    }
    */

    hasAudio = (audioListLength > 0);
    hasVideo = (videoListLength > 0);

    i = 0;
    // For now we can force requirement for audio and video
    wantsAudio = true;
    wantsVideo = true;

    if (!hasAudio && !hasVideo) {
        audioMessage = gtk_label_new("No user media available");
        gtk_misc_set_alignment(GTK_MISC(audioMessage), 0, 0);
        gtk_box_pack_start(GTK_BOX(contentArea), audioMessage, FALSE, FALSE, 6);
    } else if (wantsAudio) {
        audioMessage = gtk_label_new(hasAudio ? "Select from available user audio" : "No user audio available");
        gtk_misc_set_alignment(GTK_MISC(audioMessage), 0, 0);
        gtk_box_pack_start(GTK_BOX(contentArea), audioMessage, FALSE, FALSE, 6);
    }

    audioCheckButtons = g_new(GtkWidget*, audioListLength);
    videoCheckButtons = g_new(GtkWidget*, videoListLength);

    if (hasAudio) {
        frame = gtk_frame_new("Audio");
#if GTK_CHECK_VERSION(3, 2, 0)
        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
        vbox = gtk_vbox_new(TRUE, 0);
#endif
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 3);

        for (i = 0; i < audioListLength; i++) {
            audioCheckButtons[i] = gtk_check_button_new_with_label(webkit_web_user_media_list_get_audio_item_name(userMediaList, i));
            gtk_container_add(GTK_CONTAINER(vbox), audioCheckButtons[i]);
        }

        gtk_container_add(GTK_CONTAINER(frame), vbox);
        gtk_box_pack_start(GTK_BOX(contentArea), frame, FALSE, FALSE, 3);
    }

    if (wantsVideo) {
        if (hasVideo) {
            videoMessage = gtk_label_new("Select from available user video");
        } else if (!wantsAudio || hasAudio)
            videoMessage = gtk_label_new("No user video available");
        if (videoMessage) {
            gtk_misc_set_alignment(GTK_MISC(videoMessage), 0, 0);
            gtk_box_pack_start(GTK_BOX(contentArea), videoMessage, FALSE, FALSE, 6);
        }
    }

    if (hasVideo) {
        frame = gtk_frame_new("Video");
#if GTK_CHECK_VERSION(3, 2, 0)
        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
        vbox = gtk_vbox_new(TRUE, 0);
#endif
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 3);

        for (i = 0; i < videoListLength; i++) {
            videoCheckButtons[i] = gtk_check_button_new_with_label(webkit_web_user_media_list_get_video_item_name(userMediaList, i));
            gtk_container_add(GTK_CONTAINER(vbox), videoCheckButtons[i]);
        }

        gtk_container_add(GTK_CONTAINER(frame), vbox);
        gtk_box_pack_start(GTK_BOX(contentArea), frame, FALSE, FALSE, 3);
    }

    g_signal_connect(webView, "user-media-request-cancelled", G_CALLBACK(userMediaRequestCancelledCb), dialog);

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        for (i = 0; i < audioListLength; i++) {
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(audioCheckButtons[i])))
                webkit_web_user_media_list_select_audio_item(userMediaList, i);
        }

        for (i = 0; i < videoListLength; i++) {
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(videoCheckButtons[i])))
                webkit_web_user_media_list_select_video_item(userMediaList, i);
        }
        webkit_web_view_succeed_user_media_request(webView, request, userMediaList);
    } else
        webkit_web_view_fail_user_media_request(webView, request);

    g_signal_handlers_disconnect_by_func(webView, (gpointer) userMediaRequestCancelledCb, dialog);
    gtk_widget_destroy(dialog);
#endif // if 0
}

gboolean scheduleUserMediaSelector(gpointer data)
{
    UserMediaSelectorData* selectorData = (UserMediaSelectorData*) data;
    runUserMediaSelector(selectorData->webView, selectorData->request, selectorData->userMediaList);
    g_free(selectorData);

    return FALSE;
}

static void userMediaRequestedCb(WebKitWebView *webView, WebKitWebUserMediaRequest* request, WebKitWebUserMediaList* userMediaList)
{
    UserMediaSelectorData* data = g_new(UserMediaSelectorData, 1);
    data->webView = webView;
    data->request = request;
    data->userMediaList = userMediaList;

    g_idle_add((GSourceFunc) scheduleUserMediaSelector, data);
}

void addUserMediaSupport(WebKitWebView* webView)
{
    g_signal_connect(webView, "user-media-requested", G_CALLBACK(userMediaRequestedCb), NULL);
}

#endif // ENABLE(MEDIA_STREAM)
