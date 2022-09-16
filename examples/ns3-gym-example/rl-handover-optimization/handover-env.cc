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
#include <algorithm>
#include "ns3/opengym-module.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OpenGymHandoverOptimization");

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

NodeContainer ueNodes;
NodeContainer enbNodes;

Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
NetDeviceContainer ueLteDevs;
NetDeviceContainer enbLteDevs;

// for each UE - simulation time at which measurement was collected
std::vector<double> measTime;
// for each UE - first RSRP of all Enbs, then RSRQ of all Enbs and then one-hot 
// encoding of current/serving cell ID
std::vector<std::vector<uint32_t>> measAsState;
// current RNTI of each UE
std::vector<uint16_t> measRnti;
// Capture occurrence of handover for reward calculation
std::vector<bool> handoverOccurred;
// Handover cost in reward formulation
double handoverCost = 1;

// Convert 2D matrix to 1D vector as opengym currently supports only 1D array as states
std::vector<uint32_t>
toVector (std::vector<std::vector<uint32_t>> matrix){
  std::vector<uint32_t> vector;
  for (std::vector<uint32_t> v: matrix){
    for (uint32_t e: v){
      vector.push_back(e);
    }
  }
  return vector;
}

void printStateMatrix (){
  for (std::vector<uint32_t> v: measAsState){
    for (uint32_t e: v){
      std::cout << e << " ";
    }
    std::cout << std::endl;
  }
}

