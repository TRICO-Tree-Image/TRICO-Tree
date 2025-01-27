#include "PIFO.h"

namespace ns3
{
    class Node;
    // const int PIFO::SIZE = ((1 << 10) - 1) / 3;
    const int PIFO::SIZE = 500;
    
    NS_OBJECT_ENSURE_REGISTERED(PIFO);
    TypeId PIFO::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::PIFO")
			.SetParent<QueueDisc>()
			.SetGroupName("TrafficControl")
			.AddConstructor<PIFO>();
		return tid;
	}


    PIFO::PIFO()
    {
        flow_map.clear();
        sch_tree.clear();
        queue_map.clear();
        root_size = 0;
        
        // Only WebSearch tenant need the scheduling tree
        for (int i = 0; i <= 1; ++i)
            sch_tree[i] = std::make_shared<pFabric>();
    }

    std::string PIFO::GetFlowLabel(Ptr<QueueDiscItem> item)
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

    void PIFO::InitializeParams(void) {
        // Read the Web Search traffic to initialize pFabric flow size
        flow_map["16783795349153"] = 11;
        flow_map["167838466100"] = 12;
        flow_map["16783820949153"] = 13;
        flow_map["167838466771"] = 14;
        queue_map[11] = std::queue<Ptr<QueueDiscItem>>();
        queue_map[12] = std::queue<Ptr<QueueDiscItem>>();
        queue_map[13] = std::queue<Ptr<QueueDiscItem>>();
        queue_map[14] = std::queue<Ptr<QueueDiscItem>>();
        sch_tree[0]->InitializeSize(11, 200);
        sch_tree[0]->InitializeSize(12, 200);
        sch_tree[0]->InitializeSize(13, 200);
        sch_tree[0]->InitializeSize(14, 200);
        //初始化flow scheduler
        //flow_scheduler = FlowScheduler(5);
    }

    bool PIFO::DoEnqueue(Ptr<QueueDiscItem> item)
    {

        string flow_label = GetFlowLabel(item);
        //cout << "flow_label: " << flow_label << " flow_map: " << flow_map[flow_label]<< endl;
        
        // The root queue is full
        if (root_size >= SIZE) {
           // cout << "drop" << endl;
            Drop(item);
            return false;
        }
        root_size++;
        // if(root_size >= 3)
             //cout << "root_size: " << root_size << endl;

        int flow_id = flow_map[flow_label];
        //cout << "flow_id: " << flow_id << endl;
        long long pri = sch_tree[0]->RankComputation(flow_id, 0);
        //cout << "pri: " << pri << endl;

        //flow scheduler
        //FlowScheduler::per_flow_item pf_item = {flow_id, pri, item};
        //flow_scheduler.flow_queue_push(pf_item);
        //FlowScheduler::per_flow_item schedule_item = flow_scheduler.flow_queue_schedule();

        //per-packet
        per_item pitem = {flow_id, pri, item};
        schedule_queue.push(pitem);
        //cout << "queue_size: " << schedule_queue.size() << endl;
        queue_map[flow_id].push(item);

        return true;
    }

    Ptr<QueueDiscItem> PIFO::DoDequeue()
    {
        // if (!bypath_queue.empty()) {
        //     Ptr<QueueDiscItem> item = bypath_queue.front();
        //     bypath_queue.pop();
        //     return item;
        // }
        
        if(schedule_queue.empty()){
            return nullptr;
        }
        per_item pitem = schedule_queue.top();
        int queue_id = pitem.flow_id;
        
        schedule_queue.pop();
        root_size--;
        Ptr<QueueDiscItem> item = queue_map[queue_id].front();
        queue_map[queue_id].pop();

        cout << "queu_id: " << queue_id << endl;
        cout << "queue_lengyj: " << schedule_queue.size() << endl;
        // printf("my log: has a real packet %d!\n", ans);
        
        sch_tree[0]->Dequeue(queue_id);
        
        return item;
    }
    
    Ptr<const QueueDiscItem> PIFO::DoPeek(void) const {
        return 0;
    }
    
    bool PIFO::CheckConfig(void) {
        return 1;
    }

}
