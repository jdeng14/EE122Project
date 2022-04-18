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
#include <algorithm>

/*
1. Arranges nCsma access points in a grid, all are connected by csma channel
2. There is a gateway router on the csma channel as well
3. gateway has p2p connection with another node that represents the internet. This
node serves the content.
4. all access points have between [1, nWifi) user devices attached.
5. devices have 20% chance of streaming video, 30% chance of voip, 50% chance of light
browsing.
*/

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Evalvid-ESS-Random-Traffic");

int main (int argc, char *argv[]) {
  NS_LOG_UNCOND ("Evalvid-ESS-Random-Traffic");
  LogComponentEnable ("EvalvidClient", LOG_LEVEL_INFO);
  LogComponentEnable ("EvalvidServer", LOG_LEVEL_INFO);

  LogComponentEnable ("UdpTraceClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  const std::string bandwidth("100Kbps");
  
  int nCsma = 2;
  int nWifi = 3;
  NodeContainer csmaNodes;
  csmaNodes.Create (nCsma+1); //nCsma ap's, one gateway

  NodeContainer p2pNodes;
  p2pNodes.Add(csmaNodes.Get(nCsma)); //gateway
  p2pNodes.Create (1); //internet

  PointToPointHelper pointToPoint;
  //intentional bottleneck is here. video needs about 75Kbps. 
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
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
  NetDeviceContainer apDevices; 
  
  for (int i=0;i<nCsma;i++) {
    wifiUserNodes[i].Create (nWifi);//(rand() % nWifi)+1);
    phy.SetChannel (channel.Create ());
    mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
    userDevices[i] = wifi.Install (phy, mac, wifiUserNodes[i]);
    mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
    apDevices.Add(wifi.Install (phy, mac, wifiApNodes.Get(i)));  
  }  

  double tick = 3.0;
  //double lan_width = std::max(1, (int) floor(sqrt(nWifi)))*tick;
  double lan_delta = 100;                               
    
  for (int i=0;i<nCsma;i++) {
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(lan_delta*i),
                                    "MinY", DoubleValue(0),
                                    "DeltaX", DoubleValue(tick),
                                    "DeltaY", DoubleValue(tick),
                                    "GridWidth", UintegerValue (3),//(lan_width+1)/tick),
                                    "LayoutType", StringValue ("RowFirst"));   
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue (Rectangle (lan_delta*i-50, lan_delta*i+50, 
                                0, 50)));                                     
    mobility.Install (wifiUserNodes[i]);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");  
    mobility.Install (wifiApNodes.Get(i));
  }

  InternetStackHelper stack;
  stack.Install(wifiApNodes);
  for (int i=0;i<nCsma;i++) {
    stack.Install(wifiUserNodes[i]);
  }
  
  stack.Install(csmaNodes.Get(nCsma));
  stack.Install(p2pNodes.Get(1));
  
  Ipv4AddressHelper address;
  
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);
  
  Ipv4InterfaceContainer apInterfaces[nCsma];
  Ipv4InterfaceContainer userInterfaces[nCsma];
  for (int i=0;i<nCsma;i++) {
    std::string ip;
    ip = "10.1." + std::to_string(i+2) + ".0";
    address.SetBase (ip.c_str(), "255.255.255.0");
    apInterfaces[i] = address.Assign(apDevices.Get(i));
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
  ApplicationContainer apps = server.Install (p2pNodes.Get(1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (300.0)); 
  
  EvalvidClientHelper client (p2pInterfaces.GetAddress(1),port);
  client.SetAttribute ("ReceiverDumpFilename", StringValue("rd_a01"));
  apps = client.Install (wifiUserNodes[0].Get(0));
  apps.Start (Seconds (3.0));

  port = 2000;
  UdpServerHelper serverUdp (port);
  for (int i=0;i<nCsma;i++) { 
    for (uint32_t j=0;j<wifiUserNodes[i].GetN();j++) {
      if (i==0 && j==0) continue;
      apps = serverUdp.Install (wifiUserNodes[i].Get (j));
      apps.Start(Seconds(0.0));
      apps.Stop(Seconds(300.0));
    }
  }

  uint32_t MaxPacketSize = 1024;
  for (int i=0;i<nCsma;i++) { 
    for (uint32_t j=0;j<wifiUserNodes[i].GetN();j++) {
      if (i==0 && j==0) continue;  
      UdpTraceClientHelper clientUdp(userInterfaces[i].GetAddress(j), port, "");
      clientUdp.SetAttribute ("MaxPacketSize", UintegerValue (MaxPacketSize));
      apps = clientUdp.Install (p2pNodes.Get(1));
      apps.Start(Seconds(5.0));
      apps.Stop(Seconds(300.0));
    }
  }
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  Simulator::Stop (Seconds (300.0));

  NS_LOG_UNCOND ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_UNCOND ("Done.");
  return 0;
}
