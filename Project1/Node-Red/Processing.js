// RPL Update & Metrics — Simplified for single network / no dedupe
// Assumptions: single DODAG/OF at a time, every msg has "Node ID", no need to de-dup,

'use strict';

// ---- Tunables ----
const STALE_MS = 120000;                 
const COUNT_ROOT_IN_DEPTH_STATS = false; 
const ROOT_ID = '1';                     
const ROOT_LL_FALLBACK = 'fe80::201:1:1:1';

// ---- Helpers ----


function getETX(a, b) {
  const ia = store.nodes[a], ib = store.nodes[b];
  const ab = ia?.neighCosts?.[b];
  const ba = ib?.neighCosts?.[a];
  if (Number.isFinite(ab) && Number.isFinite(ba)) return (ab + ba) / 2; // symmetric weight
  if (Number.isFinite(ab)) return ab;
  if (Number.isFinite(ba)) return ba;
  return 1; // fallback
}

function dijkstraAllPairs(adjW, nodes) {
  let diamW = 0, total = 0, pairs = 0;
  for (const s of nodes) {
    // init
    const dist = new Map(nodes.map(n => [n, Infinity]));
    dist.set(s, 0);
    const pq = [[0, s]]; // tiny array-based PQ (ok for small N)

    while (pq.length) {
      // extract-min
      let mi = 0;
      for (let i = 1; i < pq.length; i++) if (pq[i][0] < pq[mi][0]) mi = i;
      const [du, u] = pq.splice(mi, 1)[0];
      if (du > dist.get(u)) continue;

      for (const [v, w] of (adjW[u] || [])) {
        const nd = du + w;
        if (nd < dist.get(v)) {
          dist.set(v, nd);
          pq.push([nd, v]);
        }
      }
    }

    for (const t of nodes) {
      if (t === s) continue;
      const d = dist.get(t);
      if (d !== undefined && d < Infinity) {
        if (d > diamW) diamW = d;
        total += d; pairs += 1;
      }
    }
  }
  return { diameterW: diamW, avgPathLenW: (pairs ? total / pairs : 0) };
}

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

function addWeightedEdge(adjW, a, b, w) {
  if (!adjW[a]) adjW[a] = [];
  if (!adjW[b]) adjW[b] = [];
  adjW[a].push([b, w]);
  adjW[b].push([a, w]);
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
  history: [],
  nodeNeighborStats: {} // {[nodeId]: {n, avg, min, max}}
};

// ---- Ingest ----
const p = msg.payload || {};
const nowMs = Date.now();

// Keep current round id (Seq #) so UI can gate updates once per round if desired
store.currentSeq = Number(p['Seq #'] ?? store.currentSeq ?? 0);

