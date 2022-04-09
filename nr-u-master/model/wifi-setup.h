/* Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; */
/*
 *   Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *   Copyright (c) 2021 Orange Labs
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2 as
 *   published by the Free Software Foundation;
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef WIFISETUP_H
#define WIFISETUP_H

#include "l2-setup.h"
#include <ns3/wifi-phy.h>
#include <unordered_map>

namespace ns3 {

class SpectrumChannel;
class ThreeGppPropagationLossModel;
class ThreeGppSpectrumPropagationLossModel;

/**
 * \brief Setup Wi-Fi as L2 technology
 */
class WifiSetup : public L2Setup
{
public:
  WifiSetup (const NodeContainer &apNodes, const NodeContainer &staNodes,
             const Ptr<SpectrumChannel> &channel,
             const Ptr<ThreeGppPropagationLossModel> &propagation,
             const Ptr<ThreeGppSpectrumPropagationLossModel> & spectrumPropagation,
             double freq, double bw,
             double apTxPower, double staTxPower,
             double ccaThresholdAp, double ccaThresholdSta,
             enum WifiStandard standard, const std::string &ssid);
  virtual ~WifiSetup () override;

  virtual Ipv4InterfaceContainer AssignIpv4ToUe (const std::unique_ptr<Ipv4AddressHelper> &address) const override;
  virtual Ipv4InterfaceContainer AssignIpv4ToStations (const std::unique_ptr<Ipv4AddressHelper> &address) const override;

  virtual void ConnectToRemotes (const NodeContainer &remoteHosts, const std::string &base) override;

  typedef Callback<void, uint32_t, Time>  ChannelRequestToAccessDurationCb;

  void SetChannelRTACallback (const ChannelRequestToAccessDurationCb &cb);

private:
  void PhyTxDrop (std::string context, Ptr<const Packet> p);
  void PhyRxDrop (std::string context, Ptr<const Packet> p, WifiPhyRxfailureReason);
  /**
   * Tx Occupancy Callback Function.
   *
   * \param start the start time of the new state
   * \param duration the duration of the new state
   * \param newState the new state
   */
  void TxOccupancy (std::string context, Time t1, Time t2, WifiPhyState newState);
  void RxDataTrace (std::string context, Ptr<const Packet>, uint16_t, WifiTxVector, MpduInfo, SignalNoiseDbm, uint16_t);
  void ChannelRequestToAccessDuration (std::string context, Time oldValue, Time newValue);

  ChannelRequestToAccessDurationCb m_craCb;
};

} // namespace ns3

#endif /* WIFISETUP_H */
