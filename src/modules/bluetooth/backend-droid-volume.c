/***
  This file is part of PulseAudio.

  Copyright 2017 Jolla Ltd.
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

#include "bluez5-util.h"

#define HSP_MAX_GAIN 15

#define OFONO_SERVICE "org.ofono"
#define BT_VOLUME_INTERFACE "org.nemomobile.ofono.bluetooth.CallVolume"
#define VOICECALL_MANAGER_INTERFACE OFONO_SERVICE ".VoiceCallManager"

struct pa_droid_volume_control {
    pa_core *core;
    pa_bluetooth_discovery *discovery;
    pa_dbus_connection *connection;
    pa_hook_slot *sink_input_volume_changed_slot;
    pa_hook_slot *transport_speaker_gain_changed_slot;
    pa_bluetooth_transport *transport;
    char *modem_path;
};

static void headset_volume_changed(pa_droid_volume_control *control, int gain) {
    pa_sink_input *si;
    uint32_t idx = 0;

    pa_assert(control);

    PA_IDXSET_FOREACH(si, control->core->sink_inputs, idx) {
        if (pa_safe_streq(pa_proplist_gets(si->proplist, PA_PROP_MEDIA_ROLE), "phone")) {
            pa_cvolume volume;
            pa_volume_t v;

            v = (pa_volume_t) (gain * PA_VOLUME_NORM / HSP_MAX_GAIN);

            /* increment volume by one to correct rounding errors */
            if (v < PA_VOLUME_NORM)
                v++;

            pa_cvolume_set(&volume, si->sample_spec.channels, v);

            pa_log_debug("headset volume changes to %d -> %d", gain, v);

            pa_sink_input_set_volume(si, &volume, true, false);

            break;
        }
    }
}

static void headset_volume_set(pa_droid_volume_control *control, unsigned char gain) {
    DBusMessage *m;
    DBusMessageIter arg_i, var_i;
    const char *p = "SpeakerVolume";

    pa_assert(control);

    if (!control->modem_path) {
        pa_log_warn("Set volume: modem path unknown");
        return;
    }

    pa_assert_se(m = dbus_message_new_method_call(OFONO_SERVICE, control->modem_path, BT_VOLUME_INTERFACE, "SetProperty"));

    dbus_message_iter_init_append(m, &arg_i);
    dbus_message_iter_append_basic(&arg_i, DBUS_TYPE_STRING, &p);
    dbus_message_iter_open_container(&arg_i, DBUS_TYPE_VARIANT, DBUS_TYPE_BYTE_AS_STRING, &var_i);
    dbus_message_iter_append_basic(&var_i, DBUS_TYPE_BYTE, &gain);
    dbus_message_iter_close_container(&arg_i, &var_i);

    dbus_connection_send(pa_dbus_connection_get(control->connection), m, NULL);
    dbus_message_unref(m);
}

static pa_hook_result_t sink_input_volume_changed_cb(pa_core *c, pa_sink_input *si, pa_droid_volume_control *control) {
    pa_cvolume volume;

    pa_assert(c);
    pa_assert(si);
    pa_assert(control);
    pa_assert(control->transport);

    if (pa_safe_streq(pa_proplist_gets(si->proplist, PA_PROP_MEDIA_ROLE), "phone")) {
        pa_volume_t v;
        pa_volume_t gain;

        pa_sink_input_get_volume(si, &volume, true);
        v = pa_cvolume_avg(&volume);

        gain = (pa_volume_t) (v * HSP_MAX_GAIN / PA_VOLUME_NORM);

        if (gain > HSP_MAX_GAIN)
            gain = HSP_MAX_GAIN;

        pa_log_debug("phone volume changes to %d -> %d", v, gain);

        switch (control->transport->profile) {
            case PA_BLUETOOTH_PROFILE_DROID_HEADSET_HFP:
                headset_volume_set(control, (unsigned char) gain);
                break;

            case PA_BLUETOOTH_PROFILE_DROID_HEADSET_HSP:
                pa_assert(control->transport->set_speaker_gain);
                control->transport->set_speaker_gain(control->transport, gain);
                break;

            default:
                pa_log_debug("droid hsp/hfp not up, ignoring.");
                break;
        }
    }

    return PA_HOOK_OK;
}

