/**
 * @file
 * Compose Attachments
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page compose_attach Compose Attachments
 *
 * Compose Attachments
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "menu/lib.h"
#include "send/lib.h"
#include "attach_data.h"
#include "format_flags.h"
#include "muttlib.h"
#include "recvattach.h"
#include "shared_data.h"

/**
 * cum_attachs_size - Cumulative Attachments Size
 * @param sub   Config Subset
 * @param adata Attachment data
 * @retval num Bytes in attachments
 *
 * Returns the total number of bytes used by the attachments in the attachment
 * list _after_ content-transfer-encodings have been applied.
 */
unsigned long cum_attachs_size(struct ConfigSubset *sub, struct ComposeAttachData *adata)
{
  if (!adata || !adata->actx)
    return 0;

  size_t s = 0;
  struct Content *info = NULL;
  struct Body *b = NULL;
  struct AttachCtx *actx = adata->actx;
  struct AttachPtr **idx = actx->idx;

  for (unsigned short i = 0; i < actx->idxlen; i++)
  {
    b = idx[i]->body;

    if (!b->content)
      b->content = mutt_get_content_info(b->filename, b, sub);

    info = b->content;
    if (info)
    {
      switch (b->encoding)
      {
        case ENC_QUOTED_PRINTABLE:
          s += 3 * (info->lobin + info->hibin) + info->ascii + info->crlf;
          break;
        case ENC_BASE64:
          s += (4 * (info->lobin + info->hibin + info->ascii + info->crlf)) / 3;
          break;
        default:
          s += info->lobin + info->hibin + info->ascii + info->crlf;
          break;
      }
    }
  }

  return s;
}

/**
 * attach_recalc - Recalculate the Window data - Implements MuttWindow::recalc()
 */
int attach_recalc(struct MuttWindow *win)
{
#if 0
  struct ComposeBarData *attach_data = win->wdata;
  struct ComposeRedrawData *rd = attach_data->rd;

  char buf[1024] = { 0 };
  const char *const c_compose_format =
      cs_subset_string(rd->sub, "compose_format");
  mutt_expando_format(buf, sizeof(buf), 0, win->state.cols, NONULL(c_compose_format),
                      compose_format_str, (intptr_t) rd->menu, MUTT_FORMAT_NO_FLAGS);

  if (!mutt_str_equal(buf, attach_data->compose_format))
  {
    mutt_str_replace(&attach_data->compose_format, buf);
    win->actions |= WA_REPAINT;
  }
#endif
  win->actions |= WA_REPAINT;
  return 0;
}

/**
 * attach_repaint - Repaint the Window - Implements MuttWindow::repaint()
 */
int attach_repaint(struct MuttWindow *win)
{
  if (!mutt_window_is_visible(win))
    return 0;
#if 0
  struct ComposeBarData *attach_data = win->wdata;

  mutt_window_move(win, 0, 0);
  mutt_curses_set_color(MT_COLOR_STATUS);
  mutt_window_clrtoeol(win);

  mutt_window_move(win, 0, 0);
  mutt_draw_statusline(win->state.cols, attach_data->compose_format,
                       mutt_str_len(attach_data->compose_format));
  mutt_curses_set_color(MT_COLOR_NORMAL);
#endif
  return 0;
}

/**
 * attach_compose_observer - Listen for compose changes affecting the attachments - Implements ::observer_t
 */
static int attach_compose_observer(struct NotifyCallback *nc)
{
  if (!nc->global_data)
    return -1;
  if (nc->event_type != NT_COMPOSE)
    return 0;
  if (nc->event_subtype != NT_COMPOSE_ATTACH)
    return 0;

  struct MuttWindow *win_attach = nc->global_data;

  win_attach->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "compose, request WA_RECALC\n");

  return 0;
}

/**
 * attach_config_observer - Listen for changes to the Config - Implements ::observer_t
 */
int attach_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (nc->event_subtype == NT_CONFIG_INITIAL_SET)
    return 0;

  struct EventConfig *ec = nc->event_data;
  if (!mutt_str_equal(ec->name, "attach_format"))
    return 0;

  struct MuttWindow *win_attach = nc->global_data;
  win_attach->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "config, request WA_RECALC\n");

  return 0;
}

