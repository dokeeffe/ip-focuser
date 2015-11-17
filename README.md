# ip-focuser

*work in progress*

The focuser hardware is based on the following parts

 1. 1 x Arduino nano (http://www.dx.com/p/arduino-nano-v3-0-81877#.ViKE2RCrRE4)
 2. 1 x PCB ENC28J60 Ethernet Module HanRun HR911105A (14/10) (http://www.dx.com/p/pcb-arduino-enc28j60-ethernet-module-blue-140971#.ViKE_xCrRE4)
 3. 1 x TB6560 3A Single-Axis Controller Stepper Motor Driver(http://www.dx.com/p/tb6560-3a-single-axis-controller-stepper-motor-driver-board-green-black-red-217142#.ViKE8hCrRE4)
 4. 1 x Old uniplar stepper motor that I found in the shed.

Device HTTP API
---------------

### Get the state of the focuser
**Method** GET 

**Path** 'http://192.168.1.203/focuser'

**Response code** 200

**Response body**

```javascript
{
    "uptime": "00:01:23",
    "speed": 112,
    "temperature": null,
    "temperatureCompensationOn": false,
    "backlashSteps": 200,
    "absolutePosition": 10000,
    "maxPosition": 20000,
    "minPosition": 0,
    "gearBoxMultiplier": 10
}
```

### Move the focuser to an absolute position
This request will block until motor motion is complete. Take care with timeouts if you are writing a new client.

**Method** GET 

**Path** 'http://192.168.1.203/focuser?absolutePosition=8000'

**Response code** 200

**Response body**

```javascript
{
    "uptime": "00:01:46",
    "speed": 112,
    "temperature": null,
    "temperatureCompensationOn": false,
    "backlashSteps": 200,
    "absolutePosition": 8000,
    "maxPosition": 20000,
    "minPosition": 0,
    "gearBoxMultiplier": 10
}
```
