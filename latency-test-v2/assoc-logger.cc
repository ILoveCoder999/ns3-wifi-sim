#include "assoc-logger.h"
#include "ns3/simulator.h"


NS_LOG_COMPONENT_DEFINE("AssocLogger");

AssocLogger::AssocLogger(std::string out_file_path, std::string header, Ptr<MobilityModel> mobility):
     _header(header), _mobility(mobility)
{
    _output_file.open(out_file_path.c_str(), std::ios::out | std::ios::trunc);
}

void AssocLogger::logHeader()
{
    //_output_file << "[" << std::endl << _header << std::endl;
}

void AssocLogger::logFooter()
{
    //_output_file << "]" << std::endl;
}

void AssocLogger::assocCallback(Mac48Address value)
{
    _output_file << _currentInfo() << ", Association, " << value << std::endl;
}

void AssocLogger::deAssocCallback(Mac48Address value)
{
    _output_file << _currentInfo() << ", De-Association, " << value << std::endl;
}

void AssocLogger::beaconArrivalCallback(Time t)
{
    _output_file << _currentInfo(t) << ", Beacon arrived" << std::endl;
}

void AssocLogger::receivedBeaconInfoCallback(StaWifiMac::ApInfo apInfo)
{
    _output_file << _currentInfo() << ", Beacon info, " << apInfo << std::endl;
}

std::string AssocLogger::_currentInfo()
{
    return _currentInfo(Simulator::Now());
}

std::string  AssocLogger::_currentInfo(Time t) {
    std::stringstream ss;
    Vector position = _mobility->GetPosition();
    ss << t.ToInteger(ns3::Time::Unit::NS) << ", " << "(" << position.x << ", " <<  position.y << ", " << position.z << ")";
    return ss.str();
}