// Require Node ID (guaranteed by assumption)
const nodeId = String(p['Node ID']);
if (!nodeId) {
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

// Parent (LL IPv6 -> node id) — normalized to string
const prefParentAddr = p['Preferred Parent'];
n.parentId = prefParentAddr ? String(nodeIdFromIPv6(prefParentAddr, n.parentId)) : n.parentId;

// Neighbor se
n.neighbors = new Set();
n.neighCosts = {};
const neigh = Array.isArray(p.neighbors) ? p.neighbors : [];
for (const entry of neigh) {
  const nid = nodeIdFromIPv6(entry.addr, undefined);
  if (nid != null) {
    const idStr = String(nid);
    n.neighbors.add(idStr);
    const raw = Number(entry.etx ?? entry.rpl_link_metric ?? entry.link_metric_to_neighbor);
    let cost = 1;
    if (Number.isFinite(raw) && raw > 0) cost = raw / 128;
    n.neighCosts[idStr] = Math.max(1e-6, cost);
  }
}

// --- Per-node neighbor stats ---
const seq = n.lastSeq;
const degNow = n.neighbors.size;
if (n._lastSeqCounted !== seq) {
  let s = store.nodeNeighborStats[nodeId];
  if (!s) s = store.nodeNeighborStats[nodeId] = { n: 0, avg: 0, min: degNow, max: degNow };
  s.n += 1;
  // s.avg += (degNow - s.avg) / s.n;
  if (degNow < s.min) s.min = degNow;
  if (degNow > s.max) s.max = degNow;
  s.avg = s.min + (s.max - s.min) / 2; // rough avg
  n._lastSeqCounted = seq;
}

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

// ---- Build graphs (mutual & fresh links) ----
const nodeIds = Object.keys(store.nodes);
const adjW = {};
const adj = {};     // undirected topology
const parents = {}; // child -> parent
let rootId = null;


// Settings for freshness 
const LINK_FRESH_MS = 3000;     // consider a node's neighbor set fresh for 3s

function isFresh(id) {
  const info = store.nodes[id];
  return info && (nowMs - (info.lastUpdateMs || 0) <= LINK_FRESH_MS);
}

function aHasB(a, b) {
  const ia = store.nodes[a];
  return !!(ia && ia.neighbors && ia.neighbors.has(String(b)));
}

// Undirected neighbor edges:
for (const a of nodeIds) {
  const infoA = store.nodes[a];
  if (!adj[a]) adj[a] = new Set();

  for (const b of (infoA.neighbors || [])) {
    const A = String(a), B = String(b);
    const isRootEdge = (B === ROOT_ID);
    const freshEnough = isRootEdge ? isFresh(A) : (isFresh(A) && isFresh(B));
    const mutual = isRootEdge ? aHasB(A, B) : (aHasB(A, B) && aHasB(B, A));
    if (freshEnough && mutual) {
      addEdge(adj, A, B);
      const w = getETX(A, B);
      addWeightedEdge(adjW, A, B, w);
    }
  }
  // Parent pointers for the RPL tree
  if (infoA.parentId && infoA.parentId !== a) parents[String(a)] = String(infoA.parentId);
  if (infoA.isRoot) rootId = String(a);
}

// Prefer explicit root; else synthetic root; else pick a node with no parent
if (!rootId && store.nodes[ROOT_ID]) rootId = ROOT_ID;
if (!rootId && nodeIds.length) {
  const children = new Set(Object.keys(parents));
  rootId = nodeIds.find(id => !children.has(String(id))) || nodeIds[0];
}

// ---- Metrics ----
const depths = computeDepths(parents, rootId, nodeIds); // map: id -> depth
const depthValsAll = Object.values(depths); // values only
const depthVals = COUNT_ROOT_IN_DEPTH_STATS ? [0, ...depthValsAll] : depthValsAll; // values with root
const depthStats = basicStats(depthVals);

// Snapshot neighbor stats by reported counts 
const reportedDegs = nodeIds
  .filter(id => id !== ROOT_ID)
  .map(id => (store.nodes[id]?.neighbors?.size || 0)); // Neighbor counts
const neighborStats = basicStats(reportedDegs);

// Diameter/APL computed on the mutual/fresh undirected graph
const { diameter, avgPathLen } = bfsAllPairs(adj, nodeIds);
const { diameterW, avgPathLenW } = dijkstraAllPairs(adjW, nodeIds);
// Compute longest shortest path and APL on the tree
const treeAdj = {};
for (const [child, parent] of Object.entries(parents)) {
  addEdge(treeAdj, String(child), String(parent));
}
const treeNodes = Object.keys(treeAdj);

// Longest shortest path + APL on the tree
const { diameter: treeDiameter, avgPathLen: treeAPL } = bfsAllPairs(treeAdj, treeNodes);

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
  diameter: treeDiameter,
  avg_path_len: treeAPL,           
  neighbor_avg: neighborStats.avg,
  neighbor_min: neighborStats.min,
  neighbor_max: neighborStats.max,
  round_seq: store.currentSeq,       
  computed_at_ms: nowMs
};

// Since-boot running averages
updateRunningAvg(store.metricsAvg.diameter,    instant.diameter);
updateRunningAvg(store.metricsAvg.depthAvg,    instant.depth_avg);
updateRunningAvg(store.metricsAvg.neighborAvg, instant.neighbor_avg);
updateRunningAvg(store.metricsAvg.apl,         instant.avg_path_len);

const sinceBoot = {
  samples: store.metricsAvg.diameter.n || 0,
  diameter: treeDiameter,
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

// ---- Per-node neighbor stats output table ----
const perNodeNeighborStats = Object.keys(store.nodes)
  .filter(id => id !== ROOT_ID) // usually exclude synthetic root
  .sort((a,b)=> (+a)-(+b))
  .map(id => {
    const s = store.nodeNeighborStats[id] || { n:0, avg:0, min:0, max:0 };
    return {
      node: id,
      samples: s.n,
      avg_neighbors: +Number(s.avg).toFixed(2),
      min_neighbors: s.min,
      max_neighbors: s.max
    };
  });

msg.payload = {
  instant,
  since_boot: sinceBoot,
  topology: {
    nodes: nodeIds.map(id => {
      const info = store.nodes[id];
      return {
        id: String(id),
        is_root: id === rootId,
        dag_rank: info.dagRank,
        rank: info.rank,
        parent: info.parentId ? String(info.parentId) : null,
        neighbors: Array.from(info.neighbors || []).map(String).sort((a,b)=>(+a)-(+b)),
        neighbor_count: (info.neighbors ? info.neighbors.size : 0) 
      };
    }),
    edges: topoEdges
  },
  rpl_tree: {
    root_id: rootId,
    edges: treeEdgesList
  },
  per_node_neighbor_stats: perNodeNeighborStats
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

msg.summary = {
  nodes: instant.node_count,
  diameter: instant.diameter,
  depth_avg: Number((instant.depth_avg || 0).toFixed(3)),
  neighbor_avg: Number((instant.neighbor_avg || 0).toFixed(3)),
  of: instant.objective_function,
  root: instant.root_id
};

return msg;
