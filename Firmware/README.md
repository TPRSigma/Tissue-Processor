# Firmware v22.a
<p align="center">
<img src="https://github.com/simple-icons/simple-icons/blob/develop/icons/arduino.svg?raw=1" width="60">
</p>
<span style="font-size: 12px;">© 2025 Jane Doe – Released under the MIT License.</span>

This directory contains the firmware used to control the Tissue Processor device.  
It includes:

- **Main Arduino project** (`tissue_processor.ino`)  
  The primary source code for the microcontroller.  
- **Precompiled firmware binaries** (in the `/bin` subfolder)  
  These files can be flashed directly onto the device without rebuilding the project.

The firmware controls all key functions of the device, including:
- carousel rotation  
- lift motion  
- protocol execution  
- SD card management  
- display and control panel interface  
- error reporting and network notifications
- Bluetooth interface for communication with the Android companion app, which provides a user-friendly way to configure the device and create tissue-processing protocols.

---

## 📦 Structure
```
/Firmware
│
├── tissue_processor.ino        # Arduino IDE project
│
└── bin/
    ├── firmware_v1.0.bin
    ├── firmware_v1.0.elf
    └── ...
```
---

## 🛠️ Flashing the Firmware

To upload the firmware:

1. Open the `.ino` file in **Arduino IDE** (or PlatformIO).  
2. Select the correct board and COM port.  
3. Click **Upload**.

Or, to flash the precompiled binary using `esptool.py`:

```bash
esptool.py --chip esp32 write_flash 0x10000 firmware_v1.0.bin
