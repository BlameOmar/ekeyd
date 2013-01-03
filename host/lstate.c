/* daemon/lstate.c
 *
 * Entropy Key Daemon, Lua state for configuration and fiddling.
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <dirent.h>

#include "lstate.h"
#include "keydb.h"
#include "stats.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

static lua_State *L_conf = NULL;
static bool daemonise = true;

static int
l_addfd(lua_State *L)
{
    if (lstate_cb_newfd(luaL_checkinteger(L, 1)) == false) {
        /* nil */
        return 0;
    }
    /* Return the FD on success */
    return 1;
}

static int
l_delfd(lua_State *L)
{
    lstate_cb_delfd(luaL_checkinteger(L, 1));
    /* Return the FD on success */
    return 1;
}

static int
l_writefd(lua_State *L)
{
    lstate_cb_writefd(luaL_checkinteger(L, 1));
    /* Return the FD on success */
    return 1;
}

static int
l_nowritefd(lua_State *L)
{
    lstate_cb_nowritefd(luaL_checkinteger(L, 1));
    /* Return the FD on success */
    return 1;
}

static int
l_add_ekey(lua_State *L)
{
    OpaqueEkey *ekey = add_ekey(luaL_checkstring(L, 1), luaL_optstring(L, 2, NULL));
    if (ekey == NULL) {
        lua_pushnil(L);
        lua_pushfstring(L, "Could not construct key from '%s' (optional serial: %s). Errno was %d (%s)",
                        luaL_checkstring(L, 1), luaL_optstring(L, 2, "not provided"),
                        errno, strerror(errno));
        return 2;
    }
    lua_pushlightuserdata(L, ekey);
    return 1;
}

static int
l_del_ekey(lua_State *L)
{
    OpaqueEkey *ekey = (OpaqueEkey *)lua_touserdata(L, 1);
    if (ekey != NULL) {
        kill_ekey(ekey);
    }
    return 1;
}