/**
 * attach_window_observer - Listen for window changes affecting the attachelope - Implements ::observer_t
 */
static int attach_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->event_data || !nc->global_data)
    return -1;

  struct MuttWindow *win_attach = nc->global_data;

  if (nc->event_subtype == NT_WINDOW_STATE)
  {
    mutt_debug(LL_DEBUG5, "state, request WA_RECALC\n");
    win_attach->actions |= WA_RECALC;
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    mutt_debug(LL_DEBUG5, "delete\n");
    notify_observer_remove(nc->current, attach_compose_observer, win_attach);
    notify_observer_remove(nc->current, attach_config_observer, win_attach);
    notify_observer_remove(nc->current, attach_window_observer, win_attach);
  }

  return 0;
}

/**
 * compose_menu_repaint - Repaint the Compose Window - Implements MuttWindow::repaint()
 */
static int compose_menu_repaint(struct MuttWindow *win)
{
  if (win->type != WT_MENU)
    return 0;

  struct Menu *menu = win->wdata;

  if (menu->redraw & MENU_REDRAW_FLOW)
  {
    mutt_window_reflow(dialog_find(menu->win_index));
  }

  menu_check_recenter(menu);

  if (menu->redraw & MENU_REDRAW_INDEX)
    menu_redraw_index(menu);
  else if (menu->redraw & (MENU_REDRAW_MOTION | MENU_REDRAW_MOTION))
    menu_redraw_motion(menu);
  else if (menu->redraw == MENU_REDRAW_CURRENT)
    menu_redraw_current(menu);

  menu->redraw = MENU_REDRAW_NO_FLAGS;
  return 0;
}

/**
 * compose_attach_tag - Tag an attachment - Implements Menu::tag()
 */
static int compose_attach_tag(struct Menu *menu, int sel, int act)
{
  struct ComposeAttachData *adata = menu->mdata;
  struct AttachCtx *actx = adata->actx;
  struct Body *cur = actx->idx[actx->v2r[sel]]->body;
  bool ot = cur->tagged;

  cur->tagged = ((act >= 0) ? act : !cur->tagged);
  return cur->tagged - ot;
}

/**
 * compose_make_entry - Format a menu item for the attachment list - Implements Menu::make_entry()
 */
static void compose_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  struct ComposeAttachData *adata = menu->mdata;
  struct AttachCtx *actx = adata->actx;
  struct ComposeSharedData *shared = menu->win_index->parent->wdata;
  struct ConfigSubset *sub = shared->sub;

  const char *const c_attach_format = cs_subset_string(sub, "attach_format");
  mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols, NONULL(c_attach_format),
                      attach_format_str, (intptr_t) (actx->idx[actx->v2r[line]]),
                      MUTT_FORMAT_STAT_FILE | MUTT_FORMAT_ARROWCURSOR);
}

/**
 * attach_new - Create the Attachments Menu
 * @param parent Parent Window
 * @param shared Shared compose data
 */
struct MuttWindow *attach_new(struct MuttWindow *parent, struct ComposeSharedData *shared)
{
  struct MuttWindow *win_attach = menu_new_window(MENU_COMPOSE, NeoMutt->sub);

  struct ComposeAttachData *adata = attach_data_new(shared->email);

  shared->adata = adata;

  // win_attach->recalc = attach_recalc;
  // win_attach->repaint = attach_repaint;

  // NT_COLOR is handled by the Menu Window
  notify_observer_add(parent->notify, NT_COMPOSE, attach_compose_observer, win_attach);
  notify_observer_add(NeoMutt->notify, NT_CONFIG, attach_config_observer, win_attach);
  notify_observer_add(win_attach->notify, NT_WINDOW, attach_window_observer, win_attach);

  struct Menu *menu = win_attach->wdata;
  menu->pagelen = win_attach->state.rows;
  menu->win_index = win_attach;

  menu->make_entry = compose_make_entry;
  menu->tag = compose_attach_tag;
  menu->mdata = adata;
  menu->mdata_free = attach_data_free;
  adata->menu = menu;

  //QWQ Override the Menu's repaint function
  menu->win_index->repaint = compose_menu_repaint;

  return win_attach;
}
