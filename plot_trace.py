"""Convert the demo's CSV to a standalone SVG timeline."""
import csv
import sys
from pathlib import Path

source = Path(sys.argv[1] if len(sys.argv) > 1 else "trace.csv")
with source.open(newline="") as file:
    spans = list(csv.DictReader(file))
start = min(float(s["start_us"]) for s in spans)
end = max(float(s["end_us"]) for s in spans)
workers = 1 + max(int(s["worker"]) for s in spans)
colors = ("#1683a6", "#9b4bb7", "#267d51", "#bf631d")
svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="{54 + 28 * workers}">',
       '<rect width="100%" height="100%" fill="#f8fafc"/>',
       f'<text x="12" y="22" font-family="sans-serif">Job execution · {(end-start)/1000:.2f} ms</text>']
for worker in range(workers):
    svg.append(f'<text x="12" y="{54 + worker * 28}" font-family="sans-serif">Worker {worker}</text>')
for span in spans:
    worker = int(span["worker"])
    x = 105 + 880 * (float(span["start_us"]) - start) / (end - start)
    width = 880 * (float(span["end_us"]) - float(span["start_us"])) / (end - start)
    svg.append(f'<rect x="{x:.2f}" y="{39 + worker * 28}" width="{max(1,width):.2f}" height="18" '
               f'fill="{colors[worker % len(colors)]}"><title>Job {int(span["id"])}</title></rect>')
source.with_suffix(".svg").write_text("\n".join(svg + ["</svg>"]), encoding="utf-8")
