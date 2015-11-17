# ip-focuser

*work in progress*

The focuser hardware is based on the following parts

 1. 1 x Arduino nano (http://www.dx.com/p/arduino-nano-v3-0-81877#.ViKE2RCrRE4)
 2. 1 x PCB ENC28J60 Ethernet Module HanRun HR911105A (14/10) (http://www.dx.com/p/pcb-arduino-enc28j60-ethernet-module-blue-140971#.ViKE_xCrRE4)
 3. 1 x TB6560 3A Single-Axis Controller Stepper Motor Driver(http://www.dx.com/p/tb6560-3a-single-axis-controller-stepper-motor-driver-board-green-black-red-217142#.ViKE8hCrRE4)
 4. 1 x Old uniplar stepper motor that I found in the shed.

Example HTTP interface
curl 'http://192.168.1.203/focuser'

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
