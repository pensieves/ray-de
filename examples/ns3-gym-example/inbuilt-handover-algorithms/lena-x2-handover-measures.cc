/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Author: Manuel Requena <manuel.requena@cttc.es>
 */

#include <vector>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LenaX2HandoverMeasures");

void
NotifyConnectionEstablishedUe (std::string context,
                               uint64_t imsi,
                               uint16_t cellid,
                               uint16_t rnti)
{
  std::cout << context
            << " at " << Simulator::Now ().GetSeconds () << " (sec)"
            << " UE IMSI " << imsi
            << ": connected to CellId " << cellid
            << " with RNTI " << rnti
            << std::endl;
}

void
NotifyHandoverStartUe (std::string context,
                       uint64_t imsi,
                       uint16_t cellid,
                       uint16_t rnti,
                       uint16_t targetCellId)
{
  std::cout << context
            << " at " << Simulator::Now ().GetSeconds () << " (sec)"
            << " UE IMSI " << imsi
            << ": previously connected to CellId " << cellid
            << " with RNTI " << rnti
            << ", doing handover to CellId " << targetCellId
            << std::endl;
}

void
NotifyHandoverEndOkUe (std::string context,
                       uint64_t imsi,
                       uint16_t cellid,
                       uint16_t rnti)
{
  std::cout << context
            << " at " << Simulator::Now ().GetSeconds () << " (sec)"
            << " UE IMSI " << imsi
            << ": successful handover to CellId " << cellid
            << " with RNTI " << rnti
            << std::endl;
}

void
NotifyConnectionEstablishedEnb (std::string context,
                                uint64_t imsi,
                                uint16_t cellid,
                                uint16_t rnti)
{
  std::cout << context
            << " at " << Simulator::Now ().GetSeconds () << " (sec)"
            << " eNB CellId " << cellid
            << ": successful connection of UE with IMSI " << imsi
            << " RNTI " << rnti
            << std::endl;
}

void
NotifyHandoverStartEnb (std::string context,
                        uint64_t imsi,
                        uint16_t cellid,
                        uint16_t rnti,
                        uint16_t targetCellId)
{
  std::cout << context
            << " at " << Simulator::Now ().GetSeconds () << " (sec)"
            << " eNB CellId " << cellid
            << ": start handover of UE with IMSI " << imsi
            << " RNTI " << rnti
            << " to CellId " << targetCellId
            << std::endl;
}

void
NotifyHandoverEndOkEnb (std::string context,
                        uint64_t imsi,
                        uint16_t cellid,
                        uint16_t rnti)
{
  std::cout << context
            << " at " << Simulator::Now ().GetSeconds () << " (sec)"
            << " eNB CellId " << cellid
            << ": completed handover of UE with IMSI " << imsi
            << " RNTI " << rnti
            << std::endl;
}

// Global measurement and id containers
uint32_t numberOfUes = 2; // can be changed in main()
uint32_t numberOfEnbs = 2; // can be changed in main()

// for each UE - simulation time at which measurement was collected
std::vector<double> measTime;
// for each UE - first RSRP of all Enbs, then RSRQ of all Enbs
// and then current/serving cell ID
std::vector<std::vector<uint32_t>> measAsState;

void printStateMatrix (){
  for (std::vector<uint32_t> v: measAsState){
    for (uint32_t e: v){
      std::cout << e << " ";
    }
    std::cout << std::endl;
  }
}

void
printCurrentStateAndTime(){
  std::cout << "Time:" << std::endl;
  for (double t: measTime){
    std::cout << t << " ";
  }
  std::cout << std::endl;

  std::cout << "RSRP-RSRQ-ServingCellId State:" << std::endl;
  printStateMatrix ();
}

