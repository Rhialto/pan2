//========================================================================
/**@file	pan/pan/gui/gtk_compat.h
 * @author	kid
 * @date
 * 	Created:	Mon 09 May 2011 04:42:46 PM MDT \n
 * 	Last Update:	Mon 09 May 2011 04:42:46 PM MDT
 */
/*------------------------------------------------------------------------
 * Description:	«description»
 * 
 *========================================================================
 */

#ifndef PAN_GTK_COMPAT_H
#define PAN_GTK_COMPAT_H

#ifdef __cplusplus
namespace
{
#endif
#if !GTK_CHECK_VERSION(2,18,0)
  void gtk_widget_get_allocation( GtkWidget *w, GtkAllocation *a)
  {
    *a = w->allocation;
  }
  bool gtk_widget_has_focus( GtkWidget *w)
  {
    return GTK_WIDGET_HAS_FOCUS(w);
  }
  bool gtk_widget_get_sensitive( GtkWidget *w)
  {
    return GTK_WIDGET_SENSITIVE(w);
  }
  bool gtk_widget_get_visible(GtkWidget *w)
  {
    return GTK_WIDGET_VISIBLE(w);
  }
  bool gtk_widget_is_toplevel(GtkWidget *w)
  {
    return GTK_WIDGET_TOPLEVEL(w);
  }
#endif

#if !GTK_CHECK_VERSION(2,20,0)
  gboolean gtk_widget_get_realized(GtkWidget *w)
  {
    return GTK_WIDGET_REALIZED(w);
  }
#endif

#ifdef GTK_DISABLE_DEPRECATED
#if GTK_CHECK_VERSION(2,22,0)
#define GtkNotebookPage void
#endif
#endif

#if !GTK_CHECK_VERSION(2,24,0)
#define GTK_COMBO_BOX_TEXT(cb) GTK_COMBO_BOX(cb)
  typedef GtkComboBox GtkComboBoxText;
  GtkWidget* gtk_combo_box_text_new()
  {
    return gtk_combo_box_new_text();
  }
  GtkWidget* gtk_combo_box_text_new_with_entry()
  {
    return gtk_combo_box_entry_new_text();
  }
  void gtk_combo_box_text_append_text(GtkComboBoxText *cb, const gchar *t)
  {
    gtk_combo_box_append_text(cb,t);
  }
  gchar *gtk_combo_box_text_get_active_text(GtkComboBoxText *cb)
  {
    return gtk_combo_box_get_active_text(cb);
  }
  void gtk_combo_box_text_remove(GtkComboBoxText *cb, int p)
  {
    gtk_combo_box_remove_text(cb, p);
  }
#endif
#ifdef __cplusplus
}
#endif
  
#endif