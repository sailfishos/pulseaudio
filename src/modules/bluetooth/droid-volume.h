#ifndef foobluez5droidvolumefoo
#define foobluez5droidvolumefoo

/***
  This file is part of PulseAudio.

  Copyright 2018 Jolla Ltd.

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

#include <pulsecore/core.h>
#include <pulsecore/hook-list.h>
#include "bluez5-util.h"

typedef struct pa_droid_volume_control pa_droid_volume_control;

pa_droid_volume_control* pa_droid_volume_control_new(pa_core *c, pa_hook *speaker_gain_changed);
void pa_droid_volume_control_free(pa_droid_volume_control *control);

void pa_droid_volume_control_acquire(pa_droid_volume_control *control, pa_bluetooth_transport *t);
void pa_droid_volume_control_release(pa_droid_volume_control *control);

#endif
