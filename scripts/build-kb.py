#!/usr/bin/env python3

import argparse
import csv
import glob
import os
import statistics
import sys
import time


def log(msg):
    sys.stderr.write(msg + "\n")


def read_meta(path):
    meta = {}
    with open(path) as handle:
        for line in handle:
            line = line.strip()
            if not line or "=" not in line:
                continue
            key, value = line.split("=", 1)
            meta[key.strip()] = value.strip()
    return meta


def read_nodes_table(path):
    nodes = {}
    with open(path, newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            node_id = (row.get("node_id") or "").strip()
            if not node_id:
                continue
            power_log = (row.get("power_log") or "").strip()
            idle_raw = (row.get("idle_power_w") or "0").strip()
            try:
                idle = float(idle_raw)
            except ValueError:
                idle = 0.0
            nodes[node_id] = {
                "power_log": power_log,
                "idle_power_w": idle,
                "samples": load_power_log(power_log) if power_log else [],
            }
    return nodes


def load_power_log(path):
    samples = []
    if not path or not os.path.isfile(path):
        log("WARN: power log not found: %s" % path)
        return samples
    with open(path, newline="") as handle:
        for raw in handle:
            raw = raw.strip()
            if not raw or raw.startswith("timestamp"):
                continue
            parts = raw.split(",")
            if len(parts) < 2:
                continue
            try:
                ts = int(float(parts[0]))
                uw = float(parts[1])
            except ValueError:
                continue
            samples.append((ts, uw / 1_000_000.0))
    samples.sort(key=lambda item: item[0])
    return samples


def interpolate(samples, t):
    if not samples:
        return None
    if t <= samples[0][0]:
        return samples[0][1]
    if t >= samples[-1][0]:
        return samples[-1][1]
    lo, hi = 0, len(samples) - 1
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if samples[mid][0] <= t:
            lo = mid
        else:
            hi = mid
    t0, p0 = samples[lo]
    t1, p1 = samples[hi]
    if t1 == t0:
        return p0
    frac = (t - t0) / (t1 - t0)
    return p0 + frac * (p1 - p0)


def integrate_power_j(samples, start_ms, end_ms):
    if not samples or end_ms <= start_ms:
        return None
    points = [(start_ms, interpolate(samples, start_ms))]
    for ts, p in samples:
        if start_ms < ts < end_ms:
            points.append((ts, p))
    points.append((end_ms, interpolate(samples, end_ms)))
    if any(p is None for _, p in points):
        return None
    energy = 0.0
    for (t0, p0), (t1, p1) in zip(points, points[1:]):
        dt_s = (t1 - t0) / 1000.0
        energy += 0.5 * (p0 + p1) * dt_s
    return energy


def node_window_energy(node, start_ms, end_ms):
    gross = integrate_power_j(node["samples"], start_ms, end_ms)
    if gross is None:
        return None
    duration_s = (end_ms - start_ms) / 1000.0
    dynamic = gross - node["idle_power_w"] * duration_s
    return max(0.0, dynamic)


def parse_accuracy_map(values):
    acc = {}
    for item in values or []:
        if "=" not in item:
            continue
        key, val = item.split("=", 1)
        try:
            acc[key.strip()] = float(val)
        except ValueError:
            pass
    return acc


def resolve_server_node(meta, requests):
    server = (meta.get("server_node_id") or "").strip()
    if server:
        return server, None
    node_ids = sorted({(r.get("node_id") or "").strip() for r in requests if (r.get("node_id") or "").strip()})
    if len(node_ids) == 1:
        return node_ids[0], None
    if not node_ids:
        return "", "no server node_id in meta.env or requests.csv"
    return "", "multiple server nodes %s; KB cells must target one server (direct mode)" % node_ids


def collect_cell(cell_dir, nodes, accuracy_map):
    meta_path = os.path.join(cell_dir, "meta.env")
    req_path = os.path.join(cell_dir, "requests.csv")
    win_path = os.path.join(cell_dir, "windows.csv")
    for required in (meta_path, req_path, win_path):
        if not os.path.isfile(required):
            log("WARN: skipping %s (missing %s)" % (cell_dir, os.path.basename(required)))
            return None

    meta = read_meta(meta_path)
    with open(req_path, newline="") as handle:
        requests = list(csv.DictReader(handle))
    with open(win_path, newline="") as handle:
        windows = list(csv.DictReader(handle))

    server_node, err = resolve_server_node(meta, requests)
    if err:
        log("WARN: skipping %s (%s)" % (cell_dir, err))
        return None
    client_node = (meta.get("client_node_id") or "").strip()

    concurrency = int(meta.get("concurrency") or 1)
    num_threads = int(meta.get("num_threads") or 0)
    backend = meta.get("routing_backend") or meta.get("backend") or ""
    if backend == "cheetah":
        backend_token = "cheetah"
    elif backend in ("sci-he", "SCI_HE"):
        backend_token = "sci-he"
    else:
        backend_token = backend
    network = meta.get("network") or ""

    energy_samples = []
    missing_nodes = set()
    for win in windows:
        try:
            start_ms = int(win["start_ms"])
            end_ms = int(win["end_ms"])
        except (KeyError, ValueError):
            continue
        if end_ms <= start_ms:
            continue
        total = 0.0
        ok = True
        for role_node in (client_node, server_node):
            if not role_node:
                continue
            node = nodes.get(role_node)
            if node is None:
                missing_nodes.add(role_node)
                ok = False
                break
            e = node_window_energy(node, start_ms, end_ms)
            if e is None:
                missing_nodes.add(role_node)
                ok = False
                break
            total += e
        if ok:
            energy_samples.append(total / max(1, concurrency))

    if missing_nodes:
        log("WARN: %s missing power data for nodes: %s" % (cell_dir, sorted(missing_nodes)))

    latencies = []
    success_latencies = []
    success = 0
    total_requests = 0
    for r in requests:
        total_requests += 1
        try:
            latency = float(r["latency_ms"])
        except (KeyError, ValueError):
            latency = None
        is_success = (r.get("success") or "0").strip() == "1"
        if is_success:
            success += 1
        if latency is not None:
            latencies.append(latency)
            if is_success:
                success_latencies.append(latency)

    return {
        "key": (server_node, backend_token, network, num_threads, concurrency),
        "node_role": "server",
        "accuracy": accuracy_map.get(network, -1.0),
        "energy_samples": energy_samples,
        "latencies": success_latencies if success_latencies else latencies,
        "success": success,
        "requests": total_requests,
        "cell_dir": cell_dir,
        "had_energy": bool(energy_samples),
    }


def mean_std(values):
    if not values:
        return 0.0, 0.0
    if len(values) == 1:
        return values[0], 0.0
    return statistics.fmean(values), statistics.stdev(values)


def main():
    parser = argparse.ArgumentParser(description="Build the SNNI orchestrator knowledge base from run-load.sh cells.")
    parser.add_argument("cells", nargs="*", help="Cell directories (each with meta.env, requests.csv, windows.csv).")
    parser.add_argument("--root", help="Directory searched recursively for */meta.env cells.")
    parser.add_argument("--nodes", required=True, help="CSV with columns node_id,power_log,idle_power_w.")
    parser.add_argument("--out", default="knowledge_base.csv", help="Output KB CSV path.")
    parser.add_argument("--accuracy", nargs="*", default=[], help="Literature top-1 accuracy as network=value (e.g. resnet50=76.45).")
    parser.add_argument("--allow-missing-energy", action="store_true", help="Emit KB rows even when energy could not be computed (energy left blank -> loads as 0).")
    args = parser.parse_args()

    cell_dirs = list(args.cells)
    if args.root:
        for meta_path in glob.glob(os.path.join(args.root, "**", "meta.env"), recursive=True):
            cell_dirs.append(os.path.dirname(meta_path))
    cell_dirs = sorted(set(cell_dirs))
    if not cell_dirs:
        parser.error("no cell directories given (pass directories or --root)")

    nodes = read_nodes_table(args.nodes)
    accuracy_map = parse_accuracy_map(args.accuracy)

    grouped = {}
    for cell_dir in cell_dirs:
        cell = collect_cell(cell_dir, nodes, accuracy_map)
        if cell is None:
            continue
        key = cell["key"]
        agg = grouped.setdefault(key, {
            "node_role": cell["node_role"],
            "accuracy": cell["accuracy"],
            "energy_samples": [],
            "latencies": [],
            "success": 0,
            "requests": 0,
            "had_energy": False,
        })
        agg["energy_samples"].extend(cell["energy_samples"])
        agg["latencies"].extend(cell["latencies"])
        agg["success"] += cell["success"]
        agg["requests"] += cell["requests"]
        agg["had_energy"] = agg["had_energy"] or cell["had_energy"]
        if cell["accuracy"] >= 0:
            agg["accuracy"] = cell["accuracy"]

    kb_headers = [
        "node_id", "node_role", "backend", "network", "input_shape",
        "num_threads", "concurrency_level", "accuracy_top1",
        "mean_latency_ms", "std_latency_ms", "mean_energy_j", "std_energy_j",
        "samples", "last_updated_ms",
    ]
    detail_headers = kb_headers + ["correctness_rate", "energy_reps", "total_requests"]

    now_ms = int(time.time() * 1000)
    rows = []
    detail_rows = []
    emitted = 0
    skipped = 0
    for key, agg in sorted(grouped.items()):
        node_id, backend_token, network, num_threads, concurrency = key
        has_energy = bool(agg["energy_samples"])
        if not has_energy and not args.allow_missing_energy:
            log("SKIP: %s/%s/%s c%d on %s has no energy (use --allow-missing-energy to keep)"
                % (backend_token, network, num_threads, concurrency, node_id))
            skipped += 1
            continue
        mean_latency, std_latency = mean_std(agg["latencies"])
        mean_energy, std_energy = mean_std(agg["energy_samples"])
        correctness = (agg["success"] / agg["requests"]) if agg["requests"] else 0.0

        kb_row = {
            "node_id": node_id,
            "node_role": agg["node_role"],
            "backend": backend_token,
            "network": network,
            "input_shape": "",
            "num_threads": num_threads,
            "concurrency_level": concurrency,
            "accuracy_top1": ("%.4f" % agg["accuracy"]) if agg["accuracy"] >= 0 else "-1",
            "mean_latency_ms": "%.3f" % mean_latency,
            "std_latency_ms": "%.3f" % std_latency,
            "mean_energy_j": ("%.6f" % mean_energy) if has_energy else "",
            "std_energy_j": ("%.6f" % std_energy) if has_energy else "",
            "samples": agg["requests"],
            "last_updated_ms": now_ms,
        }
        rows.append(kb_row)
        detail = dict(kb_row)
        detail["correctness_rate"] = "%.4f" % correctness
        detail["energy_reps"] = len(agg["energy_samples"])
        detail["total_requests"] = agg["requests"]
        detail_rows.append(detail)
        emitted += 1

    with open(args.out, "w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=kb_headers)
        writer.writeheader()
        writer.writerows(rows)

    detail_path = os.path.splitext(args.out)[0] + ".detail.csv"
    with open(detail_path, "w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=detail_headers)
        writer.writeheader()
        writer.writerows(detail_rows)

    log("Wrote %d KB rows to %s (skipped %d). Detail: %s" % (emitted, args.out, skipped, detail_path))


if __name__ == "__main__":
    main()
