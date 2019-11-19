/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2019 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef __WEB_SERVER_H__
#define __WEB_SERVER_H__

typedef struct t_mg_user_data {
    void *config; //pointer to mympd config
    sds music_directory;
    sds rewrite_patterns;
    sds coverimage_name;
    bool feat_library;
    bool feat_mpd_albumart;
    int conn_id;
} t_mg_user_data;

void *web_server_loop(void *arg_mgr);
bool web_server_init(void *arg_mgr, t_config *config, t_mg_user_data *mg_user_data);
void web_server_free(void *arg_mgr);

#endif
