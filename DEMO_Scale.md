# Deploy All-in-One on the Server 10.5.25.33

## 1) Deploy the gNB  

From the path:  
`/oai-core_ran_ric_ue/openairinterface5g/cmake_targets(ics_oai)`

Run the following command:

```sh
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF_25-33/gnb_scale_nr_ric.conf --rfsimulator.serveraddr server --rfsim --sa --gNBs.[0].min_rxtxtime 6
```

---

## 2) Deploy UE RF SIM  

From the path:  
`/oai-core_ran_ric_ue/openairinterface5g/cmake_targets(ics_oai)`

Run the following command:

```sh
sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3319320000 --ssb 192 --rfsim --rfsimulator.serveraddr 127.0.0.1 --sa  -O  ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF_25-33/ue_ics_core.conf
```

---

## 3) Create the Dummy Configured IP Interfaces for Near-RT RIC Instances  

From the path:  
`~/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/demo-scale`

Run:

```sh
dummy_ric_ips.sh
```

The IPs are in the range `192.168.130.20x` for 4 interfaces:  
`192.168.130.200, .. 192.168.130.203`

---

## 4) Deploy the 4 Near-RT RIC Instances  

From the path:  
`~/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/build/exam`

Run:

```sh
./nearRT-RIC -c ../../../demo-scale/scaleRIC_200.conf
./nearRT-RIC -c ../../../demo-scale/scaleRIC_201.conf
./nearRT-RIC -c ../../../demo-scale/scaleRIC_202.conf
./nearRT-RIC -c ../../../demo-scale/scaleRIC_203.conf
```

---

## 5) Run the xApps Connected to the Different Near-RT RICs  

### Run the First xApp with 4 E2SMs (MAC, PDCP, RLC, GTP)  

From the path:  
`~/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/build/examples/xApp/ics`

Run:

```sh
sudo ./scale_ics_xapp -c ../../../../demo-scale/scaleRIC_200.conf -db latency_all_sm.db -sm all
```

The SQLite3 database `latency_all_sm.db` will be created and saved at:  
`/var/lib/grafana`

### Run the Other xApps  

From the same path:

```sh
sudo ./scale_ics_xapp -c ../../../../demo-scale/scaleRIC_201.conf -db latency_mac_sm.db -sm all
sudo ./scale_ics_xapp -c ../../../../demo-scale/scaleRIC_202.conf -db latency_rlc_sm.db -sm all
sudo ./scale_ics_xapp -c ../../../../demo-scale/scaleRIC_203.conf -db latency_pdcp_sm.db -sm all
```
# Deploy All-in-One on the Server 10.5.25.33

## 1) Deploy the gNB  

From the path:  
`/oai-core_ran_ric_ue/openairinterface5g/cmake_targets(ics_oai)`

Run the following command:

```sh
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF_25-33/gnb_scale_nr_ric.conf --rfsimulator.serveraddr server --rfsim --sa --gNBs.[0].min_rxtxtime 6
```

---

## 2) Deploy UE RF SIM  

From the path:  
`/oai-core_ran_ric_ue/openairinterface5g/cmake_targets(ics_oai)`

Run the following command:

```sh
sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3319320000 --ssb 192 --rfsim --rfsimulator.serveraddr 127.0.0.1 --sa  -O  ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF_25-33/ue_ics_core.conf
```

---

## 3) Create the Dummy Configured IP Interfaces for Near-RT RIC Instances  

From the path:  
`~/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/demo-scale`

Run:

```sh
dummy_ric_ips.sh
```

The IPs are in the range `192.168.130.20x` for 4 interfaces:  
`192.168.130.200, .. 192.168.130.203`

---

## 4) Deploy the 4 Near-RT RIC Instances  

From the path:  
`~/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/build/exam`

Run:

```sh
./nearRT-RIC -c ../../../demo-scale/scaleRIC_200.conf
./nearRT-RIC -c ../../../demo-scale/scaleRIC_201.conf
./nearRT-RIC -c ../../../demo-scale/scaleRIC_202.conf
./nearRT-RIC -c ../../../demo-scale/scaleRIC_203.conf
```

---

## 5) Run the xApps Connected to the Different Near-RT RICs  

### Run the First xApp with 4 E2SMs (MAC, PDCP, RLC, GTP)  

From the path:  
`~/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/build/examples/xApp/ics`

Run:

```sh
sudo ./scale_ics_xapp -c ../../../../demo-scale/scaleRIC_200.conf -db latency_all_sm.db -sm all
```

The SQLite3 database `latency_all_sm.db` will be created and saved at:  
`/var/lib/grafana`

### Run the Other xApps  

From the same path:

```sh
sudo ./scale_ics_xapp -c ../../../../demo-scale/scaleRIC_201.conf -db latency_mac_sm.db -sm all
sudo ./scale_ics_xapp -c ../../../../demo-scale/scaleRIC_202.conf -db latency_rlc_sm.db -sm all
sudo ./scale_ics_xapp -c ../../../../demo-scale/scaleRIC_203.conf -db latency_pdcp_sm.db -sm all
```

---

## 6) Visualization via Grafana  

The link after accessing the testbed network via VPN:  
[http://10.5.25.33:3000/dashboards](http://10.5.25.33:3000/dashboards)

### Dashboard Name: HiPerRAN Southbound RIC Scalability  

[View the Dashboard](http://10.5.25.33:3000/d/behax1g21u5fke/hiperran-southbound-ric-scalability?orgId=1&from=2025-03-30T15:31:14.000Z&to=2025-03-30T15:53:33.000Z&timezone=browser&refresh=30s)

---

## 6) Visualization via Grafana  

The link after accessing the testbed network via VPN:  
[http://10.5.25.33:3000/dashboards](http://10.5.25.33:3000/dashboards)

### Dashboard Name: HiPerRAN Southbound RIC Scalability  

[View the Dashboard](http://10.5.25.33:3000/d/behax1g21u5fke/hiperran-southbound-ric-scalability?orgId=1&from=2025-03-30T15:31:14.000Z&to=2025-03-30T15:53:33.000Z&timezone=browser&refresh=30s)
