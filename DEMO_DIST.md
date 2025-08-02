# Deploy Distributed RIC System


## 1) Deploy of 5G Core

Deploy the 5G Core Network following the [Home Deployment Guide](https://github.com/zitouni/oai-cn5g-fed/blob/master/docs/DEPLOY_HOME.md):

```bash
cd oai-cn5g-fed/docker-compose
python3 core-network.py --type start-basic --scenario 1
```

Verify all 5G Core modules are running with their status and IPs:

```bash
for container in $(docker ps --format "{{.Names}}" | grep -E "(amf|smf|upf|nrf|ausf|udm|udr|pcf|nssf)"); do
  echo "$container: $(docker inspect $container | jq -r '.[0].State.Status') - IP: $(docker inspect $container | jq -r '.[0].NetworkSettings.IPAddress')"
done
```

Expected result showing all modules running:

![5G Core Network Status](../../../../5GCN-OK.png)


## 2) Deploy the gNB  

From the path:  
`/distributedRIC/openairinterface5g/cmake_targets/ran_build/build`

Run the following command:

```sh
sudo ./nr-softmodem -O ../../../ci-scripts/CONF_25-33/gnb_oaicore.conf --rfsimulator.serveraddr server --rfsim --sa --gNBs.[0].min_rxtxtime 6
```

---

## 3) Deploy UE RF SIM  

From the path:  
`/distributedRIC/openairinterface5g/cmake_targets/ran_build/build`

Run the following command:

```sh
sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3319320000 --ssb 192 --rfsim --rfsimulator.serveraddr 127.0.0.1 --sa  -O  ../ci-scripts/CONF_25-33/ue_oaicore.conf
```

---

## 4) Create the Dummy Configured IP Interfaces for Near-RT RIC Instances  

From the path:  
`~/distributedRIC/flexric/demo-scale`

Run:

```sh
dummy_ric_ips.sh
```

The IPs are in the range `192.168.130.20x` for 4 interfaces:  
`192.168.130.200, .. 192.168.130.203`

---

## 5) Deploy the 4 Near-RT RIC Instances  

From the path:  
`~/distributedRIC/flexric/build/examples/ric`

Run:

```sh
./nearRT-RIC -c ../../../demo-scale/scaleRIC_200.conf
./nearRT-RIC -c ../../../demo-scale/scaleRIC_201.conf
./nearRT-RIC -c ../../../demo-scale/scaleRIC_202.conf
./nearRT-RIC -c ../../../demo-scale/scaleRIC_203.conf
```

---

## 6) Run the xApps Connected to the Different Near-RT RICs  

### Run the First xApp with 4 E2SMs (MAC, PDCP, RLC, GTP)  

From the path:  
`~/distributedRIC/flexric/build/examples/xApp/c/monitor`

Run:

```sh
sudo ./xapp_gtp_mac_rlc_pdcp_moni -c ../../../../demo-scale/scaleRIC_200.conf
```

### Run the Other xApps  

From the same path:

```sh
sudo ./xapp_gtp_mac_rlc_pdcp_moni -c ../../../../demo-scale/scaleRIC_201.conf
sudo ./xapp_gtp_mac_rlc_pdcp_moni -c ../../../../demo-scale/scaleRIC_202.conf
sudo ./xapp_gtp_mac_rlc_pdcp_moni -c ../../../../demo-scale/scaleRIC_203.conf
```

---

## 7) Visualization via Grafana  

The link after accessing the testbed network via VPN:  
[http://10.5.25.33:3000/dashboards](http://10.5.25.33:3000/dashboards)

### Dashboard Name: HiPerRAN Southbound RIC Scalability  

[View the Dashboard](http://10.5.25.33:3000/d/behax1g21u5fke/hiperran-southbound-ric-scalability?orgId=1&from=2025-03-30T15:31:14.000Z&to=2025-03-30T15:53:33.000Z&timezone=browser&refresh=30s)