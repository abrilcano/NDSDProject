// To Tables — 3 outputs: [summary table], [per-node table], [avg diameter text]
'use strict';

if (!msg.payload) return [null, null, null];

const snap = msg.payload;
const nodesMeta = snap.topology?.nodes || [];
const edges = snap.topology?.edges || [];
const root = snap.rpl_tree?.root_id;

// Build adjacency from undirected edges
const adj = {};
for (const n of nodesMeta) adj[n.id] = new Set();
for (const [a,b] of edges) {
  if (!adj[a]) adj[a] = new Set();
  if (!adj[b]) adj[b] = new Set();
  adj[a].add(b); adj[b].add(a);
}

// Parent map from directed tree edges (child -> parent)
const parents = {};
for (const [child, parent] of (snap.rpl_tree?.edges || [])) parents[child] = parent;

// Per-node depth (hops to root) by following parent pointers
function depthOf(id) {
  if (id === root) return 0;
  let d = 0, cur = id;
  const seen = new Set();
  while (cur !== root) {
    const p = parents[cur];
    if (!p || seen.has(cur)) return null; // unreachable / loop
    seen.add(cur);
    cur = p;
    d++;
    if (d > 10000) return null;
  }
  return d;
}

// Per-node rows
const idToMeta = Object.fromEntries(nodesMeta.map(n => [n.id, n]));
const perNodeRows = nodesMeta.map(n => {
  const id = n.id;
  const degree = adj[id]?.size ?? 0;
  const depth = depthOf(id);
  const parent = parents[id] || (id === root ? '—' : '');
  const neighList = adj[id] ? Array.from(adj[id]).sort((a,b)=>(+a)-(+b)).join(', ') : '';
  return {
    node: id,
    depth: depth === null ? '—' : depth,
    degree,
    parent,
    neighbors: neighList,
    rank: n.rank ?? '',
    dag_rank: n.dag_rank ?? '',
    is_root: id === root
  };
}).sort((a,b) => (+a.node) - (+b.node));

// Summary rows (depth + neighbor stats)
function round(val, d=3){ return (val==null)?null: +Number(val).toFixed(d); }
const summaryRows = [
  { metric: 'Depth (hops to root)',
    min: snap.instant?.depth_min ?? null,
    avg: round(snap.instant?.depth_avg),
    max: snap.instant?.depth_max ?? null
  },
  { metric: 'Neighbors (degree)',
    min: snap.instant?.neighbor_min ?? null,
    avg: round(snap.instant?.neighbor_avg),
    max: snap.instant?.neighbor_max ?? null
  }
];

// Messages to widgets
const msgSummary = { payload: summaryRows };
const msgPerNode = { payload: perNodeRows };
const msgDiameterText = {
  topic: 'Average network diameter',
  payload: round(snap.since_boot?.diameter_avg ?? snap.instant?.diameter, 3)
};

// Helpful logs
node.log(`[UI] rows: summary=${summaryRows.length}, per-node=${perNodeRows.length}, avg_diam=${msgDiameterText.payload}`);

return [msgSummary, msgPerNode, msgDiameterText];