void pa_droid_volume_control_acquire(pa_droid_volume_control *control, pa_bluetooth_transport *t) {
    pa_sink_input *si;
    uint32_t idx = 0;

    pa_assert(control);
    pa_assert(t);

    pa_droid_volume_control_release(control);

    pa_log_debug("volume control acquire %s", pa_bluetooth_profile_to_string(t->profile));
    control->transport = t;
    control->sink_input_volume_changed_slot = pa_hook_connect(&control->core->hooks[PA_CORE_HOOK_SINK_INPUT_VOLUME_CHANGED],
                                                              PA_HOOK_LATE,
                                                              (pa_hook_cb_t) sink_input_volume_changed_cb,
                                                              control);

    /* Apply currently active volume immediately. */
    PA_IDXSET_FOREACH(si, control->core->sink_inputs, idx)
        if (pa_safe_streq(pa_proplist_gets(si->proplist, PA_PROP_MEDIA_ROLE), "phone"))
            sink_input_volume_changed_cb(control->core, si, control);
}

void pa_droid_volume_control_release(pa_droid_volume_control *control) {
    pa_assert(control);

    if (!control->sink_input_volume_changed_slot || !control->transport)
        return;

    pa_log_debug("volume control release %s", pa_bluetooth_profile_to_string(control->transport->profile));
    pa_hook_slot_free(control->sink_input_volume_changed_slot);
    control->sink_input_volume_changed_slot = NULL;
    control->transport = NULL;
}

