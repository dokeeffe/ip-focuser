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

// We declare an auto pointer to ipFocus.
std::auto_ptr<IpFocus> ipFocus(0);

#define SIM_SEEING  0
#define SIM_FWHM    1

void ISPoll(void *p);

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(ipFocus.get() == 0) ipFocus.reset(new IpFocus());

}

void ISGetProperties(const char *dev)
{
        ISInit();
        ipFocus->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        ipFocus->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        ipFocus->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        ipFocus->ISNewNumber(dev, name, values, names, num);
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
    ipFocus->ISSnoopDevice(root);
}

IpFocus::IpFocus()
{
    ticks=0;

    SetFocuserCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE); //TODO: add FOCUSE R_HAS_VARIABLE_SPEED. the http interface supports it
}

bool IpFocus::SetupParms()
{
    IDSetNumber(&FWHMNP, NULL);
    return true;
}

/**
 * Connect and set position values from focuser response.
**/
bool IpFocus::Connect()
{

    if (FocuserEndpointT[0].text == NULL)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Focuser HTTP API endpoint is not available. Set it in the options tab");
        return false;   
    }
    
    CURL *curl;
    CURLcode res;
    std::string readBuffer;
 
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, FocuserEndpointT[0].text);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); //10 sec timeout
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        /* Check for errors */ 
        if(res != CURLE_OK) {
	        DEBUGF(INDI::Logger::DBG_ERROR, "Connecttion to FOCUSER failed:%s",curl_easy_strerror(res));
            DEBUG(INDI::Logger::DBG_ERROR, "Is the HTTP API endpoint correct? Set it in the options tab. Can you ping the focuser?");
            return false;
        }
        /* always cleanup */ 
        curl_easy_cleanup(curl);

        char srcBuffer[readBuffer.size()];
        strncpy(srcBuffer, readBuffer.c_str(), readBuffer.size());
        char *source = srcBuffer;
        // do not forget terminate source string with 0
        char *endptr;
        JsonValue value;
        JsonAllocator allocator;
        int status = jsonParse(source, &endptr, &value, allocator);
        if (status != JSON_OK)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "%s at %zd", jsonStrError(status), endptr - source);
            DEBUGF(INDI::Logger::DBG_DEBUG, "%s", readBuffer.c_str());
            return IPS_ALERT;
        }
        DEBUGF(INDI::Logger::DBG_DEBUG, "Focuser response %s", readBuffer.c_str());
        JsonIterator it;
        for (it = begin(value); it!= end(value); ++it) {
            DEBUGF(INDI::Logger::DBG_DEBUG, "iterating %s", it->key);
            if (!strcmp(it->key, "absolutePosition")) {
                DEBUGF(INDI::Logger::DBG_DEBUG, "Setting absolute position from response %g", it->value.toNumber());
                FocusAbsPosN[0].value = it->value.toNumber();
            }
            if (!strcmp(it->key, "maxPosition")) {
                DEBUGF(INDI::Logger::DBG_DEBUG, "Setting max position from response %g", it->value.toNumber());
                FocusAbsPosN[0].max = it->value.toNumber();
            }
            if (!strcmp(it->key, "minPosition")) {
                DEBUGF(INDI::Logger::DBG_DEBUG, "Setting min position from response %g", it->value.toNumber());
                FocusAbsPosN[0].min = it->value.toNumber();
            }
        }
    }

    return true;
}

IpFocus::~IpFocus()
{
    //dtor
}

const char * IpFocus::getDefaultName()
{
        return (char *)"IP Focuser";
}

