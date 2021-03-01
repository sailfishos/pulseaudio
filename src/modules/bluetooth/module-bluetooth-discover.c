/***
  This file is part of PulseAudio.

  Copyright 2013 João Paulo Rechi Vita

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

#include <pulsecore/core-util.h>
#include <pulsecore/macro.h>
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/strbuf.h>

PA_MODULE_AUTHOR("João Paulo Rechi Vita");
PA_MODULE_DESCRIPTION("Detect available Bluetooth daemon and load the corresponding discovery module");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_USAGE(
    "headset=ofono|native|auto (bluez 5 only) "
    "autodetect_mtu=<boolean> (bluez 5 only) "
    "sco_sink=<name of sink> (bluez 4 only) "
    "sco_source=<name of source> (bluez 4 only)"
);

struct versioned_arg {
    const uint32_t version;
    const char *string;
};

#define BZ_VERSION_4     (0)
#define BZ_VERSION_5     (1)
#define BZ_VERSION_COUNT (2)

static const struct versioned_arg args[] = {
    { BZ_VERSION_4, "sco_sink" },
    { BZ_VERSION_4, "sco_source" },
    { BZ_VERSION_5, "headset" },
    { BZ_VERSION_5, "autodetect_mtu" },
};

struct userdata {
    uint32_t bluez5_module_idx;
    uint32_t bluez4_module_idx;
};

static bool key_exists(const char *key, uint32_t *version) {
    uint32_t i;

    pa_assert(version);

    for (i = 0; i < sizeof(args) / sizeof(struct versioned_arg); i++) {
        if (pa_safe_streq(args[i].string, key)) {
            *version = args[i].version;
            return true;
        }
    }

    return false;
}

static void add_arg(pa_strbuf **sb, const char *key, const char *value) {
    pa_assert(sb);
    pa_assert(key);
    pa_assert(value);

    if (!*sb)
        *sb = pa_strbuf_new();
    else
        pa_strbuf_putc(*sb, ' ');

    pa_strbuf_printf(*sb, "%s=%s", key, value);
}

int pa__init(pa_module* m) {
    struct userdata *u;
    pa_module *mm;
    pa_modargs *ma;
    pa_strbuf *bluez_args[BZ_VERSION_COUNT] = { };
    const char *key;
    uint32_t version;
    char *arg_string;
    void *state = NULL;
    uint32_t i;

    pa_assert(m);

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->bluez5_module_idx = PA_INVALID_INDEX;
    u->bluez4_module_idx = PA_INVALID_INDEX;

    ma = pa_modargs_new(m->argument, NULL);

    while ((key = pa_modargs_iterate(ma, &state))) {
        if (key_exists(key, &version))
            add_arg(&bluez_args[version], key, pa_modargs_get_value(ma, key, ""));
    }

    if (pa_module_exists("module-bluez5-discover")) {
        arg_string = bluez_args[BZ_VERSION_5] ? pa_strbuf_to_string(bluez_args[BZ_VERSION_5]) : NULL;
        if (pa_module_load(&mm, m->core, "module-bluez5-discover", pa_modargs_get_value(ma, "bluez5_args", arg_string)) == 0)
            u->bluez5_module_idx = mm->index;
        pa_xfree(arg_string);
    }

    if (pa_module_exists("module-bluez4-discover")) {
        arg_string = bluez_args[BZ_VERSION_4] ? pa_strbuf_to_string(bluez_args[BZ_VERSION_4]) : NULL;
        if (pa_module_load(&mm, m->core, "module-bluez4-discover",  pa_modargs_get_value(ma, "bluez4_args", arg_string)) == 0)
            u->bluez4_module_idx = mm->index;
        pa_xfree(arg_string);
    }

    pa_modargs_free(ma);
    for (i = 0; i < BZ_VERSION_COUNT; i++)
        if (bluez_args[i])
            pa_strbuf_free(bluez_args[i]);

    if (u->bluez5_module_idx == PA_INVALID_INDEX && u->bluez4_module_idx == PA_INVALID_INDEX) {
        pa_xfree(u);
        return -1;
    }

    return 0;
}

void pa__done(pa_module* m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->bluez5_module_idx != PA_INVALID_INDEX)
        pa_module_unload_by_index(m->core, u->bluez5_module_idx, true);

    pa_xfree(u);
}
