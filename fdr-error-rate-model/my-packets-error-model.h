//
// Created by matteo on 26/08/24.
//

#ifndef MY_PACKETS_RATE_ERROR_MODEL_H
#define MY_PACKETS_RATE_ERROR_MODEL_H
#include "ns3/error-model.h"

namespace ns3 {

class MyPacketsErrorModel: public ErrorModel {
public:

  MyPacketsErrorModel();
  ~MyPacketsErrorModel() override;

  static TypeId GetTypeId();

private:
  bool DoCorrupt(Ptr<Packet> pkt) override;
  void DoReset() override;

  Ptr<RateErrorModel> m_errorModel;
};

} // ns3

#endif //MY_PACKETS_RATE_ERROR_MODEL_H
