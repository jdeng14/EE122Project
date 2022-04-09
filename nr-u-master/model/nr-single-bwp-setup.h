/* Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; */
/*
 *   Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
#ifndef NR_SINGLE_BWP_SETUP_H
#define NR_SINGLE_BWP_SETUP_H

#include "l2-setup.h"
#include <unordered_map>
#include <ns3/nr-module.h>

namespace ns3 {

class NrHelper;
class NrPointToPointEpcHelper;
class ThreeGppSpectrumChannel;
class ThreeGppPropagationLossModel;

/**
 * \brief Setup NR as L2 technology with one bandwith part
 */
class NrSingleBwpSetup : public L2Setup
{
public:
  NrSingleBwpSetup (const NodeContainer &gnbNodes, const NodeContainer &ueNodes,
                    const Ptr<SpectrumChannel> &channel,
                    const Ptr<ThreeGppPropagationLossModel> &propagation,
                    const Ptr<ThreeGppSpectrumPropagationLossModel> &spectrumPropagation,
                    double freq, double bw, uint32_t num, double gnbTxPower, double ueTxPower,
                    const std::unordered_map<uint32_t, uint32_t> &ueGnbMap, const std::string &gnbCamType, const std::string &ueCamType,
                    const std::string &scheduler, const BandwidthPartInfo::Scenario &scenario);
  virtual ~NrSingleBwpSetup () override;

  virtual void ConnectToRemotes (const NodeContainer &remoteHosts, const std::string &base) override;
  virtual Ipv4InterfaceContainer AssignIpv4ToUe (const std::unique_ptr<Ipv4AddressHelper> &address) const override;
  virtual Ipv4InterfaceContainer AssignIpv4ToStations (const std::unique_ptr<Ipv4AddressHelper> &address) const override;

private:
  void UeReception (std::string context, RxPacketTraceParams params);
  void TxDataTrace (std::string context, Time t);
  void TxCtrlTrace (std::string context, Time t);
  void GnbReception (std::string context, RxPacketTraceParams params);

private:
  Ptr<NrHelper> m_helper;
  Ptr<NrPointToPointEpcHelper> m_epcHelper;
  std::unordered_map<uint32_t, uint32_t> m_ueGnbMap;
  uint32_t m_ueNum {0};
};

} //namespace ns3
#endif // NR_SINGLE_BWP_SETUP_H
