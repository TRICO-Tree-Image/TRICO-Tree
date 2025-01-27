#include "SimplePIFO.h"

namespace ns3
{
    class Node;
    const int SimplePIFO::ROOT = 5;
    const int SimplePIFO::SIZE = ((1 << 10) - 1) / 3;
    //const int SimplePIFO::SIZE = 1;
    
    NS_OBJECT_ENSURE_REGISTERED(SimplePIFO);
    TypeId SimplePIFO::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::SimplePIFO")
			.SetParent<QueueDisc>()
			.SetGroupName("TrafficControl")
			.AddConstructor<SimplePIFO>();
		return tid;
	}

    bool SimplePIFO::IsHadoop(int x)
    {
        return x > 1;
    }

    SimplePIFO::SimplePIFO()
    {
        flow_map.clear();
        sch_tree.clear();
        queue_map.clear();
        while(!bypath_queue.empty())
            bypath_queue.pop();
        root_size = 0;
        
        // Only WebSearch tenant need the scheduling tree
        for (int i = 0; i <= 1; ++i)
            sch_tree[i] = std::make_shared<pFabric>();
    }

    std::string SimplePIFO::GetFlowLabel(Ptr<QueueDiscItem> item)
    {
        //printf("my log: flow label?\n");
        Ptr<const Ipv4QueueDiscItem> ipItem =
            DynamicCast<const Ipv4QueueDiscItem>(item);

        const Ipv4Header ipHeader = ipItem->GetHeader();
        TcpHeader header;
        GetPointer(item)->GetPacket()->PeekHeader(header);

        std::stringstream ss;
        ss << ipHeader.GetSource().Get();
        ss << header.GetSourcePort();
        //ss << ipHeader.GetDestination().Get();
        //ss << header.GetDestinationPort();
        
        /*cout << "my log: find tenant log" << ipHeader.GetSource().Get() << " "<<ipHeader.GetDestination().Get()
            << " "<< header.GetSourcePort() <<" "<< header.GetDestinationPort() << endl;*/

        std::string flowLabel = ss.str();
        //cout << "my log: flow label " << flowLabel << endl;
        return flowLabel;
    }
    
    // void SimplePIFO::InitializeParams(void) {
    //     // Read the Web Search traffic to initialize pFabric flow size
    //     uint32_t id = this->GetNetDevice()->GetNode()->GetId();
    //     std::ifstream infile;
    //     infile.open("SimpleResult/flowlabel.txt");
    //     int no, size, tenant;
    //     uint32_t src_ip, dst_ip;
    //     string flow_label;
    //     infile >> no;
    //     while (no--)
    //     {
    //         infile >> flow_label >> src_ip >> dst_ip >> size >> tenant;
    //         // Server node
    //         if (id < 144) {
    //             if (src_ip != id && dst_ip != id)
    //                 continue;
    //         }
    //         // Leaf node
    //         /*else if (id < 144 + 9) {
    //             if (src_ip / 16 != id && dst_ip / 16 != id)
    //                 continue;
    //         }*/
    //         flow_map[flow_label] = ++queue_cnt;
    //         queue_map[queue_cnt] = std::queue<Ptr<QueueDiscItem>>();
    //         sch_tree[tenant]->InitializeSize(queue_cnt, size);
    //     }
    //     //初始化flow scheduler
    //     //flow_scheduler = FlowScheduler(queue_cnt);
    //     flow_scheduler = FlowScheduler(5);
    // }

    void SimplePIFO::InitializeParams(void) {
        // Read the Web Search traffic to initialize pFabric flow size
        // flow_map["16783795349153"] = 11;
        // flow_map["167838466100"] = 12;
        // flow_map["16783820949153"] = 13;
        // flow_map["167838466771"] = 14;
        // queue_map[11] = std::queue<Ptr<QueueDiscItem>>();
        // queue_map[12] = std::queue<Ptr<QueueDiscItem>>();
        // queue_map[13] = std::queue<Ptr<QueueDiscItem>>();
        // queue_map[14] = std::queue<Ptr<QueueDiscItem>>();
        // sch_tree[0]->InitializeSize(11, 200);
        // sch_tree[0]->InitializeSize(12, 200);
        // sch_tree[0]->InitializeSize(13, 200);
        // sch_tree[0]->InitializeSize(14, 200);
        //初始化flow scheduler
        flow_scheduler = FlowScheduler(5);
    }

    // bool SimplePIFO::DoEnqueue(Ptr<QueueDiscItem> item)
    // {
    //     TenantTag my_tag;
    //     Ptr<Packet> packet = GetPointer(item->GetPacket());
    //     packet->PeekPacketTag(my_tag);
    //     int tenant = my_tag.GetTenantId();

    //     string flow_label = GetFlowLabel(item);
    //     cout << "flow_label: " << flow_label << endl;
        
    //     // For TCP shake hands packets, bypath to output directly
    //     if (tenant == 10233) {
    //         bypath_queue.push(item);
    //         return true;
    //     }
        
    //     //printf("my log: DoEnqueue %d\n", tenant);
        
    //     // The root queue is full
    //     if (root_size >= SIZE) {
    //         Drop(item);
    //         return false;
    //     }
    //     root_size++;

    //     // A Hadoop tenant, the priority is arrival time
    //     if (IsHadoop(tenant))
    //     {
    //         time++;
    //         if (!queue_map.count(tenant))
    //             queue_map[tenant] = std::queue<Ptr<QueueDiscItem>>();
    //         // Directly and only insert to the root queue
    //         pipo.AddPush(ROOT, time, tenant);
    //         queue_map[tenant].push(item);
    //     }
    //     // A WebSearch tenant, each flow need a real packet queue
    //     else {
    //         string flow_label = GetFlowLabel(item);
    //         int flow_id = flow_map[flow_label];
    //         long long pri = sch_tree[tenant]->RankComputation(flow_id, 0);
    //         //printf("my log: web search pfabric flow rank %d %d\n", tenant, rk4);

    //         //flow scheduler
    //         FlowScheduler::per_flow_item pf_item = {flow_id, pri, item};
    //         flow_scheduler.flow_queue_push(pf_item);
    //         FlowScheduler::per_flow_item schedule_item = flow_scheduler.flow_queue_schedule();

    //         // pipo.AddPush(ROOT, pri, flow_id);
    //         // queue_map[flow_id].push(item);
    //         pipo.AddPush(ROOT, schedule_item.rank, schedule_item.flow_id);
    //         queue_map[schedule_item.flow_id].push(schedule_item.item);
    //     }

    //     return true;
    // }

    bool SimplePIFO::DoEnqueue(Ptr<QueueDiscItem> item)
    {
        //TenantTag my_tag;
        //Ptr<Packet> packet = GetPointer(item->GetPacket());
        //packet->PeekPacketTag(my_tag);
        //int tenant = my_tag.GetTenantId();
        //cout << "tenant: " << tenant << endl;

        string flow_label = GetFlowLabel(item);
        if(flow_map.find(flow_label) == flow_map.end()){
            cout << "flow_label: " << flow_label << endl;
            flow_map[flow_label] = queue_cnt++;
        }
        //cout << "flow_label: " << flow_label << " flow_map: " << flow_map[flow_label]<< endl;
        
        // For TCP shake hands packets, bypath to output directly
        // if (tenant == 10233) {
        //     bypath_queue.push(item);
        //     return true;
        // }

        // The root queue is full
        if (root_size >= SIZE) {
            //cout << "drop" << endl;
            Drop(item);
            return false;
        }
        root_size++;
        //cout << "root_size: " << root_size << endl;

        int flow_id = flow_map[flow_label];
        //cout << "flow_id: " << flow_id << endl;
        long long pri = sch_tree[0]->RankComputation(flow_id, 0);
        //cout << "pri: " << pri << endl;

        //flow scheduler
        //FlowScheduler::per_flow_item pf_item = {flow_id, pri, item};
        //flow_scheduler.flow_queue_push(pf_item);
        //FlowScheduler::per_flow_item schedule_item = flow_scheduler.flow_queue_schedule();

        pipo.AddPush(ROOT, pri, flow_id);
        queue_map[flow_id].push(item);
        //pipo.AddPush(ROOT, schedule_item.rank, schedule_item.flow_id);
        //queue_map[schedule_item.flow_id].push(schedule_item.item);

        return true;
    }

    // Ptr<QueueDiscItem> SimplePIFO::DoDequeue()
    // {
    //     if (!bypath_queue.empty()) {
    //         Ptr<QueueDiscItem> item = bypath_queue.front();
    //         bypath_queue.pop();
    //         return item;
    //     }
        
    //     int ans = pipo.GetToken();
    //     // No packet in buffer now
    //     if (ans == -1) {
    //         // empty_queue++;
    //         // printf("? empty ans %d\n", empty_queue);
    //         return nullptr;
    //     }
    //     root_size--;
    //     Ptr<QueueDiscItem> item = queue_map[ans].front();
    //     queue_map[ans].pop();

    //     // printf("my log: has a real packet %d!\n", ans);
        
    //     TenantTag my_tag;
    //     Ptr<Packet> packet = GetPointer(item->GetPacket());
    //     packet->PeekPacketTag(my_tag);
    //     int tenant = my_tag.GetTenantId();
    //     if (!IsHadoop(tenant))
    //         sch_tree[tenant]->Dequeue(ans);
        
    //     return item;
    // }

    Ptr<QueueDiscItem> SimplePIFO::DoDequeue()
    {
        if (!bypath_queue.empty()) {
            Ptr<QueueDiscItem> item = bypath_queue.front();
            bypath_queue.pop();
            return item;
        }
        
        int ans = pipo.GetToken();
        // No packet in buffer now
        if (ans == -1) {
            return nullptr;
        }
        root_size--;
        Ptr<QueueDiscItem> item = queue_map[ans].front();
        queue_map[ans].pop();

        // printf("my log: has a real packet %d!\n", ans);
        
        sch_tree[0]->Dequeue(ans);
        
        return item;
    }
    
    Ptr<const QueueDiscItem> SimplePIFO::DoPeek(void) const {
        return 0;
    }
    
    bool SimplePIFO::CheckConfig(void) {
        return 1;
    }

}
