/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
  
  Author(s):  Peter Kazanzides
  Created on: 2013-09-23
  
  (C) Copyright 2013 Johns Hopkins University (JHU), All Rights Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty.  The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/

#include <cppunit/TestCase.h>
#include <cppunit/extensions/HelperMacros.h>

class mtsSocketProxyTest: public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(mtsSocketProxyTest);

    CPPUNIT_TEST(TestCommandHandle);

    CPPUNIT_TEST_SUITE_END();
    
public:
    void setUp(void) {}
    
    void tearDown(void) {}
    
    void TestCommandHandle(void);

};


CPPUNIT_TEST_SUITE_REGISTRATION(mtsSocketProxyTest);

