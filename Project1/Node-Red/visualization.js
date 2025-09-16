// To Dashboard Tables (3 outputs) — read parent/neighbors from analyzer
// Out 1: Topology table (Node|Parent|Neighbors)
// Out 2: Per-node neighbor stats (Node|Avg # Neigh|min|max)  — running over time
// Out 3: General stats (avg diameter|avg depth|min depth|max depth)

'use strict';

// -------- Reset handling --------
const isReset = msg?.topic === 'rpl/reset' || msg?.reset === true || (msg?.payload && msg.payload.reset === true);
if (isReset) {
  flow.set('neighborStats', {});      // clear per-node running averages
  return [{payload:[]},{payload:[]},{payload:[]}];
}

const snap = msg.payload;
if (!snap || !snap.topology || !snap.rpl_tree) {
  return [{payload:[]},{payload:[]},{payload:[]}];
}

// -------- Settings --------
const INCLUDE_ROOT = false;
const AVG_DECIMALS = 2;
const KPI_DECIMALS = 3;

// -------- Helpers --------
const round = (v, d=2) => (v == null ? null : +Number(v).toFixed(d));
const numSort = (a, b) => (+a) - (+b);

// Canonical per-node data from analyzer
const nodesMeta = (snap.topology.nodes || []).map(n => ({
  id: String(n.id),
  is_root: !!n.is_root,
  parent: n.parent != null ? String(n.parent) : null,
  neighbors: Array.isArray(n.neighbors) ? n.neighbors.map(String) : []
}));

const root = String(snap.rpl_tree.root_id || '');
const ids = nodesMeta
  .filter(n => INCLUDE_ROOT ? true : !n.is_root)
  .map(n => n.id)
  .sort(numSort);
const byId = Object.fromEntries(nodesMeta.map(n => [n.id, n]));

// =====================================================
// Output #1: Topology table (Node|Parent|Neighbors)
// =====================================================
const topoRows = ids.map(id => {
  const meta = byId[id] || { neighbors: [], parent: null };
  const neighStr = meta.neighbors.slice().sort(numSort).join(',');
  const parentStr = meta.is_root ? '—' : (meta.parent ?? '');
  return { Node: id, Parent: parentStr, Neighbors: neighStr };
});

// =====================================================
// Output #2: Per-node neighbor stats (running averages)
// =====================================================
const stats = flow.get('neighborStats') || {};  // { [nodeId]: { n, avg, min, max, last } }

for (const id of ids) {
  // ALWAYS compute a NUMBER here
  const neighborsArr = Array.isArray(byId[id]?.neighbors) ? byId[id].neighbors : [];
  const deg = neighborsArr.length;   // <-- count, not the IDs

  let s = stats[id];
  if (!s) s = stats[id] = { n: 0, avg: 0, min: deg, max: deg, last: deg };

  s.n  += 1;
  s.avg += (deg - s.avg) / s.n;      // online mean of counts
  if (deg < s.min) s.min = deg;
  if (deg > s.max) s.max = deg;
  s.last = deg;
}
flow.set('neighborStats', stats);

const perNodeRows = ids.map(id => {
  const s = stats[id] || { avg: 0, min: 0, max: 0 };
  return {
    'Node': id,
    'Avg # Neigh': +Number(s.avg).toFixed(2),
    'min # Neigh': s.min ?? 0,
    'max # Neigh': s.max ?? 0
  };
});

// =====================================================
// Output #3: General network stats (one row)
// =====================================================
const avgDiameter = round(snap.since_boot?.diameter_avg ?? snap.instant?.diameter, KPI_DECIMALS);
const avgDepth    = round(snap.since_boot?.depth_avg_over_time ?? snap.instant?.depth_avg, KPI_DECIMALS);
const minDepth    = snap.instant?.depth_min ?? null;
const maxDepth    = snap.instant?.depth_max ?? null;

const generalRows = [{
  'avg diameter': avgDiameter,
  'avg depth':    avgDepth,
  'min depth':    minDepth,
  'max depth':    maxDepth
}];

node.log(`[UI] topo=${topoRows.length} nodes; avg_diam=${avgDiameter}; avg_depth=${avgDepth}`);
return [
  { payload: topoRows },
  { payload: perNodeRows },
  { payload: generalRows }
];
