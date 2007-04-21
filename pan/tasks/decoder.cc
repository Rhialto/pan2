/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Pan - A Newsreader for Gtk+
 * Copyright (C) 2002-2007  Charles Kerr <charles@rebelbase.com>
 *
 * This file
 * Copyright (C) 2007 Calin Culianu <calin@ajvar.org>
 * Copyright (C) 2007 Charles Kerr <charles@rebelbase.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <config.h>
#include <algorithm>
#include <cerrno>
extern "C" {
#  define PROTOTYPES
#  include <uulib/uudeview.h>
#  include <glib/gi18n.h>
};
#include <pan/general/debug.h>
#include <pan/general/file-util.h>
#include <pan/general/foreach.h>
#include <pan/general/utf8-utils.h>
#include "decoder.h"

using namespace pan;

Decoder :: Decoder (WorkerPool& pool):
  _worker_pool (pool),
  _gsourceid (-1)
{
}

Decoder :: ~Decoder()
{
  disable_progress_update();
}

/***
****
***/

void
Decoder :: enqueue (TaskArticle                 * task,
                    const Quark                 & save_path,
                    const strings_t             & input_files,
                    const TaskArticle::SaveMode & save_mode)
{
  disable_progress_update ();

  this->task = task;
  this->save_path = save_path;
  this->input_files = input_files;
  this->save_mode = save_mode;

  mark_read = false;
  percent = 0;
  num_scanned_files = 0;
  current_file.clear ();
  log_infos.clear();
  log_errors.clear();

  // gentlemen, start your saving...
  _worker_pool.push_work (this, task, false);
}

// save article IN A WORKER THREAD to avoid network stalls
void
Decoder :: do_work()
{
  const int bufsz = 4096;
  char buf[bufsz];

  enable_progress_update();

  if (save_mode & TaskArticle::RAW)
  {
    int i = 0;
    foreach_const (strings_t, input_files, it)
    {
      if (was_cancelled()) break; // poll WorkerPool::Worker stop flag

      gchar * contents (0);
      gsize length (0);
      if (g_file_get_contents (it->c_str(), &contents, &length, NULL) && length>0)
      {
        file :: ensure_dir_exists (save_path.c_str());
        gchar * basename (g_path_get_basename (it->c_str()));
        gchar * filename (g_build_filename (save_path.c_str(), basename, NULL));
        FILE * fp = fopen (filename, "w+");

        mut.lock();
        current_file = filename;
        mut.unlock();

        if (!fp) {
          g_snprintf(buf, bufsz, _("Couldn't save file \"%s\": %s"), filename, file::pan_strerror(errno));
          log_errors.push_back (buf); // log error
        } else {
          fwrite (contents, 1, (size_t)length, fp);
          fclose (fp);
        }
        g_free (filename);
        g_free (basename);
      }
      g_free (contents);

      mut.lock();
      percent = ++i*100/input_files.size();
      mut.unlock();
    }
  }

  if (save_mode & TaskArticle::DECODE)
  {
    // decode
    int res;
    if (((res = UUInitialize())) != UURET_OK)
      log_errors.push_back(_("Error initializing uulib")); // log error
    else
    {
      UUSetMsgCallback (this, uu_log);
      UUSetOption (UUOPT_DESPERATE, 1, NULL); // keep incompletes; they're useful to par2
      UUSetBusyCallback (this, uu_busy_poll, 500); // .5 secs busy poll?

      int i (0);
      foreach_const (strings_t, input_files, it)
      {
        if (was_cancelled()) break;
        if ((res = UULoadFileWithPartNo (const_cast<char*>(it->c_str()), 0, 0, ++i)) != UURET_OK) {
          g_snprintf(buf, bufsz,
                     _("Error reading from %s: %s"),
                     it->c_str(),
                     (res==UURET_IOERR)
                     ?  file::pan_strerror (UUGetOption (UUOPT_ERRNO, NULL,
                                                         NULL, 0))
                     : UUstrerror(res));
          log_errors.push_back(buf); // log error
        }

        mut.lock();
        num_scanned_files = i;
        mut.unlock();
      }

      uulist * item;
      i = 0;
      while ((item = UUGetFileListItem (i++)))
      {
        if (was_cancelled()) break; // poll WorkerPool::Worker stop flag

        // make sure the directory exists...
        if (!save_path.empty())
          file :: ensure_dir_exists (save_path.c_str());

        // find a unique filename...
        char * fname = file::get_unique_fname(save_path.c_str(), 
                                              (item->filename 
                                               && *item->filename)
                                              ? item->filename
                                              : "pan-saved-file" );

        // decode the file...
        if ((res = UUDecodeFile (item, fname)) == UURET_OK) {
          g_snprintf(buf, bufsz,_("Saved \"%s\""), fname);
          log_infos.push_back(buf); // log info
        } else if (res == UURET_NODATA) {
          // silently let this error by... user probably tried to
          // save attachements on a text-only post
        } else {
          const int the_errno (UUGetOption (UUOPT_ERRNO, NULL, NULL, 0));
          if (res==UURET_IOERR && the_errno==ENOSPC) {
            g_snprintf (buf, bufsz, _("Error saving \"%s\":\n%s. %s"), fname, file::pan_strerror(the_errno), "ENOSPC");
            log_severe.push_back(buf);
          } else {
            g_snprintf (buf, bufsz,_("Error saving \"%s\":\n%s."),
                             fname,
                             res==UURET_IOERR ? file::pan_strerror(the_errno) : UUstrerror(res));
            log_errors.push_back(buf); // log error
          }
        }

        // cleanup
        g_free (fname);

      }

      mark_read = true;
    }
    UUCleanUp ();
  }

  disable_progress_update();
}

