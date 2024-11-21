#include "empty-propagation-loss-model.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(EmptyPropagationLossModel);

TypeId
EmptyPropagationLossModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::EmptyPropagationLossModel")
            .SetParent<PropagationLossModel>()
            .SetGroupName("Propagation")
            .AddConstructor<EmptyPropagationLossModel>();
    return tid;
}

EmptyPropagationLossModel::EmptyPropagationLossModel()
{
}

int64_t
EmptyPropagationLossModel::DoAssignStreams(int64_t stream)
{
    return 0;
}

double
EmptyPropagationLossModel::DoCalcRxPower(double txPowerDbm,
                                         Ptr<MobilityModel> a,
                                         Ptr<MobilityModel> b) const
{
    return txPowerDbm;
}

}