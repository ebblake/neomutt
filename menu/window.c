/**
 * @file
 * Window wrapper around a Menu
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
 * @page menu_window Window wrapper around a Menu
 *
 * Window wrapper around a Menu
 */

#include "config.h"
#include "private.h"
#include "gui/lib.h"
#include "lib.h"
#include "type.h"

struct ConfigSubset;

/**
 * menu_recalc - Recalculate the Window data - Implements MuttWindow::recalc()
 */
static int menu_recalc(struct MuttWindow *win)
{
  if (win->type != WT_MENU)
    return 0;

  // struct Menu *menu = win->wdata;

  win->actions |= WA_REPAINT;
  return 0;
}

/**
 * menu_repaint - Repaint the Window - Implements MuttWindow::repaint()
 */
static int menu_repaint(struct MuttWindow *win)
{
  if (win->type != WT_MENU)
    return 0;

  struct Menu *menu = win->wdata;

  if (menu->redraw & MENU_REDRAW_INDEX)
    menu_redraw_index(menu);
  else if (menu->redraw & MENU_REDRAW_MOTION)
    menu_redraw_motion(menu);
  else if (menu->redraw == MENU_REDRAW_CURRENT)
    menu_redraw_current(menu);

  menu->redraw = MENU_REDRAW_NO_FLAGS;
  return 0;
}

/**
 * menu_window_observer - Listen for window changes affecting the Menu - Implements ::observer_t
 */
static int menu_window_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_WINDOW)
    return 0;

  struct MuttWindow *win = nc->global_data;
  win->actions |= WA_RECALC | WA_REPAINT;
  return 0;
}

/**
 * menu_wdata_free - Destroy a Menu Window - Implements MuttWindow::wdata_free()
 */
static void menu_wdata_free(struct MuttWindow *win, void **ptr)
{
  notify_observer_remove(win->notify, menu_window_observer, win);
  menu_free((struct Menu **) ptr);
}

/**
 * menu_new_window - Create a new Menu Window
 * @param type Menu type, e.g. #MENU_PAGER
 * @param sub  Config items
 * @retval ptr New MuttWindow wrapping a Menu
 */
struct MuttWindow *menu_new_window(enum MenuType type, struct ConfigSubset *sub)
{
  struct MuttWindow *win =
      mutt_window_new(WT_MENU, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct Menu *menu = menu_new(type, win, sub);

  win->recalc = menu_recalc;
  win->repaint = menu_repaint;
  win->wdata = menu;
  win->wdata_free = menu_wdata_free;

  notify_observer_add(win->notify, NT_WINDOW, menu_window_observer, win);

  menu->win_index = win;

  return win;
}
