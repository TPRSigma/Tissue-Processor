# Tissue Processor - Quick Start Guide
---
### 1. Prepare the SD Card
Create or copy the following files to the SD card:

**proto_XX.txt** protocol file that defines immersion steps
**config.txt** device configuration (network, timings, etc.)
**log.txt** created and updated automatically

Protocol files must follow the format:
proto_01.txt ... proto_99.txt


Each protocol contains:
- 3 comment lines  
- 20× (bath name + duration in seconds)

---

### 2. Fill the Reagent Baths
1. Turn on the device (230 V AC).  
2. Insert the SD card on the left side of the device.  
3. Navigate to **FILL** using the `MENU` button.  
4. Press **ENTER** → carousel rotates to bath 1.  
5. Fill using a pipette.  
6. Press **ENTER** again to move to bath 2.  
7. Repeat for all baths.

Recommended volumes (for 4 baskets):  
- **Bath 1:** 12 ml  
- **Baths 2–20:** 10 ml each  

---

### 3. Load the Sample Baskets
- Insert up to **4 baskets** (Leica-compatible) into the holder.  
- Suspend the holder on the motorized lift.

---

### 4. Run the Immersion Process
1. Navigate to **STAT** and press **ENTER**.  
2. Confirm the selected protocol.  
3. The processor begins immersing samples according to the timing in the protocol.

Displayed status includes:
- current bath  
- remaining time  
- power/battery state  
- network status  
- selected protocol file  
- overall process state

---

### 5. Manual Control (Optional)
Under **RUN** tab:
- **Lift UP / Down**  
- **Carousel zero**  
- **Carousel 1 FWD / BCW**  
- **STOP / ZERO position**

---

### 6. After Processing
- Remove and clean all tubes.  
- Turn off the device by disconnecting power.

---

## Full User Manual

The complete detailed manual (purpose, description, interface, protocol format, operation, etc.) is in the manual.md and/or manual.pdf files in this ("User-Manual") repository folder.

