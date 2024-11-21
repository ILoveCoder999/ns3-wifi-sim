#ifndef MY_TEST_APPLICATION_HELPER_H
#define MY_TEST_APPLICATION_HELPER_H

#include "ns3/application-helper.h"

class TestApplicationHelper : public ns3::ApplicationHelper
{
    public:
        TestApplicationHelper(const ns3::Address peerAddress, const uint32_t peerPort);
};

#endif