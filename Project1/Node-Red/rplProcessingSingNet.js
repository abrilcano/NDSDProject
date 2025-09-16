// RPL Update & Metrics — Simplified for single network / no dedupe
// Assumptions: single DODAG/OF at a time, every msg has "Node ID", no need to de-dup,
// synthetic root is desired, repeated Seq# harmless.

'use strict';

// ---- Tunables ----
const STALE_MS = 120000;                 // prune nodes idle > 2 min (wall-clock)
const COUNT_ROOT_IN_DEPTH_STATS = false; // include depth(root)=0 in stats?
const ROOT_ID = '1';                     // non-reporting border router
const ROOT_LL_FALLBACK = 'fe80::201:1:1:1';

// ---- Helpers ----
function nodeIdFromIPv6(addr, fallback) {
  if (typeof addr !== 'string') return fallback;
  // Match ::201:1:1:1 or ::203:3:3:3  -> second hextet repeated is node-id
  const m = addr.match(/::([0-9a-f]+):([0-9a-f]+):\2:\2$/i);
  if (m && m[2]) {
    const id = parseInt(m[2], 16);
    if (!Number.isNaN(id)) return String(id);
  }
  // Alternate ::20XX:XX:XX:XX (less common)
  const m2 = addr.match(/::20([0-9a-f]{2})([0-9a-f]{2}):\2:\2:\2$/i);
  if (m2) {
    const id = parseInt(m2[2], 16);
    if (!Number.isNaN(id)) return String(id);
  }
  return fallback;
}

function addEdge(adj, a, b) {
  if (!adj[a]) adj[a] = new Set();
  if (!adj[b]) adj[b] = new Set();
  adj[a].add(b);
  adj[b].add(a);
}

function bfsAllPairs(adj, nodes) {
  let diameter = 0, total = 0, pairs = 0;
  for (const s of nodes) {
    const dist = {}; const q = [s]; let qi = 0;
    dist[s] = 0;
    while (qi < q.length) {
      const u = q[qi++];
      for (const v of (adj[u] || [])) {
        if (dist[v] === undefined) {
          dist[v] = dist[u] + 1;
          q.push(v);
        }
      }
    }
    for (const t of nodes) {
      if (t === s) continue;
      if (dist[t] !== undefined) {
        diameter = Math.max(diameter, dist[t]);
        total += dist[t];
        pairs += 1;
      }
    }
  }
  const avgPathLen = pairs > 0 ? total / pairs : 0;
  return { diameter, avgPathLen };
}

function computeDepths(parents, rootId, nodeList) {
  const depths = {};
  for (const n of nodeList) {
    let cur = n, d = 0;
    const seen = new Set(); let ok = false;
    while (true) {
      if (cur === rootId) { ok = true; break; }
      const p = parents[cur];
      if (!p) { ok = false; break; }
      if (seen.has(cur)) { ok = false; break; }
      seen.add(cur);
      cur = p; d += 1;
      if (d > 10000) { ok = false; break; }
    }
    if (ok) depths[n] = d;
  }
  return depths;
}

function basicStats(arr) {
  if (!arr.length) return { avg: 0, min: null, max: null };
  let sum = 0, min = arr[0], max = arr[0];
  for (const v of arr) {
    sum += v;
    if (v < min) min = v;
    if (v > max) max = v;
  }
  return { avg: sum / arr.length, min, max };
}

function updateRunningAvg(avgObj, sample) {
  avgObj.n = (avgObj.n || 0) + 1;
  const n = avgObj.n;
  avgObj.value = (avgObj.value || 0) + (sample - (avgObj.value || 0)) / n;
}

function deriveRootIpv6FromDodag(dodagId) {
  if (typeof dodagId === 'string' && /::[0-9a-f]+:1:1:1$/i.test(dodagId)) return dodagId;
  return ROOT_LL_FALLBACK;
}

