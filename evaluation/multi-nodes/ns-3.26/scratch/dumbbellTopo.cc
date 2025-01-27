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
 *
 * ==== Full network simulation experiment for vPIFO
 * ==== 4 spine 9 leaf 144 server  leaf-spine topo
 *
 */



#include <iomanip>
#include <chrono>
#include <thread>
# include <algorithm>
# include <cstdio>
# include <fstream>
# include <iostream>
# include <map>
# include <unordered_map>

# include "ns3/applications-module.h"
# include "ns3/core-module.h"
# include "ns3/flow-monitor-module.h"
# include "ns3/internet-module.h"
# include "ns3/ipv4-header.h"
# include "ns3/netanim-module.h"
# include "ns3/network-module.h"
# include "ns3/point-to-point-module.h"
# include "ns3/random-variable-stream.h"
# include "ns3/stats-module.h"
# include "ns3/traffic-control-module.h"
# include "ns3/udp-header.h"
# include "ns3/packet.h"
# include "ns3/tag.h"
# include "ns3/Tenant-tag.h"
#include "ns3/flow-id-tag.h"




using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("MyTopoTCP");

// const int LEAF_CNT = 9;                              // number of leaf nodes
// const int SPINE_CNT = 4;                             // number of spine nodes
// const int SERVER_CNT = 144;                          // number of server nodes
int dumbbellSize = 16;
const int LINK_CNT = 2*dumbbellSize+1; // number of links
const int FLOW_NUM = 5000;                           // number of WebSearch flow
const int TENANT_NUM = 4;                // number of tenant
uint32_t totalPktSize = 0;                 // count the total pkt # of all flows
uint32_t total_send = 0;
const char *ACCESS_RATE = "40Gbps";   // Access Link Bandwith 10G
const char *BACKBONE_RATE = "100Gbps"; // Leaf-spine Link Bandwith 10G
const char *DATARATE = "10Gbps";  // Bandwith 10G
const char *DELAY =
    "3us";         // Delay 3us (0.000001s -> 0.0001s)  (3ms = 0.001s -> 0.1s)
double simulator_stop_time = 4; // 0.0301; //0.0086404
const int PKTSIZE = 1448;        // pkt size payload

uint32_t websearch_count = 0;

// ip address pairs on the same links
std::vector<Ipv4InterfaceContainer> interfaces(LINK_CNT);
// Create nodes: leaf + spine + server = nodes
NodeContainer leftNodes, rightNodes, switchNodes, allnodes,nodes;


class MyApp : public Application
{
public:
  MyApp();
  virtual ~MyApp();
  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId(void);
  void Setup(Ptr<Socket> socket, uint32_t sip, Ipv4Address saddr, 
      Address address, uint32_t packetSize,
      uint32_t nBytes, DataRate dataRate, uint32_t flowId, uint32_t tenant, uint16_t dport);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);
  void ScheduleTx(uint32_t);
  void SendPacket(void);
  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_nBytes;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_running;
  uint32_t m_bytesSent;
  uint32_t m_flowId;
  uint32_t m_tenant;

  uint32_t m_lastProgressSend;  
};

MyApp::MyApp()
    : m_socket(0),
      m_peer(),
      m_packetSize(0),
      m_nBytes(0),
      m_dataRate(0),
      m_sendEvent(),
      m_running(false),
      m_bytesSent(0),
      m_tenant(0),
      m_lastProgressSend(0) {}

MyApp::~MyApp() { m_socket = 0; }

/* static */
TypeId MyApp::GetTypeId(void)
{
  static TypeId tid = TypeId("MyApp")
                          .SetParent<Application>()
                          .SetGroupName("Tutorial")
                          .AddConstructor<MyApp>();
  return tid;
}

void MyApp::Setup(Ptr<Socket> socket, uint32_t sip, Ipv4Address saddr,
    Address address, uint32_t packetSize, 
    uint32_t nBytes, DataRate dataRate, uint32_t flowId, uint32_t tenant, uint16_t dport)
{
  m_socket = socket;

  if(dport == 0){
    dport = 999;
  }
  m_socket->Bind(InetSocketAddress(saddr, dport));

  m_peer = address;
  m_packetSize = packetSize;
  m_nBytes = nBytes;
  m_dataRate = dataRate;
  m_flowId = dport;
  m_tenant = tenant;

}

