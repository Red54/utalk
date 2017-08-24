/* utalk, a UDP-based "talk" replacement, using srdp
   by Roger Espel Llima <roger.espel.llima@pobox.com>

   functions.h

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "struct.h"

extern int active_window;	/* -1 = mine, read-write
				    0 = mine, read-only
				    1 = first other, read-only
				   ...
				*/

extern struct logical_line *current_lline;
extern struct function the_functions[];

extern void f_self_insert(unsigned char c);
extern void f_insert_in_place(unsigned char c);
extern void f_tab(unsigned char c);
extern void f_new_line(unsigned char c);
extern void f_delete(unsigned char c);
extern void f_delete_end_of_line(unsigned char c);
extern void f_delete_beginning_of_line(unsigned char c);
extern void f_delete_line(unsigned char c);
extern void f_delete_word(unsigned char c);
extern void f_delete_end_of_word(unsigned char c);
extern void f_backspace(unsigned char c);
extern void f_backspace_word(unsigned char c);
extern void f_backward(unsigned char c);
extern void f_forward(unsigned char c);
extern void f_backward_word(unsigned char c);
extern void f_forward_word(unsigned char c);
extern void f_end_of_word(unsigned char c);
extern void f_beginning_of_line(unsigned char c);
extern void f_end_of_line(unsigned char c);
extern void f_up(unsigned char c);
extern void f_down(unsigned char c);
extern void f_up_page(unsigned char c);
extern void f_down_page(unsigned char c);
extern void f_up_half_page(unsigned char c);
extern void f_down_half_page(unsigned char c);
extern void f_top_of_screen(unsigned char c);
extern void f_middle_of_screen(unsigned char c);
extern void f_bottom_of_screen(unsigned char c);
extern void f_top_or_up_page(unsigned char c);
extern void f_bottom_or_down_page(unsigned char c);
extern void f_top(unsigned char c);
extern void f_bottom(unsigned char c);
extern void f_vi_goto_line(unsigned char c);
extern void f_redisplay(unsigned char u);
extern void f_resynch(unsigned char u);
extern void f_next_window(unsigned char u);
extern void f_nop(unsigned char u);
extern void f_beep(unsigned char u);
extern void f_vi_insert_mode(unsigned char c);
extern void f_vi_command_mode(unsigned char c);
extern void f_emacs_mode(unsigned char c);
extern void f_quit(unsigned char c);
extern void f_vi_add(unsigned char c);
extern void f_vi_add_at_end_of_line(unsigned char c);
extern void f_vi_insert_at_beginning_of_line(unsigned char c);
extern void f_vi_escape(unsigned char c);
extern void f_vi_open(unsigned char c);
extern void f_vi_Open(unsigned char c);
extern void f_vi_replace_char(unsigned char c);
extern void f_vi_find_char(unsigned char c);
extern void f_vi_reverse_find_char(unsigned char c);
extern void f_vi_till_char(unsigned char c);
extern void f_vi_reverse_till_char(unsigned char c);
extern void f_vi_repeat_find(unsigned char c);
extern void f_vi_reverse_repeat_find(unsigned char c);
extern void f_vi_delete_find_char(unsigned char c);
extern void f_vi_delete_reverse_find_char(unsigned char c);
extern void f_vi_delete_till_char(unsigned char c);
extern void f_vi_delete_reverse_till_char(unsigned char c);
extern void f_vi_flip_case(unsigned char c);
extern void f_quote_char(unsigned char c);
extern void f_do_help(unsigned char c);
extern void f_set_topic(unsigned char c);
extern void f_do_command(unsigned char c);
extern void f_test_menu(unsigned char c); 	/* xxx */
extern void f_test_entry(unsigned char c); 	/* xxx */
extern void f_test_selection(unsigned char c); 	/* xxx */

#endif

