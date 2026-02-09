<img width="1330" height="887" alt="Screenshot From 2025-12-17 02-36-46" src="https://github.com/user-attachments/assets/af8ae915-9656-4139-ad9c-05965a8cdb66" />

# MixiD

Unofficial Linux control panel for the Audient iD series interfaces based on libusb, glfw and imgui.

## Description

Since there is no official support by Audient for the iD interfaces on Linux, MixID got created as an alternative to enable the functionality not available in the default class complient USB driver.

## Notes and To Do

* Support list can be found [here](https://github.com/TheOnlyJoey/MixiD/wiki/Support-List)
  * As of 17 December 2025 all known iD interfaces should be functional to some degree
  * If a new device gets released, please add your USB iD and amount of input/output details in an [Issue](https://github.com/TheOnlyJoey/MixiD/issues) so it can be added and verified in an update.
* The protocol is mostly figured out, just needs verification/testing
   * Reading information back from the interfaces is still in progress.
   * Things like VU meters and some switches/modes have yet to be implemented
* Technically works on macOS
   * Should probably not use on Windows
* UI needs some additional work
* For a complete to-do list please check the [issues](https://github.com/TheOnlyJoey/MixiD/issues)

## Compilation

### Dependencies

* CMake
* libglew-dev
* GCC or Clang

### Compile
* git clone the repository
* mkdir Release
* cd Release
* cmake -DCMAKE_BUILD_TYPE=Release ..
* make

## Usage

* Either run through sudo, or setup apropriate udev rules for your interface
* Run the MixiD executable

### udev rules

Since by default, the audio interface is grabbed by the kernel module, and we require exclusive device grab to send information, we need to setup udev rules to allow not needing to use root permissions when opening MixiD.
Luckily, all we have to do is add the Audient vendor id to the udev rules.
The specific user might be different for your distro, but "plugdev" and "audio" seem to be the most commonly used.

Either:
```
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="2708", MODE="0666", GROUP="audio"' >> /etc/udev/rules.d/84-audient.rules
```
or
```
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="2708", MODE="0666", GROUP="plugdev"' >> /etc/udev/rules.d/84-audient.rules
```
depending on your distro's permission group.

Then either reboot or use the following command to reload the udev rules for the running system.
```
udevadm control --reload-rules
```
All done!

## Authors

[@TheOnlyJoey](https://mastodon.online/@TheOnlyJoey)

## Version History

* 0.1.6
   * All known iD usb-id's are now known and implemented, MixiD should work on every known interface!
* 0.1.4
    * Now probes usb devices based on the supported id list and selects if possible
    * Auto disconnects and re-attach to kernel when quitting the application (no more having to manually disconnect before closing)
    * Now should properly set all faders depending on the individual device inputs
    * Small UI Fixes
* 0.1
    * Initial Release based around the iD14 and iD14 MKII with most essential features implemented.

## License

This project is licensed under the MIT License - see the LICENSE.md file for details

## Acknowledgments

* [mymixer](https://github.com/r00tman/mymixer), prior attempt to reverse engineer the original iD14 with some minimal functionality.
* [imgui](https://github.com/ocornut/imgui), my favorite modern lightweight gui
