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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ScratchSimulator");

int 
main (int argc, char *argv[])
{
  NS_LOG_UNCOND ("Scratch Simulator");
  LogComponentEnable ("EvalvidClient", LOG_LEVEL_INFO);
  LogComponentEnable ("EvalvidServer", LOG_LEVEL_INFO);
  //LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  //LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  //bool verbose = true;
  uint32_t nCsma = 3;
  uint32_t nWifi = 3;
  //bool tracing = false;

  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  NodeContainer csmaNodes;
  csmaNodes.Add (p2pNodes.Get (1));
  csmaNodes.Create (nCsma);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate(5000000)));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1400));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  NodeContainer wifiApNode = p2pNodes.Get (0);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  //channel.SetPropagationDelay(std::string("Delay"), std::string("1ms"));
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  //mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
  //                           "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));                                           
  //mobility.Install (wifiStaNodes);
  
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNodes);

  InternetStackHelper stack;
  stack.Install (csmaNodes);
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);

  Ipv4InterfaceContainer apInterfaces;
  address.SetBase ("10.1.3.0", "255.255.255.0");
  address.Assign (staDevices);
  apInterfaces = address.Assign (apDevices);
  /*
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (csmaNodes.Get (nCsma));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (csmaInterfaces.GetAddress (nCsma), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = 
    echoClient.Install (wifiStaNodes.Get (nWifi - 1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));
  */
  
  uint16_t port = 4000;
  EvalvidServerHelper server (port);
  server.SetAttribute ("SenderTraceFilename", StringValue("st_highway_cif.st"));
  server.SetAttribute ("SenderDumpFilename", StringValue("sd_a01"));
  server.SetAttribute ("PacketPayload",UintegerValue(1014));
  ApplicationContainer apps = server.Install (wifiApNode.Get(0));//(csmaNodes.Get (nCsma));
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (1000.0));
  
  EvalvidClientHelper client (apInterfaces.GetAddress(0),port);
  client.SetAttribute ("ReceiverDumpFilename", StringValue("rd_a01"));
  apps = client.Install (wifiStaNodes.Get (0));
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (1000.0));
  

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  Simulator::Stop (Seconds (1000.0));
  
  /*
  NodeContainer nodes;
  nodes.Create(2);

  NodeContainer nodes2;
  nodes2.Create(1);
  nodes2.Add(nodes.Get(1));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = p2p.Install(nodes); //devices vs nodes?

  NetDeviceContainer devices2;
  devices2 = p2p.Install(nodes2);

  //PointToPointHelper p2p;
  //p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  //p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  //NetDeviceContainer devices;
  //devices = p2p.Install(NodeContainer(nodes.Get(1), node3)); //devices vs nodes?

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 4000;
  EvalvidServerHelper server (port);
  server.SetAttribute ("SenderTraceFilename", StringValue("st_highway_cif.st"));
  server.SetAttribute ("SenderDumpFilename", StringValue("sd_a01"));
  server.SetAttribute ("PacketPayload",UintegerValue(1014));
  ApplicationContainer apps = server.Install (nodes.Get(0));
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (101.0));
  
  EvalvidClientHelper client (interfaces.GetAddress (0),port);
  client.SetAttribute ("ReceiverDumpFilename", StringValue("rd_a01"));
  apps = client.Install (nodes.Get (1));
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (100.0));
  */
  //EvalvidClientHelper client2 (interfaces.GetAddress (0),port);
  //client.SetAttribute ("ReceiverDumpFilename", StringValue("rd_a02"));
  //apps = client.Install (nodes.Get (2));
  //apps.Start (Seconds (0.0));
  //apps.Stop (Seconds (100.0));


  NS_LOG_INFO ("Run Sim ulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
  return 0;
}
