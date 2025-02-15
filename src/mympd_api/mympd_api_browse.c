/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "mympd_config_defs.h"
#include "mympd_api_browse.h"

#include "../lib/jsonrpc.h"
#include "../lib/log.h"
#include "../lib/mem.h"
#include "../lib/mimetype.h"
#include "../lib/mympd_configuration.h"
#include "../lib/sds_extras.h"
#include "../lib/validate.h"
#include "../mpd_shared/mpd_shared_search.h"
#include "../mpd_shared/mpd_shared_sticker.h"
#include "../mpd_shared/mpd_shared_tags.h"
#include "mympd_api_utility.h"
#include "mympd_api_albumart.h"

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <libgen.h>
#include <pcre.h>
#include <string.h>

//private definitions
static bool _search_song(struct mpd_song *song, struct t_list *expr_list, struct t_tags *browse_tag_types);
static pcre *_compile_regex(const char *regex_str);
static bool _cmp_regex(pcre *re_compiled, sds value);
static void _free_filesystem_list_user_data(struct t_list_node *current);

//public functions
sds mympd_api_browse_fingerprint(struct t_mympd_state *mympd_state, sds buffer, sds method, long request_id,
                               const char *uri)
{   
    char fp_buffer[8192];
    const char *fingerprint = mpd_run_getfingerprint_chromaprint(mympd_state->mpd_state->conn, uri, fp_buffer, sizeof(fp_buffer));
    if (fingerprint == NULL) {
        check_error_and_recover2(mympd_state->mpd_state, &buffer, method, request_id, false);
        return buffer;
    }
    
    buffer = jsonrpc_result_start(buffer, method, request_id);
    buffer = tojson_char(buffer, "fingerprint", fingerprint, false);
    buffer = jsonrpc_result_end(buffer);
    
    mpd_response_finish(mympd_state->mpd_state->conn);
    check_error_and_recover2(mympd_state->mpd_state, &buffer, method, request_id, false);
    
    return buffer;
}

