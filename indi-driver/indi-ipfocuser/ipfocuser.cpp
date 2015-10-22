/*******************************************************************************
  INDI driver for a motorized telescope focuser which uses HTTP communication.
  See arduino-firmware for details of the device firmware that this INDI driver is for.

*******************************************************************************/
#include "ipfocuser.h"
#include "gason.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <memory>

#include <curl/curl.h>

// We declare an auto pointer to focusSim.
std::auto_ptr<IpFocus> focusSim(0);

#define SIM_SEEING  0
#define SIM_FWHM    1
#define FOCUS_MOTION_DELAY  100                /* Focuser takes 100 microsecond to move for each step, completing 100,000 steps in 10 seconds */

void ISPoll(void *p);


void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(focusSim.get() == 0) focusSim.reset(new IpFocus());

}

void ISGetProperties(const char *dev)
{
        ISInit();
        focusSim->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        focusSim->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        focusSim->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        focusSim->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root)
{
    ISInit();
    focusSim->ISSnoopDevice(root);
}

IpFocus::IpFocus()
{
    ticks=0; //TODO: add other params to .h file and set here

    SetFocuserCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE); //TODO: add FOCUSE R_HAS_VARIABLE_SPEED. the http interface supports it
}

bool IpFocus::SetupParms()
{
    IDSetNumber(&FWHMNP, NULL);
    return true;
}

bool IpFocus::Connect()
{

    CURL *curl;
    CURLcode res;
 
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.1.203/focuser");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); //10 sec timeout
        res = curl_easy_perform(curl);
        /* Check for errors */ 
        if(res != CURLE_OK) {
	    DEBUGF(INDI::Logger::DBG_ERROR, "Connecttion to FOCUSER failed:%s",curl_easy_strerror(res));            
            return false;
        }
        /* always cleanup */ 
        curl_easy_cleanup(curl);
        //TODO: parse json and get current abs position then set FocusAbsPosN[0].value = absPositionFromDevice; ... Or do it in initProperties below.
    }

    SetTimer(1000);     //  start the timer
    return true;
}

IpFocus::~IpFocus()
{
    //dtor
}

const char * IpFocus::getDefaultName()
{
        return (char *)"Focuser Simulator";
}

bool IpFocus::initProperties()
{
    INDI::Focuser::initProperties();

    IUFillNumber(&SeeingN[0],"SIM_SEEING","arcseconds","%4.2f",0,60,0,3.5);
    IUFillNumberVector(&SeeingNP,SeeingN,1,getDeviceName(),"SEEING_SETTINGS","Seeing",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&FWHMN[0],"SIM_FWHM","arcseconds","%4.2f",0,60,0,7.5);
    IUFillNumberVector(&FWHMNP,FWHMN,1,getDeviceName(), "FWHM","FWHM",MAIN_CONTROL_TAB,IP_RO,60,IPS_IDLE);

    ticks = initTicks = sqrt(FWHMN[0].value - SeeingN[0].value) / 0.75;

    /* Relative and absolute movement */
    FocusRelPosN[0].min = 0.;
    FocusRelPosN[0].max = 5000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step = 1000;

    FocusAbsPosN[0].min = 0.;
    FocusAbsPosN[0].max = 20000.;  //TODO: Pair with firmware
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step = 1000;

    FocusAbsPosN[0].value = FocusAbsPosN[0].max / 2;    //FIXME: remove this and maybe call HTTP GET again.

    return true;
}

bool IpFocus::updateProperties()
{

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&SeeingNP);
        defineNumber(&FWHMNP);
        SetupParms();
    }
    else
    {
        deleteProperty(SeeingNP.name);
        deleteProperty(FWHMNP.name);
    }

    return true;
}


bool IpFocus::Disconnect()
{
    return true;
}


void IpFocus::TimerHit()
{
    int nexttimer=1000;

    if(isConnected() == false) return;  //  No need to reset timer if we are not connected anymore

    SetTimer(nexttimer);
    return;
}

/**
 * Chain of command for setting numeric values
**/
bool IpFocus::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if(strcmp(name,"SEEING_SETTINGS")==0)
        {
            SeeingNP.s = IPS_OK;
            IUUpdateNumber(&SeeingNP, values, names, n);

            IDSetNumber(&SeeingNP,NULL);
            return true;

        }

    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Focuser::ISNewNumber(dev,name,values,names,n);
}