#define L_KEY_STAT(name,var)                    \
    lua_pushliteral(L, #name);                  \
    lua_pushnumber(L, key_stats->var);          \
    lua_settable(L, -3)

static int
l_stat_ekey(lua_State *L)
{
    OpaqueEkey *ekey = (OpaqueEkey *)lua_touserdata(L, 1);
    connection_stats_t *key_stats;

    if (ekey == NULL) {
        lua_pushnil(L);
        lua_pushliteral(L, "Unable to query a NULL ekey.");
        return 2;
    }

    key_stats = get_key_stats(ekey);

    if (key_stats == NULL) {
        lua_pushnil(L);
        lua_pushliteral(L, "Unable to get statistics.");
        return 2;
    }

    lua_newtable(L);

    L_KEY_STAT(BytesRead, stream_bytes_read);
    L_KEY_STAT(BytesWritten, stream_bytes_written);
    L_KEY_STAT(FrameByteLast, frame_byte_last);
    L_KEY_STAT(FramingErrors, frame_framing_errors);
    L_KEY_STAT(FramesOk, frame_frames_ok);
    L_KEY_STAT(PacketErrors, pkt_error);
    L_KEY_STAT(PacketOK, pkt_ok);
    L_KEY_STAT(TotalEntropy, con_entropy);
    L_KEY_STAT(KeyTemperatureK, key_temp);
    L_KEY_STAT(KeyVoltage, key_voltage);
    L_KEY_STAT(FipsFrameRate, fips_frame_rate);

    L_KEY_STAT(ConnectionPackets, con_pkts);
    L_KEY_STAT(ConnectionResets, con_reset);
    L_KEY_STAT(ConnectionNonces, con_nonces);
    L_KEY_STAT(ConnectionRekeys, con_rekeys);

    L_KEY_STAT(KeyRawShannonPerByteL, key_raw_entl);
    L_KEY_STAT(KeyRawShannonPerByteR, key_raw_entr);
    L_KEY_STAT(KeyRawShannonPerByteX, key_raw_entx);

    L_KEY_STAT(KeyDbsdShannonPerByteL, key_dbsd_entl);
    L_KEY_STAT(KeyDbsdShannonPerByteR, key_dbsd_entr);

    lua_pushliteral(L, "ConnectionTime");
    lua_pushnumber(L, (time(NULL) - key_stats->con_start));
    lua_settable(L, -3);

    lua_pushliteral(L, "KeyRawBadness");
    lua_pushfstring(L, "%c", key_stats->key_badness);
    lua_settable(L, -3);

    free(key_stats);

    return 1;
}

static int
l_query_ekey(lua_State *L)
{
    OpaqueEkey *ekey = (OpaqueEkey *)lua_touserdata(L, 1);
    int status;
    char *serialnumber;

    if (ekey == NULL) {
        lua_pushnil(L);
        lua_pushliteral(L, "Unable to query a NULL ekey.");
        return 2;
    }

    status = query_ekey_status(ekey);
    if (status < 0) {
        /* Errno status */
        lua_pushboolean(L, false);
        lua_pushstring(L, strerror(-status));
        return 2;
    }

    switch (status) {
    case EKEY_STATUS_UNKNOWN:
        /* Unknown state currently */
        lua_pushboolean(L, true);
        lua_pushliteral(L, "Unknown");
        break;

    case EKEY_STATUS_GOODSERIAL:
        /* Serial number is good, not got anywhere else yet. */
        lua_pushboolean(L, true);
        lua_pushliteral(L, "Serial ok");
        break;

    case EKEY_STATUS_UNKNOWNSERIAL:
        /* Serial number is not known to the keyring. */
        lua_pushboolean(L, false);
        lua_pushliteral(L, "Serial unknown");
        break;

    case EKEY_STATUS_BADKEY:
        /* Key is bad for some reason (e.g. wrong long-term-key) */
        lua_pushboolean(L, false);
        lua_pushliteral(L, "Long-Term-Key is bad");
        break;

    case EKEY_STATUS_GONEBAD:
        /* Key reported that its generators have gone bad. */
        lua_pushboolean(L, false);
        lua_pushliteral(L, "Key has self-stopped");
        break;

    case EKEY_STATUS_KEYED:
        /* Session is keyed, generating entropy. */
        lua_pushboolean(L, true);
        lua_pushliteral(L, "Running OK");
        break;

    case EKEY_STATUS_KEYCLOSED:
        /* Key has been closed for whatever reason. */
        lua_pushboolean(L, false);
        lua_pushliteral(L, "Key closed. (Vanished?)");
        break;
    }

    serialnumber = retrieve_ekey_serial(ekey);
    if (serialnumber != NULL) {
        lua_pushlstring(L, serialnumber, 16);
        free(serialnumber);
        return 3;
    }

    return 2;
}

static int
l_load_keys(lua_State *L)
{
    lua_pushnumber(L, read_keyring(luaL_checkstring(L, 1)));
    return 1;
}

static int
l_open_file_output(lua_State *L)
{
    if (open_file_output(luaL_checkstring(L, 1))) {
        return 1;
    }
    lua_pushnil(L);
    lua_pushfstring(L, "Cannot open %s: errno %d (%s)",
                    luaL_checkstring(L, 1),
                    errno,
                    strerror(errno));
    return 2;
}

static int
l_open_kernel_output(lua_State *L)
{
    if (open_kernel_output(luaL_optnumber(L, 1, 4))) {
        return 1;
    }
    lua_pushnil(L);
    lua_pushfstring(L, "Cannot open kernel output for %d bits per byte: errno %d (%s)",
                    luaL_optnumber(L, 1, 4), errno, strerror(errno));
    return 2;
}

static int
l_open_foldback_output(lua_State *L)
{
    if (open_foldback_output()) {
        lua_pushboolean(L, 1); /* Return true */
        return 1;
    }
    lua_pushnil(L);
    lua_pushfstring(L, "Cannot open foldback output: errno %d (%s)",
                    errno, strerror(errno));
    return 2;
}

static int
l_daemonise(lua_State *L)
{
    daemonise = lua_toboolean(L, 1);
    return 0;
}

static int
l_unlink(lua_State *L)
{
    const char *fname = luaL_checkstring(L, 1);

    if (unlink(fname) == -1) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, strerror(errno));
        return 2;
    }

    return 1;
}

