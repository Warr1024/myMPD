/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef MYMPD_MPD_SHARED_TAGS_H
#define MYMPD_MPD_SHARED_TAGS_H

#include "../../dist/src/sds/sds.h"
#include "../lib/mympd_state.h"

void copy_tag_types(struct t_tags *src_tag_list, struct t_tags *dst_tag_list);
void reset_t_tags(struct t_tags *tags);
void disable_all_mpd_tags(struct t_mpd_state *mpd_state);
void enable_all_mpd_tags(struct t_mpd_state *mpd_state);
void enable_mpd_tags(struct t_mpd_state *mpd_state, struct t_tags *enable_tags);
enum mpd_tag_type get_sort_tag(enum mpd_tag_type tag);
sds get_song_tags(sds buffer, struct t_mpd_state *mpd_state, const struct t_tags *tagcols, 
                  const struct mpd_song *song);
sds get_empty_song_tags(sds buffer, struct t_mpd_state *mpd_state, const struct t_tags *tagcols, 
                        const char *uri);
void check_tags(sds taglist, const char *taglistname, struct t_tags *tagtypes,
                struct t_tags allowed_tag_types);
bool mpd_shared_tag_exists(const enum mpd_tag_type tag_types[64], const size_t tag_types_len, 
                           const enum mpd_tag_type tag);
sds mpd_shared_get_tags(struct mpd_song const *song, const enum mpd_tag_type tag, sds tags);
void album_cache_free(rax **album_cache);
#endif
