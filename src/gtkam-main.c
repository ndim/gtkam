/* gtkam-main.c
 *
 * Copyright (C) 2001 Lutz M�ller <urc8@rz.uni-karlsruhe.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "gtkam-main.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include <stdio.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <gtk/gtkitemfactory.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtktoolbar.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenubar.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkfilesel.h>

#include <gphoto2/gphoto2-camera.h>

#include "gtkam-cancel.h"
#include "gtkam-chooser.h"
#include "gtkam-close.h"
#include "gtkam-config.h"
#include "gtkam-context.h"
#include "gtkam-debug.h"
#include "gtkam-delete.h"
#include "gtkam-error.h"
#include "gtkam-list.h"
#include "gtkam-preview.h"
#include "gtkam-status.h"
#include "gtkam-tree.h"

#include "support.h"

struct _GtkamMainPrivate
{
	GtkWidget *tree, *list;
	GtkItemFactory *factory;

	GtkToggleButton *toggle_preview;

	GtkWidget *item_summary, *item_about, *item_manual, *item_config;

	GtkWidget *item_delete, *item_delete_all;

	GtkWidget *status;

	GtkWidget *vbox;

	Camera *camera;
	gboolean multi;
};

#define PARENT_TYPE GTK_TYPE_WINDOW
static GtkWindowClass *parent_class;

static void
gtkam_main_destroy (GtkObject *object)
{
	GtkamMain *m = GTKAM_MAIN (object);

	m = NULL;

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtkam_main_finalize (GObject *object)
{
	GtkamMain *m = GTKAM_MAIN (object);

	g_free (m->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtkam_main_class_init (gpointer g_class, gpointer class_data)
{
	GtkObjectClass *object_class;
	GObjectClass *gobject_class;

	object_class = GTK_OBJECT_CLASS (g_class);
	object_class->destroy  = gtkam_main_destroy;

	gobject_class = G_OBJECT_CLASS (g_class);
	gobject_class->finalize = gtkam_main_finalize;

	parent_class = g_type_class_peek_parent (g_class);
}

static void
gtkam_main_init (GTypeInstance *instance, gpointer g_class)
{
	GtkamMain *m = GTKAM_MAIN (instance);

	m->priv = g_new0 (GtkamMainPrivate, 1);
}

GType
gtkam_main_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo ti;

		memset (&ti, 0, sizeof (GTypeInfo));
		ti.class_size     = sizeof (GtkamMainClass);
		ti.class_init     = gtkam_main_class_init;
		ti.instance_size  = sizeof (GtkamMain);
		ti.instance_init  = gtkam_main_init;

		type = g_type_register_static (PARENT_TYPE, "GtkamMain",
					       &ti, 0);
	}

	return (type);
}

static void
on_thumbnails_toggled (GtkToggleButton *toggle, GtkamMain *m)
{
	gtkam_list_set_thumbnails (GTKAM_LIST (m->priv->list), toggle->active);
}

static void
action_save_sel (gpointer callback_data, guint callback_action,
		 GtkWidget *widget)
{
	GtkamMain *m = GTKAM_MAIN (callback_data);
	
	gtkam_list_save_selected (GTKAM_LIST (m->priv->list));
}

static void
action_save_all (gpointer callback_data, guint callback_action,
		 GtkWidget *widget)
{
	GtkamMain *m = GTKAM_MAIN (callback_data);

	gtkam_list_save_all (GTKAM_LIST (m->priv->list));
}

static void
action_quit (gpointer callback_data, guint callback_action,
	     GtkWidget *widget)
{
	gtk_main_quit ();
}

#if 0
static void
on_file_deleted (GtkamDelete *delete, Camera *camera, gboolean multi,
		 const gchar *folder, const gchar *name, GtkamMain *m)
{
	gtkam_list_update_folder (m->priv->list, camera, multi, folder);
}
#endif

static void
action_delete_sel (gpointer callback_data, guint callback_action,
		   GtkWidget *widget)
{
#if 0
	GtkIconListItem *item;
	GtkamList *list = m->priv->list;
	guint i;
	GtkWidget *delete;

	if (!g_list_length (GTK_ICON_LIST (list)->selection))
		return;

	delete = gtkam_delete_new (m->priv->status);
	gtk_window_set_transient_for (GTK_WINDOW (delete), GTK_WINDOW (m));
	gtk_signal_connect (GTK_OBJECT (delete), "file_deleted",
			    GTK_SIGNAL_FUNC (on_file_deleted), m);
	for (i = 0; i < g_list_length (GTK_ICON_LIST (list)->selection); i++) {
		item = g_list_nth_data (GTK_ICON_LIST (list)->selection, i);
		gtkam_delete_add (GTKAM_DELETE (delete),
			gtk_object_get_data (GTK_OBJECT (item->entry),
				"camera"),
			GPOINTER_TO_INT (
				gtk_object_get_data (GTK_OBJECT (item->entry),
					"multi")),
			gtk_object_get_data (GTK_OBJECT (item->entry),
				"folder"),
			item->label);
	}
	gtk_widget_show (delete);
#endif
}

#if 0
static void
on_all_deleted (GtkamDelete *delete, Camera *camera, gboolean multi,
		const gchar *folder, GtkamMain *m)
{
	g_return_if_fail (GTKAM_IS_MAIN (m));

	gtkam_list_update_folder (m->priv->list, camera, multi, folder);
}
#endif

static void
action_delete_all (gpointer callback_data, guint callback_action,
		   GtkWidget *widget)
{
#if 0
	GtkWidget *delete;
	GtkamTreeItem *item;
	GList *selection;
	gint i;

	selection = GTK_TREE (m->priv->tree)->selection;
	if (!g_list_length (selection))
		return;

	delete = gtkam_delete_new (m->priv->status);
	for (i = 0; i < g_list_length (selection); i++) {
		item = g_list_nth_data (selection, i);
		gtkam_delete_add (GTKAM_DELETE (delete),
			gtkam_tree_item_get_camera (item),
			gtkam_tree_item_get_multi (item),
			gtkam_tree_item_get_folder (item), NULL);
	}
	gtk_signal_connect (GTK_OBJECT (delete), "file_deleted",
			    GTK_SIGNAL_FUNC (on_file_deleted), m);
	gtk_signal_connect (GTK_OBJECT (delete), "all_deleted",
			    GTK_SIGNAL_FUNC (on_all_deleted), m);

	gtk_window_set_transient_for (GTK_WINDOW (delete), GTK_WINDOW (m));
	gtk_widget_show (delete);
#endif
}

static void
action_select_all (gpointer callback_data, guint callback_action,
		   GtkWidget *widget)
{
#if 0
	GtkIconList *ilist = GTK_ICON_LIST (m->priv->list);
	guint i;

	for (i = 0; i < g_list_length (ilist->icons); i++)
		gtk_icon_list_select_icon (ilist,
				g_list_nth_data (ilist->icons, i));
#endif
}

static void
action_select_none (gpointer callback_data, guint callback_action,
		    GtkWidget *widget)
{
//	gtk_icon_list_unselect_all (GTK_ICON_LIST (m->priv->list));
}

static void
action_select_inverse (gpointer callback_data, guint callback_action,
		       GtkWidget *widget)
{
#if 0
	GtkIconList *ilist = GTK_ICON_LIST (m->priv->list);
	GtkIconListItem *item;
	guint i;

	for (i = 0; i < g_list_length (ilist->icons); i++) {
		item = g_list_nth_data (ilist->icons, i);
		if (item->state == GTK_STATE_SELECTED)
			gtk_icon_list_unselect_icon (ilist, item);
		else
			gtk_icon_list_select_icon (ilist, item);
	}
#endif
}

static void
on_camera_selected (GtkamChooser *chooser,
		    GtkamChooserCameraSelectedData *data,
		    GtkamMain *m)
{
	g_return_if_fail (GTKAM_IS_CHOOSER (chooser));
	g_return_if_fail (GTKAM_IS_MAIN (m));

	gtkam_tree_add_camera (GTKAM_TREE (m->priv->tree), data->camera,
			       data->multi);
}

static void
action_add_camera (gpointer callback_data, guint callback_action,
		   GtkWidget *widget)
{
	GtkWidget *dialog;
	GtkamMain *m = GTKAM_MAIN (callback_data);

	dialog = gtkam_chooser_new ();
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (m));
	gtk_widget_show (dialog);
	g_signal_connect (GTK_OBJECT (dialog), "camera_selected",
			  G_CALLBACK (on_camera_selected), m);
}

static void
gtkam_main_update_sensitivity (GtkamMain *m)
{
#if 0
	CameraAbilities a;
#endif
	guint i, s;

	i = gtkam_list_count_all (GTKAM_LIST (m->priv->list));
	s = gtkam_list_count_selected (GTKAM_LIST (m->priv->list));

	gtk_widget_set_sensitive (
		gtk_item_factory_get_widget (m->priv->factory, "/Select/None"),
		(s != 0));
	gtk_widget_set_sensitive (
		gtk_item_factory_get_widget (m->priv->factory, 
					     "/File/Delete Photos/Selected"),
		(s != 0));
	gtk_widget_set_sensitive (
		gtk_item_factory_get_widget (m->priv->factory,
					     "/File/Save Photos/Selected"),
		(s != 0));
	gtk_widget_set_sensitive (
		gtk_item_factory_get_widget (m->priv->factory, "/Select/All"),
		(s != i));
	gtk_widget_set_sensitive (
		gtk_item_factory_get_widget (m->priv->factory,
					     "/Select/Inverse"),
		(i != 0));

#if 0
	/* Camera menu */
	gtk_widget_set_sensitive (m->priv->item_summary, FALSE);
	gtk_widget_set_sensitive (m->priv->item_manual, FALSE);
	gtk_widget_set_sensitive (m->priv->item_about, FALSE);
	gtk_widget_set_sensitive (m->priv->item_config, FALSE);
	if (m->priv->camera) {
		gtk_widget_set_sensitive (m->priv->item_summary, TRUE);
		gtk_widget_set_sensitive (m->priv->item_manual, TRUE);
		gtk_widget_set_sensitive (m->priv->item_about, TRUE);

		gp_camera_get_abilities (m->priv->camera, &a);
		if (a.operations & GP_OPERATION_CONFIG)
			gtk_widget_set_sensitive (m->priv->item_config, TRUE);
	}
