#ifndef _H_DEVICEPROPERTIES_H_
#define _H_DEVICEPROPERTIES_H_

#include <string>
#include <vector>

struct device_properties {
	std::string name;
	uint16_t usb_id;
	int mic_inputs;
	int digital_inputs;
	int outputs;
	int digital_outputs;
	int inserts = 0;
};

static std::vector<device_properties> devices;

void setup_devices()
{
	struct device_properties iD14;
	iD14.name = "iD14";
	iD14.usb_id = 0x0002;
	iD14.mic_inputs = 2;
	iD14.digital_inputs = 8;
	iD14.outputs = 4;
	iD14.digital_outputs = 0;
	devices.push_back(iD14);

	struct device_properties iD14MKII;
	iD14MKII.name = "iD14 MKII";
	iD14MKII.usb_id = 0x0008;
	iD14MKII.mic_inputs = 2;
	iD14MKII.digital_inputs = 8;
	iD14MKII.outputs = 4;
	iD14MKII.digital_outputs = 0;
	devices.push_back(iD14MKII);

	struct device_properties iD22;
	iD22.name = "iD22";
	iD22.usb_id = 0x0001;
	iD22.mic_inputs = 2;
	iD22.digital_inputs = 8;
	iD22.outputs = 4;
	iD22.digital_outputs = 8;
	iD22.inserts = 2;
	devices.push_back(iD22);

	struct device_properties iD44;
	iD44.name = "iD44";
	iD44.usb_id = 0x0005;
	iD44.mic_inputs = 4;
	iD44.digital_inputs = 16;
	iD44.outputs = 4;
	iD44.digital_outputs = 16;
	iD44.inserts = 2;
	devices.push_back(iD44);

	struct device_properties iD48;
	iD48.name = "iD48";
	iD48.usb_id = 0x0012;
	iD48.mic_inputs = 8;
	iD48.digital_inputs = 16;
	iD48.outputs = 4;
	iD48.digital_outputs = 16;
	iD48.inserts = 8;
	devices.push_back(iD48);

}

#endif