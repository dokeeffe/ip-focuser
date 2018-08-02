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
#include <connectionplugins/connectiontcp.h>

#include <curl/curl.h>

// We declare an auto pointer to ipFocus.
std::unique_ptr<IpFocus> ipFocus(new IpFocus());

#define SIM_SEEING  0
#define SIM_FWHM    1

void ISPoll(void *p);

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void ISGetProperties(const char *dev)
{
    ipFocus->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ipFocus->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ipFocus->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
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
    ipFocus->ISSnoopDevice(root);
}

IpFocus::IpFocus()
{
    SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE);
    setSupportedConnections(CONNECTION_TCP);
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

    tcpConnection->setDefaultHost("192.168.1.203");
    tcpConnection->setDefaultPort(80);

    IUFillText(&AlwaysApproachDirection[0], "ALWAYS_APPROACH_DIR", "Always approach CW/CCW/blank", "CCW");
    IUFillTextVector(&AlwaysApproachDirectionP, AlwaysApproachDirection, 1, getDeviceName(), "BACKLASH_APPROACH_SETTINGS", "Backlash Direction", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    IUFillText(&BacklashSteps[0],"BACKLASH_STEPS","Backlash steps","300");
    IUFillTextVector(&BacklashStepsP,BacklashSteps,1,getDeviceName(),"BACKLASH_STEPS_SETTINGS","Backlash Steps",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    /* props to reboot the focuser device if connection faile. This is easier than debugging strange arduino network issues  */
    IUFillText(&PowerOffEndpointT[0], "POWEROFF_ENDPOINT", "Power Off URL", "http://192.168.2.225:8080/power/focuser/off");
    IUFillTextVector(&PowerOffEndpointP, PowerOffEndpointT, 1, getDeviceName(), "POWEROFF_ENDPOINT", "Power Off", OPTIONS_TAB, IP_RW, 5, IPS_IDLE);
    IUFillText(&PowerOnEndpointT[0], "POWERON_ENDPOINT", "Power On URL", "http://192.168.2.225:8080/power/focuser/on");
    IUFillTextVector(&PowerOnEndpointP, PowerOnEndpointT, 1, getDeviceName(), "POWERON_ENDPOINT", "Power On", OPTIONS_TAB, IP_RW, 5, IPS_IDLE);

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
        defineText(&BacklashStepsP);
        defineText(&AlwaysApproachDirectionP);
        defineText(&PowerOffEndpointP);
        defineText(&PowerOnEndpointP);
    }
    else
    {
        deleteProperty(BacklashStepsP.name);
        deleteProperty(AlwaysApproachDirectionP.name);
    }

    return true;
}

bool IpFocus::Connect() {
  return Handshake();
}
/**
 * Connect and set position values from focuser device response.
**/
bool IpFocus::Handshake()
{
    DEBUG(INDI::Logger::DBG_SESSION, "***** connecting ******");
    APIEndPoint = std::string("http://") + std::string(tcpConnection->host()) + std::string(":80") + std::string("/focuser"); //FIXME: for some reason std::to_string(tcpConnection->getPortFD()) returns 127. So hard code 80 for now.
    DEBUGF(INDI::Logger::DBG_SESSION, "API endpoint %s", APIEndPoint.c_str());
    CURL *curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, APIEndPoint.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); //10 sec timeout
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        DEBUG(INDI::Logger::DBG_DEBUG, "***** performing curl ******");
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK)
        {
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
        DEBUG(INDI::Logger::DBG_DEBUG, "***** completed curl ******");
        DEBUGF(INDI::Logger::DBG_DEBUG, "Focuser response %s", readBuffer.c_str());
        JsonValue value;
        JsonAllocator allocator;
        int status = jsonParse(source, &endptr, &value, allocator);
        //DEBUGF(INDI::Logger::DBG_DEBUG, "Focuser response %s", readBuffer.c_str());
        if (status != JSON_OK)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "%s at %zd", jsonStrError(status), endptr - source);
            DEBUGF(INDI::Logger::DBG_DEBUG, "%s", readBuffer.c_str());
            return false;
        }

        JsonIterator it;
        for (it = begin(value); it!= end(value); ++it)
        {
            DEBUGF(INDI::Logger::DBG_DEBUG, "iterating %s", it->key);
            if (!strcmp(it->key, "absolutePosition"))
            {
                DEBUGF(INDI::Logger::DBG_DEBUG, "Setting absolute position from response %g", it->value.toNumber());
                FocusAbsPosN[0].value = it->value.toNumber();
            }
            if (!strcmp(it->key, "maxPosition"))
            {
                DEBUGF(INDI::Logger::DBG_DEBUG, "Setting max position from response %g", it->value.toNumber());
                FocusAbsPosN[0].max = it->value.toNumber();
            }
            if (!strcmp(it->key, "minPosition"))
            {
                DEBUGF(INDI::Logger::DBG_DEBUG, "Setting min position from response %g", it->value.toNumber());
                FocusAbsPosN[0].min = it->value.toNumber();
            }
        }
    }

    return true;
}