bool IpFocus::initProperties()
{
    INDI::Focuser::initProperties();
    
    IUFillText(&FocuserEndpointT[0], "API_ENDPOINT", "API Endpoint", "http://192.168.1.203/focuser");
    IUFillTextVector(&FocuserEndpointTP, FocuserEndpointT, 1, getDeviceName(), "FOCUSER_API_ENDPOINT", "Focuser", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    IUFillText(&AlwaysApproachDirection[0], "ALWAYS_APPROACH_DIR", "Always approach CW/CCW/blank", "CCW");
    IUFillTextVector(&AlwaysApproachDirectionP, AlwaysApproachDirection, 1, getDeviceName(), "BACKLASH_APPROACH_SETTINGS", "Backlash Direction", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&BacklashSteps[0],"BACKLASH_STEPS","Backlash steps","%4.0f",0,60,0,100);
    IUFillNumberVector(&BacklashStepsP,BacklashSteps,1,getDeviceName(),"BACKLASH_STEPS_SETTINGS","Backlash Steps",OPTIONS_TAB,IP_RW,60,IPS_IDLE);


    IUFillNumber(&SeeingN[0],"SIM_SEEING","arcseconds","%4.2f",0,60,0,3.5);
    IUFillNumberVector(&SeeingNP,SeeingN,1,getDeviceName(),"SEEING_SETTINGS","Seeing",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&FWHMN[0],"SIM_FWHM","arcseconds","%4.2f",0,60,0,7.5);
    IUFillNumberVector(&FWHMNP,FWHMN,1,getDeviceName(), "FWHM","FWHM",MAIN_CONTROL_TAB,IP_RO,60,IPS_IDLE);

    ticks = initTicks = sqrt(FWHMN[0].value - SeeingN[0].value) / 0.75;

    /* Relative and absolute movement settings which are not set on connect*/
    FocusRelPosN[0].min = 0.;
    FocusRelPosN[0].max = 5000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step = 1000;

    FocusAbsPosN[0].step = 1000;

    addDebugControl();
    return true;
}

bool IpFocus::updateProperties()
{

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&SeeingNP);
        defineNumber(&FWHMNP);
        defineNumber(&BacklashStepsP);
        defineText(&FocuserEndpointTP);
        defineText(&AlwaysApproachDirectionP);
        SetupParms();
    }
    else
    {
        deleteProperty(SeeingNP.name);
        deleteProperty(FWHMNP.name);
        deleteProperty(BacklashStepsP.name);
        deleteProperty(FocuserEndpointTP.name);
        deleteProperty(AlwaysApproachDirectionP.name);
    }

    return true;
}


bool IpFocus::Disconnect()
{
    return true;
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
   return INDI::Focuser::ISNewNumber(dev,name,values,names,n);
}

void IpFocus::ISGetProperties(const char *dev)
{
    INDI::Focuser::ISGetProperties(dev);

    defineText(&FocuserEndpointTP);
    defineText(&AlwaysApproachDirectionP);
    defineNumber(&BacklashStepsP);

    loadConfig(true, "FOCUSER_API_ENDPOINT");
    loadConfig(true, "BACKLASH_APPROACH_SETTINGS");
    loadConfig(true, "BACKLASH_STEPS_SETTINGS");
}

bool IpFocus::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if(strcmp(name,"FOCUSER_API_ENDPOINT")==0)
        {
            IUUpdateText(&FocuserEndpointTP, texts, names, n);
            FocuserEndpointTP.s = IPS_OK;
            IDSetText(&FocuserEndpointTP, NULL);
            return true;
        }
        if(strcmp(name,"BACKLASH_APPROACH_SETTINGS")==0)
        {
            IUUpdateText(&AlwaysApproachDirectionP, texts, names, n);
            AlwaysApproachDirectionP.s = IPS_OK;
            IDSetText(&AlwaysApproachDirectionP, NULL);
            return true;
        }

    }

     return INDI::Focuser::ISNewText(dev,name,texts,names,n);
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
    //TODO: calc and delegate to MoveAbsFocuser
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

    if (isDebug()) {
        IDLog("Current ticks: %g\n", ticks);
        IDLog("Current ticks: %i\n", targetTicks);
    }

    //Now create HTTP GET to move focuser
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if(curl) {
        //holy crap is it really this hard to build a string in c++
        std::string queryStringPosn = "?absolutePosition=";
        std::string queryStringBacklash = "&amp;backlashSteps=";
        std::string queryStringApproachDir = "&amp;alwaysApproach=";
        auto str = FocuserEndpointT[0].text + queryStringPosn + std::to_string(targetTicks) + queryStringBacklash + std::to_string(BacklashSteps[0].value) + queryStringApproachDir + AlwaysApproachDirection[0].text;
        char* getRequestUrl = new char[str.size() + 1];  // +1 char for '\0' terminator
        strcpy(getRequestUrl, str.c_str());
        //end holy crap!!!!

        DEBUGF(INDI::Logger::DBG_DEBUG, "Performing request %s", getRequestUrl);
        curl_easy_setopt(curl, CURLOPT_URL, getRequestUrl);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 45L); //45sec should be enough
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

bool IpFocus::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);

    IUSaveConfigText(fp, &FocuserEndpointTP);
    IUSaveConfigText(fp, &AlwaysApproachDirectionP);

    return true;
}

