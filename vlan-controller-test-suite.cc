/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Network topology is the same as "src/openflow/examples/openflow-switch.cc"
 */

#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/openflow-module.h"
#include "ns3/log.h"

#include "openflow-vlan-controller.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VlanControllerTest");

bool verbose = false;
bool vlan = false;
ns3::Time timeout = ns3::Seconds (0);

bool
SetVerbose (std::string value)
{
	verbose = true;
	return true;
}

bool
SetVlan (std::string value)
{
	vlan = true;
	return true;
}

bool
SetTimeout (std::string value)
{
	try {
		timeout = ns3::Seconds (atof (value.c_str ()));
		return true;
	}
	catch (...) { return false; }
	return false;
}

int
main (int argc, char *argv[])
{	
	CommandLine cmd;
	cmd.AddValue ("verbose", "Verbose (turns on logging).", MakeCallback (&SetVerbose));
	cmd.AddValue ("vlan", "Enable VLAN Mode", MakeCallback (&SetVlan));
	cmd.AddValue ("timeout", "Learning Controller Timeout", MakeCallback (&SetTimeout));

	cmd.Parse (argc, argv);

	if (verbose)
	{
		LogComponentEnable ("VlanControllerTest", LOG_LEVEL_INFO);
		LogComponentEnable ("OpenFlowInterface", LOG_LEVEL_INFO);
		LogComponentEnable ("OpenFlowSwitchNetDevice", LOG_LEVEL_INFO);
	}

	NS_LOG_INFO ("Create nodes.");
	NodeContainer terminals;
	terminals.Create (4);

	NodeContainer csmaSwitch;
	csmaSwitch.Create (1);

	NS_LOG_INFO ("Build Topology.");
	CsmaHelper csma;
	csma.SetChannelAttribute ("DataRate", DataRateValue (5000000));
	csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

	// Create the csma links, from each terminal to the switch.
	NetDeviceContainer terminalDevices;
	NetDeviceContainer switchDevices;
	for (int i = 0; i < 4; i++)
	{
		NetDeviceContainer link = csma.Install (NodeContainer (terminals.Get (i), csmaSwitch));
		terminalDevices.Add (link.Get (0));
		switchDevices.Add (link.Get (1));
	}

	// Create the switch netdevice, which will do the packet switching
	Ptr<Node> switchNode = csmaSwitch.Get (0);
	OpenFlowSwitchHelper swtch;

	if (vlan)
	{
		Ptr<VlanController> controller = CreateObject<VlanController> ();
		if (!timeout.IsZero ())
		{
			controller->SetAttribute ("ExpirationTime", TimeValue (timeout));
		}
		swtch.Install (switchNode, switchDevices, controller);
	}

	// Set VLAN ID
	#if 0	
	ns3::ofi::VlanController::SetVlanId (swtch, 0, 1);
	ns3::ofi::VlanController::SetVlanId (swtch, 1, 1);
	ns3::ofi::VlanController::SetVlanId (swtch, 2, 2);
	ns3::ofi::VlanController::SetVlanId (swtch, 3, 2);
	#endif

	// Add internet stack to the terminals
	InternetStackHelper internet;
	internet.Install (terminals);

	// We have got the "hardware" in place. Now we need to add IP addresses.
	NS_LOG_INFO ("Assign IP Addresses.");
	Ipv4AddressHelper ipv4;
	ipv4.SetBase ("10.1.1.0", "255.255.255.0");
	ipv4.Assign (terminalDevices);

	// Create an On-Off application to send UDP datagrams from n0 to n1.
	NS_LOG_INFO ("Create Applications.");
	uint16_t port = 9; // Discard port

	OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address ("10.1.1.2"), port)));
	onoff.SetConstantRate (DataRate ("500kb/s"));

	ApplicationContainer app = onoff.Install (terminals.Get (0));

	// Start the application
	app.Start (Seconds (1.0));
	app.Stop (Seconds (10.0));

	// Create ana optional packet sink to receive these packets
	PacketSinkHelper  sink ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address::GetAny(), port)));
	app = sink.Install (terminals.Get (1));
	app.Start (Seconds (0.0));

	//
	// Create a similar flow from n3 to n2, starting at time 1.1 seconds
	//
	onoff.SetAttribute ("Remote", AddressValue (InetSocketAddress (Ipv4Address  ("10.1.1.1"), port)));
	app = onoff.Install (terminals.Get (3));
	app.Start (Seconds (1.1));
	app.Stop (Seconds (10.0));

	app = sink.Install (terminals.Get (2));
	app.Start (Seconds (0.0));

	NS_LOG_INFO ("Configure Tracing.");

	//
	// Configure tracing of all enqueue, dequeue, and NetDevice receive events.
	// Trace output will be sent to the file "openflow-switch.tr"
	//
	AsciiTraceHelper ascii;
	csma.EnableAsciiAll (ascii.CreateFileStream ("openflow-vlan-switch.tr"));

	//
	// Also configure some tcpdump traces; each interface will be traced.
	// The output files will be named: openflow-vlan-switch-<nodeId>-<interfaceId>.pcap
	// and can be read by the "tcpdump -r" command (use "-tt" option to display timestamps correctly)
	//
	csma.EnablePcapAll ("openflow-vlan-switch", false);

	//
	// Now, do the actual simulation.
	//
	NS_LOG_INFO ("Run Simulation.");
	Simulator::Run ();
	Simulator::Destroy ();
	NS_LOG_INFO ("Done.");
//	#else
//	NS_LOG_INFO ("NS-3 OpenFlow is not enabled. Cannnot run simulation.");
//	#endif // NS3_OPENFLOW_VLAN_EXAMPLE
}
