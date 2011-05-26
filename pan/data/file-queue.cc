/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Pan - A Newsreader for Gtk+
 * Copyright (C) 2002-2006  Charles Kerr <charles@rebelbase.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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


#include <algorithm>

#include <config.h>
#include <pan/data/article.h>
#include <pan/data/data.h>
#include <pan/data/xref.h>
#include <pan/tasks/nntp.h>
#include <pan/tasks/task.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file-queue.h"

using namespace pan;

FileQueue :: ~FileQueue ()
{}

FileQueue :: FileQueue() {}

void
FileQueue :: add (const char* filename,
                  FileQueue::InsertType type)
{
  static FileData a;
  struct stat sb;
  a.filename = filename;
  a.basename = g_path_get_basename(filename);
  stat(filename,&sb);
  a.byte_count = sb.st_size;
  a.part_in_queue = type == END ? _articles_v.size() : 1;

  type == END ?
    _articles_v.push_back(a) :
    _articles_v.push_front(a);
}




