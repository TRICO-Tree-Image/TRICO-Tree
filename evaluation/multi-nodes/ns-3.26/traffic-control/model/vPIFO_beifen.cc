// #include "vPIFO.h"

// namespace ns3
// {
//     class Node;
//     const int vPIFO::ROOT = 5;
//     const int vPIFO::SIZE = ((1 << 10) - 1) / 3;
    
//     NS_OBJECT_ENSURE_REGISTERED(vPIFO);
//     TypeId vPIFO::GetTypeId(void)
// 	{
// 		static TypeId tid = TypeId("ns3::vPIFO")
// 			.SetParent<QueueDisc>()
// 			.SetGroupName("TrafficControl")
// 			.AddConstructor<vPIFO>();
// 		return tid;
// 	}

//     bool vPIFO::IsHadoop(int x)
//     {
//         return x > 1;
//     }

//     vPIFO::vPIFO()
//     {
//         flow_map.clear();
//         sch_tree.clear();
//         queue_map.clear();
//         while(!bypath_queue.empty())
//             bypath_queue.pop();
//         root_size = 0;
        
//         // Build the scheduling tree

//         // Root node: root to tenant group
//         sch_tree[ROOT] = std::make_shared<SP>();
        
//         // Layer 1: tenant group to tenant type
//         for (int i = 0; i <= 1; ++i)
//             sch_tree[i] = std::make_shared<pFabric>();

//     }

//     std::string vPIFO::GetFlowLabel(Ptr<QueueDiscItem> item)
//     {
//         //printf("my log: flow label?\n");
//         Ptr<const Ipv4QueueDiscItem> ipItem =
//             DynamicCast<const Ipv4QueueDiscItem>(item);

//         const Ipv4Header ipHeader = ipItem->GetHeader();
//         TcpHeader header;
//         GetPointer(item)->GetPacket()->PeekHeader(header);

//         std::stringstream ss;
//         ss << ipHeader.GetSource().Get();
//         ss << header.GetSourcePort();
//         //ss << ipHeader.GetDestination().Get();
//         //ss << header.GetDestinationPort();
        
//         /*cout << "my log: find tenant log" << ipHeader.GetSource().Get() << " "<<ipHeader.GetDestination().Get()
//             << " "<< header.GetSourcePort() <<" "<< header.GetDestinationPort() << endl;*/

//         std::string flowLabel = ss.str();
//         //cout << "my log: flow label " << flowLabel << endl;
//         return flowLabel;
//     }
    
//     void vPIFO::InitializeParams(void) {
//         // Read the Web Search traffic to initialize pFabric flow size
//         uint32_t id = this->GetNetDevice()->GetNode()->GetId();
//         std::ifstream infile;
//         infile.open("vPIFOResult/flowlabel.txt");
//         int no, size, tenant;
//         uint32_t src_ip, dst_ip;
//         string flow_label;
//         infile >> no;
//         while (no--)
//         {
//             infile >> flow_label >> src_ip >> dst_ip >> size >> tenant;
//             // Server node
//             if (id < 144) {
//                 if (src_ip != id && dst_ip != id)
//                     continue;
//             }
//             // Leaf node
//             /*else if (id < 144 + 9) {
//                 if (src_ip / 16 != id && dst_ip / 16 != id)
//                     continue;
//             }*/
//             flow_map[flow_label] = ++queue_cnt;
//             queue_map[queue_cnt] = std::queue<Ptr<QueueDiscItem>>();
//             sch_tree[tenant]->InitializeSize(queue_cnt, size);
//         }
//     }

//     bool vPIFO::DoEnqueue(Ptr<QueueDiscItem> item)
//     {
//         TenantTag my_tag;
//         Ptr<Packet> packet = GetPointer(item->GetPacket());
//         packet->PeekPacketTag(my_tag);
//         int tenant = my_tag.GetTenantId();
        
//         // For TCP shake hands packets, bypath to output directly
//         if (tenant == 10233) {
//             bypath_queue.push(item);
//             return true;
//         }
        
//         //printf("my log: DoEnqueue %d\n", tenant);
        
//         // The root queue is full
//         if (root_size >= SIZE) {
//             Drop(item);
//             return false;
//         }
//         root_size++;

//         long long rk1 = sch_tree[ROOT]->RankComputation(tenant, 0);
//         pipe.AddPush(ROOT, rk1, tenant);

//         if (IsHadoop(tenant)) {
//             if (!queue_map.count(tenant))
//                 queue_map[tenant] = std::queue<Ptr<QueueDiscItem>>();
//             queue_map[tenant].push(item);
//         }
//         else {
//             string flow_label = GetFlowLabel(item);
//             int flow_id = flow_map[flow_label];
//             long long rk4 = sch_tree[tenant]->RankComputation(flow_id, 0);
//             //printf("my log: web search pfabric flow rank %d %d\n", tenant, rk4);
//             pipe.AddPush(tenant, rk4, flow_id);
//             queue_map[flow_id].push(item);
//         }

//         return true;
//     }

//     Ptr<QueueDiscItem> vPIFO::DoDequeue()
//     {
//         if (!bypath_queue.empty()) {
//             Ptr<QueueDiscItem> item = bypath_queue.front();
//             bypath_queue.pop();
//             return item;
//         }
        
//         int ans = pipe.GetToken();
//         //cout << "ans: " << ans << endl;
//         // No packet in buffer now
//         if (ans == -1) {
//             //empty_queue++;
//             // printf("? empty ans %d\n", empty_queue);
//             return nullptr;
//         }
//         root_size--;
//         Ptr<QueueDiscItem> item = queue_map[ans].front();
//         queue_map[ans].pop();

//         //printf("my log: has a real packet %d!", ans);
//         // cout << Simulator::Now() << endl;
//         TenantTag my_tag;
//         Ptr<Packet> packet = GetPointer(item->GetPacket());
//         packet->PeekPacketTag(my_tag);
//         int tenant = my_tag.GetTenantId();
//         if (tenant < 2)
//             sch_tree[tenant]->Dequeue(ans);
    
//         return item;
//     }
    
//     Ptr<const QueueDiscItem> vPIFO::DoPeek(void) const {
//         return 0;
//     }
    
//     bool vPIFO::CheckConfig(void) {
//         return 1;
//     }

// }