void MyApp::StartApplication(void)
{
  
  m_running = true;
  m_bytesSent = 0;
  m_socket->Connect(m_peer);
  SendPacket();
}

void MyApp::StopApplication(void)
{
  m_running = false;
  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }
  if (m_socket)
  {
    m_socket->Close();
  }
}

void MyApp::SendPacket(void)
{
  
    uint32_t bytesLeft = m_nBytes - m_bytesSent;
  uint32_t pktSize = std::min(m_packetSize, bytesLeft);

  Ptr<Packet> packet = Create<Packet>(pktSize);
  TenantTag tag;
  tag.SetTenantId(m_nBytes);
  packet->AddPacketTag(tag);

  FlowIdTag flowTag;
  flowTag.SetFlowId(Simulator::Now().GetNanoSeconds()); 
  packet->AddPacketTag(flowTag);


  m_socket->Send(packet);

  //Progress display
  if (!(m_flowId % 1000 == 1 && m_flowId >= 1 && m_flowId <= 15001))
    total_send += pktSize;

  static const uint32_t progressThreshold = 1000000; 
  if (total_send - m_lastProgressSend >= progressThreshold)
  {
    double percentage = (double)total_send / totalPktSize * 100;
    std::cout << "\r" << percentage << "%   " << total_send << "/" << totalPktSize << std::flush;
    m_lastProgressSend = total_send;  
  }

  m_bytesSent += pktSize;
  if (m_bytesSent < m_nBytes)
  {
    ScheduleTx(pktSize);
  }
}

void MyApp::ScheduleTx(uint32_t pktSize)
{
  if (m_running)
  {
    Time tNext(Seconds(pktSize * 8 /
                       static_cast<double>(m_dataRate.GetBitRate())));
    m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
  }
}

ofstream Output;

class TraceFlow
{
private:
  ifstream flow;
  uint32_t flow_num;
  struct FlowInput
  {
    uint32_t src, dst, byte_count;
    uint16_t dport;
    double start_time;
    uint32_t idx;
    uint32_t tenant;
  };

  FlowInput flow_input = {0};

public:
  enum type
  {
    WebSearch,
    Hadoop
  } flow_type;
  
  TraceFlow(string file_name, type flow_t)
  {
    flow.open(file_name);
    flow >> flow_num;
    cout << "flow_num:" << flow_num << endl;
    if (flow_t == WebSearch)
      websearch_count = flow_num;
    flow_input.idx = 0;
    flow_type = flow_t;
  }

  void ReadFlowInput()
  {
    flow >> flow_input.src >> flow_input.dst >>
        flow_input.dport >> flow_input.byte_count >> flow_input.start_time >>flow_input.tenant;


    totalPktSize += flow_input.byte_count;
  }


  void ScheduleFlowInput()
  {
    // Server
    Address sinkAddress(InetSocketAddress(
        interfaces[dumbbellSize+flow_input.dst].GetAddress(0), 30011));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", 
      InetSocketAddress(Ipv4Address::GetAny(), 30011));
    ApplicationContainer sinkApp =
        sinkHelper.Install(nodes.Get(dumbbellSize+flow_input.dst));
    sinkApp.Start(Seconds(0));
    sinkApp.Stop(Seconds(simulator_stop_time));

    // Client
    TypeId tid = TypeId::LookupByName("ns3::TcpNewReno");
    Config::Set("/NodeList/*/$ns3::TcpL4Protocol/SocketType",
                TypeIdValue(tid));
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(
        nodes.Get(flow_input.src), TcpSocketFactory::GetTypeId());
    ns3TcpSocket->SetAttribute(
        "SndBufSize", ns3::UintegerValue(1438000000));
    Ipv4Address saddr = interfaces[flow_input.src].GetAddress(0);
    uint32_t sip = interfaces[flow_input.src].GetAddress(0).Get();

    Ptr<MyApp> app = CreateObject<MyApp>();
    app->Setup(
        ns3TcpSocket, sip, saddr, sinkAddress, PKTSIZE, flow_input.byte_count,
        DataRate(DATARATE), flow_input.idx, flow_input.tenant, flow_input.dport);
    nodes.Get(flow_input.src)->AddApplication(app);