// ---- Single-network state in flow context ----
const store = flow.get('rplStoreSimple') || {
  nodes: {},                        // nodeId -> {parentId, neighbors:Set, ...}
  startedAt: Date.now(),
  metricsAvg: { diameter:{value:0,n:0}, depthAvg:{value:0,n:0}, neighborAvg:{value:0,n:0}, apl:{value:0,n:0} },
  history: []
};

// ---- Ingest ----
const p = msg.payload || {};
const nowMs = Date.now();

// Require Node ID (guaranteed by assumption)
const nodeId = String(p['Node ID']);
if (!nodeId) {
  node.log('[RPL] drop: missing Node ID');
  return null;
}

// Upsert node
const n = store.nodes[nodeId] || (store.nodes[nodeId] = { neighbors: new Set() });

// Update basic fields
n.ipv6     = p['IPv6 Address'] || n.ipv6;
n.of       = p.objective_function || n.of;
n.dag      = p['DODAG ID'] || n.dag;
n.version  = p['DODAG Version'] ?? n.version;
n.rank     = Number(p['RPL Rank'] ?? n.rank);
n.dagRank  = Number(p['RPL DAG Rank'] ?? n.dagRank);
n.isRoot   = Boolean(p.is_root); // usually false if border router is silent
n.lastSeq  = Number(p['Seq #'] ?? n.lastSeq);
n.lastTs   = Number(p['Timestamp'] ?? 0); // sim time (unused for pruning)
n.lastUpdateMs = nowMs;

// Parent (LL IPv6 -> node id)
const prefParentAddr = p['Preferred Parent'];
n.parentId = prefParentAddr ? nodeIdFromIPv6(prefParentAddr, n.parentId) : n.parentId;

// Neighbor set
n.neighbors = new Set();
const neigh = Array.isArray(p.neighbors) ? p.neighbors : [];
for (const entry of neigh) {
  const nid = nodeIdFromIPv6(entry.addr, undefined);
  if (nid) n.neighbors.add(nid);
}

node.log(`[RPL] rx node=${nodeId} seq=${n.lastSeq} of=${n.of||'-'} parent=${n.parentId||'-'} neigh=${n.neighbors.size}`);

// ---- Ensure synthetic root presence ----
if (!store.nodes[ROOT_ID]) {
  store.nodes[ROOT_ID] = {
    neighbors: new Set(),
    isRoot: true,
    synthetic: true,
    ipv6: deriveRootIpv6FromDodag(n.dag),
    dag: n.dag,
    of: n.of,
    lastUpdateMs: nowMs
  };
  node.log(`[RPL] injected synthetic root id=${ROOT_ID} ipv6=${store.nodes[ROOT_ID].ipv6}`);
} else {
  const r = store.nodes[ROOT_ID];
  r.isRoot = true;
  if (r.synthetic) r.lastUpdateMs = nowMs; // never prune synthetic root
  r.dag = r.dag || n.dag;
  r.of  = r.of  || n.of;
}

// ---- Prune stale nodes (except synthetic root) ----
for (const [nid, info] of Object.entries(store.nodes)) {
  if (nid === ROOT_ID && info.synthetic) continue;
  if (nowMs - (info.lastUpdateMs || 0) > STALE_MS) {
    node.log(`[RPL] prune stale node id=${nid}`);
    delete store.nodes[nid];
  }
}

// ---- Build graphs ----
const nodeIds = Object.keys(store.nodes);
const adj = {};     // undirected topology
const parents = {}; // child -> parent
let rootId = null;

for (const id of nodeIds) {
  const info = store.nodes[id];
  for (const nid of (info.neighbors || [])) addEdge(adj, id, nid);
  if (info.parentId && info.parentId !== id) parents[id] = info.parentId;
  if (info.isRoot) rootId = id;
}
if (!rootId && store.nodes[ROOT_ID]) rootId = ROOT_ID;
if (!rootId && nodeIds.length) {
  const children = new Set(Object.keys(parents));
  rootId = nodeIds.find(id => !children.has(id)) || nodeIds[0];
}