#endif
}

static void
on_folder_selected (GtkamTree *tree, GtkamTreeFolderSelectedData *data,
		    GtkamMain *m)
{
	gtkam_list_add_folder (GTKAM_LIST (m->priv->list), 
			       data->camera, data->multi, data->folder);
	gtkam_main_update_sensitivity (m);
}

static void
on_folder_unselected (GtkamTree *tree, GtkamTreeFolderUnselectedData *data,
		      GtkamMain *m)
{
	gtkam_list_remove_folder (GTKAM_LIST (m->priv->list),
				  data->camera, data->multi, data->folder);
	gtkam_main_update_sensitivity (m);
}

#if 0
static void
on_debug_activate (GtkMenuItem *item, GtkamMain *m)
{
	GtkWidget *debug;

	debug = gtkam_debug_new ();
	gtk_widget_show (debug);
}

static void
on_about_activate (GtkMenuItem *item, GtkamMain *m)
{
	GtkWidget *dialog;
	char buf[4096];
	
	snprintf(buf, sizeof(buf), 
		 _("%s %s\n\n"
		   "gtKam was written by:\n"
		   " - Scott Fritzinger <scottf@unr.edu>,\n"
		   " - Lutz Mueller <urc8@rz.uni-karlsruhe.de>,\n"
		   " - and many others.\n"
		   "\n"
		   "gtKam uses libgphoto2, a library to access a\n"
		   "multitude of digital cameras. More \n"
		   "information is available at\n"
		   "http://www.gphoto.net.\n"
		   "\n"
		   "Enjoy the wonderful world of gphoto!"),
		 PACKAGE, VERSION);

	dialog = gtkam_close_new (buf, GTK_WIDGET (m));
	gtk_widget_show (dialog);
}
#endif

