#include "ns3/core-module.h"
/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/evalvid-client-server-helper.h"
#include <string.h>

// Default Network Topology
//
//   Wifi 10.1.3.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0                  (n4 isServer)
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//(n5 is Client)
//                   point-to-point  |    |    |    |
//                                   ================
//                                     LAN 10.1.2.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Evalvid-ESS");

int main (int argc, char *argv[]) {
  NS_LOG_UNCOND ("Evalvid-ESS");
  LogComponentEnable ("EvalvidClient", LOG_LEVEL_INFO);
  LogComponentEnable ("EvalvidServer", LOG_LEVEL_INFO);
  
  int nCsma = 4;
  int nWifi = 10;

  NodeContainer csmaNodes;
  csmaNodes.Create (nCsma+1); //nCsma ap's, one gateway

  NodeContainer p2pNodes;
  p2pNodes.Add(csmaNodes.Get(nCsma));
  p2pNodes.Create (1);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate(5000000)));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1400));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  NodeContainer wifiUserNodes[nCsma];
  NodeContainer wifiApNodes;

  auto itr = csmaNodes.Begin();
  while (itr != csmaNodes.End()-1) {
    wifiApNodes.Add(*itr);
    itr++;
  }

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac; 
  Ssid ssid = Ssid ("ns-3-ssid"); 
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  
  NetDeviceContainer userDevices[nCsma]; //nWifi per entry
  NetDeviceContainer apDevices[nCsma]; //1 per entry
  for (int i=0;i<nCsma;i++) {
    
    wifiUserNodes[i].Create (nWifi);
    phy.SetChannel (channel.Create ());
    mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
    userDevices[i] = wifi.Install (phy, mac, wifiUserNodes[i]);
    mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
    apDevices[i] = wifi.Install (phy, mac, wifiApNodes.Get(i));  
  }  

  MobilityHelper mobility;

  double delta = 5.0;
  int row_length = 2;
  for (int i=0;i<nCsma;i++) { 
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue((i%row_length)*delta),
                                    "MinY", DoubleValue(floor(i/row_length)*delta),
                                    "DeltaX", DoubleValue(((i%row_length)+1)*delta),
                                    "DeltaY", DoubleValue((floor(i/row_length)+1)*delta),
                                    "GridWidth", UintegerValue (3),
                                    "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));                                           
    mobility.Install (wifiUserNodes[i]);
    
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNodes.Get(i));
  }

  InternetStackHelper stack;
  stack.Install(wifiApNodes);
  for (int i=0;i<nCsma;i++) {
    //stack.Install(wifiApNodes.Get(i)); //Does this affect csmaNodes?
    stack.Install(wifiUserNodes[i]);
  }
  stack.Install(csmaNodes.Get(nCsma));
  stack.Install(p2pNodes.Get(1));
  
  Ipv4AddressHelper address;

  
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);
  //Ipv4InterfaceContainer apInterface = address.Assign(apDevices[0]);
  
  Ipv4InterfaceContainer apInterfaces[nCsma];
  Ipv4InterfaceContainer userInterfaces[nCsma];
  for (int i=0;i<nCsma;i++) {
    std::string ip;
    ip = "10.1." + std::to_string(i+2) + ".0";
    address.SetBase (ip.c_str(), "255.255.255.0");
    apInterfaces[i] = address.Assign(apDevices[i]);
    userInterfaces[i] = address.Assign(userDevices[i]);
  }

  address.SetBase ("10.2.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);
  
  uint16_t port = 4000;
  EvalvidServerHelper server (port);
  server.SetAttribute ("SenderTraceFilename", StringValue("st_highway_cif.st"));
  server.SetAttribute ("SenderDumpFilename", StringValue("sd_a01"));
  server.SetAttribute ("PacketPayload",UintegerValue(1014));
  ApplicationContainer apps = server.Install (p2pNodes.Get(1));//(csmaNodes.Get (nCsma));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (1000.0)); 
  
  EvalvidClientHelper client (p2pInterfaces.GetAddress(1),port);
  client.SetAttribute ("ReceiverDumpFilename", StringValue("rd_a01"));
  apps = client.Install (wifiUserNodes[2].Get(2));
  apps.Start (Seconds (5.0));
  apps.Stop (Seconds (1000.0));
  

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  Simulator::Stop (Seconds (1000.0));


  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
  return 0;
}