void
printCurrentStateRntiAndTime (){
  std::cout << "Time:" << std::endl;
  for (double t: measTime){
    std::cout << t << " ";
  }
  std::cout << std::endl;

  std::cout << "RNTI:" << std::endl;
  for (uint16_t r: measRnti){
    std::cout << r << " ";
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
    measRnti[imsi] = rnti;
    measAsState[imsi][cellid] = curCellRSRP; // first RSRPs
    measAsState[imsi][numberOfEnbs+cellid] = curCellRSRQ; // then RSRQs
    measAsState[imsi][2*numberOfEnbs+cellid] = 1;

    // iterate through measurements for neighboring cells and update measurement container
    for (std::list <LteRrcSap::MeasResultEutra>::iterator it = measReport.measResults.measResultListEutra.begin ();
        it != measReport.measResults.measResultListEutra.end ();
        ++it)
    {
      measAsState[imsi][it->physCellId-1] = (it->haveRsrpResult ? (uint32_t) it->rsrpResult : 0); // RSRP values between 0 and 97
      measAsState[imsi][numberOfEnbs+it->physCellId-1] = (it->haveRsrqResult ? (uint32_t) it->rsrqResult : 0);
      measAsState[imsi][2*numberOfEnbs+it->physCellId-1] = 0;
    }

    // printCurrentStateRntiAndTime();
}

/* 
Define Observation Space as a 2D Box with RSRP, RSRQ and serving Cell Id as states for each UE.
Since low and high are currently only supported to be same across fields, considering RSRP, RSRQ
and serving cell id, low is set to be 0 and high as 200, considering 0-200 being the range of 
RSRP and RSRQ
*/
Ptr<OpenGymSpace> GetObservationSpace (){
  uint32_t low = 0;
  uint32_t high = 200;
  // State space size per UE = 3*Enbs for RSRP, RSRQ and serving cell id
  std::vector<uint32_t> shape = {ueNodes.GetN () * 3 * enbNodes.GetN ()};
  std::string type = TypeNameGet<uint32_t> ();
  Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace> (low, high, shape, type);
  NS_LOG_UNCOND ("GetObservationSpace: " << space);
  return space;
}

Ptr<OpenGymSpace> GetActionSpace (){
  // enb_numbers to handover to for each node, obtained using decimal_to_nary function
  int action_size = ueNodes.GetN () * enbNodes.GetN () - 1; // Action space 0 to N-1
  Ptr<OpenGymDiscreteSpace> space = CreateObject<OpenGymDiscreteSpace> (action_size);
  NS_LOG_UNCOND ("GetActionSpace: " << space);
  return space;
}

Ptr<OpenGymDataContainer> GetObservation (){
  std::vector<uint32_t> shape = {ueNodes.GetN () * 3 * enbNodes.GetN ()};
  Ptr<OpenGymBoxContainer<uint32_t>> box = CreateObject<OpenGymBoxContainer<uint32_t>> (shape);

  std::vector<uint32_t> data;
  std::vector<uint32_t> curState = toVector (measAsState);
  data.assign(curState.begin(), curState.end());

  box->SetData (data);
  return box;
}

// Prepare action execution

std::vector<uint32_t> decimal_to_nary(uint32_t decimal, uint32_t n = 3, uint32_t resize = 2){
  
  std::vector<uint32_t> nary;
  if (!decimal){
    for (uint32_t i = 0; i < resize; ++i){
      nary.push_back(0);
    }
  }

  
  while (decimal){
    nary.push_back(decimal % n);
    decimal /= n;
  }
  
  resize = std::max((uint32_t) nary.size(), resize);
  for (uint32_t i = 0; i < (resize - nary.size()); ++i){
    nary.push_back(0);
  }
  
  std::reverse(nary.begin(), nary.end());
  return nary;
}

bool ExecuteActions (Ptr<OpenGymDataContainer> action){
  Ptr<OpenGymDiscreteContainer> discrete_action = DynamicCast<OpenGymDiscreteContainer> (action);
  
  uint32_t action_value = discrete_action->GetValue ();
  uint32_t nary_n = enbNodes.GetN ();
  uint32_t nary_resize = ueNodes.GetN ();
  
  std::vector<uint32_t> handoverIds = decimal_to_nary(action_value, nary_n, nary_resize);

  // reset handoverOccurred
  for (uint32_t i = 0; i < ueNodes.GetN (); ++i){
    handoverOccurred[i] = false;
  }

  // X2-based handover
  for (uint32_t imsi = 0; imsi < ueNodes.GetN (); ++imsi){
    uint16_t curCellId = 0;
    // position idx for one-hot encoded cell id vector for current imsi
    uint32_t start_idx = 2 * enbNodes.GetN ();
    uint32_t end_idx = start_idx + enbNodes.GetN ();
    
    // Identify the current cell id for current ue
    for (uint32_t i = start_idx; i < end_idx; ++i){
      if (measAsState[imsi][i] == 1){
        break;
      }
      curCellId += 1;
    }

    if (curCellId != handoverIds[imsi]){
      lteHelper->HandoverRequest (Seconds (0), ueLteDevs.Get (imsi), 
        enbLteDevs.Get (curCellId), enbLteDevs.Get (handoverIds[imsi]));
      handoverOccurred[imsi] = true;
    }
  }

  return true;
}

// Define Reward
// Reward for handover optimization from the paper 
// Intelligent Handover Algorithm for Vehicle-to-Network Communications With Double-Deep Q-Learning at:
// https://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=9762537
float GetReward (){
  double reward = 0;

  for (uint32_t imsi = 0; imsi < ueNodes.GetN (); ++imsi){
    uint16_t transitionedCellId = 0;
    // position idx for one-hot encoded cell id vector for current imsi
    uint32_t start_idx = 2 * enbNodes.GetN ();
    uint32_t end_idx = start_idx + enbNodes.GetN ();

    // Identify the transitioned cell id for current ue
    for (uint32_t i = start_idx; i < end_idx; ++i){
      if (measAsState[imsi][i] == 1){
        break;
      }
      transitionedCellId += 1;
    }

    // Identify Max measurement value and transitioned cell measurement value
    double maxMeas = measAsState[imsi][0];
    double transitionedCellMeas = measAsState[imsi][0];

    // The range index i to extract meas is currently for RSRP values
    // This will change if RSRQ is decided to be used.
    for (uint32_t i = 1; i < enbNodes.GetN (); ++i){
      if (maxMeas < measAsState[imsi][i]){
        maxMeas = measAsState[imsi][i];
      }

      if (i == transitionedCellId){
        transitionedCellMeas = measAsState[imsi][i];
      }
    }
    
    // reward summation
    // reward += (maxMeas - transitionedCellMeas);
    reward += (transitionedCellMeas == maxMeas ? 0.1 : transitionedCellMeas - maxMeas);
    if (handoverOccurred[imsi]){
      if (transitionedCellMeas == maxMeas){
        reward += 0.5;
      }
      reward -= handoverCost;
    }
  }

  // average the reward
  reward /= ueNodes.GetN ();

  return reward;
}

// Define extra info
std::string GetExtraInfo(void)
{
  std::string info = "Current Simulation Time: ";
  info += std::to_string(Simulator::Now ().GetSeconds ());
  NS_LOG_UNCOND("GetExtraInfo: " << info);
  return info;
}

// Define game over condition
bool GetGameOver(void){
  return Simulator::IsFinished();
}

// Notify current state and schedule for next state reading
void ScheduleNextStateRead(double envStepTime, Ptr<OpenGymInterface> openGym){
  Simulator::Schedule (Seconds(envStepTime), &ScheduleNextStateRead, envStepTime, openGym);
  openGym->NotifyCurrentState();
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
  // Parameters of the environment
  uint32_t simSeed = 1;
  // double simulationTime = 10; //seconds // defined later as simTime
  double envStepTime = 0.25; //seconds, ns3gym env step time interval
  uint32_t openGymPort = 5555;
  uint32_t testArg = 0;

  // Parameters of the simulation
  double distance = 500.0; // m
  double yForUe = 500.0;   // m
  double speed = 20;       // m/s
  double simTime = (double)(numberOfEnbs + 1) * distance / speed; // 1500 m / 20 m/s = 75 secs
  double enbTxPowerDbm = 46.0;
  std::string handoverAlgo = "A3-rsrp";

  Config::SetDefault ("ns3::LteHelper::UseIdealRrc", BooleanValue (true));

  // Command line arguments
  CommandLine cmd (__FILE__);
  // required parameters for openGym Interface
  cmd.AddValue ("openGymPort", "Port number for OpenGym env. Default: 5555", openGymPort);
  cmd.AddValue ("simSeed", "Seed for random generator. Default: 1", simSeed);
  // optional parameters for openGym Interface
  // cmd.AddValue ("simTime", "Simulation time in seconds. Default: 10s", simulationTime); // used later as simTime
  cmd.AddValue ("testArg", "Extra simulation argument. Default: 0", testArg);
  // optional reward parameter for opengym
  cmd.AddValue ("handoverCost", "Positive value as a handover cost. Default: 1", handoverCost);
  // parameters for ns3 simulation
  cmd.AddValue ("simTime", "Total duration of the simulation (in seconds)", simTime);
  cmd.AddValue ("speed", "Speed of the UE (default = 20 m/s)", speed);
  cmd.AddValue ("enbTxPowerDbm", "TX power [dBm] used by HeNBs (default = 46.0)", enbTxPowerDbm);
  cmd.AddValue ("handoverAlgo", "Handover algorithm to be used (default = A3-rsrp)", handoverAlgo);

  cmd.Parse (argc, argv);

  NS_LOG_UNCOND("Ns3Env parameters:");
  // NS_LOG_UNCOND("--simulationTime: " << simulationTime); // defined as simTime
  NS_LOG_UNCOND("--simulationTime: " << simTime);
  NS_LOG_UNCOND("--openGymPort: " << openGymPort);
  NS_LOG_UNCOND("--envStepTime: " << envStepTime);
  NS_LOG_UNCOND("--seed: " << simSeed);
  NS_LOG_UNCOND("--testArg: " << testArg);

  RngSeedManager::SetSeed (1);
  RngSeedManager::SetRun (simSeed);

  // Instantiate data containers
  for (uint32_t i = 0; i < numberOfUes; ++i){
    // Initialize simulation time record with 0 for each UE
    measTime.push_back(0);
    // Initialize RNTI with 0 for each UE
    measRnti.push_back(0);
    // each numberOfEnbs corresponds to RSRP, RSRQ and the one-hot vector for serving cell id
    measAsState.push_back(std::vector<uint32_t> (3 * numberOfEnbs, 0));
    // Initialize handover occurrence with false for each UE
    handoverOccurred.push_back(false);
  }

  // Create LTE helper
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

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
  enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  InternetStackHelper internet;
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIfaces;
  ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

  // Attach all UEs to the eNodeBs
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

  // OpenGym Env-------------------------------------------------------------------------------------------
  
  Ptr<OpenGymInterface> openGym = CreateObject<OpenGymInterface> (openGymPort);
  openGym->SetGetActionSpaceCb( MakeCallback (&GetActionSpace) );
  openGym->SetGetObservationSpaceCb( MakeCallback (&GetObservationSpace) );
  openGym->SetGetGameOverCb( MakeCallback (&GetGameOver) );
  openGym->SetGetObservationCb( MakeCallback (&GetObservation) );
  openGym->SetGetRewardCb( MakeCallback (&GetReward) );
  openGym->SetGetExtraInfoCb( MakeCallback (&GetExtraInfo) );
  openGym->SetExecuteActionsCb( MakeCallback (&ExecuteActions) );


  Simulator::Schedule (Seconds(envStepTime), &ScheduleNextStateRead, envStepTime, openGym);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  openGym->NotifySimulationEnd();
  Simulator::Destroy ();
  return 0;

}
