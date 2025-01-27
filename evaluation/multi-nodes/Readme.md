# ns3 for TricoTree

## Data Center Scenario

1. Leaf-Spine Topology
2. **Target Data**: X-axis is load, Y-axis is 95% FCT, load range 0.3~0.95, 5 schedulers, 40 data sets.
3. Traffic Generation: Run the following command in the `ns-3.26/traffic_gen` folder:
    
    `python traffic_gen.py -c WebSearch_distribution.txt -n 144 -l 0.95 -b 10G -t 0.01 -o traffic_ws9.txt`
    
    `python traffic_gen.py -c csl_web_server_distribution.txt -n 144 -l 0.05 -b 10G -t 0.01 -o traffic_csl_web7.txt`
    
    Traffic Allocation: In `ns-3.26/traffic_gen/traffic_tenant.py`, this script randomly allocates the generated `traffic_ws.txt` and `traffic_hd.txt` traffic to 5 tenants, and sets the source port for each flow to `((tenant_id-1)*1000, tenant_id*1000]`.
    
4. Run TricoTree
    
    Run the following command in the `ns-3.26` folder:
    
    `nohup ./waf --run myTopo_TricoTree > ./TricoTree_Log/TricoTree.log 2>&1 &`
    
5. Result Recording
Data is saved in the `ns-3.26/TricoTreeResult` folder.
    - The `AllFct.txt` file contains one flow per line with the following fields: FCT, PacketCount, Throughput, Tenant. It is first sorted by tenant, and then by FCT in ascending order within each tenant.
    - The `95Fct.txt` file's first 5 lines represent the 5 tenants. Each line contains: Tenant, the flow ID corresponding to the initial data's 95% FCT, the 95% FCT, and the average 95% FCT within the tenant. The middle line contains the cumulative 95% FCT and average 95% FCT across the 5 tenants. The following 5 lines represent the 5 tenants, each line containing: Tenant, the flow ID corresponding to the initial data's 95% FCT, the 95% FCT, the average FCT within the tenant, and the last line contains the cumulative 95% FCT and average FCT across the 5 tenants.

# Wide Area Network Scenario

1. Dumbbell Topology
2. **Target Data**: X-axis is time, Y-axis is bandwidth.
3. Traffic Generation: Run the following command in the `ns-3.26/traffic_gen` folder:
    
    `python traffic_gen.py -c WebSearch_distribution.txt -n 16 -l 0.05 -b 10G -t 1 -o ./test2/traffic_ws.txt`
    
    Traffic Allocation: In `ns-3.26/traffic_gen/traffic_tenant.py`, run this script to randomly allocate the generated `traffic_ws.txt` traffic to 16 tenants, and set the source port for each flow to `((tenant_id-1)*1000, tenant_id*1000]`. Note that ports of the form `(tenant_id-1)*1000+1` are for UDP flows, and when setting port numbers, make sure no duplicates occur. Some modifications may be needed in the `traffic_tenant.py` part.

4. Run TricoTree
    
    Run the following command in the `ns-3.26` folder:
    
    `nohup ./waf --run dumbbellTopo > ./TricoTree_Log/dumbbell_TricoTree.log 2>&1 &`
    
5. Result Recording
    
    The scheduler records the packet transmission order in the `ns-3.26/TricoTreeTrace.txt` file. Each line contains: flowid, pkt.length, and packet sending time. You can use `ns-3.26/traffic_gen/plot_bandwidth.py` to calculate bandwidth changes over time.
