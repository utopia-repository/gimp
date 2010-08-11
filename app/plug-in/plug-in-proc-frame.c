/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * plug-in-proc-frame.c
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
 */

#include "config.h"

#include <string.h>

#include <glib-object.h>

#include "plug-in-types.h"

#include "core/gimpcontext.h"
#include "core/gimpprogress.h"

#include "pdb/gimpprocedure.h"

#include "plug-in-proc-frame.h"
#include "plug-in-progress.h"


static void  plug_in_proc_frame_free (PlugInProcFrame *proc_frame,
                                      PlugIn          *plug_in);


/*  publuc functions  */

PlugInProcFrame *
plug_in_proc_frame_new (GimpContext   *context,
                        GimpProgress  *progress,
                        GimpProcedure *procedure)
{
  PlugInProcFrame *proc_frame;

  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (progress == NULL || GIMP_IS_PROGRESS (progress), NULL);
  g_return_val_if_fail (GIMP_IS_PROCEDURE (procedure), NULL);

  proc_frame = g_new0 (PlugInProcFrame, 1);

  proc_frame->ref_count = 1;

  plug_in_proc_frame_init (proc_frame, context, progress, procedure);

  return proc_frame;
}

void
plug_in_proc_frame_init (PlugInProcFrame *proc_frame,
                         GimpContext     *context,
                         GimpProgress    *progress,
                         GimpProcedure   *procedure)
{
  g_return_if_fail (proc_frame != NULL);
  g_return_if_fail (GIMP_IS_CONTEXT (context));
  g_return_if_fail (progress == NULL || GIMP_IS_PROGRESS (progress));

  proc_frame->main_context       = g_object_ref (context);
  proc_frame->context_stack      = NULL;
  proc_frame->procedure          = procedure;
  proc_frame->main_loop          = NULL;
  proc_frame->return_vals        = NULL;
  proc_frame->progress           = progress ? g_object_ref (progress) : NULL;
  proc_frame->progress_created   = FALSE;
  proc_frame->progress_cancel_id = 0;
}

void
plug_in_proc_frame_dispose (PlugInProcFrame *proc_frame,
                            PlugIn          *plug_in)
{
  g_return_if_fail (proc_frame != NULL);
  g_return_if_fail (plug_in != NULL);

  if (proc_frame->progress)
    {
      plug_in_progress_end (plug_in);

      if (proc_frame->progress)
        {
          g_object_unref (proc_frame->progress);
          proc_frame->progress = NULL;
        }
    }

  if (proc_frame->context_stack)
    {
      g_list_foreach (proc_frame->context_stack, (GFunc) g_object_unref, NULL);
      g_list_free (proc_frame->context_stack);
      proc_frame->context_stack = NULL;
    }

  if (proc_frame->main_context)
    {
      g_object_unref (proc_frame->main_context);
      proc_frame->main_context = NULL;
    }
}

static void
plug_in_proc_frame_free (PlugInProcFrame *proc_frame,
                         PlugIn          *plug_in)
{
  g_return_if_fail (proc_frame != NULL);
  g_return_if_fail (plug_in != NULL);

  plug_in_proc_frame_dispose (proc_frame, plug_in);

  g_free (proc_frame);
}

PlugInProcFrame *
plug_in_proc_frame_ref (PlugInProcFrame *proc_frame)
{
  g_return_val_if_fail (proc_frame != NULL, NULL);

  proc_frame->ref_count++;

  return proc_frame;
}

void
plug_in_proc_frame_unref (PlugInProcFrame *proc_frame,
                          PlugIn          *plug_in)
{
  g_return_if_fail (proc_frame != NULL);
  g_return_if_fail (plug_in != NULL);

  proc_frame->ref_count--;

  if (proc_frame->ref_count < 1)
    plug_in_proc_frame_free (proc_frame, plug_in);
}

GValueArray *
plug_in_proc_frame_get_return_vals (PlugInProcFrame *proc_frame)
{
  GValueArray *return_vals;

  g_return_val_if_fail (proc_frame != NULL, NULL);

  if (proc_frame->return_vals &&
      proc_frame->return_vals->n_values ==
      proc_frame->procedure->num_values + 1)
    {
      return_vals = proc_frame->return_vals;
    }
  else if (proc_frame->return_vals)
    {
      /* Allocate new return values of the correct size. */
      return_vals = gimp_procedure_get_return_values (proc_frame->procedure,
                                                      FALSE);

      /* Copy all of the arguments we can. */
      memcpy (return_vals->values, proc_frame->return_vals->values,
              sizeof (GValue) * MIN (proc_frame->return_vals->n_values,
                                     proc_frame->procedure->num_values + 1));

      /* Free the old argument pointer.  This will cause a memory leak
       * only if there were more values returned than we need (which
       * shouldn't ever happen).
       */
      g_free (proc_frame->return_vals);
    }
  else
    {
      /* Just return a dummy set of values. */
      return_vals = gimp_procedure_get_return_values (proc_frame->procedure,
                                                      FALSE);
    }

  /* We have consumed any saved values, so clear them. */
  proc_frame->return_vals = NULL;

  return return_vals;
}
