/**
 * @file
 * A simple wrapper for the Pager
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2010,2012-2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page pager_dopager A simple wrapper for the Pager
 *
 * A simple wrapper for the Pager
 */

#include "config.h"
#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "context.h"
#include "protos.h"

/**
 * dopager_config_observer - Listen for config changes affecting the dopager menus - Implements ::observer_t
 */
static int dopager_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ev_c = nc->event_data;
  if (!mutt_str_equal(ev_c->name, "status_on_top"))
    return 0;

  struct MuttWindow *dlg = nc->global_data;
  window_status_on_top(dlg, NeoMutt->sub);
  return 0;
}

/**
 * mutt_do_pager - Display some page-able text to the user (help or attachment)
 * @param pview PagerView to construct Pager object
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_do_pager(struct PagerView *pview)
{
  assert(pview);
  assert(pview->pdata);
  assert(pview->pdata->fname);
  assert((pview->mode == PAGER_MODE_ATTACH) ||
         (pview->mode == PAGER_MODE_HELP) || (pview->mode == PAGER_MODE_OTHER));

  struct MuttWindow *dlg =
      mutt_window_new(WT_DLG_DO_PAGER, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct IndexSharedData *shared = index_shared_data_new();
  shared->ctx = pview->pdata->ctx;
  shared->mailbox = shared->ctx ? shared->ctx->mailbox : NULL;
  shared->account = shared->mailbox ? shared->mailbox->account : NULL;
  shared->email = pview->pdata->email;
  notify_set_parent(shared->notify, dlg->notify);

  dlg->wdata = shared;
  dlg->wdata_free = index_shared_data_free;

  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
  struct MuttWindow *panel_pager = create_panel_pager(c_status_on_top, shared);
  dlg->focus = panel_pager;
  mutt_window_add_child(dlg, panel_pager);

  notify_observer_add(NeoMutt->notify, NT_CONFIG, dopager_config_observer, dlg);
  dialog_push(dlg);

  pview->win_ibar = NULL;
  pview->win_index = NULL;
  pview->win_pbar = mutt_window_find(panel_pager, WT_STATUS_BAR);
  pview->win_pager = mutt_window_find(panel_pager, WT_MENU);

  int rc;

  const char *const c_pager = cs_subset_string(NeoMutt->sub, "pager");
  if (!c_pager || mutt_str_equal(c_pager, "builtin"))
  {
    rc = mutt_pager(pview);
  }
  else
  {
    struct Buffer *cmd = mutt_buffer_pool_get();

    mutt_endwin();
    mutt_buffer_file_expand_fmt_quote(cmd, c_pager, pview->pdata->fname);
    if (mutt_system(mutt_buffer_string(cmd)) == -1)
    {
      mutt_error(_("Error running \"%s\""), mutt_buffer_string(cmd));
      rc = -1;
    }
    else
      rc = 0;
    mutt_file_unlink(pview->pdata->fname);
    mutt_buffer_pool_release(&cmd);
  }

  dialog_pop();
  notify_observer_remove(NeoMutt->notify, dopager_config_observer, dlg);
  mutt_window_free(&dlg);
  return rc;
}
