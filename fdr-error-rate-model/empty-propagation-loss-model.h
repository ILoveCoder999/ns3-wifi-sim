#ifndef EMPTY_PROPAGATION_LOSS_MODEL_H
#define EMPTY_PROPAGATION_LOSS_MODEL_H

#include "ns3/propagation-loss-model.h"

namespace ns3 
{
class EmptyPropagationLossModel : public PropagationLossModel
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    EmptyPropagationLossModel();

    // Delete copy constructor and assignment operator to avoid misuse
    EmptyPropagationLossModel(const EmptyPropagationLossModel&) = delete;
    EmptyPropagationLossModel& operator=(const EmptyPropagationLossModel&) = delete;

  private:
    double DoCalcRxPower(double txPowerDbm,
                         Ptr<MobilityModel> a,
                         Ptr<MobilityModel> b) const override;

    int64_t DoAssignStreams(int64_t stream) override;
};

}

#endif