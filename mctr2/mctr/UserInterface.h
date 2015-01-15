///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
//
// Description:           Header file for UserInterface
// Author:                Vilmos Varga
// mail:                  ethvva@eth.ericsson.se
//
// Copyright Ericsson Telecom AB 2000-2014
//
#ifndef MCTR_USERINTERFACE_H
#define MCTR_USERINTERFACE_H
//----------------------------------------------------------------------------

#include <sys/time.h>

//----------------------------------------------------------------------------

namespace mctr {

//----------------------------------------------------------------------------

/**
 * The user interface singleton interface class.
 */
class UserInterface
{
public:
    /**
     * Constructs the UserInterface.
     */
    UserInterface() { }

    /**
     * Destructor.
     */
    virtual ~UserInterface();

    /**
     * Initialize the user interface.
     */
    virtual void initialize();

    /**
     * Enters the main loop.
     */
    virtual int enterLoop(int argc, char* argv[]) = 0;

    /**
     * Status of MC has changed.
     */
    virtual void status_change() = 0;

    /**
     * Error message from MC.
     */
    virtual void error(int severity, const char* message) = 0;

    /**
     * General notification from MC.
     */
    virtual void notify(const struct timeval* timestamp, const char* source,
                        int severity, const char* message) = 0;

};

//----------------------------------------------------------------------------

} /* namespace mctr */

//----------------------------------------------------------------------------
#endif // MCTR_USERINTERFACE_H

// Local Variables:
// mode: C++
// indent-tabs-mode: nil
// c-basic-offset: 4
// End:
