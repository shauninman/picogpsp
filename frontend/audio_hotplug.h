#ifndef CHECK_AUDIO_H
#define CHECK_AUDIO_H

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

/* Only check once every 60 calls, so this can be called every frame
 * without much penalty */
#define AUDIO_CHECK_THROTTLE 60

bool found_usb_audio = false;
bool has_usb_audio = false;

/* Every AUDIO_CHECK_THROTTLE calls, check to see if the USB audio
 * device has been plugged or unplugged. When the status changes,
 * calls `reinit_callback` with the new status. */
void audio_hotplug_check(void (*reinit_callback)(bool has_usb_audio)) {
  static int count;

  if (++count < AUDIO_CHECK_THROTTLE) return;

  count = 0;

  if (access("/dev/dsp1", R_OK | W_OK) == 0) {
    found_usb_audio = true;
  } else {
    found_usb_audio = false;
  }

  if (!has_usb_audio && found_usb_audio) {
    has_usb_audio = true;
    reinit_callback(true);
  } else if (has_usb_audio && !found_usb_audio) {
    has_usb_audio = false;
    reinit_callback(false);
  }
}

/* For SDL apps, set audio to the correct device. */
void audio_hotplug_set_device(bool has_usb_audio) {
  if (has_usb_audio) {
    SDL_putenv("AUDIODEV=/dev/dsp1");
  } else {
    SDL_putenv("AUDIODEV=/dev/dsp");
  }
}

#endif
