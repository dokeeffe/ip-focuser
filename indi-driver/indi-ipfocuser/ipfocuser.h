/*******************************************************************************
IP focuser based on the INDI focuser simulator
*******************************************************************************/

#ifndef IPFOCUS_H
#define IPFOCUS_H

#include "indifocuser.h"

/*  Some headers we need */
#include <math.h>
#include <sys/time.h>


class IpFocus : public INDI::Focuser
{
public:
    IpFocus();
    virtual ~IpFocus();

    const char *getDefaultName();

    bool initProperties();
    bool updateProperties();

    bool Handshake();

    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool Connect();

    virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration);
    virtual IPState MoveAbsFocuser(uint32_t ticks);
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);

protected:
    virtual bool saveConfigItems(FILE *fp);

private:
    ITextVectorProperty BacklashStepsP;
    ITextVectorProperty AlwaysApproachDirectionP;
    ITextVectorProperty PowerOffEndpointP;
    ITextVectorProperty PowerOnEndpointP;
    
    IText BacklashSteps[1];
    IText AlwaysApproachDirection[1];
    IText PowerOffEndpointT[1];
    IText PowerOnEndpointT[1];

    bool SendGetRequest(const char *path);
    void PowerCycle();
    std::string APIEndPoint;    
};

#endif
