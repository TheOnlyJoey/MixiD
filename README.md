<img width="1276" height="809" alt="Screenshot 2025-11-15 at 04 34 14" src="https://github.com/user-attachments/assets/0cd46f82-3bb4-4cd8-8c75-fe303f6d0922" />

# MixiD

Unofficial Linux control panel for the Audient iD series interfaces based on libusb, glfw and imgui.

## Description

Since there is no official support by Audient for the iD interfaces on Linux, MixID got created as an alternative to enable the functionality not available in the default class complient USB driver.

## Notes and To Do

* Currently only tested on iD14 and iD14 MKII, but should work with every iD series interface.
  * Support list can be found [here](https://github.com/TheOnlyJoey/MixiD/wiki/Support-List)
  * Please add your USB iD and amount of input/output details in an Issue so it can be added and verified in an update.
* More usb ID's need to be added together with proper channel information to support all remaining iD interfaces.
   * The protocol is mostly figured out, just needs verification/testing
* Technically works on macOS
   * Should probably not use on Windows
* UI needs some additional work

## Compilation

### Dependencies

* CMake
* libglew-dev
* GCC or Clang

### Usage

* Either run through sudo, or setup apropriate udev rules for your interface
* Run the MixiD executable

## udev rules

Since by default, the audio interface is grabbed by the kernel module, and we require exclusive device grab to send information, we need to setup udev rules to allow not needing to use root permissions when opening MixiD.
Luckily, all we have to do is add the Audient vendor id to the udev rules.

```
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="2708", MODE="0666", GROUP="plugdev"' >> /etc/udev/rules.d/84-audient.rules
udevadm control --reload-rules
```

## Authors

[@TheOnlyJoey](https://mastodon.online/@TheOnlyJoey)

## Version History

* 0.1
    * Initial Release based around the iD14 and iD14 MKII with most essential features implemented.

## License

This project is licensed under the MIT License - see the LICENSE.md file for details

## Acknowledgments

* [mymixer](https://github.com/r00tman/mymixer), prior attempt to reverse engineer the original iD14 with some minimal functionality.
* [imgui](https://github.com/ocornut/imgui), my favorite modern lightweight gui