void
ReceiveMeasurementReport (std::string context,
                          uint64_t imsi,
                          uint16_t cellid,
                          uint16_t rnti,
                          LteRrcSap::MeasurementReport measReport)
{
    uint16_t measId = (uint16_t) measReport.measResults.measId;
    uint32_t curCellRSRP = (uint32_t) measReport.measResults.rsrpResult;
    uint32_t curCellRSRQ = (uint32_t) measReport.measResults.rsrqResult;
    double curSimTime = Simulator::Now ().GetSeconds ();

    std::cout << context
              << " at time " << curSimTime << " (sec)"
              << " eNB CellId " << cellid
              << " UE with IMSI " << imsi // International Mobile Subscriber Identity
              << " RNTI " << rnti // Radio Network Temporary Identifier
              << " measId " << measId
              << " RSRP " << curCellRSRP // Reference Signal Received Power -140 dbm (Bad) to -44 dbm (Good)
              << " RSRQ " << curCellRSRQ // Reference Signal Received Quality -20 dB (Bad) to -3 dB (Good)
              << std::endl;

    imsi -= 1;
    cellid -= 1;

    measTime[imsi] = curSimTime;
    measAsState[imsi][cellid] = curCellRSRP; // first RSRPs
    measAsState[imsi][numberOfEnbs+cellid] = curCellRSRQ; // then RSRQs
    measAsState[imsi][2*numberOfEnbs] = cellid;

    // iterate through measurements for neighboring cells and update measurement container
    for (std::list <LteRrcSap::MeasResultEutra>::iterator it = measReport.measResults.measResultListEutra.begin ();
        it != measReport.measResults.measResultListEutra.end ();
        ++it)
    {
      measAsState[imsi][it->physCellId-1] = (it->haveRsrpResult ? (uint32_t) it->rsrpResult : 0); // RSRP values between 0 and 97
      measAsState[imsi][numberOfEnbs+it->physCellId-1] = (it->haveRsrqResult ? (uint32_t) it->rsrqResult : 0);
    }

    printCurrentStateAndTime();
}


/**
 * Sample simulation script for an automatic X2-based handover based on the RSRQ measures.
 * It instantiates two eNodeB, attaches one UE to the 'source' eNB.
 * The UE moves between both eNBs, it reports measures to the serving eNB and
 * the 'source' (serving) eNB triggers the handover of the UE towards
 * the 'target' eNB when it considers it is a better eNB.
 */
