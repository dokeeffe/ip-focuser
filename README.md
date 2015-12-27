# ip-focuser

ip-focuser is an arduino based focuser for an astronomical telescope. Communication is via an HTTP json API. Also included in this project is an Indi driver to allow it to be used with any Indi client. When used with the Ekos Indi client, reliable auto focus can be achieved, limited only by local seeing conditions. for more information on Ekos and Indi see here http://indilib.org/about/ekos/focus-module.html.

Currently the system is providing automatic focus for [Ballyhoura Observatory](https://ballyhouraobservatory.wordpress.com)

The focuser hardware is based on the following parts

 1. 1 x Arduino nano (http://www.dx.com/p/arduino-nano-v3-0-81877#.ViKE2RCrRE4)
 2. 1 x PCB ENC28J60 Ethernet Module HanRun HR911105A (14/10) (http://www.dx.com/p/pcb-arduino-enc28j60-ethernet-module-blue-140971#.ViKE_xCrRE4)
 3. 1 x TB6560 3A Single-Axis Controller Stepper Motor Driver(http://www.dx.com/p/tb6560-3a-single-axis-controller-stepper-motor-driver-board-green-black-red-217142#.ViKE8hCrRE4)
 4. 1 x Old uniplar stepper motor that I found in the shed.

Device HTTP API
---------------

### Getting state

| Task | Method | Path | 
| ------------- | ------------- | ------------- |
| Get the state of the focuser | GET | http://192.168.1.203/focuser | 

**Response Code** 200

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
### Motion

| Task | Method | Path | 
| ------------- | ------------- | ------------- |
| Move the focuser to an absolute position | GET | http://192.168.1.203/focuser?absolutePosition=8000 | 

NOTE: This call will block until motor motion completes.

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
### Configuring speed and backlash

| Task | Method | Path | 
| ------------- | ------------- | ------------- |
| Configure stepper speed | GET | http://192.168.1.203/focuser?speed=250 | 

This call will result in no motor motion. It will set the speed for future motion requests.

**Response code** 200

**Response body**

```javascript
{
    "uptime": "00:01:52",
    "speed": 250,
    "temperature": null,
    "temperatureCompensationOn": false,
    "backlashSteps": 200,
    "absolutePosition": 8000,
    "maxPosition": 20000,
    "minPosition": 0,
    "gearBoxMultiplier": 10
}
```