static DBusHandlerResult filter_cb(DBusConnection *bus, DBusMessage *m, void *data) {
    DBusError err;
    pa_droid_volume_control *control = data;

    pa_assert(bus);
    pa_assert(m);
    pa_assert(control);

    dbus_error_init(&err);

    if (dbus_message_is_signal(m, BT_VOLUME_INTERFACE, "PropertyChanged")) {
        DBusMessageIter arg_i, var_i;
        const char *p;

        if (!dbus_message_iter_init(m, &arg_i) || !pa_streq(dbus_message_get_signature(m), "sv")) {
            pa_log_error("Failed to parse " BT_VOLUME_INTERFACE ".PropertyChanged");
            goto fail;
        }

        dbus_message_iter_get_basic(&arg_i, &p);
        pa_assert_se(dbus_message_iter_next(&arg_i));

        pa_assert(dbus_message_iter_get_arg_type(&arg_i) == DBUS_TYPE_VARIANT);

        dbus_message_iter_recurse(&arg_i, &var_i);

        if (pa_streq(p, "SpeakerVolume")) {
            unsigned char volume;

            pa_assert(dbus_message_iter_get_arg_type(&var_i) == DBUS_TYPE_BYTE);
            dbus_message_iter_get_basic(&var_i, &volume);
            pa_log_debug(BT_VOLUME_INTERFACE " property SpeakerVolume changes to %d", (int) volume);
            headset_volume_changed(control, (int) volume);
        } else if (pa_streq(p, "MicrophoneVolume")) {
            unsigned char volume;

            pa_assert(dbus_message_iter_get_arg_type(&var_i) == DBUS_TYPE_BYTE);
            dbus_message_iter_get_basic(&var_i, &volume);
            pa_log_debug(BT_VOLUME_INTERFACE " property MicrophoneVolume changes to %d", (int) volume);
        } else if (pa_streq(p, "Muted")) {
            dbus_bool_t muted;

            pa_assert(dbus_message_iter_get_arg_type(&var_i) == DBUS_TYPE_BOOLEAN);
            dbus_message_iter_get_basic(&var_i, &muted);
            pa_log_debug(BT_VOLUME_INTERFACE " property Muted changes to %s", muted ? "true" : "false");
        }
    } else if (dbus_message_is_signal(m, VOICECALL_MANAGER_INTERFACE, "CallAdded")) {
        const char *p = NULL;

        /* We are only interested in the object path, and the modem part of it. */
        dbus_message_get_args(m, NULL, DBUS_TYPE_OBJECT_PATH, &p, DBUS_TYPE_INVALID);

        if (p && strlen(p) > 2) {
            char *modem;
            char *d;
            modem = pa_xstrdup(p);
            if ((d = strstr(modem + 1, "/"))) {
                d[0] = '\0';
                pa_log_debug("Setting modem path %s", modem);
                pa_xfree(control->modem_path);
                control->modem_path = modem;
            } else
                pa_xfree(modem);
        }
    }

fail:
    dbus_error_free(&err);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static pa_hook_result_t transport_speaker_gain_changed_cb(pa_bluetooth_discovery *y, pa_bluetooth_transport *t, pa_droid_volume_control *control) {

    pa_assert(t);
    pa_assert(control);

    if (t->profile != PA_BLUETOOTH_PROFILE_DROID_HEADSET_HSP)
        return PA_HOOK_OK;

    headset_volume_changed(control, t->speaker_gain);

    return PA_HOOK_OK;
}

pa_droid_volume_control *pa_droid_volume_control_new(pa_core *c, pa_bluetooth_discovery *y) {
    pa_droid_volume_control *control;
    DBusError err;

    pa_assert(c);

    control = pa_xnew0(pa_droid_volume_control, 1);
    control->core = c;
    control->discovery = y;

    dbus_error_init(&err);

    if (!(control->connection = pa_dbus_bus_get(c, DBUS_BUS_SYSTEM, &err))) {
        pa_log("Failed to get D-Bus connection: %s", err.message);
        dbus_error_free(&err);
        pa_xfree(control);
        return NULL;
    }

    if (!dbus_connection_add_filter(pa_dbus_connection_get(control->connection), filter_cb, control, NULL)) {
        pa_log_error("Failed to add filter function");
        pa_dbus_connection_unref(control->connection);
        pa_xfree(control);
        return NULL;
    }

    if (pa_dbus_add_matches(pa_dbus_connection_get(control->connection), &err,
            "type='signal',sender='" OFONO_SERVICE "',interface='" BT_VOLUME_INTERFACE "',member='PropertyChanged'",
            "type='signal',sender='" OFONO_SERVICE "',interface='" VOICECALL_MANAGER_INTERFACE "',member='CallAdded'",
            NULL) < 0) {
        pa_log("Failed to add oFono D-Bus matches: %s", err.message);
        dbus_connection_remove_filter(pa_dbus_connection_get(control->connection), filter_cb, control);
        pa_dbus_connection_unref(control->connection);
        pa_xfree(control);
        return NULL;
    }

    control->transport_speaker_gain_changed_slot =
        pa_hook_connect(pa_bluetooth_discovery_hook(y, PA_BLUETOOTH_HOOK_TRANSPORT_SPEAKER_GAIN_CHANGED), PA_HOOK_NORMAL, (pa_hook_cb_t) transport_speaker_gain_changed_cb, control);

    return control;
}

void pa_droid_volume_control_free(pa_droid_volume_control *control) {
    pa_assert(control);

    if (control->transport_speaker_gain_changed_slot)
        pa_hook_slot_free(control->transport_speaker_gain_changed_slot);

    pa_dbus_remove_matches(pa_dbus_connection_get(control->connection),
            "type='signal',sender='" OFONO_SERVICE "',interface='" BT_VOLUME_INTERFACE "',member='PropertyChanged'",
            "type='signal',sender='" OFONO_SERVICE "',interface='" VOICECALL_MANAGER_INTERFACE "',member='CallAdded'",
            NULL);

    pa_droid_volume_control_release(control);
    dbus_connection_remove_filter(pa_dbus_connection_get(control->connection), filter_cb, control);

    pa_dbus_connection_unref(control->connection);
    pa_xfree(control->modem_path);
    pa_xfree(control);
}
