/*
 * Copyright (C) 2012 Collabora.
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
 */

#include "config.h"
#include "GtkMediaChooserDialog.h"
#include "MediaStreamSource.h"
#include "UserMediaRequest.h"
#include "GtkVersioning.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <wtf/text/CString.h>

namespace WebCore {

GtkMediaChooserDialog::GtkMediaChooserDialog(GtkWidget* parent, UserMediaRequest* request, const MediaStreamSourceVector& audioSource, const MediaStreamSourceVector& videoSource)
    : m_dialog(0)
    , m_audioCombo(0)
    , m_videoCombo(0)
{
    GtkWidget* contentArea;
    GtkWidget* actionArea;
    GtkWidget* frame;
    GtkWidget* vbox;
    GtkWidget* audioMessage = 0;
    GtkWidget* videoMessage = 0;
    gboolean wantsAudio = request->audio();
    gboolean wantsVideo = request->video();
    gint audioListLength = audioSource.size();
    gint videoListLength = videoSource.size();
    gboolean hasAudio = (audioListLength > 0);
    gboolean hasVideo = (videoListLength > 0);
    gint i = 0;

    m_dialog = gtk_dialog_new_with_buttons(_("User Media Selector"),
                                         parent? GTK_WINDOW(parent) : 0,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL,
                                         GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK,
                                         GTK_RESPONSE_OK,
                                         NULL);

    gtk_window_set_resizable(GTK_WINDOW(m_dialog), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(m_dialog), 5);

    contentArea = gtk_dialog_get_content_area(GTK_DIALOG(m_dialog));
    gtk_box_set_spacing(GTK_BOX(contentArea), 2);

    actionArea = gtk_dialog_get_action_area(GTK_DIALOG(m_dialog));
    gtk_container_set_border_width(GTK_CONTAINER(actionArea), 5);
    gtk_box_set_spacing(GTK_BOX(actionArea), 6);

    if (!hasAudio && !hasVideo) {
        audioMessage = gtk_label_new(_("No user media available"));
        gtk_misc_set_alignment(GTK_MISC(audioMessage), 0, 0);
        gtk_box_pack_start(GTK_BOX(contentArea), audioMessage, FALSE, FALSE, 6);
    } else if (wantsAudio) {
        audioMessage = gtk_label_new(hasAudio ? _("Select from available user audio") : _("No user audio available"));
        gtk_misc_set_alignment(GTK_MISC(audioMessage), 0, 0);
        gtk_box_pack_start(GTK_BOX(contentArea), audioMessage, FALSE, FALSE, 6);
    }

    if (hasAudio) {
        frame = gtk_frame_new(_("Audio"));
#if GTK_CHECK_VERSION(3, 2, 0)
        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
        vbox = gtk_vbox_new(TRUE, 0);
#endif
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 3);

        m_audioCombo = gtk_combo_box_text_new();
        for (i = 0; i < audioListLength; i++)
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_audioCombo), g_strdup(audioSource[i]->name().utf8().data()));
        gtk_combo_box_set_active(GTK_COMBO_BOX(m_audioCombo), 0);
        gtk_container_add(GTK_CONTAINER(vbox), m_audioCombo);

        gtk_container_add(GTK_CONTAINER(frame), vbox);
        gtk_box_pack_start(GTK_BOX(contentArea), frame, FALSE, FALSE, 3);
    }

    if (wantsVideo) {
        if (hasVideo) {
            videoMessage = gtk_label_new(_("Select from available user video"));
        } else if (!wantsAudio || hasAudio)
            videoMessage = gtk_label_new(_("No user video available"));
        if (videoMessage) {
            gtk_misc_set_alignment(GTK_MISC(videoMessage), 0, 0);
            gtk_box_pack_start(GTK_BOX(contentArea), videoMessage, FALSE, FALSE, 6);
        }
    }

    if (hasVideo) {
        frame = gtk_frame_new(_("Video"));
#if GTK_CHECK_VERSION(3, 2, 0)
        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
        vbox = gtk_vbox_new(TRUE, 0);
#endif
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 3);

        m_videoCombo = gtk_combo_box_text_new();
        for (i = 0; i < videoListLength; i++)
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_videoCombo), g_strdup(videoSource[i]->name().utf8().data()));
        gtk_combo_box_set_active(GTK_COMBO_BOX(m_videoCombo), 0);
        gtk_container_add(GTK_CONTAINER(vbox), m_videoCombo);

        gtk_container_add(GTK_CONTAINER(frame), vbox);
        gtk_box_pack_start(GTK_BOX(contentArea), frame, FALSE, FALSE, 3);
    }
}

GtkMediaChooserDialog::~GtkMediaChooserDialog()
{
    gtk_widget_destroy(m_dialog);
}

int GtkMediaChooserDialog::selectedAudio()
{
    if (m_audioCombo)
        return gtk_combo_box_get_active(GTK_COMBO_BOX(m_audioCombo));
    else
        return -1;
}

int GtkMediaChooserDialog::selectedVideo()
{
    if (m_videoCombo)
       return gtk_combo_box_get_active(GTK_COMBO_BOX(m_videoCombo));
    else
        return -1;
}

void* GtkMediaChooserDialog::show()
{
    gtk_widget_show_all(m_dialog);
}

GtkWidget* GtkMediaChooserDialog::widget()
{
    return m_dialog;
}

}
