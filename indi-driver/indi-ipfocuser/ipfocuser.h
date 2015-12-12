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
    protected:
        virtual bool saveConfigItems(FILE *fp);
    private:


        double ticks;
        double initTicks;

        INumberVectorProperty SeeingNP;
        INumberVectorProperty FWHMNP;
        INumberVectorProperty BacklashStepsP;
        ITextVectorProperty FocuserEndpointTP;
        ITextVectorProperty AlwaysApproachDirectionP;
        INumber SeeingN[1];
        INumber FWHMN[1];
        INumber BacklashSteps[1];
        IText FocuserEndpointT[1];
        IText AlwaysApproachDirection[1];

        bool SetupParms();

    public:
        IpFocus();
        virtual ~IpFocus();

        const char *getDefaultName();

        bool initProperties();
        bool updateProperties();

        bool Connect();
        bool Disconnect();

        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual void ISGetProperties (const char *dev);

        virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration);
        virtual IPState MoveAbsFocuser(uint32_t ticks);
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);


};

#endif