/**
 * Chain of command for setting switch values
**/
bool IpFocus::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    //  Nobody has claimed this, so, ignore it, pass it up the chain
    return INDI::Focuser::ISNewSwitch(dev,name,states,names,n);
}


IPState IpFocus::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
//    double targetTicks = (speed * duration) / (FocusSpeedN[0].max * FocusTimerN[0].max);
//    double plannedTicks=ticks;
//    double plannedAbsPos=0;
//
//    if (dir == FOCUS_INWARD)
//        plannedTicks -= targetTicks;
//    else
//        plannedTicks += targetTicks;
//
//    if (isDebug())
//        IDLog("Current ticks: %g - target Ticks: %g, plannedTicks %g\n", ticks, targetTicks, plannedTicks);
//
//    plannedAbsPos = (plannedTicks - initTicks) * 5000 + (FocusAbsPosN[0].max - FocusAbsPosN[0].min)/2;
//
//    if (plannedAbsPos < FocusAbsPosN[0].min || plannedAbsPos > FocusAbsPosN[0].max)
//    {
//        IDMessage(getDeviceName(), "Error, requested position is out of range.");
//        return IPS_ALERT;
//    }
//
//    ticks = plannedTicks;
//    if (isDebug())
//          IDLog("Current absolute position: %g, current ticks is %g\n", plannedAbsPos, ticks);
//
//
//    FWHMN[0].value = 0.5625*ticks*ticks +  SeeingN[0].value;
//    FocusAbsPosN[0].value = plannedAbsPos;
//
//
//    if (FWHMN[0].value < SeeingN[0].value)
//        FWHMN[0].value = SeeingN[0].value;
//
//    IDSetNumber(&FWHMNP, NULL);
//    IDSetNumber(&FocusAbsPosNP, NULL);
//
    IDLog("RELMOVE speed: %i\n", speed);
    return IPS_OK;

}

IPState IpFocus::MoveAbsFocuser(uint32_t targetTicks)
{
    if (targetTicks < FocusAbsPosN[0].min || targetTicks > FocusAbsPosN[0].max)
    {
        IDMessage(getDeviceName(), "Error, requested absolute position is out of range.");
        return IPS_ALERT;
    }

    double mid = (FocusAbsPosN[0].max - FocusAbsPosN[0].min)/2;

    IDMessage(getDeviceName() , "Focuser is moving to requested position");

    // Limit to +/- 10 from initTicks
    //WTF? ticks = initTicks + (targetTicks - mid) / 5000.0;

    //if (isDebug())
    IDLog("Current ticks: %g\n", ticks);
    IDLog("Current ticks: %i\n", targetTicks);

    //Now create HTTP GET to move focuser
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if(curl) {
        //holy crap is it really this hard to build a string in c++
        std::string url = "http://192.168.1.203/focuser?absolutePosition="; 
        auto str = url + std::to_string(targetTicks); 
        char* temp_line = new char[str.size() + 1];  // +1 char for '\0' terminator
        strcpy(temp_line, str.c_str());
        //end holy crap!!!!
    
        curl_easy_setopt(curl, CURLOPT_URL, temp_line);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); //30sec should be enough
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK) {
            DEBUGF(INDI::Logger::DBG_ERROR, "COMMS to focuser failed.:%s",curl_easy_strerror(res));
            IDMessage(getDeviceName(), "Error. Could not communicate with focuser");
            return IPS_ALERT;
        }
        curl_easy_cleanup(curl);
    }




    FocusAbsPosN[0].value = targetTicks; //TODO: Parse the json and grab the real value returned from the arduino device

    FWHMN[0].value = 0.5625*ticks*ticks +  SeeingN[0].value;

    if (FWHMN[0].value < SeeingN[0].value)
        FWHMN[0].value = SeeingN[0].value;

    IDSetNumber(&FWHMNP, NULL);

    return IPS_OK;

}

IPState IpFocus::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    uint32_t targetTicks = FocusAbsPosN[0].value + (ticks * (dir == FOCUS_INWARD ? -1 : 1));

    FocusAbsPosNP.s = IPS_BUSY;
    IDSetNumber(&FocusAbsPosNP, NULL);

    return MoveAbsFocuser(targetTicks);
}

