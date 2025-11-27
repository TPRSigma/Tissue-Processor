# Tissue Processor – User Manual

## Purpose of the Tissue Processor

The purpose of this tissue processor is to automate part of the preparation of biological samples prior to imaging in a scanning electron microscope (SEM). Samples intended for SEM must undergo several preparation steps. They are typically cleaned, trimmed to the required size, and—in the case of biological tissues—subjected to more complex preparation.

No sample containing moisture may enter the vacuum chamber of an electron microscope. Any remaining liquid may cause the sample to explode, potentially damaging delicate detectors. Biological tissues also degrade rapidly, leading to changes in structure and composition. Once dried, the samples become non-conductive and accumulate charge under the electron beam, which reduces image quality.

Therefore, biological tissue samples must be fixed, dehydrated, and later coated with a conductive layer. Fixation and dehydration consist of a series of immersions in suitable liquids, such as various concentrations of ethanol, formaldehyde, or distilled water for rinsing. Performing these immersions manually is monotonous, time-consuming, and prone to errors. The tissue processor automates this sequence.

Although the device is designed for preparing biological samples for SEM imaging, it can also be used for other laboratory procedures that require sequential immersion in multiple baths.

---

## Description of the Tissue Processor

The basic principle of the tissue processor is shown in the schematic diagram in **Fig. 1**, and the finished device is shown in **Fig. 2**. The core of the device is a rotating carousel with 20 openings for glass tubes. Each tube is 100 mm tall and 24 mm in outer diameter and serves as a bath for sample immersion.

These dimensions allow inserting a stack of **four sample baskets**, each containing biological material. These baskets are compatible with the Leica EM CPD300 device and have an outer diameter of 16 mm and a height of 11 mm. The baskets are held in a frame suspended from a motorized lift positioned at the edge of the carousel. The lift, controlled by software, lowers the basket assembly into the tube and raises it again. A detail of this basket assembly is shown in **Fig. 3**.

<img width="1081" height="1443" alt="TP-assembly" src="https://github.com/user-attachments/assets/db4667b2-ca06-4a82-8753-f23126696ace" />

**Fig. 1 –** Overview schematic of the tissue processor with front panel, carousel, lift mechanism, and protective lid.  

![PB250017](https://github.com/user-attachments/assets/2aa4bad4-8f14-4cbd-9fca-2e1008564761)

**Fig. 2 –** Overall view of the manufactured tissue processor with tubes and sample baskets.  

![08](https://github.com/user-attachments/assets/418d5930-504e-4655-b9a6-2b1d70bcb2af)

**Fig. 3 –** Detail of the basket assembly suspended on the lift above the baths.

---

The synchronized motion of the carousel and the lift allows samples to be immersed in different liquids placed in the tubes. A lid covers the baths; it has an opening aligned only with the lift so that the holder can be lowered into the selected tube. The lid prevents evaporation and protects the baths that are not currently in use. During carousel rotation, the lid slightly lifts.

The tissue processor is equipped with network connectivity. It can send SMS notifications about the progress of immersion steps, including error messages or detection of a power outage. In such an event, an internal backup battery ensures that the device can complete the current immersion step and send a warning message.

---

## Protocol

In this context, a *protocol* defines the precise sequence of steps required to prepare the samples—specifically, immersing them in different baths for predetermined times.

The current software rotates the carousel in only one direction, meaning that the samples are immersed sequentially from bath 1 onward.

Protocols are stored as text files on an SD card inserted on the left side of the device. The file name must follow the format:

**proto_NN.txt**  
where **NN** is a number from 01 to 99.

Each protocol file describes the immersion time in each bath. An example of such a control file is shown in **Fig. 4**.  
The first three lines contain comments. The remainder consists of 20 pairs of lines:

- the first line of each pair: name of the bath (matching its contents),  
- the second line: immersion time in seconds.

Future versions of the software may include additional parameters controlling lift movement.

<img width="1081" height="605" alt="VypisSouboru" src="https://github.com/user-attachments/assets/3f4e464b-560a-404e-9403-b61b864fa145" />

**Fig. 4 –** Example of a control file defining the immersion protocol.

---

## Operating the Device

The control panel is located on the front side of the device (see **Fig. 5**). On the left are two buttons, **ENTER** and **MENU**; in the center is a narrow display; and on the right is a rotary knob used for vertical navigation in the menu. Before switching between menu items, the rotary knob must be pressed, similar to a button.

On the left side of the device is the SD card slot. The SD card must contain:

- the protocol control file,  
- the configuration file **config.txt**,  
- the log file **log.txt**.

The device starts when it is connected to a 230 V AC mains power supply. Operating the device consists of two main phases:  
1. filling the baths (tubes) with the required liquids,  
2. running the immersion program.

![11](https://github.com/user-attachments/assets/a36ff0a6-69d1-4835-bf87-db5f3e5de5da)
**Fig. 5 –** Front panel of the tissue processor showing the display with menu and tabs.

---

After turning the device on, the screen displays a menu with five tabs:

- **STAT** – starts the immersion process; shows the status of the process  
- **RUN** – allows manual movements of the carousel and lift  
- **PROG** – selects the protocol file  
- **FILL** – used for filling the baths  
- **SET** – displays configuration settings from config.txt  

Pressing the MENU button cycles through the tabs. Pressing ENTER activates a command or confirms a selection.

---

### Filling the baths

Filling the baths with liquids is an essential step before immersion begins. The device can accommodate up to 20 different baths containing solutions for fixation, rinsing, or dehydration, arranged in the order defined by the protocol.

It is assumed that the liquids are added shortly before the immersion process begins and removed afterward. If the basket holder contains all four baskets, the recommended volumes are:

- **12 ml** for bath 1  
- **10 ml** for each subsequent bath  

To fill the baths, navigate to the **FILL** tab and press ENTER. The carousel rotates to position bath 1 under the opening in the lid. Fill it using a pipette. Press ENTER again to rotate to bath 2, and continue until all baths are filled.

---

### Running the immersion process

Once all baths are filled and the basket holder is suspended on the lift, the immersion process may begin. Navigate to the **STAT** tab and press ENTER. After confirmation, the program immerses the samples in the baths according to the protocol.

The bottom part of the display shows:

- the protocol filename  
- the name of the current bath  
- the remaining time for the immersion  

The upper part of the STAT tab displays:

- current date and time  
- **Power:** whether the device is running on mains power or battery  
- **Battery:** battery status  
- **Network:** the network ID the device is connected to  
- **Protocol:** active protocol filename  
- **Status:** current stage of the immersion sequence (or *to start program* if idle)

---

### Manual control (RUN tab)

In the **RUN** tab, simple manual operations can be performed:

- **Lift UP** – lift the samples out of the bath  
- **Lift DWN** – lower the samples into the bath  
- **Carousel zero** – rotate to the defined starting position  
- **Carousel 1 FWD** – rotate forward by one position  
- **Carousel 1 BCW** – rotate backward by one position  
- **STOP** – immediately stop the running process  
- **ZERO position** – stop and rotate to the starting position without lifting samples  

Carousel movements can be used to sequentially position each tube under the lid, allowing easy removal and cleaning.

The tissue processor is switched off by disconnecting the power supply.
