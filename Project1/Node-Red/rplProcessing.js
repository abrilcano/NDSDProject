// 767719ef4fa9858f
// RPL Update & Metrics (with synthetic root + improved de-dup + logging)

  'use strict';

  // ---- Tunables ----
  const STALE_MS = 120000;                 // drop nodes that haven't updated in this many ms (2 min)
  const COUNT_ROOT_IN_DEPTH_STATS = false; // depth(root)=0 included in avg/min/max?
  const ROOT_ID = '1';                     // border router ID (non-reporting)
  const ROOT_LL_FALLBACK = 'fe80::201:1:1:1';
  const KEY = (p) => `${p["DODAG ID"]}|${p.objective_function || "UNKNOWN"}`;

  // Helpers to parse COOJA-like IPv6 patterns like ::203:3:3:3 => node "3", ::201:1:1:1 => node "1"
  function nodeIdFromIPv6(addr, fallback) {
    if (typeof addr !== 'string') return fallback;
    // Match e.g. ::201:1:1:1 or ::203:3:3:3
    const m = addr.match(/::([0-9a-f]+):([0-9a-f]+):\2:\2$/i);
    if (m && m[2]) {
      const id = parseInt(m[2], 16);
      if (!Number.isNaN(id)) return String(id);
    }
    // Also try ::20XX:XX:XX:XX (some Contiki configs)
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
    let diameter = 0;
    let total = 0, pairs = 0;
    for (const s of nodes) {
      const dist = {};
      const q = [s];
      dist[s] = 0;
      while (q.length) {
        const u = q.shift();
        const neigh = Array.from(adj[u] || []);
        for (const v of neigh) {
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
      const seen = new Set();
      let ok = false;
      while (true) {
        if (cur === rootId) { ok = true; break; }
        const p = parents[cur];
        if (!p) { ok = false; break; }
        if (seen.has(cur)) { ok = false; break; }
        seen.add(cur);
        cur = p;
        d += 1;
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

  function buildFp(parentId, neighborSet) {
    return JSON.stringify({
      parentId: parentId || null,
      neighbors: Array.from(neighborSet || []).sort()
    });
  }

  function deriveRootIpv6FromDodag(dodagId) {
    // Best-effort: if DODAG looks like fd00::201:1:1:1, use it; else fallback to LL
    if (typeof dodagId === 'string' && /::[0-9a-f]+:1:1:1$/i.test(dodagId)) return dodagId;
    return ROOT_LL_FALLBACK;
  }

  // ---- State ----
  const store = flow.get('rplStore') || { byKey: {}, startedAt: Date.now() };
  
  function getBucket(key) {
    if (!store.byKey[key]) {
      store.byKey[key] = {
        nodes: {}, // nodeId -> { ... }
        history: [],
        metricsAvg: { diameter: {value:0,n:0}, depthAvg: {value:0,n:0}, neighborAvg: {value:0,n:0}, apl: {value:0,n:0} },
      };
    }
    return store.byKey[key];
  }

  // ---- Ingest message ----
  const p = msg.payload || {};
  const key = KEY(p);
  const bucket = getBucket(key);

  const nowMs = Date.now();
  const nodeId = String(p["Node ID"] ?? nodeIdFromIPv6(p["IPv6 Address"], undefined));
  if (!nodeId) { node.log(`[RPL] drop: cannot infer node id from payload`); return null; }

  const seq = Number(p["Seq #"] ?? -1);
  const existing = bucket.nodes[nodeId] || (bucket.nodes[nodeId] = { neighbors: new Set() });

  // Minimal fields first (safe to set always)
  existing.ipv6 = p["IPv6 Address"] || existing.ipv6;
  existing.of   = p.objective_function || existing.of;
  existing.dag  = p["DODAG ID"] || existing.dag;
  existing.version = p["DODAG Version"] ?? existing.version;
  existing.rank    = Number(p["RPL Rank"] ?? existing.rank);
  existing.dagRank = Number(p["RPL DAG Rank"] ?? existing.dagRank);
  existing.isRoot  = Boolean(p.is_root); // might be false for all (root is silent)

  // Compute **proposed** parent + neighbours from this message (for fingerprinting)
  const proposedParentAddr = p["Preferred Parent"];
  const proposedParentId = proposedParentAddr ? nodeIdFromIPv6(proposedParentAddr, existing.parentId) : existing.parentId;

  const proposedNeighbors = new Set();
  const neigh = Array.isArray(p.neighbors) ? p.neighbors : [];
  for (const n of neigh) {
    const nid = nodeIdFromIPv6(n.addr, undefined);
    if (nid) proposedNeighbors.add(nid);
  }

  const newFp = buildFp(proposedParentId, proposedNeighbors);

  // Duplicate within the same round? (same Seq # and same effective content)
  if (existing.lastSeq === seq && existing.lastFp === newFp) {
    node.log(`[RPL] duplicate unchanged -> drop (node=${nodeId} seq=${seq})`);
    return null;
  }

  // Accept update
  existing.parentId = proposedParentId;
  existing.neighbors = proposedNeighbors;
  existing.lastSeq = seq;
  existing.lastFp = newFp;
  existing.lastTs = Number(p["Timestamp"] ?? 0); // sim time
  existing.lastUpdateMs = nowMs;

  node.log(`[RPL] rx node=${nodeId} seq=${seq} of=${existing.of} parent=${existing.parentId || '-'} neigh=${existing.neighbors.size}`);

  // ---- Ensure synthetic root presence (non-reporting border router) ----
  if (!bucket.nodes[ROOT_ID]) {
    bucket.nodes[ROOT_ID] = {
      neighbors: new Set(),
      isRoot: true,
      synthetic: true,
      ipv6: deriveRootIpv6FromDodag(existing.dag),
      dag: existing.dag,
      of: existing.of,
      lastUpdateMs: nowMs
    };
    node.log(`[RPL] injected synthetic root id=${ROOT_ID} ipv6=${bucket.nodes[ROOT_ID].ipv6}`);
  } else {
    // Refresh synthetic root timestamp and flags
    const r = bucket.nodes[ROOT_ID];
    if (r.synthetic) r.lastUpdateMs = nowMs;
    r.isRoot = true;
    r.dag = r.dag || existing.dag;
    r.of = r.of || existing.of;
  }

  // ---- Prune stale (but never the synthetic root) ----
  for (const [nid, info] of Object.entries(bucket.nodes)) {
    if (nid === ROOT_ID && info.synthetic) continue;
    if (nowMs - (info.lastUpdateMs || 0) > STALE_MS) {
      node.log(`[RPL] prune stale node id=${nid}`);
      delete bucket.nodes[nid];
    }
  }

  // ---- Build current graphs ----
  const nodeIds = Object.keys(bucket.nodes);
  const adj = {};       // wireless topology (undirected)
  const parents = {};   // RPL tree (child -> parent)
  let rootId = null;

  for (const id of nodeIds) {
    const info = bucket.nodes[id];
    // Undirected edges from reported neighbor sets
    for (const nid of (info.neighbors || [])) addEdge(adj, id, nid);
    // Parent pointers
    if (info.parentId && info.parentId !== id) parents[id] = info.parentId;
    if (info.isRoot) rootId = id;
  }

  // If no explicit root flagged, force ROOT_ID as root (because border router is silent)
  if (!rootId && bucket.nodes[ROOT_ID]) rootId = ROOT_ID;
  // If somehow still no root, pick a node with no parent
  if (!rootId && nodeIds.length) {
    const children = new Set(Object.keys(parents));
    rootId = nodeIds.find(id => !children.has(id)) || nodeIds[0];
  }

  // ---- Instant metrics ----
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
    dag_id: bucket.nodes[nodeId]?.dag,
    objective_function: bucket.nodes[nodeId]?.of,
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

  // ---- Since-boot running averages ----
  const bucketAvg = bucket.metricsAvg;
  updateRunningAvg(bucketAvg.diameter, instant.diameter);
  updateRunningAvg(bucketAvg.depthAvg, instant.depth_avg);
  updateRunningAvg(bucketAvg.neighborAvg, instant.neighbor_avg);
  updateRunningAvg(bucketAvg.apl, instant.avg_path_len);

  const sinceBoot = {
    samples: bucketAvg.diameter.n || 0,
    diameter_avg: bucketAvg.diameter.value || 0,
    depth_avg_over_time: bucketAvg.depthAvg.value || 0,
    neighbor_avg_over_time: bucketAvg.neighborAvg.value || 0,
    avg_path_len_over_time: bucketAvg.apl.value || 0,
    started_at_ms: store.startedAt
  };

  // ---- Prepare output payload ----
  const topoEdges = [];
  for (const [a, set] of Object.entries(adj)) {
    for (const b of set) if (String(a) < String(b)) topoEdges.push([a, b]);
  }
  const treeEdgesList = Object.entries(parents).map(([child, parent]) => [child, parent]);

  msg.payload = {
    key,
    instant,
    since_boot: sinceBoot,
    topology: {
      nodes: nodeIds.map(id => ({
        id,
        is_root: id === rootId,
        dag_rank: bucket.nodes[id].dagRank,
        rank: bucket.nodes[id].rank
      })),
      edges: topoEdges
    },
    rpl_tree: {
      root_id: rootId,
      edges: treeEdgesList
    }
  };

  // For FlowFuse charts (group by msg.topic)
  msg.metrics_series = {
    time: nowMs,
    diameter:    { instant: instant.diameter,    avg: sinceBoot.diameter_avg },
    depth_avg:   { instant: instant.depth_avg,   avg: sinceBoot.depth_avg_over_time },
    neighbor_avg:{ instant: instant.neighbor_avg,avg: sinceBoot.neighbor_avg_over_time },
    apl:         { instant: instant.avg_path_len,avg: sinceBoot.avg_path_len_over_time }
  };

  // Short history buffer
  bucket.history.push({ t: nowMs, instant });
  if (bucket.history.length > 500) bucket.history.shift();

  // Save state
  store.byKey[key] = bucket;
  flow.set('rplStore', store);

  node.log(`[RPL] key=${key} root=${instant.root_id} nodes=${instant.node_count} edges=${undirectedEdges}/${treeEdges} depth_avg=${(instant.depth_avg||0).toFixed(2)} diam=${instant.diameter}`);

  // Lightweight summary
  msg.summary = {
    key,
    nodes: instant.node_count,
    diameter: instant.diameter,
    depth_avg: Number((instant.depth_avg || 0).toFixed(3)),
    neighbor_avg: Number((instant.neighbor_avg || 0).toFixed(3)),
    of: instant.objective_function,
    root: instant.root_id
  };

  return msg;

