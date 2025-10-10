// To Dashboard Tables (3 outputs) — reads parent/neighbors + per-node stats from analyzer
// Out 1: Topology table (Node|Parent|Neighbors)
// Out 2: Per-node neighbor stats (Node|Avg # Neigh|min|max) — from analyzer's since-boot stats
// Out 3: General stats (avg diameter|avg depth|min depth|max depth)

'use strict';

// -------- Reset handling (also clears analyzer store) --------
const isReset =
  msg?.topic === 'rpl/reset' ||
  msg?.reset === true ||
  (msg?.payload && msg.payload.reset === true);

if (isReset) {
  // Recreate the analyzer's store in the same shape it expects
  const fresh = {
    nodes: {},
    startedAt: Date.now(),
    metricsAvg: {
      diameter:    { value: 0, n: 0 },
      depthAvg:    { value: 0, n: 0 },
      neighborAvg: { value: 0, n: 0 },
      apl:         { value: 0, n: 0 }
    },
    history: [],
    nodeNeighborStats: {}
  };

  // Clear analyzer state (same flow scope)
  flow.set('rplStoreSimple', fresh);

  // If you ever kept per-node viz stats in flow, clear them too
  flow.set('neighborStats', {});  // harmless if unused

  // Clear dashboard tables immediately
  return [
    { payload: [] }, // topology table
    { payload: [] }, // per-node neighbor stats table
    { payload: [] }  // general stats table
  ];
}

// ... rest of your visualization code below ...

const snap = msg.payload;
if (!snap || !snap.topology || !snap.rpl_tree) {
  return [{ payload: [] }, { payload: [] }, { payload: [] }];
}

const summ = snap.rpl_tree.summary || {};

// -------- Settings --------
const INCLUDE_ROOT = false;
const KPI_DECIMALS = 3;

// -------- Helpers --------
const round = (v, d = 2) => (v == null ? null : +Number(v).toFixed(d));
const numSort = (a, b) => (+a) - (+b);

// Canonical per-node data from analyzer
const nodesMeta = (snap.topology.nodes || []).map(n => ({
  id: String(n.id),
  is_root: !!n.is_root,
  parent: n.parent != null ? String(n.parent) : null,
  neighbors: Array.isArray(n.neighbors) ? n.neighbors.map(String) : []
}));

const ids = nodesMeta
  .filter(n => (INCLUDE_ROOT ? true : !n.is_root))
  .map(n => n.id)
  .sort(numSort);

const byId = Object.fromEntries(nodesMeta.map(n => [n.id, n]));

// =====================================================
// Output #1: Topology table (Node|Parent|Neighbors)
// =====================================================
const topoRows = ids.map(id => {
  const meta = byId[id] || { neighbors: [], parent: null, is_root: false };
  const neighStr = meta.neighbors.slice().sort(numSort).join(',');
  const parentStr = meta.is_root ? '—' : (meta.parent ?? '');
  return { Node: id, Parent: parentStr, Neighbors: neighStr };
});

// =====================================================
// Output #2: Per-node neighbor stats (since-boot) — from analyzer
// =====================================================
let perNodeRows = [];
if (Array.isArray(snap.per_node_neighbor_stats)) {
  // Filter/sort to match the nodes we’re displaying (and exclude root if configured)
  const wanted = new Set(ids);
  perNodeRows = snap.per_node_neighbor_stats
    .filter(r => wanted.has(String(r.node)))
    .sort((a, b) => (+a.node) - (+b.node))
    .map(r => ({
      'Node': String(r.node),
      'Avg # Neigh': round(r.avg_neighbors, 2),
      'min # Neigh': r.min_neighbors ?? 0,
      'max # Neigh': r.max_neighbors ?? 0
    }));
} else {
  // Fallback (shouldn't be needed): show current counts only
  perNodeRows = ids.map(id => {
    const deg = (byId[id]?.neighbors?.length ?? 0);
    return {
      'Node': id,
      'Avg # Neigh': deg,
      'min # Neigh': deg,
      'max # Neigh': deg
    };
  });
}

// =====================================================
// Output #3: General network stats
// =====================================================
const avgDiameter = round(snap.since_boot?.diameter ?? snap.instant?.diameter, KPI_DECIMALS);
const avgDepth    = round(snap.since_boot?.depth_avg_over_time ?? snap.instant?.depth_avg, KPI_DECIMALS);
const minDepth    = snap.instant?.depth_min ?? null;
const maxDepth    = snap.instant?.depth_max ?? null;
const objf        = summ.objective_function || 'N/A';

const generalRows = [{
  'Objective Function': objf,
  'Diameter': avgDiameter,
  'avg depth':    avgDepth,
  'min depth':    minDepth,
  'max depth':    maxDepth
}];

return [
  { payload: topoRows },
  { payload: perNodeRows },
  { payload: generalRows }
];
