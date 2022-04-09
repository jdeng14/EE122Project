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
#include "nr-single-bwp-setup.h"
#include <ns3/nr-module.h>
#include <ns3/names.h>
#include <ns3/object-map.h>
#include <ns3/internet-stack-helper.h>
#include <ns3/ipv4-static-routing-helper.h>
#include <ns3/point-to-point-helper.h>
#include <ns3/nr-lbt-access-manager.h>
#include <ns3/three-gpp-propagation-loss-model.h>
#include <ns3/three-gpp-spectrum-propagation-loss-model.h>

namespace ns3 {

static void
AttachToClosestGnb (Ptr<NrHelper> helper, Ptr<NetDevice> ueDevice, NetDeviceContainer gnbDevices)
{
  NS_ASSERT_MSG (gnbDevices.GetN () > 0, "empty gNB device container");
  Vector uepos = ueDevice->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
  double minDistance = std::numeric_limits<double>::infinity ();
  Ptr<NetDevice> closestGnbDevice;
  for (NetDeviceContainer::Iterator i = gnbDevices.Begin (); i != gnbDevices.End (); ++i)
    {
      Vector gnbpos = (*i)->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
      double distance = CalculateDistance (uepos, gnbpos);
      if (distance < minDistance)
        {
          minDistance = distance;
          closestGnbDevice = *i;
        }
    }
  NS_ASSERT (closestGnbDevice != nullptr);

  std::cout << "Attaching " << Names::FindName (ueDevice->GetNode ()) << " to " <<
               Names::FindName (closestGnbDevice->GetNode ()) << std::endl;

  helper->AttachToEnb (ueDevice, closestGnbDevice);
}

static void
AttachToClosestGnb (Ptr<NrHelper> helper, NetDeviceContainer ueDevices, NetDeviceContainer gnbDevices)
{
  for (NetDeviceContainer::Iterator i = ueDevices.Begin (); i != ueDevices.End (); i++)
    {
      AttachToClosestGnb (helper, *i, gnbDevices);
    }
}

NrSingleBwpSetup::NrSingleBwpSetup (const NodeContainer &gnbNodes, const NodeContainer &ueNodes,
                                    const Ptr<SpectrumChannel> &channel,
                                    const Ptr<ThreeGppPropagationLossModel> &propagation,
                                    const Ptr<ThreeGppSpectrumPropagationLossModel> &spectrumPropagation,
                                    double freq, double bw, uint32_t num, double gnbTxPower, double ueTxPower,
                                    const std::unordered_map<uint32_t, uint32_t> &ueGnbMap,
                                    const std::string &gnbCamType, const std::string &ueCamType,
                                    const std::string &scheduler, const BandwidthPartInfo::Scenario &scenario)
  : L2Setup (gnbNodes, ueNodes),
    m_ueGnbMap (ueGnbMap)
{
  std::cout << "Configuring NR with " << gnbNodes.GetN ()
            << " gNBs, and " << ueNodes.GetN () << " UEs" << std::endl;

  for (auto it = gnbNodes.Begin(); it != gnbNodes.End(); ++it)
    {
      std::stringstream ss;
      ss << "GNB-NR-" << (*it)->GetId();
      Names::Add(ss.str (), *it);
    }

  for (auto it = ueNodes.Begin(); it != ueNodes.End(); ++it)
    {
      std::stringstream ss;
      ss << "UE-NR-" << (*it)->GetId();
      Names::Add(ss.str (), *it);
    }

  // setup the NR simulation
  m_helper = CreateObject<NrHelper> ();

  // Configure beamforming method
  Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper> ();
  m_helper->SetBeamformingHelper (idealBeamformingHelper);

  // Configure scheduler
  m_helper->SetSchedulerTypeId (TypeId::LookupByName(scheduler));

  // Configure BWPs
  BandwidthPartInfoPtrVector allBwps;
  CcBwpCreator ccBwpCreator;
  const uint8_t numCcPerBand = 1;  // each band is composed of a single component carrier
  CcBwpCreator::SimpleOperationBandConf bandConf (freq, bw, numCcPerBand, scenario);
  OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc (bandConf);

  // Use provided channel, propagation, and fading models (instead of initializing them in InitializeOperationBand)
  m_helper->InitializeOperationBand (&band, 0x00); //flag to avoid initializing them
  NS_ASSERT (band.m_cc.size () == 1 && band.m_cc[0]->m_bwp.size ());
  const auto & bwp = band.m_cc[0]->m_bwp[0];
  bwp->m_channel = channel;
  bwp->m_propagation = propagation;
  bwp->m_3gppChannel = spectrumPropagation;

  allBwps = CcBwpCreator::GetAllBwps ({band});

  // Configure CAMs
  m_helper->SetGnbChannelAccessManagerTypeId (TypeId::LookupByName(gnbCamType));
  m_helper->SetUeChannelAccessManagerTypeId (TypeId::LookupByName(ueCamType));

  m_epcHelper = CreateObject<NrPointToPointEpcHelper> ();
  m_helper->SetEpcHelper (m_epcHelper);
  m_helper->Initialize ();

  // install NR net devices
  m_gnbDev = m_helper->InstallGnbDevice (GetGnbNodes (), allBwps);
  m_ueDev = m_helper->InstallUeDevice (GetUeNodes (), allBwps);

  double gnbX = pow (10, gnbTxPower / 10);
  double ueX = pow (10, ueTxPower / 10);

  for (auto it = m_gnbDev.Begin (); it != m_gnbDev.End (); ++it)
    {
      Ptr<NrGnbPhy> phy = m_helper->GetGnbPhy (*it, 0);
      phy->SetNumerology (num);
      phy->SetTxPower (10 * log10 (gnbX));

      Ptr<NrSpectrumPhy> gnbSpectrumPhy = phy->GetSpectrumPhy ();
      std::stringstream nodeId;
      nodeId << (*it)->GetNode ()->GetId ();
      gnbSpectrumPhy->TraceConnect ("RxPacketTraceEnb", nodeId.str (),
                                    MakeCallback (&NrSingleBwpSetup::GnbReception, this));
      gnbSpectrumPhy->TraceConnect ("TxDataTrace", nodeId.str (),
                                    MakeCallback (&NrSingleBwpSetup::TxDataTrace, this));
      gnbSpectrumPhy->TraceConnect ("TxCtrlTrace", nodeId.str (),
                                    MakeCallback (&NrSingleBwpSetup::TxCtrlTrace, this));
    }

  m_ueNum = m_ueDev.GetN ();

  for (auto it = m_ueDev.Begin (); it != m_ueDev.End (); ++it)
    {
      Ptr<NrUePhy> phy = m_helper->GetUePhy (*it, 0);
      phy->SetNumerology (num);
      phy->SetTxPower (10 * log10 (ueX));

      Ptr<NrSpectrumPhy> ueSpectrumPhy = phy->GetSpectrumPhy ();
      std::stringstream nodeId;
      nodeId << (*it)->GetNode ()->GetId ();

      ueSpectrumPhy->TraceConnect ("RxPacketTraceUe", nodeId.str (),
                                   MakeCallback (&NrSingleBwpSetup::UeReception, this));
      ueSpectrumPhy->TraceConnect ("TxDataTrace", nodeId.str (),
                                   MakeCallback (&NrSingleBwpSetup::TxDataTrace, this));
      ueSpectrumPhy->TraceConnect ("TxCtrlTrace", nodeId.str (),
                                   MakeCallback (&NrSingleBwpSetup::TxCtrlTrace, this));
    }

  // When all the configuration is done, explicitly call UpdateConfig ()
  for (auto it = m_gnbDev.Begin (); it != m_gnbDev.End (); ++it)
    {
      DynamicCast<NrGnbNetDevice> (*it)->UpdateConfig ();
    }

  for (auto it = m_ueDev.Begin (); it != m_ueDev.End (); ++it)
    {
      DynamicCast<NrUeNetDevice> (*it)->UpdateConfig ();
    }
}

NrSingleBwpSetup::~NrSingleBwpSetup ()
{

}

void
NrSingleBwpSetup::ConnectToRemotes (const NodeContainer &remoteHosts, const std::string &base)
{
  // create the internet and install the IP stack on the UEs
  // get SGW/PGW and create a single RemoteHost
  Ptr<Node> pgw = m_epcHelper->GetPgwNode ();
  // connect a remoteHost to pgw. Setup routing too
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (2500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.000)));

  std::cout << "Connecting remote hosts to EPC with a 100 Gb/s link, MTU 2500 B, delay 0 s" << std::endl;

  NetDeviceContainer internetDevices;
  for (auto it = remoteHosts.Begin (); it != remoteHosts.End (); ++it)
    {
      internetDevices.Add (p2ph.Install (pgw, *it));
    }

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase (base.c_str (), "255.255.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
}

