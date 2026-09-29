#!/usr/bin/env python3
import argparse,subprocess,time,pathlib,json,shutil
def run(cmd,path):
 t=time.perf_counter();p=subprocess.run(cmd+[str(path)],capture_output=True,text=True);return {"seconds":time.perf_counter()-t,"returncode":p.returncode,"stdout":p.stdout,"stderr":p.stderr}
ap=argparse.ArgumentParser();ap.add_argument("directory");ap.add_argument("--dent",default="dent");ap.add_argument("--highs",default="highs");ap.add_argument("--output",default="highs_compare.json");a=ap.parse_args()
if shutil.which(a.dent) is None:raise SystemExit("DENT executable not found")
if shutil.which(a.highs) is None:raise SystemExit("HiGHS executable not found")
rows=[]
for p in sorted(pathlib.Path(a.directory).glob("*")):
 if p.suffix.lower() not in {".mps",".lp"}:continue
 rows.append({"model":str(p),"dent":run([a.dent],p),"highs":run([a.highs],p)})
pathlib.Path(a.output).write_text(json.dumps(rows,indent=2));print(json.dumps(rows,indent=2))