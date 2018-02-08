
/***
  This file is part of PulseAudio.

  Copyright 2017-2018 Jolla Ltd.
            Contact: Juho Hämäläinen <juho.hamalainen@jolla.com>

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>

#include <pulsecore/core-util.h>
#include <pulsecore/dbus-shared.h>
#include <pulsecore/shared.h>
#include <pulsecore/core-error.h>

#include "native-call-control.h"
#include "bluez5-util.h"

struct pa_bluetooth_call_control {
    pa_core *core;
    pa_dbus_connection *connection;
    pa_bluetooth_transport *native_transport;
    pa_hashmap *call_paths;
    pa_hashmap *active_calls;
    pa_hashmap *held_calls;
    const char *incoming_call_path;
    PA_LLIST_HEAD(pa_dbus_pending, pending);
};

#define OFONO_SERVICE                       "org.ofono"
#define OFONO_MANAGER_INTERFACE             OFONO_SERVICE ".Manager"
#define OFONO_VOICECALL_INTERFACE           OFONO_SERVICE ".VoiceCall"
#define OFONO_VOICECALL_MANAGER_INTERFACE   OFONO_SERVICE ".VoiceCallManager"

static void voicecall_get_all_calls(pa_bluetooth_call_control *control);
static void voicecall_clear_calls(pa_bluetooth_call_control *control);

static pa_dbus_pending* send_and_add_to_pending(pa_bluetooth_call_control *control, DBusMessage *m,
        DBusPendingCallNotifyFunction func, void *call_data) {

    pa_dbus_pending *p;
    DBusPendingCall *call;

    pa_assert(control);
    pa_assert(m);

    pa_assert_se(dbus_connection_send_with_reply(pa_dbus_connection_get(control->connection), m, &call, -1));

    p = pa_dbus_pending_new(pa_dbus_connection_get(control->connection), m, call, control, call_data);
    PA_LLIST_PREPEND(pa_dbus_pending, control->pending, p);
    dbus_pending_call_set_notify(call, func, p, NULL);

    return p;
}

static void voicecall_send(pa_bluetooth_call_control *control, const char *path, const char *action) {
    DBusMessage *m;

    m = dbus_message_new_method_call(OFONO_SERVICE, path, OFONO_VOICECALL_INTERFACE, action);
    dbus_connection_send(pa_dbus_connection_get(control->connection), m, NULL);
}

static char *path_get_modem(const char *path) {
    char *modem;
    char *d;

    if (!path || strlen(path) < 2)
        return NULL;

    modem = pa_xstrdup(path);
    if ((d = strstr(modem + 1, "/"))) {
        d[0] = '\0';
        return modem;
    }

    pa_xfree(modem);
    return NULL;
}

static void voicecall_hold_and_answer(pa_bluetooth_call_control *control,  const char *path) {
    DBusMessage *m;
    char *modem;

    if ((modem = path_get_modem(path))) {
        m = dbus_message_new_method_call(OFONO_SERVICE, modem, OFONO_VOICECALL_MANAGER_INTERFACE, "HoldAndAnswer");
        dbus_connection_send(pa_dbus_connection_get(control->connection), m, NULL);
        pa_xfree(modem);
    }
}

static void voicecall_swap_calls(pa_bluetooth_call_control *control,  const char *path) {
    DBusMessage *m;
    char *modem;

    if ((modem = path_get_modem(path))) {
        m = dbus_message_new_method_call(OFONO_SERVICE, modem, OFONO_VOICECALL_MANAGER_INTERFACE, "SwapCalls");
        dbus_connection_send(pa_dbus_connection_get(control->connection), m, NULL);
        pa_xfree(modem);
    }
}

void pa_bluetooth_call_control_handle_button(pa_bluetooth_call_control *control) {
    if (!control)
        return;

    if (control->incoming_call_path) {
        if (pa_hashmap_size(control->call_paths) == 1) {
            pa_log_debug("answer incoming %s", control->incoming_call_path);
            voicecall_send(control, control->incoming_call_path, "Answer");
        } else {
            pa_log_debug("hold active calls and answer incoming %s", control->incoming_call_path);
            voicecall_hold_and_answer(control, control->incoming_call_path);
        }
    } else if (pa_hashmap_size(control->active_calls)) {
        pa_log_debug("hangup active call %s", (char *) pa_hashmap_last(control->active_calls));
        voicecall_send(control, pa_hashmap_last(control->active_calls), "Hangup");
        if (pa_hashmap_size(control->held_calls))
            voicecall_swap_calls(control, pa_hashmap_last(control->held_calls));
    } else if (pa_hashmap_size(control->held_calls)) {
        pa_log_debug("hangup held call %s", (char *) pa_hashmap_last(control->held_calls));
        voicecall_send(control, pa_hashmap_last(control->held_calls), "Hangup");
    }
}

static void voicecall_parse_call(pa_bluetooth_call_control *control, DBusMessageIter *arg_i) {
    DBusMessageIter array_i;
    const char *call_state = NULL;
    const char *path;
    char *call_path;

    pa_assert(control);
    pa_assert(arg_i);

    pa_assert(dbus_message_iter_get_arg_type(arg_i) == DBUS_TYPE_OBJECT_PATH);
    dbus_message_iter_get_basic(arg_i, &path);

    pa_assert_se(dbus_message_iter_next(arg_i));
    pa_assert(dbus_message_iter_get_arg_type(arg_i) == DBUS_TYPE_ARRAY);
    dbus_message_iter_recurse(arg_i, &array_i);

    while (dbus_message_iter_get_arg_type(&array_i) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter dict_i;
        const char *entry;

        dbus_message_iter_recurse(&array_i, &dict_i);
        pa_assert(dbus_message_iter_get_arg_type(&dict_i) == DBUS_TYPE_STRING);
        dbus_message_iter_get_basic(&dict_i, &entry);

        if (pa_streq(entry, "State")) {
            DBusMessageIter value_i;
            pa_assert_se(dbus_message_iter_next(&dict_i));
            pa_assert(dbus_message_iter_get_arg_type(&dict_i) == DBUS_TYPE_VARIANT);
            dbus_message_iter_recurse(&dict_i, &value_i);
            dbus_message_iter_get_basic(&value_i, &call_state);
            break;
        }

        dbus_message_iter_next(&array_i);
    }

    pa_log_debug("new call %s: %s", path, call_state ? call_state : "<none>");

    call_path = pa_xstrdup(path);
    if (pa_hashmap_put(control->call_paths, call_path, call_path)) {
        pa_xfree(call_path);
        call_path = pa_hashmap_get(control->call_paths, path);
    }

    if (call_state && (pa_streq(call_state, "incoming") || pa_streq(call_state, "waiting"))) {
        control->incoming_call_path = call_path;
        if (pa_hashmap_size(control->call_paths) == 1)
            pa_bluetooth_native_backend_ring(control->native_transport, true);
    } else {
        pa_hashmap_remove(control->active_calls, call_path);
        pa_hashmap_put(control->active_calls, call_path, call_path);
    }
}

static void get_calls_reply(DBusPendingCall *pending, void *userdata) {
    DBusMessage *r;
    pa_dbus_pending *p;
    pa_bluetooth_call_control *control;
    DBusMessageIter arg_i, array_i, struct_i;

    pa_assert(pending);
    pa_assert_se(p = userdata);
    pa_assert_se(control = p->context_data);
    pa_assert_se(r = dbus_pending_call_steal_reply(pending));

    if (!dbus_message_iter_init(r, &arg_i) || !pa_streq(dbus_message_get_signature(r), "a(oa{sv})")) {
        pa_log_error("Failed to parse " OFONO_VOICECALL_MANAGER_INTERFACE ".GetCalls");
        goto finish;
    }

    pa_assert(dbus_message_iter_get_arg_type(&arg_i) == DBUS_TYPE_ARRAY);
    dbus_message_iter_recurse(&arg_i, &array_i);

    while (dbus_message_iter_get_arg_type(&array_i) == DBUS_TYPE_STRUCT) {
        dbus_message_iter_recurse(&array_i, &struct_i);
        pa_assert(dbus_message_iter_get_arg_type(&struct_i) == DBUS_TYPE_OBJECT_PATH);
        voicecall_parse_call(control, &struct_i);
        dbus_message_iter_next(&array_i);
    }

finish:
    dbus_message_unref(r);

    PA_LLIST_REMOVE(pa_dbus_pending, control->pending, p);
    pa_dbus_pending_free(p);
}

static void voicecall_get_calls(pa_bluetooth_call_control *control, const char *modem_path) {
    DBusMessage *m;

    pa_assert(control);
    pa_assert_se(m = dbus_message_new_method_call(OFONO_SERVICE, modem_path, OFONO_VOICECALL_MANAGER_INTERFACE, "GetCalls"));
    send_and_add_to_pending(control, m, get_calls_reply, NULL);
}

static void get_modems_reply(DBusPendingCall *pending, void *userdata) {
    DBusMessage *r;
    pa_dbus_pending *p;
    pa_bluetooth_call_control *control;
    DBusMessageIter arg_i, array_i, struct_i;

    pa_assert(pending);
    pa_assert_se(p = userdata);
    pa_assert_se(control = p->context_data);
    pa_assert_se(r = dbus_pending_call_steal_reply(pending));

    if (dbus_message_get_type(r) == DBUS_MESSAGE_TYPE_ERROR) {
        pa_log_error(OFONO_MANAGER_INTERFACE ".GetModems() failed: %s: %s", dbus_message_get_error_name(r),
                     pa_dbus_get_error_message(r));
        goto finish;
    }

    if (!dbus_message_iter_init(r, &arg_i) || !pa_streq(dbus_message_get_signature(r), "a(oa{sv})")) {
        pa_log_error("Failed to parse " OFONO_MANAGER_INTERFACE ".GetModems");
        goto finish;
    }

    pa_assert(dbus_message_iter_get_arg_type(&arg_i) == DBUS_TYPE_ARRAY);
    dbus_message_iter_recurse(&arg_i, &array_i);

    while (dbus_message_iter_get_arg_type(&array_i) == DBUS_TYPE_STRUCT) {
        const char *modem_path;
        dbus_message_iter_recurse(&array_i, &struct_i);
        pa_assert(dbus_message_iter_get_arg_type(&struct_i) == DBUS_TYPE_OBJECT_PATH);
        dbus_message_iter_get_basic(&struct_i, &modem_path);
        voicecall_get_calls(control, modem_path);
        dbus_message_iter_next(&array_i);
    }

finish:
    dbus_message_unref(r);

    PA_LLIST_REMOVE(pa_dbus_pending, control->pending, p);
    pa_dbus_pending_free(p);
}

static void voicecall_get_all_calls(pa_bluetooth_call_control *control) {
    DBusMessage *m;

    pa_assert(control);
    pa_assert_se(m = dbus_message_new_method_call(OFONO_SERVICE, "/", OFONO_MANAGER_INTERFACE, "GetModems"));
    send_and_add_to_pending(control, m, get_modems_reply, NULL);
}

static void voicecall_clear_calls(pa_bluetooth_call_control *control) {
    pa_assert(control);

    pa_hashmap_remove_all(control->active_calls);
    pa_hashmap_remove_all(control->held_calls);
    pa_hashmap_remove_all(control->call_paths);
    control->incoming_call_path = NULL;
}

static DBusHandlerResult filter_cb(DBusConnection *bus, DBusMessage *m, void *data) {
    DBusError err;
    pa_bluetooth_call_control *control = data;

    pa_assert(bus);
    pa_assert(m);
    pa_assert(control);

    if (!control->native_transport)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    dbus_error_init(&err);

    if (dbus_message_is_signal(m, "org.freedesktop.DBus", "NameOwnerChanged")) {
        const char *name, *old_owner, *new_owner;

        if (!dbus_message_get_args(m, &err,
                                   DBUS_TYPE_STRING, &name,
                                   DBUS_TYPE_STRING, &old_owner,
                                   DBUS_TYPE_STRING, &new_owner,
                                   DBUS_TYPE_INVALID)) {
            pa_log_error("Failed to parse org.freedesktop.DBus.NameOwnerChanged: %s", err.message);
            goto fail;
        }

        if (pa_streq(name, OFONO_SERVICE)) {

            if (old_owner && *old_owner) {
                pa_log_debug("oFono disappeared");
                voicecall_clear_calls(control);
            }

            if (new_owner && *new_owner) {
                pa_log_debug("oFono appeared");
            }
        }

    } else if (dbus_message_is_signal(m, OFONO_VOICECALL_INTERFACE, "PropertyChanged")) {
        const char *path;
        const char *property;
        const char *state;
        DBusMessageIter arg_i, var_i;

        if (!dbus_message_iter_init(m, &arg_i) || !pa_streq(dbus_message_get_signature(m), "sv")) {
            pa_log_error("Failed to parse " OFONO_VOICECALL_INTERFACE ".PropertyChanged");
            goto fail;
        }

        path = dbus_message_get_path(m);
        dbus_message_iter_get_basic(&arg_i, &property);

        pa_assert_se(dbus_message_iter_next(&arg_i));
        pa_assert(dbus_message_iter_get_arg_type(&arg_i) == DBUS_TYPE_VARIANT);

        if (pa_streq(property, "State")) {
            dbus_message_iter_recurse(&arg_i, &var_i);
            pa_assert(dbus_message_iter_get_arg_type(&var_i) == DBUS_TYPE_STRING);
            dbus_message_iter_get_basic(&var_i, &state);
            pa_log_debug("PropertyChanged %s: %s %s", path, property, state);
            if (pa_streq(state, "active")) {
                char *p;
                if (pa_safe_streq(control->incoming_call_path, path))
                    control->incoming_call_path = NULL;
                pa_hashmap_remove(control->held_calls, path);
                pa_hashmap_remove(control->active_calls, path);
                if ((p = pa_hashmap_get(control->call_paths, path)))
                    pa_hashmap_put(control->active_calls, p, p);
            } else if (pa_streq(state, "held")) {
                char *p;
                pa_hashmap_remove(control->active_calls, path);
                if ((p = pa_hashmap_get(control->call_paths, path)))
                    pa_hashmap_put(control->held_calls, p, p);
            } else if (pa_streq(state, "disconnected")) {
                if (pa_safe_streq(control->incoming_call_path, path))
                    control->incoming_call_path = NULL;
                pa_hashmap_remove(control->active_calls, path);
                pa_hashmap_remove(control->held_calls, path);
                pa_hashmap_remove(control->call_paths, path);
            }
            pa_bluetooth_native_backend_ring(control->native_transport, false);
        }

    } else if (dbus_message_is_signal(m, OFONO_VOICECALL_MANAGER_INTERFACE, "CallAdded")) {
        DBusMessageIter arg_i;

        if (!dbus_message_iter_init(m, &arg_i) || !pa_streq(dbus_message_get_signature(m), "oa{sv}")) {
            pa_log_error("Failed to parse " OFONO_VOICECALL_MANAGER_INTERFACE ".CallAdded");
            goto fail;
        }

        pa_assert(dbus_message_iter_get_arg_type(&arg_i) == DBUS_TYPE_OBJECT_PATH);
        voicecall_parse_call(control, &arg_i);
    }

fail:
    dbus_error_free(&err);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

pa_bluetooth_call_control *pa_bluetooth_call_control_new(pa_core *core,
                                                         pa_bluetooth_transport *t) {
    pa_bluetooth_call_control *control;
    DBusError err;

    pa_assert(core);
    pa_assert(t);
    pa_assert(t->native);

    dbus_error_init(&err);

    control = pa_xnew0(pa_bluetooth_call_control, 1);
    control->core = core;
    control->native_transport = t;

    if (!(control->connection = pa_dbus_bus_get(core, DBUS_BUS_SYSTEM, &err))) {
        pa_log_error("Failed to get D-Bus connection: %s", err.message);
        dbus_error_free(&err);
        goto fail;
    }

    if (!dbus_connection_add_filter(pa_dbus_connection_get(control->connection), filter_cb, control, NULL)) {
        pa_log_error("Failed to add filter function");
        goto fail;
    }

    if (pa_dbus_add_matches(pa_dbus_connection_get(control->connection), &err,
            "type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged',"
            "arg0='" OFONO_SERVICE "'",
            "type='signal',sender='" OFONO_SERVICE "',interface='" OFONO_VOICECALL_INTERFACE "',member='PropertyChanged'",
            "type='signal',sender='" OFONO_SERVICE "',interface='" OFONO_VOICECALL_MANAGER_INTERFACE "',member='CallAdded'",
            NULL) < 0) {
        pa_log("Failed to add oFono D-Bus matches: %s", err.message);
        dbus_connection_remove_filter(pa_dbus_connection_get(control->connection), filter_cb, control);
        dbus_error_free(&err);
        goto fail;
    }

    control->call_paths = pa_hashmap_new_full(pa_idxset_string_hash_func,
                                              pa_idxset_string_compare_func,
                                              pa_xfree,
                                              NULL);
    control->active_calls = pa_hashmap_new_full(pa_idxset_string_hash_func,
                                                pa_idxset_string_compare_func,
                                                NULL,
                                                NULL);
    control->held_calls = pa_hashmap_new_full(pa_idxset_string_hash_func,
                                              pa_idxset_string_compare_func,
                                              NULL,
                                              NULL);

    voicecall_get_all_calls(control);

    return control;

fail:
    if (control->connection)
        pa_dbus_connection_unref(control->connection);
    pa_xfree(control);
    return NULL;
}

void pa_bluetooth_call_control_free(pa_bluetooth_call_control *control) {
    if (!control)
        return;

    pa_hashmap_free(control->active_calls);
    pa_hashmap_free(control->held_calls);
    pa_hashmap_free(control->call_paths);

    pa_dbus_remove_matches(pa_dbus_connection_get(control->connection),
            "type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged',"
            "arg0='" OFONO_SERVICE "'",
            "type='signal',sender='" OFONO_SERVICE "',interface='" OFONO_VOICECALL_INTERFACE "',member='PropertyChanged'",
            "type='signal',sender='" OFONO_SERVICE "',interface='" OFONO_VOICECALL_MANAGER_INTERFACE "',member='CallAdded'",
            NULL);
    dbus_connection_remove_filter(pa_dbus_connection_get(control->connection), filter_cb, control);

    pa_dbus_connection_unref(control->connection);

    pa_xfree(control);
}
