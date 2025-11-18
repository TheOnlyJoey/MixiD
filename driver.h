#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "device_properties.h"

static libusb_device_handle *devh = NULL;
static bool driver_connected = false;

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
  err = libusb_control_transfer(devh, 0x21, 0x1, 0x0100, 0x3c00, (uint8_t*)&one, 2, 0);
  err = libusb_control_transfer(devh, 0x21, 0x1, 0x0101, 0x3c00, (uint8_t*)&zero, 2, 0);
  err = libusb_control_transfer(devh, 0x21, 0x1, 0x0104, 0x3c00, (uint8_t*)&zero, 2, 0);
  err = libusb_control_transfer(devh, 0x21, 0x1, 0x0105, 0x3c00, (uint8_t*)&one, 2, 0);
  if (err < 0) {
    printf("libusb_control_transfer failed: %s\n", libusb_error_name(err));
    exit(1);
  }
}

void set_hp_volume(float volume) {
  assert(volume>=0 && volume<=1);
  uint16_t vol = float_to_u16(volume);
  int err = 0;
  err = libusb_control_transfer(devh, 0x21, 0x1, 0x0203, 0x0a00, (uint8_t*)&vol, 2, 0);
  err = libusb_control_transfer(devh, 0x21, 0x1, 0x0204, 0x0a00, (uint8_t*)&vol, 2, 0);
  if (err < 0) {
    printf("libusb_control_transfer failed: %s\n", libusb_error_name(err));
    exit(1);
  }
}

void set_speaker_volume(float volume) {
  assert(volume>=0 && volume<=1);
  uint16_t vol = float_to_u16(volume);
  int err = 0;
  err = libusb_control_transfer(devh, 0x21, 0x1, 0x1200, 0x3600, (uint8_t*)&vol, 2, 0);
  if (err < 0) {
    printf("libusb_control_transfer failed: %s\n", libusb_error_name(err));
    exit(1);
  }
}

void set_channel_volume(uint16_t chan, float volume) {
  assert(volume>=0 && volume<=1);
  uint16_t vol = float_to_u16(volume);
  int err = 0;
  err = libusb_control_transfer(devh, 0x21, 0x1, 0x0100+(chan*6), 0x3c00, (uint8_t*)&vol, 2, 0);
  err = libusb_control_transfer(devh, 0x21, 0x1, 0x0100+(chan*6)+1, 0x3c00, (uint8_t*)&vol, 2, 0);
  if (err < 0) {
    printf("libusb_control_transfer failed: %s\n", libusb_error_name(err));
    exit(1);
  }
}


//TODO: eventually need ranges set in pre-defined device specific structs
inline std::vector<uint16_t> chanVals {0x0600,0x0601,0x0602,0x0603,0x0604,0x0605};

void set_routing_value(int chan, int position) 
{
  int err = 0;

  err = libusb_control_transfer(devh, 0x21, 0x1, chanVals[chan], 0x3300, (uint8_t*)&routeToggle[chan][position], 1, 0);

  if (err < 0) {
    printf("libusb_control_transfer failed: %s\n", libusb_error_name(err));
    exit(1);
  }
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
  err = libusb_control_transfer(devh, 0x21, 0x1, masterVals[mode], 0x3600, (uint8_t*)&masterToggle[mode], 1, 0);

  if (err < 0) {
    printf("libusb_control_transfer failed: %s\n", libusb_error_name(err));
    exit(1);
  }
  //return masterToggle[mode];
}

inline bool phaseToggle[10] = {false,false,false,false,false,false, false,false,false,false}; // Dummy storage selection storage

void set_phase_state(int chan) //0 indexed
{
  int err = 0;
  phaseToggle[chan] = !phaseToggle[chan];
  err = libusb_control_transfer(devh, 0x21, 0x1, 0x0d01+chan, 0x0b00, (uint8_t*)&phaseToggle[chan], 1, 0);

  if (err < 0) {
    printf("libusb_control_transfer failed: %s\n", libusb_error_name(err));
    exit(1);
  }
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
    driver_connected = false;
    return false;
  }
  assert(devh != NULL);
  err = libusb_set_auto_detach_kernel_driver(devh, 1);
  if (err < 0) {
    printf("libusb_set_auto_detach_kernel_driver failed: %s\n", libusb_error_name(err));
    driver_connected = false;
    return 1;
  }
  err = libusb_claim_interface(devh, 0);
  if (err < 0) {
    printf("libusb_claim_interface failed: %s\n", libusb_error_name(err));
    driver_connected = false;
    return 1;
  }
  driver_connected = true;
  return driver_connected;
}

void driver_shutdown() 
{
  driver_connected = false;
  libusb_reset_device(devh);
  libusb_release_interface(devh, 0);
  libusb_attach_kernel_driver(devh,0);
  libusb_close(devh);
  libusb_exit(NULL);
}
