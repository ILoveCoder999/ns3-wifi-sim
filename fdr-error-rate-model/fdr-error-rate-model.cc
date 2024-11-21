/*
 * Copyright (c) 2020 University of Washington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Rohan Patidar <rpatidar@uw.edu>
 *          Sébastien Deronne <sebastien.deronne@gmail.com>
 */

#include "fdr-error-rate-model.h"

#include "ns3/wifi-tx-vector.h"
#include "ns3/wifi-utils.h"
#include "ns3/yans-error-rate-model.h"

#include "ns3/dsss-error-rate-model.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/double.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(FdrErrorRateModel);

NS_LOG_COMPONENT_DEFINE("FdrErrorRateModel");

TypeId
FdrErrorRateModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::FdrErrorRateModel")
            .SetParent<ErrorRateModel>()
            .SetGroupName("Wifi")
            .AddConstructor<FdrErrorRateModel>()
            .AddAttribute("FallbackErrorRateModel",
                          "Ptr to the fallback error rate model to be used when no matching value "
                          "is found in a table",
                          PointerValue(nullptr),
                          MakePointerAccessor(&FdrErrorRateModel::m_fallbackErrorModel),
                          MakePointerChecker<ErrorRateModel>())
            .AddAttribute("FrameDeliberyRatio",
                          "The Frame Delivery Ratio",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&FdrErrorRateModel::m_fdr),
                          MakeDoubleChecker<double>(0.0, 1.0));
    return tid;
}

FdrErrorRateModel::FdrErrorRateModel()
{
    NS_LOG_FUNCTION(this);
}

FdrErrorRateModel::~FdrErrorRateModel()
{
    NS_LOG_FUNCTION(this);
    m_fallbackErrorModel = nullptr;
}

double
FdrErrorRateModel::DoGetChunkSuccessRate(WifiMode mode,
                                                const WifiTxVector& txVector,
                                                double snr,
                                                uint64_t nbits,
                                                uint8_t numRxAntennas,
                                                WifiPpduField field,
                                                uint16_t staId) const
{
    NS_LOG_FUNCTION(this << mode << txVector << snr << nbits << +numRxAntennas << field << staId);
    return m_fdr;
}

} // namespace ns3