// ---- Metrics ----
const depths = computeDepths(parents, rootId, nodeIds);
const depthValsAll = Object.values(depths);
const depthVals = COUNT_ROOT_IN_DEPTH_STATS ? [0, ...depthValsAll] : depthValsAll;
const depthStats = basicStats(depthVals);

const degs = nodeIds.map(id => (adj[id] ? adj[id].size : 0));
const degStats = basicStats(degs);

const { diameter, avgPathLen } = bfsAllPairs(adj, nodeIds);

const undirectedEdges = Math.floor(Object.values(adj).reduce((s, set) => s + set.size, 0) / 2);
const treeEdges = Object.keys(parents).length;

const instant = {
  dag_id: store.nodes[nodeId]?.dag,
  objective_function: store.nodes[nodeId]?.of,
  root_id: rootId,
  node_count: nodeIds.length,
  undirected_edge_count: undirectedEdges,
  tree_edge_count: treeEdges,
  depth_avg: depthStats.avg,
  depth_min: depthStats.min,
  depth_max: depthStats.max,
  diameter,
  avg_path_len: avgPathLen,
  neighbor_avg: degStats.avg,
  neighbor_min: degStats.min,
  neighbor_max: degStats.max,
  computed_at_ms: nowMs
};

// Since-boot running averages
updateRunningAvg(store.metricsAvg.diameter,    instant.diameter);
updateRunningAvg(store.metricsAvg.depthAvg,    instant.depth_avg);
updateRunningAvg(store.metricsAvg.neighborAvg, instant.neighbor_avg);
updateRunningAvg(store.metricsAvg.apl,         instant.avg_path_len);

const sinceBoot = {
  samples: store.metricsAvg.diameter.n || 0,
  diameter_avg: store.metricsAvg.diameter.value || 0,
  depth_avg_over_time: store.metricsAvg.depthAvg.value || 0,
  neighbor_avg_over_time: store.metricsAvg.neighborAvg.value || 0,
  avg_path_len_over_time: store.metricsAvg.apl.value || 0,
  started_at_ms: store.startedAt
};

// ---- Output payload ----
const topoEdges = [];
for (const [a, set] of Object.entries(adj)) {
  for (const b of set) if (String(a) < String(b)) topoEdges.push([a, b]);
}
const treeEdgesList = Object.entries(parents).map(([child, parent]) => [child, parent]);

msg.payload = {
  instant,
  since_boot: sinceBoot,
  topology: {
    nodes: nodeIds.map(id => ({
      id,
      is_root: id === rootId,
      dag_rank: store.nodes[id].dagRank,
      rank: store.nodes[id].rank
    })),
    edges: topoEdges
  },
  rpl_tree: {
    root_id: rootId,
    edges: treeEdgesList
  }
};

// FlowFuse charts helper
msg.metrics_series = {
  time: nowMs,
  diameter:    { instant: instant.diameter,    avg: sinceBoot.diameter_avg },
  depth_avg:   { instant: instant.depth_avg,   avg: sinceBoot.depth_avg_over_time },
  neighbor_avg:{ instant: instant.neighbor_avg,avg: sinceBoot.neighbor_avg_over_time },
  apl:         { instant: instant.avg_path_len,avg: sinceBoot.avg_path_len_over_time }
};

// History buffer
store.history.push({ t: nowMs, instant });
if (store.history.length > 500) store.history.shift();

// Persist
flow.set('rplStoreSimple', store);

node.log(`[RPL] root=${instant.root_id} nodes=${instant.node_count} edges=${undirectedEdges}/${treeEdges} depth_avg=${(instant.depth_avg||0).toFixed(2)} diam=${instant.diameter}`);

msg.summary = {
  nodes: instant.node_count,
  diameter: instant.diameter,
  depth_avg: Number((instant.depth_avg || 0).toFixed(3)),
  neighbor_avg: Number((instant.neighbor_avg || 0).toFixed(3)),
  of: instant.objective_function,
  root: instant.root_id
};

return msg;
