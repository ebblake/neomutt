/**
 * @file
 * Display a guide to Patterns
 *
 * @authors
 * Copyright (C) 1996-2000,2006-2007,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page pattern_dlgpattern Display a guide to Patterns
 *
 * Display a guide to Patterns
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "menu/lib.h"
#include "format_flags.h"
#include "muttlib.h"
#include "opcodes.h"

/**
 * struct PatternEntry - A line in the Pattern Completion menu
 */
struct PatternEntry
{
  int num;          ///< Index number
  const char *tag;  ///< Copied to buffer if selected
  const char *expr; ///< Displayed in the menu
  const char *desc; ///< Description of pattern
};

/// Help Bar for the Pattern selection dialog
static const struct Mapping PatternHelp[] = {
  // clang-format off
  { N_("Exit"),   OP_EXIT },
  { N_("Select"), OP_GENERIC_SELECT_ENTRY },
  { N_("Help"),   OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/**
 * pattern_format_str - Format a string for the pattern completion menu - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:----------------------
 * | \%d     | Pattern description
 * | \%e     | Pattern expression
 * | \%n     | Index number
 */
static const char *pattern_format_str(char *buf, size_t buflen, size_t col, int cols,
                                      char op, const char *src, const char *prec,
                                      const char *if_str, const char *else_str,
                                      intptr_t data, MuttFormatFlags flags)
{
  struct PatternEntry *entry = (struct PatternEntry *) data;

  switch (op)
  {
    case 'd':
      mutt_format_s(buf, buflen, prec, NONULL(entry->desc));
      break;
    case 'e':
      mutt_format_s(buf, buflen, prec, NONULL(entry->expr));
      break;
    case 'n':
    {
      char tmp[32];
      snprintf(tmp, sizeof(tmp), "%%%sd", prec);
      snprintf(buf, buflen, tmp, entry->num);
      break;
    }
  }

  return src;
}

/**
 * make_pattern_entry - Create a line for the Pattern Completion menu - Implements Menu::make_entry()
 */
static void make_pattern_entry(struct Menu *menu, char *buf, size_t buflen, int num)
{
  struct PatternEntry *entry = &((struct PatternEntry *) menu->mdata)[num];

  const char *const c_pattern_format =
      cs_subset_string(NeoMutt->sub, "pattern_format");
  mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols,
                      NONULL(c_pattern_format), pattern_format_str,
                      (intptr_t) entry, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * free_pattern_menu - Free the Pattern Completion menu - Implements Menu::mdata_free()
 */
static void free_pattern_menu(struct Menu *menu, void **ptr)
{
  struct PatternEntry *entries = *ptr;

  for (size_t i = 0; i < menu->max; i++)
  {
    FREE(&entries[i].tag);
    FREE(&entries[i].expr);
    FREE(&entries[i].desc);
  }

  FREE(ptr);
}

/**
 * create_pattern_menu - Create the Pattern Completion menu
 * @param dlg Dialog holding the Menu
 * @retval ptr New Menu
 */
static struct Menu *create_pattern_menu(struct MuttWindow *dlg)
{
  int num_entries = 0, i = 0;
  struct Buffer *entrybuf = NULL;

  while (Flags[num_entries].tag)
    num_entries++;
  /* Add three more hard-coded entries */
  num_entries += 3;
  struct PatternEntry *entries =
      mutt_mem_calloc(num_entries, sizeof(struct PatternEntry));

  struct Menu *menu = dlg->wdata;
  menu->make_entry = make_pattern_entry;
  menu->mdata = entries;
  menu->mdata_free = free_pattern_menu;
  menu->max = num_entries;

  struct MuttWindow *sbar = mutt_window_find(dlg, WT_STATUS_BAR);
  // L10N: Pattern completion menu title
  sbar_set_title(sbar, _("Patterns"));

  entrybuf = mutt_buffer_pool_get();
  while (Flags[i].tag)
  {
    entries[i].num = i + 1;

    mutt_buffer_printf(entrybuf, "~%c", (char) Flags[i].tag);
    entries[i].tag = mutt_str_dup(mutt_buffer_string(entrybuf));

    switch (Flags[i].eat_arg)
    {
      case EAT_REGEX:
        /* L10N:
           Pattern Completion Menu argument type: a regular expression
        */
        mutt_buffer_add_printf(entrybuf, " %s", _("EXPR"));
        break;
      case EAT_RANGE:
      case EAT_MESSAGE_RANGE:
        /* L10N:
           Pattern Completion Menu argument type: a numeric range.
           Used by ~m, ~n, ~X, ~z.
        */
        mutt_buffer_add_printf(entrybuf, " %s", _("RANGE"));
        break;
      case EAT_DATE:
        /* L10N:
           Pattern Completion Menu argument type: a date range
           Used by ~d, ~r.
        */
        mutt_buffer_add_printf(entrybuf, " %s", _("DATERANGE"));
        break;
      case EAT_QUERY:
        /* L10N:
           Pattern Completion Menu argument type: a query
           Used by ~I.
        */
        mutt_buffer_add_printf(entrybuf, " %s", _("QUERY"));
        break;
      default:
        break;
    }
    entries[i].expr = mutt_str_dup(mutt_buffer_string(entrybuf));
    entries[i].desc = mutt_str_dup(_(Flags[i].desc));

    i++;
  }

  /* Add struct MuttThread patterns manually.
   * Note we allocated 3 extra slots for these above. */

  /* L10N:
     Pattern Completion Menu argument type: a nested pattern.
     Used by ~(), ~<(), ~>().
  */
  const char *patternstr = _("PATTERN");

  entries[i].num = i + 1;
  entries[i].tag = mutt_str_dup("~()");
  mutt_buffer_printf(entrybuf, "~(%s)", patternstr);
  entries[i].expr = mutt_str_dup(mutt_buffer_string(entrybuf));
  // L10N: Pattern Completion Menu description for ~()
  entries[i].desc = mutt_str_dup(
      _("messages in threads containing messages matching PATTERN"));
  i++;

  entries[i].num = i + 1;
  entries[i].tag = mutt_str_dup("~<()");
  mutt_buffer_printf(entrybuf, "~<(%s)", patternstr);
  entries[i].expr = mutt_str_dup(mutt_buffer_string(entrybuf));
  // L10N: Pattern Completion Menu description for ~<()
  entries[i].desc =
      mutt_str_dup(_("messages whose immediate parent matches PATTERN"));
  i++;

  entries[i].num = i + 1;
  entries[i].tag = mutt_str_dup("~>()");
  mutt_buffer_printf(entrybuf, "~>(%s)", patternstr);
  entries[i].expr = mutt_str_dup(mutt_buffer_string(entrybuf));
  // L10N: Pattern Completion Menu description for ~>()
  entries[i].desc =
      mutt_str_dup(_("messages having an immediate child matching PATTERN"));

  mutt_buffer_pool_release(&entrybuf);

  return menu;
}

/**
 * dlg_select_pattern - Show menu to select a Pattern
 * @param buf    Buffer for the selected Pattern
 * @param buflen Length of buffer
 * @retval true A selection was made
 */
bool dlg_select_pattern(char *buf, size_t buflen)
{
  struct MuttWindow *dlg = simple_dialog_new(MENU_GENERIC, WT_DLG_PATTERN, PatternHelp);
  struct Menu *menu = create_pattern_menu(dlg);

  bool rc = false;
  bool done = false;
  while (!done)
  {
    switch (menu_loop(menu))
    {
      case OP_GENERIC_SELECT_ENTRY:
      {
        const int index = menu_get_index(menu);
        struct PatternEntry *entry = ((struct PatternEntry *) menu->mdata) + index;
        mutt_str_copy(buf, entry->tag, buflen);
        rc = true;
        done = true;
        break;
      }

      case OP_EXIT:
        done = true;
        break;
    }
  }

  simple_dialog_free(&dlg);
  return rc;
}
