#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "device_properties.h"

static libusb_device_handle *devh = NULL;
static bool driver_connected = false;
static int control_iface = 0;

// libusb reads a 0 timeout as "wait forever", so a request the firmware never
// answers froze the whole UI with nothing printed at all. Bound every control
// transfer, and name the request that failed instead of the function it was in.
static const unsigned int USB_TIMEOUT_MS = 1000;

static int id_control(uint16_t wValue, uint16_t wIndex, void *data, uint16_t len)
{
  int err = libusb_control_transfer(devh, 0x21, 0x1, wValue, wIndex, (uint8_t*)data, len, USB_TIMEOUT_MS);
  if (err < 0) {
    printf("control transfer wValue=0x%04x wIndex=0x%04x len=%u failed: %s\n",
           wValue, wIndex, len, libusb_error_name(err));
  }
  return err;
}

int device_probe()
{
  // discover devices
  libusb_device **list;
  libusb_device *found = NULL;
  libusb_context *tctx = NULL;
  libusb_init(NULL);
  ssize_t cnt = libusb_get_device_list(tctx   , &list);
  ssize_t i = 0;
  int err = 0;
  if (cnt < 0) {
    return -1;
  }

  int foundid = -1;

  std::cout << "[Begin usb probe] size: " << cnt << "\n";
  for (i = 0; i < cnt; i++) 
  {
      libusb_device *device = list[i];
      libusb_device_descriptor desc = {0};
      libusb_get_device_descriptor(device, &desc);
      std::cout << "Vendor: " << std::hex << desc.idVendor << " - Product: " << std::hex << desc.idProduct << '\n';
      for (size_t d = 0; d < devices.size(); d++)
      {
        if (desc.idVendor != 0x2708) {
          continue;
        }
        if (devices[d].usb_id == desc.idProduct) {
            foundid = d;
            break;
        }
      }
  }
  libusb_free_device_list(list, 1);
  return foundid;
}

// The iD firmware accepts the mixer control requests on any of its interfaces,
// not just the AudioControl one (see issue #15). Sending them to the spare
// DFU/vendor interface means snd-usb-audio keeps the audio interfaces and
// playback continues while MixiD is connected. Returns -1 if the device has
// no such interface.
int find_control_interface(libusb_device *dev)
{
  struct libusb_config_descriptor *config;
  if (libusb_get_active_config_descriptor(dev, &config) != LIBUSB_SUCCESS)
    return -1;
  int found = -1;
  for (int i = 0; i < config->bNumInterfaces; i++) {
    const struct libusb_interface_descriptor *alt = &config->interface[i].altsetting[0];
    if (alt->bInterfaceClass == LIBUSB_CLASS_APPLICATION || alt->bInterfaceClass == LIBUSB_CLASS_VENDOR_SPEC) {
      found = alt->bInterfaceNumber;
      break;
    }
  }
  libusb_free_config_descriptor(config);
  return found;
}


//--------
// First is for Left channels, Second for Right
// Order, Main Mix, Alt Speaker, Cue A, Cue B, DAW Mix
// Left is First,x - Right is x,Second
// --------
// 
// Channel 1: {0x1b,x},{0x1d,x},{0x19,x},{0x1a,x},{0x00,x}
// Channel 2: {x,0x1c},{x,0x1e},{x,0x19},{x,0x1a},{x,0x01}
// Channel 3: {0x1b, x},{0x1d, x},{0x19, x},{0x1a, x},{0x02, x}
// Channel 4: {x, 0x1c},{x, 0x1e},{x, 0x19},{x,0x1a},{x,0x03}
// Channel 3: {0x1b, x},{0x1d, x},{0x19, x},{0x1a, x},{0x04, x}
// Channel 4: {x, 0x1c},{x, 0x1e},{x, 0x19},{x,0x1a},{x,0x05}

inline std::vector<std::vector<uint8_t>> routeToggle 
{
  {0x1b,0x1d,0x19,0x1a,0x00}, //Channel 1 (Main L)
  {0x1c,0x1e,0x19,0x1a,0x01}, //Channel 2 (Main R)
  {0x1b,0x1d,0x19,0x1a,0x02}, //Channel 3
  {0x1c,0x1e,0x19,0x1a,0x03}, //Channel 4
  {0x1b,0x1d,0x19,0x1a,0x04}, //Channel 5 (HP L)
  {0x1c,0x1e,0x19,0x1a,0x05}  //Channel 6 (HP R)
};

uint16_t float_to_u16(float volume) {
  uint16_t res = -32768+32767*volume;
  return res;
}

void set_vinyl_dm(float volume) {
  uint16_t one = float_to_u16(volume);
  uint16_t zero = float_to_u16(0);
  int err = 0;
  err = id_control(0x0100, 0x3c00 | control_iface, &one, 2);
  err = id_control(0x0101, 0x3c00 | control_iface, &zero, 2);
  err = id_control(0x0104, 0x3c00 | control_iface, &zero, 2);
  err = id_control(0x0105, 0x3c00 | control_iface, &one, 2);
}

