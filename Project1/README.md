Project #1 · RPL Routing Monitor
================================

IoT lab that instruments a Contiki-NG network to observe how the RPL routing tree evolves when switching between OF0 and MRHOF. Sensor motes publish periodic routing statistics over MQTT, while a Node-RED backend reconstructs both the full wireless topology and the active RPL tree, computing depth/diameter/neighbor metrics over time.

Layout
------
- `mqtt-client/` – Contiki-NG application extended to dump RPL statistics and publish them via MQTT.
- `Node-Red/` – Dashboard flow (`flows.json`) plus helper scripts for processing and visualisation (`Processing.js`, `Visualization.js`).
- `sample.json` – Example payload emitted by a mote, useful for backend testing without the simulator.
- `mqtt-client/project-conf.h` – Compile-time configuration (objective function toggles, broker IP/port, logging).

Prerequisites
-------------
- Contiki-NG toolchain (tested with commit compatible with the bundled `mqtt-client`).
- Cooja simulator or target hardware for the selected platform (`PLATFORMS_ONLY` in `mqtt-client/Makefile`).
- MQTT broker reachable from both the motes (or simulation host) and the Node-RED backend.
- Node-RED 3.x with the FlowFuse dashboard (`@flowfuse/node-red-dashboard`) for visualisation.

Configuring the Firmware
------------------------
1. **Objective Function** – Toggle RPL behaviour in `mqtt-client/project-conf.h`:
   - To test MRHOF (expected default), leave `#define RPL_CONF_WITH_MRHOF 0` commented and keep `RPL_CONF_OF_OCP RPL_OCP_MRHOF`.
   - To switch to OF0, set `RPL_CONF_WITH_OF0 1`, enable `RPL_CONF_OF_OCP RPL_OCP_OF0`, and disable MRHOF macros.
   Build twice (once per flavour) to compare metrics.
2. **MQTT endpoint** – Update `MQTT_CLIENT_CONF_BROKER_IP_ADDR` and `MQTT_BROKER_PORT` to the IPv6/port of your broker. Use the NAT64 address when running Cooja with the SLIP border router.
4. **Logging** – Uncomment `LOG_CONF_LEVEL_*` macros to inspect Contiki logs while tuning the network.

Building & Running (Cooja)
-------------------------
1. Clone or symlink this project inside `contiki-ng/examples/` so the relative include paths resolve.
2. Launch Cooja and open a new simulation, create a `contiki-ng/examples/rpl-border-router.c` mote as the SLIP gateway, and add several `mqtt-client.c` motes (configured via `project-conf.h`).
3. Enable the SLIP gateway's IPv6 address in the simulation settings (e.g., `fd00::1`), then start the simulation. Motes should join the RPL network and begin publishing stats.
5. Start the MQTT broker (e.g., `mosquitto -v -p 1883`) and verify that frames appear on topic `rplstats/json`.

Physical Deployment Notes
-------------------------
- Supported hardware includes CC2538DK, CC26x0/CC13x0, OpenMote, Zoul, and native Linux (`TARGET=native`).
- Use `make TARGET=native mqtt-client.native` to build a local process that publishes the same statistics, handy for rapid backend iteration.
-	Remember to update macro `MQTT_CLIENT_CONF_BROKER_IP_ADDR` to an IPv4-mapped IPv6 or global IPv6 depending on your network.

Node-RED Backend
----------------
1. Import `Node-Red/flows.json` into your Node-RED workspace.
2. Edit the **MQTT RPL Stats** broker config node to match your broker IP (`192.168.80.27` and port `1883` in the sample flow).
3. Deploy the flow. It will:
   - Subscribe to `rplstats/json` and parse each JSON message.
   - Maintain an up-to-date view of the wireless graph, mutual neighbour links, and the RPL parent/child tree.
   - Compute per-second statistics: average/min/max depth, network diameter (hop count and ETX-weighted), neighbour degree distribution, and rolling averages since boot.
   - Render three dashboard tables (topology, per-node neighbour stats, network KPIs) and expose a `Reset` button to clear state between experiments.
4. Optional: use `Processing.js` and `Visualization.js` for custom charts if integrating outside the dashboard.