sds mympd_api_browse_songdetails(struct t_mympd_state *mympd_state, sds buffer, sds method, long request_id, 
                               const char *uri)
{
    bool rc = mpd_send_list_meta(mympd_state->mpd_state->conn, uri);
    if (check_rc_error_and_recover(mympd_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_send_list_meta") == false) {
        return buffer;
    }

    buffer = jsonrpc_result_start(buffer, method, request_id);

    struct mpd_song *song;
    if ((song = mpd_recv_song(mympd_state->mpd_state->conn)) != NULL) {
        buffer = get_song_tags(buffer, mympd_state->mpd_state, &mympd_state->mpd_state->tag_types_mympd, song);
        mpd_song_free(song);
    }

    mpd_response_finish(mympd_state->mpd_state->conn);
    if (check_error_and_recover2(mympd_state->mpd_state, &buffer, method, request_id, false) == false) {
        return buffer;
    }
    
    if (mympd_state->mpd_state->feat_stickers) {
        buffer = sdscatlen(buffer, ",", 1);
        buffer = mpd_shared_sticker_list(buffer, mympd_state->sticker_cache, uri);
    }
    
    buffer = sdscatlen(buffer, ",", 1);
    buffer = get_extra_files(mympd_state, buffer, uri, false);
    buffer = jsonrpc_result_end(buffer);
    return buffer;
}

sds mympd_api_browse_read_comments(struct t_mympd_state *mympd_state, sds buffer, sds method, long request_id, 
                             const char *uri)
{
    bool rc = mpd_send_read_comments(mympd_state->mpd_state->conn, uri);
    if (check_rc_error_and_recover(mympd_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_send_list_meta") == false) {
        return buffer;
    }

    buffer = jsonrpc_result_start(buffer, method, request_id);
    buffer = sdscat(buffer, "\"data\":{");
    struct mpd_pair *pair;
    int entities_returned = 0;
    while ((pair = mpd_recv_pair(mympd_state->mpd_state->conn)) != NULL) {
        if (entities_returned++) {
            buffer = sdscatlen(buffer, ",", 1);
        }    
        buffer = tojson_char(buffer, pair->name, pair->value,false);
        mpd_return_pair(mympd_state->mpd_state->conn, pair);
    }
    mpd_response_finish(mympd_state->mpd_state->conn);
    if (check_error_and_recover2(mympd_state->mpd_state, &buffer, method, request_id, false) == false) {
        return buffer;
    }
    buffer = sdscatlen(buffer, "},", 2);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, true);
    buffer = tojson_long(buffer, "totalEntities", entities_returned, false);
    buffer = jsonrpc_result_end(buffer);
    return buffer;
}

sds mympd_api_browse_filesystem(struct t_mympd_state *mympd_state, sds buffer, sds method, long request_id, 
                              sds path, const unsigned int offset, const unsigned int limit, sds searchstr, const struct t_tags *tagcols)
{
    bool rc = mpd_send_list_meta(mympd_state->mpd_state->conn, path);
    if (check_rc_error_and_recover(mympd_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_send_list_meta") == false) {
        return buffer;
    }

    sdstolower(searchstr);

    struct t_list entity_list;
    list_init(&entity_list);
    struct mpd_entity *entity;
    size_t search_len = sdslen(searchstr);
    while ((entity = mpd_recv_entity(mympd_state->mpd_state->conn)) != NULL) {
        switch (mpd_entity_get_type(entity)) {
            case MPD_ENTITY_TYPE_UNKNOWN: {
                break;
            }
            case MPD_ENTITY_TYPE_SONG: {
                const struct mpd_song *song = mpd_entity_get_song(entity);
                sds entity_name = sdsempty();
                entity_name = mpd_shared_get_tags(song, MPD_TAG_TITLE, entity_name);
                sdstolower(entity_name);
                if (search_len == 0  || strstr(entity_name, searchstr) != NULL) {
                    sds key = sdscatfmt(sdsempty(), "2%s", mpd_song_get_uri(song));
                    sdstolower(key);
                    list_insert_sorted_by_key(&entity_list, key, MPD_ENTITY_TYPE_SONG, entity_name, mpd_song_dup(song), LIST_SORT_ASC);
                    FREE_SDS(key);
                }
                FREE_SDS(entity_name);
                break;
            }
            case MPD_ENTITY_TYPE_DIRECTORY: {
                const struct mpd_directory *dir = mpd_entity_get_directory(entity);
                const char *entity_name = mpd_directory_get_path(dir);
                char *dir_name = strrchr(entity_name, '/');
                if (dir_name != NULL) {
                    dir_name++;
                }
                else {
                    dir_name = (char *)entity_name;
                }
                sds dir_name_lower = sdsnew(dir_name);
                sdstolower(dir_name_lower);
                if (search_len == 0  || strstr(dir_name_lower, searchstr) != NULL) {
                    sds key = sdscatfmt(sdsempty(), "0%s", mpd_directory_get_path(dir));
                    sdstolower(key);
                    list_insert_sorted_by_key(&entity_list, key, MPD_ENTITY_TYPE_DIRECTORY, dir_name, mpd_directory_dup(dir), LIST_SORT_ASC);
                    FREE_SDS(key);
                }
                FREE_SDS(dir_name_lower);
                break;
            }
            case MPD_ENTITY_TYPE_PLAYLIST: {
                const struct mpd_playlist *pl = mpd_entity_get_playlist(entity);
                sds entity_name = sdsnew(mpd_playlist_get_path(pl));
                sdstolower(entity_name);
                //do not show mpd playlists in root directory
                if (strcmp(path, "/") == 0) {
                    sds ext = sds_get_extension_from_filename(entity_name);
                    if (strcmp(ext, "m3u") != 0 && strcmp(ext, "pls") != 0) {
                        FREE_SDS(ext);
                        FREE_SDS(entity_name);
                        break;
                    }
                    FREE_SDS(ext);
                }
                char *pl_name = strrchr(entity_name, '/');
                if (pl_name != NULL) {
                    pl_name++;
                }
                else {
                    pl_name = entity_name;
                }
                if (search_len == 0  || strstr(pl_name, searchstr) != NULL) {
                    sds key = sdscatfmt(sdsempty(), "1%s", mpd_playlist_get_path(pl));
                    sdstolower(key);
                    list_insert_sorted_by_key(&entity_list, key, MPD_ENTITY_TYPE_PLAYLIST, pl_name, mpd_playlist_dup(pl), LIST_SORT_ASC);
                    FREE_SDS(key);
                }
                FREE_SDS(entity_name);
                break;
            }
        }
        mpd_entity_free(entity);
    }
    mpd_response_finish(mympd_state->mpd_state->conn);
    if (check_error_and_recover2(mympd_state->mpd_state, &buffer, method, request_id, false) == false) {
        return buffer;
    }

    buffer = jsonrpc_result_start(buffer, method, request_id);
    buffer = sdscat(buffer, "\"data\":[");
    
    unsigned entity_count = 0;
    unsigned entities_returned = 0;
    if (sdslen(path) > 1) {
        char *path_cpy = strdup(path);
        char *parent_dir = dirname(path_cpy);
        buffer = sdscat(buffer, "{\"Type\":\"parentDir\",\"name\":\"parentDir\",");
        buffer = tojson_char(buffer, "uri", (parent_dir[0] == '.' ? "" : parent_dir), false);
        buffer = sdscatlen(buffer, "}", 1);
        entity_count++;
        entities_returned++;
        free(path_cpy);
    }
    
    unsigned real_limit = limit == 0 ? offset + MPD_RESULTS_MAX : offset + limit;
    
    struct t_list_node *current;
    while ((current = list_shift_first(&entity_list)) != NULL) {
        entity_count++;
        if (entity_count > offset && entity_count <= real_limit) {
            if (entities_returned++) {
                buffer = sdscatlen(buffer, ",", 1);
            }
            switch (current->value_i) {
                case MPD_ENTITY_TYPE_SONG: {
                    struct mpd_song *song = (struct mpd_song *)current->user_data;
                    buffer = sdscat(buffer, "{\"Type\":\"song\",");
                    buffer = get_song_tags(buffer, mympd_state->mpd_state, tagcols, song);
                    buffer = sdscatlen(buffer, ",", 1);
                    sds filename = sdsnew(mpd_song_get_uri(song));
                    sds_basename_uri(filename);
                    buffer = tojson_char(buffer, "Filename", filename, false);
                    FREE_SDS(filename);
                    if (mympd_state->mpd_state->feat_stickers) {
                        buffer = sdscatlen(buffer, ",", 1);
                        buffer = mpd_shared_sticker_list(buffer, mympd_state->sticker_cache, mpd_song_get_uri(song));
                    }
                    buffer = sdscatlen(buffer, "}", 1);
                    break;
                }
                case MPD_ENTITY_TYPE_DIRECTORY: {
                    struct mpd_directory *dir = (struct mpd_directory *)current->user_data;
                    buffer = sdscat(buffer, "{\"Type\":\"dir\",");
                    buffer = tojson_char(buffer, "uri", mpd_directory_get_path(dir), true);
                    buffer = tojson_char(buffer, "name", current->value_p, true);
                    buffer = tojson_char(buffer, "Filename", current->value_p, false);
                    buffer = sdscatlen(buffer, "}", 1);
                    break;
                }
                case MPD_ENTITY_TYPE_PLAYLIST: {
                    struct mpd_playlist *pl = (struct mpd_playlist *)current->user_data;
                    bool smartpls = is_smartpls(mympd_state->config->workdir, current->value_p);
                    buffer = sdscatfmt(buffer, "{\"Type\": \"%s\",", (smartpls == true ? "smartpls" : "plist"));
                    buffer = tojson_char(buffer, "uri", mpd_playlist_get_path(pl), true);
                    buffer = tojson_char(buffer, "name", current->value_p, true);
                    buffer = tojson_char(buffer, "Filename", current->value_p, false);
                    buffer = sdscatlen(buffer, "}", 1);
                    break;
                }
            }
        }
        list_node_free_user_data(current, _free_filesystem_list_user_data);
    }

    buffer = sdscatlen(buffer, "],", 2);
    buffer = get_extra_files(mympd_state, buffer, path, true);
    buffer = sdscatlen(buffer, ",", 1);
    buffer = tojson_long(buffer, "totalEntities", entity_count, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, true);
    buffer = tojson_long(buffer, "offset", offset, true);
    buffer = tojson_char(buffer, "search", searchstr, false);
    buffer = jsonrpc_result_end(buffer);
    return buffer;
}

sds mympd_api_browse_album_songs(struct t_mympd_state *mympd_state, sds buffer, sds method, long request_id,
                                  sds album, sds albumartist, const struct t_tags *tagcols)
{
    buffer = jsonrpc_result_start(buffer, method, request_id);
    buffer = sdscat(buffer, "\"data\":[");

    bool rc = mpd_search_db_songs(mympd_state->mpd_state->conn, true);
    if (check_rc_error_and_recover(mympd_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_search_db_songs") == false) {
        mpd_search_cancel(mympd_state->mpd_state->conn);
        return buffer;
    }

    sds expression = sdsnewlen("(", 1);
    expression = escape_mpd_search_expression(expression, mpd_tag_name(mympd_state->mpd_state->tag_albumartist), "==", albumartist);
    expression = sdscat(expression, " AND ");
    expression = escape_mpd_search_expression(expression, "Album", "==", album);
    expression = sdscatlen(expression, ")", 1);
    
    rc = mpd_search_add_expression(mympd_state->mpd_state->conn, expression);
    if (check_rc_error_and_recover(mympd_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_search_add_expression") == false) {
        mpd_search_cancel(mympd_state->mpd_state->conn);
        FREE_SDS(expression);
        return buffer;
    }
    FREE_SDS(expression);
    rc = mpd_search_add_sort_tag(mympd_state->mpd_state->conn, MPD_TAG_DISC, false);
    if (check_rc_error_and_recover(mympd_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_search_add_sort_tag") == false) {
        mpd_search_cancel(mympd_state->mpd_state->conn);
        return buffer;
    }
    rc = mpd_search_add_window(mympd_state->mpd_state->conn, 0, MPD_RESULTS_MAX);
    if (check_rc_error_and_recover(mympd_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_search_add_window") == false) {
        mpd_search_cancel(mympd_state->mpd_state->conn);
        return buffer;
    }
    rc = mpd_search_commit(mympd_state->mpd_state->conn);
    if (check_rc_error_and_recover(mympd_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_search_commit") == false) {
        buffer = check_error_and_recover(mympd_state->mpd_state, buffer, method, request_id);
        return buffer;
    }

    struct mpd_song *song;
    struct mpd_song *first_song = NULL;
    int entity_count = 0;
    int entities_returned = 0;
    unsigned int totalTime = 0;
    int discs = 1;

    while ((song = mpd_recv_song(mympd_state->mpd_state->conn)) != NULL) {
        entity_count++;
        if (entities_returned++) {
            buffer = sdscatlen(buffer, ",", 1);
        }
        else {
            first_song = mpd_song_dup(song);
        }
        const char *disc;
        if ((disc = mpd_song_get_tag(song, MPD_TAG_DISC, 0)) != NULL) {
            int d = (int)strtoimax(disc, NULL, 10);
            if (d > discs) {
                discs = d;
            }
        }
        buffer = sdscat(buffer, "{\"Type\": \"song\",");
        buffer = get_song_tags(buffer, mympd_state->mpd_state, tagcols, song);
        if (mympd_state->mpd_state->feat_stickers) {
            buffer = sdscatlen(buffer, ",", 1);
            buffer = mpd_shared_sticker_list(buffer, mympd_state->sticker_cache, mpd_song_get_uri(song));
        }
        buffer = sdscatlen(buffer, "}", 1);

        totalTime += mpd_song_get_duration(song);
        mpd_song_free(song);
    }

    buffer = sdscatlen(buffer, "],", 2);
    
    if (first_song != NULL) {
        buffer = get_extra_files(mympd_state, buffer, mpd_song_get_uri(first_song), false);
    }
    else {
        buffer = sdscat(buffer, "\"images\":[],\"bookletPath\":\"\"");
    }
    
    buffer = sdscatlen(buffer, ",", 1);
    buffer = tojson_long(buffer, "totalEntities", entity_count, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, true);
    buffer = tojson_char(buffer, "Album", album, true);
    buffer = tojson_char(buffer, "AlbumArtist", albumartist, true);
    buffer = tojson_long(buffer, "Discs", discs, true);
    buffer = tojson_long(buffer, "totalTime", totalTime, false);
    buffer = jsonrpc_result_end(buffer);
        
    if (first_song != NULL) {
        mpd_song_free(first_song);
    }
    mpd_response_finish(mympd_state->mpd_state->conn);
    if (check_error_and_recover2(mympd_state->mpd_state, &buffer, method, request_id, false) == false) {
        return buffer;
    }
    
    return buffer;    
}

sds mympd_api_browse_album_list(struct t_mympd_state *mympd_state, sds buffer, sds method, long request_id, 
                                       sds expression, sds sort, bool sortdesc, const unsigned int offset, unsigned int limit)
{
    if (mympd_state->album_cache == NULL) {
        buffer = jsonrpc_respond_message(buffer, method, request_id, true, "database", "error", "Albumcache not ready");
        return buffer;
    }

    buffer = jsonrpc_result_start(buffer, method, request_id);
    buffer = sdscat(buffer, "\"data\":[");

    struct mpd_song *song;
    //parse sort tag
    bool sort_by_last_modified = false;
    enum mpd_tag_type sort_tag = MPD_TAG_ALBUM;

    if (sdslen(sort) > 0) {
        enum mpd_tag_type sort_tag_org = mpd_tag_name_parse(sort);
        if (sort_tag_org != MPD_TAG_UNKNOWN) {
            sort_tag = get_sort_tag(sort_tag_org);
            if (mpd_shared_tag_exists(mympd_state->mpd_state->tag_types_mympd.tags, mympd_state->mpd_state->tag_types_mympd.len, sort_tag) == false) {
                //sort tag is not enabled, revert 
                sort_tag = sort_tag_org;
            }
        }
        else if (strcmp(sort, "Last-Modified") == 0) {
            sort_by_last_modified = true;
        }
        else {
            MYMPD_LOG_WARN("Unknown sort tag: %s", sort);
        }
    }
    //parse mpd search expression
    struct t_list expr_list;
    list_init(&expr_list);
    int count = 0;
    sds *tokens = sdssplitlen(expression, (ssize_t)sdslen(expression), ") AND (", 7, &count);
    for (int j = 0; j < count; j++) {
        sdstrim(tokens[j], "() ");
        sds tag = sdsempty();
        sds op = sdsempty();
        sds value = sdsempty();
        unsigned i = 0;
        char *p = tokens[j];
        //tag
        for (i = 0; i < sdslen(tokens[j]); i++, p++) {
            if (tokens[j][i] == ' ') {
                break;
            }
            tag = sdscatprintf(tag, "%.*s", 1, p);
        }
        if (i + 1 >= sdslen(tokens[j])) {
            MYMPD_LOG_ERROR("Can not parse search expression");
            FREE_SDS(tag);
            FREE_SDS(op);
            FREE_SDS(value);
            break;
        }
        i++;
        p++;
        //operator
        for (; i < sdslen(tokens[j]); i++, p++) {
            if (tokens[j][i] == ' ') {
                break;
            }
            op = sdscatprintf(op, "%.*s", 1, p);
        }
        if (i + 2 >= sdslen(tokens[j])) {
            MYMPD_LOG_ERROR("Can not parse search expression");
            FREE_SDS(tag);
            FREE_SDS(op);
            FREE_SDS(value);
            break;
        }
        i = i + 2;
        p = p + 2;
        //value
        for (; i < sdslen(tokens[j]) - 1; i++, p++) {
            value = sdscatprintf(value, "%.*s", 1, p);
        }
        int tag_type = mpd_tag_name_parse(tag);
        if (tag_type == -1 && strcmp(tag, "any") == 0) {
            tag_type = -2;
        }
        if (strcmp(op, "=~") == 0 || strcmp(op, "!~") == 0) {
            //is regex, compile
            pcre *re_compiled = _compile_regex(value);
            list_push(&expr_list, value, tag_type, op , re_compiled);
        }
        else {
            list_push(&expr_list, value, tag_type, op , NULL);
        }
        MYMPD_LOG_DEBUG("Parsed expression tag: \"%s\", op: \"%s\", value:\"%s\"", tag, op, value);
        FREE_SDS(tag);
        FREE_SDS(op);
        FREE_SDS(value);
    }
    sdsfreesplitres(tokens, count);
    
    //search and sort albumlist
    struct t_list album_list;
    list_init(&album_list);
    raxIterator iter;
    raxStart(&iter, mympd_state->album_cache);
    raxSeek(&iter, "^", NULL, 0);
    sds key = sdsempty();
    while (raxNext(&iter)) {
        song = (struct mpd_song *)iter.data;
        if (_search_song(song, &expr_list, &mympd_state->tag_types_browse) == true) {
            if (sort_by_last_modified == true) {
                key = sdscatlen(key, iter.key, iter.key_len);
                list_insert_sorted_by_value_i(&album_list, key, mpd_song_get_last_modified(song), NULL, iter.data,
                    (sortdesc == false ? LIST_SORT_ASC : LIST_SORT_DESC));
                sdsclear(key);
            }
            else {
                const char *sort_value = mpd_song_get_tag(song, sort_tag, 0);
                if (sort_value != NULL) {
                    list_insert_sorted_by_key(&album_list, sort_value, 0, NULL, iter.data,
                        (sortdesc == false ? LIST_SORT_ASC : LIST_SORT_DESC));
                }
                else if (sort_tag == MPD_TAG_ALBUM_ARTIST) {
                    //fallback to artist tag if albumartist tag is not set
                    sort_value = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
                    list_insert_sorted_by_key(&album_list, sort_value, 0, NULL, iter.data,
                        (sortdesc == false ? LIST_SORT_ASC : LIST_SORT_DESC));
                }
                else {
                    //sort tag not present, append to end of the list
                    list_push(&album_list, "zzzzzzzzzz", 0, NULL, iter.data);
                }
            }
        }
    }
    raxStop(&iter);
    FREE_SDS(key);
    list_clear(&expr_list);
    
    //print album list
    unsigned entity_count = 0;
    unsigned entities_returned = 0;
    unsigned real_limit = limit == 0 ? offset + MPD_RESULTS_MAX : offset + limit;
    sds album = sdsempty();
    sds artist = sdsempty();
    struct t_list_node *current;
    while ((current = list_shift_first(&album_list)) != NULL) {
        entity_count++;
        if (entity_count > offset && entity_count <= real_limit) {
            if (entities_returned++) {
                buffer = sdscatlen(buffer, ",", 1);
            }
            song = (struct mpd_song *)current->user_data;
            album = mpd_shared_get_tags(song, MPD_TAG_ALBUM, album);
            artist = mpd_shared_get_tags(song, mympd_state->mpd_state->tag_albumartist, artist);
            buffer = sdscat(buffer, "{\"Type\": \"album\",");
            buffer = tojson_char(buffer, "Album", album, true);
            buffer = tojson_char(buffer, "AlbumArtist", artist, true);
            buffer = tojson_char(buffer, "FirstSongUri", mpd_song_get_uri(song), false);
            buffer = sdscatlen(buffer, "}", 1);
        }
        list_node_free_user_data(current, list_free_cb_ignore_user_data);
        if (entity_count > real_limit) {
            break;
        }
    }
    FREE_SDS(album);
    FREE_SDS(artist);
    entity_count = album_list.length;
    list_clear_user_data(&album_list, list_free_cb_ignore_user_data);

    buffer = sdscatlen(buffer, "],", 2);
    buffer = tojson_long(buffer, "totalEntities", entity_count, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, true);
    buffer = tojson_long(buffer, "offset", offset, true);
    buffer = tojson_char(buffer, "expression", expression, true);
    buffer = tojson_char(buffer, "sort", sort, true);
    buffer = tojson_bool(buffer, "sortdesc", sortdesc, true);
    buffer = tojson_char(buffer, "tag", "Album", false);
    buffer = jsonrpc_result_end(buffer);
        
    return buffer;    
}

sds mympd_api_browse_tag_list(struct t_mympd_state *mympd_state, sds buffer, sds method, long request_id, 
                          sds searchstr, sds tag, const unsigned int offset, const unsigned int limit)
{
    sdstolower(searchstr);
    size_t searchstr_len = sdslen(searchstr);

    buffer = jsonrpc_result_start(buffer, method, request_id);
    buffer = sdscat(buffer, "\"data\":[");
   
    bool rc = mpd_search_db_tags(mympd_state->mpd_state->conn, mpd_tag_name_parse(tag));
    if (check_rc_error_and_recover(mympd_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_search_db_tags") == false) {
        mpd_search_cancel(mympd_state->mpd_state->conn);
        return buffer;
    }
    
    rc = mpd_search_commit(mympd_state->mpd_state->conn);
    if (check_rc_error_and_recover(mympd_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_search_commit") == false) {
        return buffer;
    }

    struct mpd_pair *pair;
    unsigned entity_count = 0;
    unsigned entities_returned = 0;
    enum mpd_tag_type mpdtag = mpd_tag_name_parse(tag);
    unsigned real_limit = limit == 0 ? offset + MPD_RESULTS_MAX : offset + limit;
    while ((pair = mpd_recv_pair_tag(mympd_state->mpd_state->conn, mpdtag)) != NULL) {
        if (pair->value[0] == '\0') {
            MYMPD_LOG_DEBUG("Value is empty, skipping");
            mpd_return_pair(mympd_state->mpd_state->conn, pair);
            continue;
        }
        sds value_lower = sdsnew(pair->value);
        sdstolower(value_lower);
        mpd_return_pair(mympd_state->mpd_state->conn, pair);
        if (searchstr_len == 0 ||
            (searchstr_len <= 2 && strncmp(searchstr, value_lower, searchstr_len) == 0) ||
            (searchstr_len > 2 && strstr(value_lower, searchstr) != NULL))
        {
            entity_count++;
            if (entity_count > offset && entity_count <= real_limit) {
                if (entities_returned++) {
                    buffer = sdscatlen(buffer, ",", 1);
                }
                buffer = sdscatlen(buffer, "{", 1);
                buffer = tojson_char(buffer, "value", pair->value, false);
                buffer = sdscatlen(buffer, "}", 1);
            }
        }
        FREE_SDS(value_lower);
    }
    mpd_response_finish(mympd_state->mpd_state->conn);
    if (check_error_and_recover2(mympd_state->mpd_state, &buffer, method, request_id, false) == false) {
        return buffer;
    }

    //checks if this tag has a directory with pictures in /var/lib/mympd/pics
    sds pic_path = sdscatfmt(sdsempty(), "%s/pics/%s", mympd_state->config->workdir, tag);
    bool pic = false;
    errno = 0;
    DIR* dir = opendir(pic_path);
    if (dir != NULL) {
        closedir(dir);
        pic = true;
    }
    else {
        MYMPD_LOG_DEBUG("Can not open directory \"%s\"", pic_path);
        if (errno != ENOENT) {
            MYMPD_LOG_ERRNO(errno);
        }
        //ignore error
    }
    FREE_SDS(pic_path);

    buffer = sdscatlen(buffer, "],", 2);
    buffer = tojson_long(buffer, "totalEntities", entity_count, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, true);
    buffer = tojson_long(buffer, "offset", offset, true);
    buffer = tojson_char(buffer, "searchstr", searchstr, true);
    buffer = tojson_char(buffer, "tag", tag, true);
    buffer = tojson_bool(buffer, "pics", pic, false);
    buffer = jsonrpc_result_end(buffer);
    return buffer;
}

//private functions
static bool _search_song(struct mpd_song *song, struct t_list *expr_list, struct t_tags *browse_tag_types) {
    sds value = sdsempty();
    (void) browse_tag_types;
    struct t_tags one_tag;
    one_tag.len = 1;
    struct t_list_node *current = expr_list->head;
    while (current != NULL) {
        struct t_tags *tags = NULL;
        if (current->value_i == -2) {
            //any - use all browse tags
            tags = browse_tag_types;
        }
        else {
            //use selected tag only
            tags = &one_tag;
            tags->tags[0] = current->value_i;
        }
        bool rc = false;
        sdstolower(current->key);
        for (unsigned i = 0; i < tags->len; i++) {
            rc = true;
            value = mpd_shared_get_tags(song, tags->tags[i], value);
            sdstolower(value);
            if ((strcmp(current->value_p, "contains") == 0 && strstr(value, current->key) == NULL) ||
                (strcmp(current->value_p, "starts_with") == 0 && strncmp(current->key, value, sdslen(current->key)) != 0) ||
                (strcmp(current->value_p, "==") == 0 && strcmp(value, current->key) != 0) ||
                (strcmp(current->value_p, "!=") == 0 && strcmp(value, current->key) == 0) ||
                (strcmp(current->value_p, "=~") == 0 && _cmp_regex((pcre *)current->user_data, value) == false) ||
                (strcmp(current->value_p, "!~") == 0 && _cmp_regex((pcre *)current->user_data, value) == true))
            {
                rc = false;
            }
            else {
                //tag value matched
                break;
            }
        }
        if (rc == false) {
            FREE_SDS(value);
            return false;
        }
        current = current->next;
    }
    FREE_SDS(value);
    return true;
}

static pcre *_compile_regex(const char *regex_str) {
    MYMPD_LOG_DEBUG("Compiling regex: \"%s\"", regex_str);
    const char *pcre_error_str;
    int pcre_error_offset;
    pcre *re_compiled = pcre_compile(regex_str, 0, &pcre_error_str, &pcre_error_offset, NULL);
    if (re_compiled == NULL) {
        MYMPD_LOG_DEBUG("Could not compile '%s': %s\n", regex_str, pcre_error_str);
    }
    return re_compiled;
}

static bool _cmp_regex(pcre *re_compiled, sds value) {
    if (re_compiled == NULL) {
        return false;
    }
    int substr_vec[30];
    int pcre_exec_ret = pcre_exec(re_compiled, NULL, value, (int) sdslen(value), 0, 0, substr_vec, 30);
    if (pcre_exec_ret < 0) {
        switch(pcre_exec_ret) {
            case PCRE_ERROR_NOMATCH      : break;
            case PCRE_ERROR_NULL         : MYMPD_LOG_ERROR("Something was null"); break;
            case PCRE_ERROR_BADOPTION    : MYMPD_LOG_ERROR("A bad option was passed"); break;
            case PCRE_ERROR_BADMAGIC     : MYMPD_LOG_ERROR("Magic number bad (compiled regex corrupt?)"); break;
            case PCRE_ERROR_UNKNOWN_NODE : MYMPD_LOG_ERROR("Something kooky in the compiled regex"); break;
            case PCRE_ERROR_NOMEMORY     : MYMPD_LOG_ERROR("Ran out of memory"); break;
            default                      : MYMPD_LOG_ERROR("Unknown error"); break;
        }
        return false;
    }
    return true;
}

static void _free_filesystem_list_user_data(struct t_list_node *current) {
    switch (current->value_i) {
        case MPD_ENTITY_TYPE_SONG: {
            struct mpd_song *song = (struct mpd_song *)current->user_data;
            mpd_song_free(song);
            break;
        }
        case MPD_ENTITY_TYPE_DIRECTORY: {
            struct mpd_directory *dir = (struct mpd_directory *)current->user_data;
            mpd_directory_free(dir);
            break;
        }
        case MPD_ENTITY_TYPE_PLAYLIST: {
            struct mpd_playlist *pl = (struct mpd_playlist *)current->user_data;
            mpd_playlist_free(pl);
            break;
        }
    }
}
