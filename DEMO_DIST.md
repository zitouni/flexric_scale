# Run Distributed near-RT RIC Demo


## 1) Deploy of 5G Core Network

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

![5G Core Network Status](https://github.com/zitouni/distributedRIC/blob/main/5GCN-OK.png)


## 2) Run the gNB  

From the path:  
`/distributedRIC/openairinterface5g/cmake_targets/ran_build/build`

Run the following command:

```sh
sudo ./nr-softmodem -O ../../../ci-scripts/CONF_25-33/gnb_oaicore.conf --rfsimulator.serveraddr server --rfsim --sa --gNBs.[0].min_rxtxtime 6
```

---

## 3) Run UE RF SIM  

From the path:  
`/distributedRIC/openairinterface5g/cmake_targets/ran_build/build`

Run the following command:

```sh
sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3619200000 --ssb 516 --rfsim --rfsimulator.serveraddr 127.0.0.1 --sa  -O   ../../../ci-scripts/CONF_25-33/ue_oaicore.conf
```

---

## 4) Create the Dummy Configured IP Interfaces for Near-RT RIC Instances  

From the path:  
`~/distributedRIC/flexric/demo-dist`

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
./nearRT-RIC -c ../../../demo-dist/RIC_200.conf
./nearRT-RIC -c ../../../demo-dist/RIC_201.conf
./nearRT-RIC -c ../../../demo-dist/RIC_202.conf
./nearRT-RIC -c ../../../demo-dist/RIC_203.conf
```

---

## 6) Run the xApps Connected to the Different Near-RT RICs  

### Display xApp Help

To see all available options for the distributed RIC xApp:

```sh
sudo ./dist_ric_xapp --help
```

Expected output:
```
Usage: ./dist_ric_xapp [OPTIONS]

Distributed RIC xApp - Multi-Service Model Monitor

Options:
  -c, --config <file>     Configuration file path (required)
                          Example: ../../../../demo-dist/RIC_200.conf
  -db, --database <file>  Database file name (required)
                          Example: latency_all_sm.db
  -sm, --service-model <type>  Service model type (required)
                          Options: all, gtp, mac, pdcp, rlc
  -h, --help              Show this help message

Example usage:
  sudo ./dist_ric_xapp -c ../../../../demo-dist/RIC_200.conf -db latency_all_sm.db -sm all
  sudo ./dist_ric_xapp -c ../../../../demo-dist/RIC_201.conf -db latency_mac_sm.db -sm mac

Service Models:
  all   - Monitor all service models (GTP, MAC, PDCP, RLC)
  gtp   - Monitor GTP service model only
  mac   - Monitor MAC service model only
  pdcp  - Monitor PDCP service model only
  rlc   - Monitor RLC service model only

Database location: /var/lib/grafana/<database_file>

Author: Rafik ZITOUNI
License: MIT License
```

### Run the First xApp with 4 E2SMs (MAC, PDCP, RLC, GTP) or All E2SMs 

From the path:  
`~/distributedRIC/flexric/build/examples/xApp/dist-ric`

Run:

```sh
sudo ./dist_ric_xapp -c ../../../../demo-dist/RIC_200.conf -db latency_all_sm.db -sm all
```

### Run the Other xApps  

From the same path:

```sh
sudo ./dist_ric_xapp -c ../../../../demo-dist/RIC_201.conf -db latency_mac_sm.db -sm mac
sudo ./dist_ric_xapp -c ../../../../demo-dist/RIC_202.conf -db latency_gtp_sm.db -sm gtp
sudo ./dist_ric_xapp -c ../../../../demo-dist/RIC_203.conf -db latency_rlc_sm.db -sm rlc
```

---

## 7) Grafana Dashboard Visualization

![Grafana Dashboard](https://github.com/zitouni/distributedRIC/blob/main/GrafanaDashboard.png)

---

© 2025 Rafik ZITOUNI