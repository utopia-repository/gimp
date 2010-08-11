/*
 * This is a plug-in for the GIMP.
 *
 * Generates clickable image maps.
 *
 * Copyright (C) 1998-2002 Maurits Rijk  lpeek.mrijk@consunet.nl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <gtk/gtk.h>

#include "imap_command.h"
#include "imap_menu_funcs.h"

static GtkAccelGroup *accelerator_group;

void
init_accel_group(GtkWidget *window)
{
   accelerator_group = gtk_accel_group_new();
   gtk_window_add_accel_group(GTK_WINDOW(window), accelerator_group);
}

void
add_accelerator(GtkWidget *widget, guint accelerator_key,
		guint8 accelerator_mods)
{
   gtk_widget_add_accelerator(widget, "activate", accelerator_group,
			      accelerator_key, accelerator_mods,
			      GTK_ACCEL_VISIBLE);
}

static GtkWidget*
append_item(GtkWidget *parent, GtkWidget *item)
{
   gtk_menu_shell_append(GTK_MENU_SHELL(parent), item);
   gtk_widget_show(item);
   return item;
}

static GtkWidget*
append_active_item(GtkWidget *parent, GtkWidget *item, MenuCallback activate,
		   gpointer data)
{
   g_signal_connect(item, "activate", G_CALLBACK(activate), data);
   return append_item(parent, item);
}

/* Fix me: check if 'gpointer data' can work */

GtkWidget*
make_item_with_label(GtkWidget *parent, gchar *label, MenuCallback activate,
		     gpointer data)
{
   return append_active_item(parent, gtk_menu_item_new_with_mnemonic(label),
			     activate, data);
}

GtkWidget*
make_item_with_image(GtkWidget *parent, const gchar *stock_id, 
		     MenuCallback activate, gpointer data)
{
   return append_active_item(parent, 
			     gtk_image_menu_item_new_from_stock(
				stock_id, accelerator_group),
			     activate, data);
}

GtkWidget*
prepend_item_with_label(GtkWidget *parent, gchar *label,
			MenuCallback activate, gpointer data)
{
   GtkWidget *item = gtk_menu_item_new_with_mnemonic(label);
   gtk_menu_shell_prepend(GTK_MENU_SHELL(parent), item);
   g_signal_connect(item, "activate", G_CALLBACK(activate), data);
   gtk_widget_show(item);

   return item;
}

GtkWidget*
insert_item_with_label(GtkWidget *parent, gint position, gchar *label,
		       MenuCallback activate, gpointer data)
{
   GtkWidget *item = gtk_image_menu_item_new_with_mnemonic(label);
   gtk_menu_shell_insert(GTK_MENU_SHELL(parent), item, position);
   g_signal_connect(item, "activate", G_CALLBACK(activate), data);
   gtk_widget_show(item);

   return item;
}

GtkWidget*
make_check_item(GtkWidget *parent, gchar *label, MenuCallback activate,
		gpointer data)
{
   return append_active_item(
      parent, gtk_check_menu_item_new_with_mnemonic(label),
      activate, data);
}

GtkWidget*
make_radio_item(GtkWidget *parent, GSList *group, gchar *label,
		MenuCallback activate, gpointer data)
{
   return append_active_item(
      parent, gtk_radio_menu_item_new_with_mnemonic(group, label),
      activate, data);
}

void
make_separator(GtkWidget *parent)
{
   (void) append_item(parent, gtk_menu_item_new());
}

void
insert_separator(GtkWidget *parent, gint position)
{
   GtkWidget *item = gtk_menu_item_new();
   gtk_menu_shell_insert(GTK_MENU_SHELL(parent), item, position);
   gtk_widget_show(item);
}

GtkWidget*
make_sub_menu(GtkWidget *parent, gchar *label)
{
   GtkWidget *sub_menu = gtk_menu_new();
   GtkWidget *item;

   item = append_item(parent, gtk_menu_item_new_with_mnemonic(label));
   gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), sub_menu);

   return sub_menu;
}

GtkWidget*
make_menu_bar_item(GtkWidget *menu_bar, gchar *label)
{
   GtkWidget *menu = gtk_menu_new();
   GtkWidget *item = gtk_menu_item_new_with_mnemonic(label);
   GtkWidget *tearoff = gtk_tearoff_menu_item_new();

   gtk_menu_shell_insert(GTK_MENU_SHELL(menu), tearoff, 0);
   gtk_widget_show(tearoff);
   gtk_widget_show(item);

   gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
   gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), item);
   gtk_menu_set_accel_group(GTK_MENU(menu), accelerator_group);

   return menu;
}

void
menu_command(GtkWidget *widget, gpointer data)
{
   CommandFactory_t *factory = (CommandFactory_t*) data;
   Command_t *command = (*factory)();
   command_execute(command);
}
