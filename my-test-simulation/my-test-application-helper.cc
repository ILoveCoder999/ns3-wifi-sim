#include "my-test-application-helper.h"
#include "my-test-application.h"

using namespace ns3;

TestApplicationHelper::TestApplicationHelper(const Address peerAddress, const uint32_t peerPort)
: ApplicationHelper(TestApplication::GetTypeId())
{
    SetAttribute("PeerAddress", AddressValue(peerAddress));
    SetAttribute("PeerPort", UintegerValue(peerPort));
}