int
main (int argc, char *argv[])
{

  double distance = 500.0; // m
  double yForUe = 500.0;   // m
  double speed = 20;       // m/s
  double simTime = (double)(numberOfEnbs + 1) * distance / speed; // 1500 m / 20 m/s = 75 secs
  double enbTxPowerDbm = 46.0;
  std::string handoverAlgo = "A3-rsrp";

  // change some default attributes so that they are reasonable for
  // this scenario, but do this before processing command line
  // arguments, so that the user is allowed to override these settings
  // Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (MilliSeconds (10)));
  // Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (1000000));
  Config::SetDefault ("ns3::LteHelper::UseIdealRrc", BooleanValue (true));

  // Command line arguments
  CommandLine cmd (__FILE__);
  cmd.AddValue ("simTime", "Total duration of the simulation (in seconds)", simTime);
  cmd.AddValue ("speed", "Speed of the UE (default = 20 m/s)", speed);
  cmd.AddValue ("enbTxPowerDbm", "TX power [dBm] used by HeNBs (default = 46.0)", enbTxPowerDbm);
  cmd.AddValue ("handoverAlgo", "Handover algorithm to be used (default = A3-rsrp)", handoverAlgo);

  cmd.Parse (argc, argv);

  // Instantiate measurement containers
  for (uint32_t i = 0; i < numberOfUes; ++i){
    // Initialize simulation time record with 0 for each UE
    measTime.push_back(0);
    // Initialize a vector of length numEnbs for both RSRP and RSRQ, and a single current/serving cellId
    measAsState.push_back(std::vector<uint32_t>(2*numberOfEnbs+1,0));
  }

  // Create LTE helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);
  // lteHelper->SetSchedulerType ("ns3::RrFfMacScheduler");

  if (handoverAlgo == "A3-rsrp") {
    lteHelper->SetHandoverAlgorithmType ("ns3::A3RsrpHandoverAlgorithm");
    lteHelper->SetHandoverAlgorithmAttribute ("Hysteresis",
                                              DoubleValue (3.0));
    lteHelper->SetHandoverAlgorithmAttribute ("TimeToTrigger",
                                              TimeValue (MilliSeconds (256)));
  }
  else{
    lteHelper->SetHandoverAlgorithmType ("ns3::A2A4RsrqHandoverAlgorithm");
    lteHelper->SetHandoverAlgorithmAttribute ("ServingCellThreshold",
                                              UintegerValue (30));
    lteHelper->SetHandoverAlgorithmAttribute ("NeighbourCellOffset",
                                              UintegerValue (1));
  }

  /*
   * Network topology:
   *
   *      |     + --------------------------------------------------------->
   *      |     UE
   *      |
   *      |               d                   d                   d
   *    y |     |-------------------x-------------------x-------------------
   *      |     |                 eNodeB              eNodeB
   *      |   d |
   *      |     |
   *      |     |                                             d = distance
   *            o (0, 0, 0)                                   y = yForUe
   */

  NodeContainer ueNodes;
  NodeContainer enbNodes;
  enbNodes.Create (numberOfEnbs);
  ueNodes.Create (numberOfUes);

  // Install Mobility Model in eNB
  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint16_t i = 0; i < numberOfEnbs; i++)
    {
      Vector enbPosition (distance * (i + 1), distance, 0);
      enbPositionAlloc->Add (enbPosition);
    }
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator (enbPositionAlloc);
  enbMobility.Install (enbNodes);

  // Install Mobility Model in UE
  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  ueMobility.Install (ueNodes);
  ueNodes.Get (0)->GetObject<MobilityModel> ()->SetPosition (Vector (0, distance + yForUe, 0));
  ueNodes.Get (0)->GetObject<ConstantVelocityMobilityModel> ()->SetVelocity (Vector (speed, 0, 0));
  ueNodes.Get (1)->GetObject<MobilityModel> ()->SetPosition (Vector (3*distance, distance + yForUe, 0));
  ueNodes.Get (1)->GetObject<ConstantVelocityMobilityModel> ()->SetVelocity (Vector (-speed, 0, 0));

  // Install LTE Devices in eNB and UEs
  Config::SetDefault ("ns3::LteEnbPhy::TxPower", DoubleValue (enbTxPowerDbm));
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  InternetStackHelper internet;
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIfaces;
  ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

  // Attach UEs to the eNodeBs
  lteHelper->Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));
  lteHelper->Attach (ueLteDevs.Get (1), enbLteDevs.Get (1));


  NS_LOG_LOGIC ("setting up applications");

  // Add X2 interface
  lteHelper->AddX2Interface (enbNodes);

  lteHelper->EnablePhyTraces ();
  lteHelper->EnableMacTraces ();
  lteHelper->EnableRlcTraces ();
  lteHelper->EnablePdcpTraces ();
  Ptr<RadioBearerStatsCalculator> rlcStats = lteHelper->GetRlcStats ();
  rlcStats->SetAttribute ("EpochDuration", TimeValue (Seconds (1.0)));
  Ptr<RadioBearerStatsCalculator> pdcpStats = lteHelper->GetPdcpStats ();
  pdcpStats->SetAttribute ("EpochDuration", TimeValue (Seconds (1.0)));

  // connect custom trace sinks for RRC connection establishment and handover notification
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/ConnectionEstablished",
                   MakeCallback (&NotifyConnectionEstablishedEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/ConnectionEstablished",
                   MakeCallback (&NotifyConnectionEstablishedUe));
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverStart",
                   MakeCallback (&NotifyHandoverStartEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/HandoverStart",
                   MakeCallback (&NotifyHandoverStartUe));
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverEndOk",
                   MakeCallback (&NotifyHandoverEndOkEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/HandoverEndOk",
                   MakeCallback (&NotifyHandoverEndOkUe));
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/RecvMeasurementReport",
                   MakeCallback (&ReceiveMeasurementReport));


  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;

}
