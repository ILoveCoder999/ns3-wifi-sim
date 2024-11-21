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

#ifndef FDR_ERROR_RATE_MODEL_H
#define FDR_ERROR_RATE_MODEL_H

#include "ns3/error-rate-model.h"
#include "ns3/wifi-mode.h"

#include <optional>

namespace ns3
{

class WifiTxVector;

/*
 * \ingroup wifi
 * \brief the interface for the Frame Delivery Ratio error model
 *
 */
class FdrErrorRateModel : public ErrorRateModel
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    FdrErrorRateModel();
    ~FdrErrorRateModel() override;

  private:
    double DoGetChunkSuccessRate(WifiMode mode,
                                 const WifiTxVector& txVector,
                                 double snr,
                                 uint64_t nbits,
                                 uint8_t numRxAntennas,
                                 WifiPpduField field,
                                 uint16_t staId) const override;

    Ptr<ErrorRateModel>
        m_fallbackErrorModel; //!< Error rate model to fallback to if no value is found in the table

    double m_fdr; //!< The Frame Delivery Ratio
};

} // namespace ns3

#endif
