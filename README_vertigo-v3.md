# vertigo v3

## Motor Nanotec PD4

this material and software is only needed in case the parameters (e.g. max, min speed, torque) on the motor should be changed.

- [Nanotec PD4-C5918L4204-E-08](https://www.nanotec.com/eu/de/produkte/1791-pd4-c5918l4204-e-08)
    - [objetc dictionary](https://www.nanotec.com/eu/de/produkte/manual/m/pd4c-canopen-de/p/object_dictionary%2Fobject_dir_intro.html)
- [CanOpen USB Interface](https://www.nanotec.com/eu/de/produkte/9987-zk-usb-can-1-converter-from-usb-to-canopen)
- [Cable from CAN USB to Motor](https://www.nanotec.com/eu/de/produkte/1630-zk-pd4-c-can-4-500-s-canopen-kabel)
- [PlugNDrive Studio 3](https://www.nanotec.com/eu/de/produkte/10492-plug-drive-studio-3) 
    - needed in case Motor parameters should be changed or NanoJ Program shoud be changed

## DMX 

the motor control box has a DMX 0-10V converter. Each motor needs four channels (currently only the first 3 channels are used). For channel 0 and 3 (motor ON/OFF and lights ON/OFF) send them 0 and 255 for OFF/ON. In the following table are the tresholds on when those values trigger. The actual value can depend on the actual motors and 0-10V interface.

Since each mechanism uses 4 dmx channels. The first channel has to be selected on the Mean Led interfaces to interface each motor individually. For this the ids of the devices are **`1, 5, 9, 13`**

```
channel 1 = motor on/off, on >=64, off <=24
channel 2 = motor velocity, velocity: 4-32rpm = 16-255, 0rpm <16
channel 3 = lamp on/off, on >=0xda, off <=0xba
channel 4 = lamp dim (not used)

for motor
< 24 = OFF
> 64 = ON

for lamp
< 0xba = OFF
> 0xda = ON
```

### DMX Material

- [Enttec S-PLAY NANO Recorder](https://www.enttec.com/product/light-show-control/s-play-nano-1-universe-smart-light-show-controller/)
- [DMX to 10V converter inside the boxes, MeanLed DL512](https://www.everen.de/dl512-010-dmx-0-10v-decoder-4-kanaele-hutschiene?c=3)
    - [Manual MeanLed DL512](datasheets/Manual_MeanLed_DL512-010.pdf)

### DMX Software

- [VEZÉR](https://imimot.com/vezer/)
- [Touchdesigner](https://derivative.ca/UserGuide/TouchDesigner)
- Python...


### MIDI CC to DMX Mapping (Teensy USB MIDI Prototype)

**Prototype mapping for 3 DMX units (motors/lamps):**

| Unit   | DMX Channels | MIDI CCs      | Function (per unit)         |
|--------|--------------|---------------|-----------------------------|
| Unit 1 | 1, 2, 3      | CC1, CC2, --- | ON/OFF, velocity, lamp      |
| Unit 2 | 5, 6, 7      | CC5, CC6, --- | ON/OFF, velocity, lamp      |
| Unit 3 | 9, 10, 11    | CC9, CC10, ---| ON/OFF, velocity, lamp      |

**DMX channels 3, 7, 11 (lamp ON/OFF) are intentionally not mapped to any MIDI CC for safety during prototype testing.**

**Lamp safety note:**
- The Philips MasterColour lamp requires a minimum 15-minute re-ignition time after shutdown to avoid damage.
- During prototype testing, do not send DMX ON/OFF commands to channels 3, 7, or 11.
- In future code, implement a 15-minute lockout after a lamp OFF command before allowing another ON command.

**Example mapping for safe prototype:**

| MIDI CC | DMX Channel | Function         |
|---------|-------------|------------------|
| CC1     | 1           | Unit 1 ON/OFF    |
| CC2     | 2           | Unit 1 velocity  |
|         | 3           | (lamp, not used) |
| CC5     | 5           | Unit 2 ON/OFF    |
| CC6     | 6           | Unit 2 velocity  |
|         | 7           | (lamp, not used) |
| CC9     | 9           | Unit 3 ON/OFF    |
| CC10    | 10          | Unit 3 velocity  |
|         | 11          | (lamp, not used) |

**DMX channels 4, 8, 12, 13 are unused.**

**Future improvement:**
- Add software logic to block retriggering lamp ON for 15 minutes after an OFF command is sent to any lamp channel.


### DMX pinout from MeanLED DMXInterface to 3pin XLR 

Sommercable Isopod

```
white DMX D-
cyan  DMX D+
black/blue DMX GND
```