Ipv4InterfaceContainer
NrSingleBwpSetup::AssignIpv4ToUe (const std::unique_ptr<Ipv4AddressHelper> &address) const
{
  NS_UNUSED (address);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  InternetStackHelper internet;
  internet.Install (GetUeNodes ());
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = m_epcHelper->AssignUeIpv4Address (GetUeDev ());

  // Set the default gateway for the UEs
  for (uint32_t j = 0; j < GetUeNodes ().GetN (); ++j)
    {
      auto ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (GetUeNodes ().Get (j)->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (m_epcHelper->GetUeDefaultGatewayAddress (), 1);
    }


  // if no map is provided, then attach UEs to the closest gNB
  if (m_ueGnbMap.empty ())
    {
      std::cout << "Strategy for UEs attachment: to the closest gNB. " << std::endl;
      AttachToClosestGnb (m_helper, GetUeDev (), GetGnbDev ());
    }
  else
    {
      std::cout << "Strategy for UEs attachment: manual mapping. " << std::endl;

      for (const auto & v : m_ueGnbMap)
        {
          if (v.first < GetUeDev ().GetN () && v.second < GetGnbDev ().GetN ())
            {
              std::cout << " attaching " << v.first << " to " << v.second << std::endl;
              m_helper->AttachToEnb (GetUeDev ().Get (v.first), GetGnbDev ().Get (v.second));
            }
        }
    }

  return ueIpIface;
}

Ipv4InterfaceContainer
NrSingleBwpSetup::AssignIpv4ToStations (const std::unique_ptr<Ipv4AddressHelper> &address) const
{
  NS_UNUSED (address);
  return Ipv4InterfaceContainer ();
}

void
NrSingleBwpSetup::TxDataTrace (std::string context, Time t)
{
  if (!m_channelOccupancyTimeCb.IsNull ())
    {
      m_channelOccupancyTimeCb (static_cast<uint32_t> (std::stoul (context)), t);
    }
}

void
NrSingleBwpSetup::TxCtrlTrace (std::string context, Time t)
{
  if (!m_channelOccupancyTimeCb.IsNull ())
    {
      m_channelOccupancyTimeCb (static_cast<uint32_t> (std::stoul (context)), t);
    }
}

void
NrSingleBwpSetup::GnbReception (std::string context, RxPacketTraceParams params)
{

  if (!m_sinrCb.IsNull ())
    {
      m_sinrCb (static_cast<uint32_t> (std::stoul (context)), params.m_sinr);
    }
  if (!m_macTxDataFailedCb.IsNull ())
    {
      if (params.m_corrupt)
        {
          m_macTxDataFailedCb (static_cast<uint32_t> (std::stoul (context)), params.m_tbSize);
        }
    }
}

void
NrSingleBwpSetup::UeReception (std::string context, RxPacketTraceParams params)
{
  if (!m_sinrCb.IsNull ())
    {
      m_sinrCb (static_cast<uint32_t> (std::stoul (context)), params.m_sinr);
    }

  if (!m_macTxDataFailedCb.IsNull ())
    {
      if (params.m_corrupt)
        {
          m_macTxDataFailedCb (static_cast<uint32_t> (std::stoul (context)), params.m_tbSize);
        }
    }
}

} // namespace ns3