bool IpFocus::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if(strcmp(name,"BACKLASH_APPROACH_SETTINGS")==0)
        {
            IUUpdateText(&AlwaysApproachDirectionP, texts, names, n);
            AlwaysApproachDirectionP.s = IPS_OK;
            IDSetText(&AlwaysApproachDirectionP, NULL);
            return true;
        }
        if(strcmp(name,"BACKLASH_STEPS_SETTINGS")==0)
        {
            IUUpdateText(&BacklashStepsP, texts, names, n);
            BacklashStepsP.s = IPS_OK;
            IDSetText(&BacklashStepsP, NULL);
            return true;
        }

    }

    return INDI::Focuser::ISNewText(dev,name,texts,names,n);
}

IPState IpFocus::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    IDLog("REL-MOVE Speed: %i\n", speed);
    return IPS_OK;

}

IPState IpFocus::MoveAbsFocuser(uint32_t targetTicks)
{
    DEBUGF(INDI::Logger::DBG_SESSION, "Focuser is moving to requested position %ld", targetTicks);
    DEBUGF(INDI::Logger::DBG_DEBUG, "Current Ticks: %.f Target Ticks: %ld", FocusAbsPosN[0].value, targetTicks);

    // //holy crap is it really this hard to build a string in c++
    std::string queryStringPosn = "?absolutePosition=";
    std::string queryStringBacklash = "&amp;backlashSteps=";
    std::string queryStringApproachDir = "&amp;alwaysApproach=";
    auto str = APIEndPoint + queryStringPosn + std::to_string(targetTicks) + queryStringBacklash + BacklashSteps[0].text + queryStringApproachDir + AlwaysApproachDirection[0].text;
    char* getRequestUrl = new char[str.size() + 1];  // +1 char for '\0' terminator
    strcpy(getRequestUrl, str.c_str());
    //end holy crap!!!!

    bool result = SendGetRequest(getRequestUrl);
    if (result == false) {
       PowerCycle();
       result = SendGetRequest(getRequestUrl);
    }
    FocusAbsPosN[0].value = targetTicks; //IMPROVEMENT: Parse the json and grab the real value returned from the arduino device    

    return result ? IPS_OK : IPS_ALERT;

}

/**
 * Power cycle the device if possible. This is a workaround for some instability of the focuser device (some wierd arduino bug)
**/
void IpFocus::PowerCycle() {
    DEBUG(INDI::Logger::DBG_SESSION, "***** POWER CYCLE ******");
    SendGetRequest(PowerOffEndpointT[0].text);
    sleep(3);
    SendGetRequest(PowerOnEndpointT[0].text);
    sleep(12);
    DEBUG(INDI::Logger::DBG_SESSION, "*** POWER CYCLE FINSIHED***");
}

bool IpFocus::SendGetRequest(const char *path) {
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if(curl)
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "Performing request %s", path);
        curl_easy_setopt(curl, CURLOPT_URL, path);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 40L);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Comms failed.:%s",curl_easy_strerror(res));
            return false;
        }
        curl_easy_cleanup(curl);
    }
    return true;
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

    IUSaveConfigText(fp, &AlwaysApproachDirectionP);
    IUSaveConfigText(fp, &PowerOffEndpointP);
    IUSaveConfigText(fp, &PowerOnEndpointP);

    return true;
}