/***
****
***/

void
Decoder :: uu_log (void* data, char* message, int severity)
{
  Decoder *self = static_cast<Decoder *>(data);
  char * pch (g_locale_to_utf8 (message, -1, 0, 0, 0));

  if (severity==UUMSG_PANIC || severity==UUMSG_FATAL)
    self->log_severe.push_back (pch ? pch : message);
  else if (severity==UUMSG_ERROR)
    self->log_errors.push_back (pch ? pch : message);
  else if (severity==UUMSG_WARNING || severity==UUMSG_NOTE)
    self->log_infos.push_back (pch ? pch : message);

  g_free (pch);
}

double
Decoder :: get_percentage (const uuprogress& p) const
{
  // These should add up to 100.
  // We can tweak these as needed.  Calin sees more time spent
  // in COPYING, but I'm seeing it in DECODING, so I've split
  // the difference here and given them the same weight.
  static const double WEIGHT_SCANNING = 10;
  static const double WEIGHT_DECODING = 45;
  static const double WEIGHT_COPYING = 45;

  double base = 0;

  if (p.action != UUACT_SCANNING)
    base += WEIGHT_SCANNING;
  else {
    const double percent = (100.0 * num_scanned_files + p.percent) / input_files.size();
    return base + (percent / (100.0/WEIGHT_SCANNING));
  }

  if (p.action != UUACT_DECODING)
    base += WEIGHT_DECODING;
  else {
    // uudeview's documentation is wrong:
    // the total percentage isn't (100*partno-percent)/numparts,
    // it's (100*(partno-1) + percent)/numparts
    const double percent = ((100.0 * (p.partno-1)) + p.percent) / p.numparts;
    return base + (percent / (100.0/WEIGHT_DECODING));
  }

  if (p.action != UUACT_COPYING)
    base += WEIGHT_COPYING;
  else {
    const double percent = p.percent;
    return base + (percent / (100.0/WEIGHT_COPYING));
  }

  return 0;
}

int
Decoder :: uu_busy_poll (void * d, uuprogress *p)
{
  Decoder * self (static_cast<Decoder*>(d));
  self->mut.lock();
  self->percent = self->get_percentage(*p);
  self->current_file = p->curfile;
  self->mut.unlock();

  return self->was_cancelled(); // returning true tells uulib to abort
}

// this is called in the main thread
gboolean
Decoder :: progress_update_timer_func (gpointer decoder)
{
  Decoder *self = static_cast<Decoder *>(decoder);
  Task *task = self->task;
  if (!task || self->was_cancelled()) return false;

  self->mut.lock();
  const double percent (self->percent);
  const std::string f (content_to_utf8 (self->current_file));
  self->mut.unlock();

  task->set_step(int(percent));
  task->set_status_va (_("Decoding %s"), f.c_str());

  return true; // keep timer func running
}

/***
****
***/

void
Decoder :: enable_progress_update ()
{
  if (_gsourceid == -1)
      _gsourceid = g_timeout_add(500, progress_update_timer_func, this);
}

void
Decoder :: disable_progress_update ()
{
  if (_gsourceid > -1) {
    g_source_remove (_gsourceid);
    _gsourceid = -1;
  }
}