static int
l_chown(lua_State *L)
{
    struct passwd *pw;
    struct group *gr;
    const char *fname = luaL_checkstring(L, 1);
    const char *user = luaL_checkstring(L, 2);
    const char *group = luaL_checkstring(L, 3);
    int uid = -1, gid = -1;

    if (*user != 0) {
        errno = 0;
        if ((pw = getpwnam(user)) == NULL) {
            lua_pushboolean(L, 0);
            if (errno != 0) {
                lua_pushstring(L, strerror(errno));
            } else {
                lua_pushfstring(L, "Unable to find username %s", user);
            }
            return 2;
        }
        uid = pw->pw_uid;
    }

    if (*group != 0) {
        errno = 0;
        if ((gr = getgrnam(group)) == NULL) {
            lua_pushboolean(L, 0);
            if (errno != 0) {
                lua_pushstring(L, strerror(errno));
            } else {
                lua_pushfstring(L, "Unable to find group %s", user);
            }
            return 2;
        }
        gid = gr->gr_gid;
    }

    if (chown(fname, uid, gid) < 0) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, strerror(errno));
        return 2;
    }

    lua_settop(L, 1);
    return 1;
}

static int
l_chmod(lua_State *L)
{
    const char *fname = luaL_checkstring(L, 1);
    int mode = luaL_checknumber(L, 2);

    if (chmod(fname, mode) < 0) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, strerror(errno));
        return 2;
    }

    lua_settop(L, 1);
    return 1;
}

static int
l_enumerate(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    DIR *d;
    struct dirent *de;
    int i = 1;

    if ((d = opendir(path)) == NULL) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, strerror(errno));
        return 2;
    }

    lua_newtable(L);
    while ((de = readdir(d)) != NULL) {
        lua_pushnumber(L, i++);
        lua_pushstring(L, de->d_name);
        lua_settable(L, -3);
    }

    closedir(d);

    return 1;
}

static const luaL_Reg lstate_funcs[] = {
    /* FD routines */
    {"_addfd", l_addfd},
    {"_delfd", l_delfd},
    {"_writefd", l_writefd},
    {"_nowritefd", l_nowritefd},
    /* EKey routines */
    {"_add_ekey", l_add_ekey},
    {"_del_ekey", l_del_ekey},
    {"_query_ekey", l_query_ekey},
    {"_stat_ekey", l_stat_ekey},
    /* Keyring routines */
    {"_load_keys", l_load_keys},
    /* Output routines */
    {"_open_output_file", l_open_file_output},
    {"_open_kernel_output", l_open_kernel_output},
    {"_open_foldback_output", l_open_foldback_output},
    /* Daemon features */
    {"_daemonise", l_daemonise},
    /* OS access routines */
    {"_unlink", l_unlink},
    {"_chmod", l_chmod},
    {"_chown", l_chown},
    {"_enumerate", l_enumerate},
    /* Terminator */
    {NULL, NULL}
};

bool
lstate_init(void)
{
    int result;
    lua_State *L;

    L_conf = L = luaL_newstate();
    if (L == NULL) {
        fprintf(stderr, "Unable to create configuration state.\n");
        return false;
    }
    luaL_openlibs(L);

    luaL_register(L, "_G", lstate_funcs);

#include "control.inc"

    if (result != 0) {
        fprintf(stderr, "Unable to run control setup code:\n%s\n",
                lua_tostring(L, -1));
        lua_close(L);
        L = NULL;
        return false;
    }

    return true;
}

void
lstate_finalise(void)
{
    lua_State *L = L_conf;
    lua_getglobal(L, "FINAL");
    lua_pcall(L, 0, 0, 0);
    lua_close(L);
    L_conf = NULL;
}

bool
lstate_runconfig(const char *conffile)
{
    lua_State *L = L_conf;
    lua_getglobal(L, "CONFIG");
    lua_pushstring(L, conffile);
    if (lua_pcall(L, 1, 0, 0) != 0) {
        fprintf(stderr, "Unable to run configuration file:\n%s\n",
                lua_tostring(L, -1));
        return false;
    }
    return true;
}

void
lstate_controlbytes(void)
{
    lua_State *L = L_conf;
    lua_getglobal(L, "CONTROL");
    lua_pcall(L, 0, 0, 0);
}

void
lstate_inform_about_key(OpaqueEkey *ekey)
{
    lua_State *L = L_conf;
    lua_getglobal(L, "INFORM");
    lua_pushlightuserdata(L, ekey);
    lua_pcall(L, 1, 0, 0);
}

bool
lstate_request_daemonise(void)
{
    return daemonise;
}

ssize_t
lstate_foldback_entropy(const unsigned char *eblock, unsigned int ecount)
{
    lua_State *L = L_conf;
    lua_getglobal(L, "ENTROPY");
    lua_pushlstring(L, (char *)eblock, ecount);
    lua_pcall(L, 1, 0, 0);

    return ecount;
}