    app->SetStartTime(Seconds(flow_input.start_time) - Simulator::Now());
  }


  void Input()
  {
    if (flow_type == WebSearch) {
      string path = "TricoTreeResult/flowlabelTricoTree.txt";
      Output.open(path);
      Output << flow_num << endl;
    }
    while (flow_input.idx < flow_num) {
      ReadFlowInput();
      ScheduleFlowInput();
      flow_input.idx++;
    }
    flow.close();
    if (flow_type == WebSearch)
      Output.close();
  }

} 
web_search("traffic_gen/test2/traffic_ws8_tenant2.txt", TraceFlow::WebSearch);


void PacketReceived(Ptr<const Packet> packet) {
  std::cout<<"Packet received!"<<std::endl;
}


int main(int argc, char *argv[])
{
  // LogComponentEnable ("TcpSocketBase", LOG_LEVEL_INFO);
  // LogComponentEnable ("TcpCongestionOps", LOG_LEVEL_INFO);
  // LogComponentEnable ("PointToPointNetDevice", LOG_LEVEL_INFO);

  // time out settings
  Config::SetDefault("ns3::TcpSocket::DelAckTimeout", TimeValue(Seconds(0.0)));
  // duplicate ack time out = 3RTT = 36us  //100us
  Config::SetDefault("ns3::RttEstimator::InitialEstimation",
                     TimeValue(Seconds(0.000012))); // 12us
  Config::SetDefault("ns3::TcpSocket::ConnTimeout",
                     TimeValue(Seconds(0.000060))); // syn timeout 3rtt
  Config::SetDefault("ns3::TcpSocketBase::MinRto",
                     TimeValue(Seconds(0.000060))); // 3RTT=36us
  Config::SetDefault("ns3::TcpSocketBase::ClockGranularity",
                     TimeValue(MilliSeconds(0.012))); // RTO=3RTT=36us
  // Config::SetDefault ("ns3::TcpSocketBase::ConnCount", TimeValue(MilliSeconds
  // (0.012)));//ConnCount

  // MSS
  Config::SetDefault("ns3::TcpSocket::DataRetries", UintegerValue(100));
  Config::SetDefault("ns3::TcpSocket::ConnCount", UintegerValue(100));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
  // SegmentSize

  clock_t begint, endt;
  begint = clock();
  CommandLine cmd;
  cmd.Parse(argc, argv);
  Time::SetResolution(Time::NS);


  // Create nodes
  leftNodes.Create(dumbbellSize);
  rightNodes.Create(dumbbellSize);
  switchNodes.Create(2);

  nodes = NodeContainer(leftNodes, rightNodes);
  allnodes=NodeContainer(nodes, switchNodes);
  // Configure channels
  PointToPointHelper backboneHelper;
  backboneHelper.SetDeviceAttribute("DataRate", StringValue(BACKBONE_RATE));
  backboneHelper.SetChannelAttribute("Delay", StringValue(DELAY));
  backboneHelper.SetQueue("ns3::DropTailQueue",
                              "MaxPackets", UintegerValue(100));
  PointToPointHelper accessHelper;
  accessHelper.SetDeviceAttribute("DataRate", StringValue(ACCESS_RATE));
  accessHelper.SetChannelAttribute("Delay", StringValue(DELAY));
  accessHelper.SetQueue("ns3::DropTailQueue",
                            "MaxPackets", UintegerValue(100));

  // Install configurations to devices (NIC)
  std::vector<NetDeviceContainer> devices(LINK_CNT);
  for (int i = 0; i < dumbbellSize; ++i)
  {
    // std::cout << i<<" "<<i+dumbbellSize<<endl;
    devices[i] =accessHelper.Install(leftNodes.Get(i), switchNodes.Get(0));
    devices[i+dumbbellSize] =accessHelper.Install(rightNodes.Get(i), switchNodes.Get(1));
  }
  devices[LINK_CNT-1]=backboneHelper.Install(switchNodes.Get(0), switchNodes.Get(1));

  // Install network protocals
  InternetStackHelper stack;
  stack.Install(allnodes); // install tcp, udp, ip...

  // Assign ip address
  Ipv4AddressHelper address;
  for (int i = 0; i < LINK_CNT; i++)
  {
    std::ostringstream subset;
    subset << "10.1." << i + 1 << ".0";
    // n0: 10.1.1.1    n1 NIC1: 10.1.1.2   n1 NIC2: 10.1.2.1   n2 NIC1: 10.1.2.2
    address.SetBase(subset.str().c_str(), "255.255.255.0"); // sebnet & mask
    interfaces[i] = address.Assign(
        devices[i]); // set ip addresses to NICs: 10.1.1.1, 10.1.1.2 ...
  }
  
  //unintall traffic control
  TrafficControlHelper tch;
  tch.Uninstall(devices[LINK_CNT-1].Get(0));
  tch.SetRootQueueDisc("ns3::TricoTree");
  tch.Install(devices[LINK_CNT-1].Get(0));

  // Initialize routing database and set up the routing tables in the nodes.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Input the trace input and schedule send packet
  web_search.Input();


  for (int i = 0; i < 16; i++){
      // Server (UDP Sink)
    Address sinkAddress(InetSocketAddress(
        interfaces[dumbbellSize+i].GetAddress(0),  30011));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", 
      InetSocketAddress(Ipv4Address::GetAny(),  30011));
    ApplicationContainer sinkApp =
        sinkHelper.Install(nodes.Get(dumbbellSize+i));
    sinkApp.Start(Seconds(0));
    sinkApp.Stop(Seconds(simulator_stop_time));

    // Client (UDP Source)
    Ptr<Socket> ns3UdpSocket = Socket::CreateSocket(
        nodes.Get(i), UdpSocketFactory::GetTypeId());
    ns3UdpSocket->SetAttribute("RcvBufSize", ns3::UintegerValue(1438000000)); 

    Ipv4Address saddr = interfaces[i].GetAddress(0);
    uint32_t sip = interfaces[i].GetAddress(0).Get();

    Ptr<MyApp> app = CreateObject<MyApp>();
    app->Setup(
        ns3UdpSocket, sip, saddr, sinkAddress, PKTSIZE, 2000000000,
        DataRate("15Gbps"),0, i, i*1000+1);
    nodes.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(2)- Simulator::Now());
    app->SetStopTime(Seconds(3.2)- Simulator::Now()); 
  }
  
  // for (int i = 8; i < 16; i++){
  //     // Server (UDP Sink)
  //   Address sinkAddress(InetSocketAddress(
  //       interfaces[dumbbellSize+i].GetAddress(0),  30011));
  //   PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", 
  //     InetSocketAddress(Ipv4Address::GetAny(),  30011));
  //   ApplicationContainer sinkApp =
  //       sinkHelper.Install(nodes.Get(dumbbellSize+i));
  //   sinkApp.Start(Seconds(0));
  //   sinkApp.Stop(Seconds(simulator_stop_time));

  //   // Client (UDP Source)
  //   Ptr<Socket> ns3UdpSocket = Socket::CreateSocket(
  //       nodes.Get(i), UdpSocketFactory::GetTypeId());
  //   ns3UdpSocket->SetAttribute("RcvBufSize", ns3::UintegerValue(1438000000)); 
  //   Ipv4Address saddr = interfaces[i].GetAddress(0);
  //   uint32_t sip = interfaces[i].GetAddress(0).Get();

  //   Ptr<MyApp> app = CreateObject<MyApp>();
  //   app->Setup(
  //       ns3UdpSocket, sip, saddr, sinkAddress, PKTSIZE, 2000000000,
  //       DataRate("15Gbps"),0, i, i*1000+1);
  //   nodes.Get(i)->AddApplication(app);
  //   app->SetStartTime(Seconds(2.25)- Simulator::Now());
  //   app->SetStopTime(Seconds(2.75)- Simulator::Now()); 
  // }
  

  std::cout << "Running Simulation.\n";
  fflush(stdout);
  NS_LOG_INFO("Run Simulation.");
  Simulator::Stop(Seconds(simulator_stop_time));
  Simulator::Run();

  Simulator::Destroy();
  NS_LOG_INFO("Done.");

  endt = clock();
  cout << (double)(endt - begint) / CLOCKS_PER_SEC << "\n";
  cout << "totalPktSize:" << totalPktSize << std::endl;
  cout << "total_send:" << total_send << endl;
  
  return 0;
}