void set_hp_volume(float volume) {
  assert(volume>=0 && volume<=1);
  uint16_t vol = float_to_u16(volume);
  int err = 0;
  err = id_control(0x0203, 0x0a00 | control_iface, &vol, 2);
  err = id_control(0x0204, 0x0a00 | control_iface, &vol, 2);
}

void set_speaker_volume(float volume) {
  assert(volume>=0 && volume<=1);
  uint16_t vol = float_to_u16(volume);
  int err = 0;
  err = id_control(0x1200, 0x3600 | control_iface, &vol, 2);
}

void set_channel_volume(uint16_t chan, float volume) {
  assert(volume>=0 && volume<=1);
  uint16_t vol = float_to_u16(volume);
  int err = 0;
  err = id_control(0x0100+(chan*6), 0x3c00 | control_iface, &vol, 2);
  err = id_control(0x0100+(chan*6)+1, 0x3c00 | control_iface, &vol, 2);
}


//TODO: eventually need ranges set in pre-defined device specific structs
inline std::vector<uint16_t> chanVals {0x0600,0x0601,0x0602,0x0603,0x0604,0x0605};

void set_routing_value(int chan, int position) 
{
  int err = 0;

  err = id_control(chanVals[chan], 0x3300 | control_iface, &routeToggle[chan][position], 1);

}

//TODO: eventually need ranges set in pre-defined device specific structs
inline std::vector<uint16_t> masterVals 
{
  0x0500,//Dim
  0x0c00,//Alt
  0x0700,//Talkback
  0x0300,//Phase
  0x0000,//Mono
  0x0400 //Speaker Mute
};
inline bool masterToggle[6] = {false,false,false,false,false,false}; // Dummy storage selection storage

void set_bool_state(int mode) 
{
  int err = 0;
  masterToggle[mode] = !masterToggle[mode];
  err = id_control(masterVals[mode], 0x3600 | control_iface, &masterToggle[mode], 1);

  //return masterToggle[mode];
}

inline bool phaseToggle[10] = {false,false,false,false,false,false, false,false,false,false}; // Dummy storage selection storage

void set_phase_state(int chan) //0 indexed
{
  int err = 0;
  phaseToggle[chan] = !phaseToggle[chan];
  err = id_control(0x0d01+chan, 0x0b00 | control_iface, &phaseToggle[chan], 1);

  //return masterToggle[mode];
}


//Boosted is range 0.0-1.1
int driver_init(uint16_t deviceid)
{
  int err;
  err = libusb_init(NULL);
  assert(err == LIBUSB_SUCCESS);

  devh = libusb_open_device_with_vid_pid(NULL, 0x2708, deviceid); //MKI
  //if (!devh)
//	devh = libusb_open_device_with_vid_pid(NULL, 0x2708, 0x0008); //MKII
  if (!devh) {
    // Silence here read as "connected" in the UI while nothing was controlled;
    // the usual cause is no permission on /dev/bus/usb (see the udev rules).
    printf("could not open 2708:%04x: check the udev rule or run as root\n", deviceid);
    driver_connected = false;
    return false;
  }
  assert(devh != NULL);

  // Talk to the device through its spare DFU/vendor interface, so the kernel
  // audio driver keeps the AudioControl/Streaming interfaces and playback
  // keeps running while MixiD is connected. Devices without a spare
  // interface fall back to the old exclusive grab of interface 0.
  control_iface = find_control_interface(libusb_get_device(devh));
  if (control_iface < 0) {
    printf("no spare control interface found, grabbing interface 0 (audio pauses while connected)\n");
    control_iface = 0;
    err = libusb_set_auto_detach_kernel_driver(devh, 1);
    if (err < 0) {
      printf("libusb_set_auto_detach_kernel_driver failed: %s\n", libusb_error_name(err));
      driver_connected = false;
      return false;
    }
  }
  printf("sending mixer control to interface %d\n", control_iface);
  err = libusb_claim_interface(devh, control_iface);
  if (err < 0) {
    printf("libusb_claim_interface failed: %s\n", libusb_error_name(err));
    driver_connected = false;
    return false;
  }
  driver_connected = true;
  return driver_connected;
}

void driver_shutdown()
{
  driver_connected = false;
  if (!devh)
    return;
  if (control_iface == 0) {
    // exclusive mode kicked the kernel driver off interface 0, hand it back
    libusb_reset_device(devh);
    libusb_release_interface(devh, 0);
    libusb_attach_kernel_driver(devh,0);
  }
  else {
    libusb_release_interface(devh, control_iface);
  }
  libusb_close(devh);
  libusb_exit(NULL);
  devh = NULL;
}
