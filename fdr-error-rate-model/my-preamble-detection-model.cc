#include "my-preamble-detection-model.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(MyPreambleDetectionModel);

MyPreambleDetectionModel::MyPreambleDetectionModel() {

}

TypeId 
MyPreambleDetectionModel::GetTypeId() {
    static TypeId tid =
        TypeId("ns3::MyPreambleDetectionModel").SetParent<PreambleDetectionModel>().AddConstructor<MyPreambleDetectionModel>().SetGroupName("Wifi");
    return tid;
}

bool MyPreambleDetectionModel::IsPreambleDetected(double rssi, double snr, double channelWidth) const {
    return true;
}

}