static void
on_file_selected (GtkamList *list, GtkamListFileSelectedData *data,
		  GtkamMain *m)
{
	g_warning ("Fixme: Update sensitivity!");
}

static void
on_file_unselected (GtkamList *list, GtkamListFileUnselectedData *data,
		    GtkamMain *m)
{
	g_warning ("Fixme: Update sensitivity!");
}

static void
gtkam_main_add_status (GtkamMain *m, GtkWidget *status)
{
	g_return_if_fail (GTKAM_IS_MAIN (m));

	gtk_widget_show (status);
	gtk_box_pack_start (GTK_BOX (m->priv->status), status, FALSE, FALSE, 0);
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static void
on_new_status (GtkamTree *tree, GtkWidget *status, GtkamMain *m)
{
	gtkam_main_add_status (m, status);
}

void
gtkam_main_load (GtkamMain *m)
{
	g_return_if_fail (GTKAM_IS_MAIN (m));
	
	gtkam_tree_load (GTKAM_TREE (m->priv->tree));
}

#if 0
typedef enum _CameraTextType CameraTextType;
enum _CameraTextType {
        CAMERA_TEXT_SUMMARY,
        CAMERA_TEXT_MANUAL,
        CAMERA_TEXT_ABOUT
};

static void
on_text_activate (GtkMenuItem *i, GtkamMain *m)
{
        GtkWidget *s, *dialog;
        CameraText text;
        int result;
        CameraTextType text_type;

	if (!m->priv->camera)
		return;

        text_type = GPOINTER_TO_INT (
                gtk_object_get_data (GTK_OBJECT (i), "text_type"));

        switch (text_type) {
        case CAMERA_TEXT_SUMMARY:
                s = gtkam_status_new (
                                _("Getting information about the camera..."));
                break;
        case CAMERA_TEXT_ABOUT:
                s = gtkam_status_new (
                                _("Getting information about the driver..."));
                break;
        case CAMERA_TEXT_MANUAL:
        default:
                s = gtkam_status_new (_("Getting manual..."));
                break;
        }
	gtkam_main_add_status (m, s);

        switch (text_type) {
        case CAMERA_TEXT_SUMMARY:
                result = gp_camera_get_summary (m->priv->camera, &text,
                                GTKAM_STATUS (s)->context->context);
                break;
        case CAMERA_TEXT_ABOUT:
                result = gp_camera_get_about (m->priv->camera, &text,
                                GTKAM_STATUS (s)->context->context);
                break;
        default:
        case CAMERA_TEXT_MANUAL:
                result = gp_camera_get_manual (m->priv->camera, &text,
                                GTKAM_STATUS (s)->context->context);
                break;
        }
        if (m->priv->multi)
                gp_camera_exit (m->priv->camera, NULL);
        switch (result) {
        case GP_OK:
                dialog = gtkam_close_new (text.text, NULL);
                gtk_widget_show (dialog);
                break;
        case GP_ERROR_CANCEL:
                break;
        default:
                dialog = gtkam_error_new (result, GTKAM_STATUS (s)->context,
                        NULL, _("Could not retrieve information."));
                gtk_widget_show (dialog);
        }
        gtk_object_destroy (GTK_OBJECT (s));
}

static void
on_preferences_activate (GtkMenuItem *i, GtkamMain *m)
{
        GtkWidget *dialog;

	if (!m->priv->camera)
		return;

        dialog = gtkam_config_new (m->priv->camera, m->priv->multi, NULL);
        if (!dialog)
                return;
        gtk_widget_show (dialog);
}
#endif

static GtkItemFactoryEntry mi[] =
{
	{"/_File", NULL, 0, 0, "<Branch>"},
	{"/File/_Save Photos", NULL, 0, 0, "<Branch>"},
	{"/File/Save Photos/_Selected", NULL, action_save_sel, 0, NULL},
	{"/File/Save Photos/_All", NULL, action_save_all, 0, NULL},
	{"/File/_Delete Photos", NULL, 0, 0, "<Branch>"},
	{"/File/Delete Photos/_Selected", NULL, action_delete_sel, 0, NULL},
	{"/File/Delete Photos/_All", NULL, action_delete_all, 0, NULL},
	{"/File/sep1", NULL, 0, 0, "<Separator>"},
	{"/File/_Quit", NULL, action_quit, 0, NULL},
	{"/_Select", NULL, 0, 0, "<Branch>"},
	{"/Select/_All", NULL, action_select_all, 0, NULL},
	{"/Select/_Inverse", NULL, action_select_inverse, 0, NULL},
	{"/Select/_None", NULL, action_select_none, 0, NULL},
	{"/_Camera", NULL, 0, 0, "<Branch>"},
	{"/Camera/_Add Camera...", NULL, action_add_camera, 0, NULL},
};
static int nmi = sizeof (mi) / sizeof (mi[0]);

GtkWidget *
gtkam_main_new (void)
{
	GtkamMain *m;
	GdkPixbuf *pixbuf;
	GtkAccelGroup *ag;
	GtkItemFactory *item_factory;
	GtkWidget *widget, *vbox, *frame, *scrolled, *hpaned, *check;
#if 0
	GtkWidget *menubar, *menu, *item, *separator, *submenu;
	GtkWidget *frame, *scrolled, *check, *tree, *list, *label;
	GtkWidget *button, *hpaned, *toolbar, *icon;
	GtkAccelGroup *accel_group, *accels, *subaccels;
	GtkTooltips *tooltips;
	guint key;
#endif

	m = g_object_new (GTKAM_TYPE_MAIN, NULL);
	gtk_window_set_title (GTK_WINDOW (m), PACKAGE);
	pixbuf = gdk_pixbuf_new_from_file (IMAGE_DIR "/gtkam-camera.png", NULL);
	gtk_window_set_icon (GTK_WINDOW (m), pixbuf);
	gdk_pixbuf_unref (pixbuf);

	vbox = gtk_vbox_new (FALSE, 1);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (m), vbox);

	ag = gtk_accel_group_new ();
	item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", ag);
	g_object_set_data_full (G_OBJECT (m), "<main>", item_factory,
				(GDestroyNotify) g_object_unref);
	gtk_window_add_accel_group (GTK_WINDOW (m), ag);
	gtk_item_factory_create_items (item_factory, nmi, mi, m);
	widget = gtk_item_factory_get_widget (item_factory, "<main>");
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	m->priv->factory = GTK_ITEM_FACTORY (item_factory);

#if 0

	/* Separator */
	item = gtk_menu_item_new ();
	gtk_widget_show (item);
	gtk_menu_append (GTK_MENU (menu), item);

	/* Summary */
	m->priv->item_summary = gtk_menu_item_new_with_label (_("Summary"));
	gtk_widget_show (m->priv->item_summary);
	gtk_menu_append (GTK_MENU (menu), m->priv->item_summary);
	gtk_object_set_data (GTK_OBJECT (m->priv->item_summary), "text_type",
			     GINT_TO_POINTER (CAMERA_TEXT_SUMMARY));
	gtk_signal_connect (GTK_OBJECT (m->priv->item_summary), "activate",
			    GTK_SIGNAL_FUNC (on_text_activate), m);
	gtk_widget_set_sensitive (m->priv->item_summary, FALSE);

	/* Manual */
	m->priv->item_manual = gtk_menu_item_new_with_label (_("Manual"));
	gtk_widget_show (m->priv->item_manual);
	gtk_menu_append (GTK_MENU (menu), m->priv->item_manual);
	gtk_object_set_data (GTK_OBJECT (m->priv->item_manual), "text_type",
			     GINT_TO_POINTER (CAMERA_TEXT_MANUAL));
	gtk_signal_connect (GTK_OBJECT (m->priv->item_manual), "activate",
			    GTK_SIGNAL_FUNC (on_text_activate), m);
	gtk_widget_set_sensitive (m->priv->item_manual, FALSE);

	/* About */
	m->priv->item_about = gtk_menu_item_new_with_label (_("About "
						"the driver"));
	gtk_widget_show (m->priv->item_about);
	gtk_menu_append (GTK_MENU (menu), m->priv->item_about);
	gtk_object_set_data (GTK_OBJECT (m->priv->item_about), "text_type",
			     GINT_TO_POINTER (CAMERA_TEXT_ABOUT));
	gtk_signal_connect (GTK_OBJECT (m->priv->item_about), "activate",
			    GTK_SIGNAL_FUNC (on_text_activate), m);
	gtk_widget_set_sensitive (m->priv->item_about, FALSE);

	/* Preferences */
	m->priv->item_config = gtk_menu_item_new_with_label (_("Preferences"));
	gtk_widget_show (m->priv->item_config);
	gtk_menu_append (GTK_MENU (menu), m->priv->item_config);
	gtk_signal_connect (GTK_OBJECT (m->priv->item_config), "activate",
			    GTK_SIGNAL_FUNC (on_preferences_activate), m);
	gtk_widget_set_sensitive (m->priv->item_config, FALSE);

	/*
	 * Help menu
	 */
	item = gtk_menu_item_new_with_label ("");
	gtk_widget_show (item);
	key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (item)->child),
				     _("_Help"));
	gtk_widget_add_accelerator (item, "activate_item", accel_group,
				    key, 0, 0);
	gtk_container_add (GTK_CONTAINER (menubar), item);

	menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	accels = gtk_menu_ensure_uline_accel_group (GTK_MENU (menu));

	item = gtk_menu_item_new_with_label ("");
	gtk_widget_show (item);
	key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (item)->child),
				     _("_Debug..."));
	gtk_widget_add_accelerator (item, "activate_item", accels, key, 0, 0);
	gtk_container_add (GTK_CONTAINER (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (on_debug_activate), m);

	item = gtk_menu_item_new_with_label ("");
	gtk_widget_show (item);
	key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (item)->child),
				     _("_About..."));
	gtk_widget_add_accelerator (item, "activate_item", accels, key, 0, 0);
	gtk_container_add (GTK_CONTAINER (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (on_about_activate), m);

#endif

	/*
	 * Context information
	 */
	m->priv->status = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (m->priv->status);
	gtk_box_pack_end (GTK_BOX (vbox), m->priv->status, FALSE, FALSE, 0);

	/*
	 * Main content
	 */
	hpaned = gtk_hpaned_new ();
	gtk_widget_show (hpaned);
	gtk_box_pack_start (GTK_BOX (vbox), hpaned, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hpaned), 2);
	gtk_paned_set_position (GTK_PANED (hpaned), 200);

	/*
	 * Left
	 */
	vbox = gtk_vbox_new (FALSE, 5);
	gtk_widget_show (vbox);
	gtk_paned_pack1 (GTK_PANED (hpaned), vbox, FALSE, TRUE);
	m->priv->vbox = vbox;

	frame = gtk_frame_new (_("Index Settings"));
	gtk_widget_show (frame);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_label (_("View Thumbnails"));
	gtk_widget_show (check);
	gtk_widget_set_sensitive (check, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);
	gtk_container_add (GTK_CONTAINER (frame), check);
	g_signal_connect (G_OBJECT (check), "toggled",
			  G_CALLBACK (on_thumbnails_toggled), m);
	m->priv->toggle_preview = GTK_TOGGLE_BUTTON (check);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolled);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	m->priv->tree = gtkam_tree_new ();
	gtk_widget_show (m->priv->tree);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled),
					       m->priv->tree);
	g_signal_connect (G_OBJECT (m->priv->tree), "folder_selected",
			    G_CALLBACK (on_folder_selected), m);
	g_signal_connect (G_OBJECT (m->priv->tree), "folder_unselected",
			    G_CALLBACK (on_folder_unselected), m);
	g_signal_connect (G_OBJECT (m->priv->tree), "new_status",
			  G_CALLBACK (on_new_status), m);

	/*
	 * Right
	 */
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolled);
	gtk_paned_pack2 (GTK_PANED (hpaned), scrolled, TRUE, TRUE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	m->priv->list = gtkam_list_new ();
	gtk_widget_show (m->priv->list);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled),
					       m->priv->list);
	g_signal_connect (G_OBJECT (m->priv->list), "file_selected",
			  G_CALLBACK (on_file_selected), m);
	g_signal_connect (G_OBJECT (m->priv->list), "file_unselected",
			  G_CALLBACK (on_file_unselected), m);

	return (GTK_WIDGET (m));
}
