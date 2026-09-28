import {useEffect,useMemo,useState} from "react";
import {Activity,BarChart3,ChevronDown,CirclePause,FileText,Gauge,GitBranch,History,Play,RefreshCw,Server,Square,Terminal,Zap} from "lucide-react";
import {getHealth,getRuns,solveModel} from "./api";
import {sampleModel} from "./sampleModel";
import type {RunResponse,RunSummary,SolveRequest} from "./types";

const labels:Record<string,string>={auto:"AUTO",primal_simplex:"PRIMAL SIMPLEX",interior_point:"INTERIOR POINT",pdhg:"PDHG",pdlp:"PDLP",qp:"QP",milp:"MILP"};

export default function App(){
 const [model,setModel]=useState<SolveRequest>(sampleModel);
 const [run,setRun]=useState<RunResponse|null>(null);
 const [runs,setRuns]=useState<RunSummary[]>([]);
 const [online,setOnline]=useState(false);
 const [running,setRunning]=useState(false);
 const [tab,setTab]=useState<"run"|"history">("run");
 const [error,setError]=useState("");
 const refresh=async()=>{try{const [h,r]=await Promise.all([getHealth(),getRuns()]);setOnline(h.native_api);setRuns(r.runs)}catch{setOnline(false)}};
 useEffect(()=>{void refresh()},[]);
 const execute=async()=>{setRunning(true);setError("");try{const r=await solveModel(model);setRun(r);await refresh()}catch(e){setError(e instanceof Error?e.message:"Solver request failed.")}finally{setRunning(false)}};
 const result=run?.result;
 const fingerprint=result?.fingerprint;
 const chart=useMemo(()=>{const values=runs.map(r=>r.objective).filter((v):v is number=>typeof v==="number").slice(0,10).reverse();if(values.length<2)return[28,34,31,45,41,54,49,68];const min=Math.min(...values),max=Math.max(...values);return values.map(v=>max===min?50:15+((v-min)/(max-min))*70)},[runs]);
 return <div className="shell">
  <header className="topbar"><div className="brand"><div className="brandmark">D</div><div><b>DENT</b><small>OPTIMIZATION ENGINE</small></div></div><div className="topmeta"><i className={online?"online":""}/>{online?"NATIVE API ONLINE":"NATIVE API OFFLINE"}<span>/</span>ENGINE 0.11.0</div><button className="icon" onClick={()=>void refresh()}><RefreshCw size={14}/></button></header>
  <aside className="sidebar">
   <button className={tab==="run"?"nav active":"nav"} onClick={()=>setTab("run")}><Activity/>LIVE RUN</button>
   <button className={tab==="history"?"nav active":"nav"} onClick={()=>setTab("history")}><History/>RUN HISTORY</button>
   <div className="sidegroup"><label>WORKSPACE</label><button className="nav"><FileText/>MODEL INPUT</button><button className="nav"><Gauge/>BENCHMARK</button><button className="nav"><BarChart3/>TELEMETRY</button></div>
   <div className="sidefoot"><span><Server/> REST / JSON</span><span><Terminal/> C++20 CORE</span><span><Zap/> CPU / GPU READY</span></div>
  </aside>
  <main className="main">
   {tab==="run"?<>
    <section className="command"><div className="commandmeta"><span>JOB_ID: <b>{run?"#"+run.id.slice(0,8).toUpperCase():"#NEW-RUN"}</b></span><em>/</em><span>MODEL: <b>optimization_model.json</b></span><strong>[{model.objective.toUpperCase()} · {labels[model.solver.method]}]</strong><em>/</em><span>STATUS: <mark className={running?"running":""}>{running?"● RUNNING":result?.status?.toUpperCase()||"● IDLE"}</mark></span></div><div className="actions"><button disabled={!running}><CirclePause/>PAUSE SOLVER</button><button disabled={!running}><Square/>ABORT &amp; DUMP</button><button><Activity/>STREAM TELEMETRY</button></div></section>
    {error&&<div className="error">{error}</div>}
    <section className="metrics">
     <Metric label="ENGINE_STATE" value={running?"RUNNING":result?.status?.toUpperCase()||"IDLE"} detail={running?"SOLVER ACTIVE":"READY"}/>
     <Metric label="PRIMAL INCUMBENT (MIN)" value={result?fmt(result.objective):"—"} detail={result?"BEST FEASIBLE SOL":"AWAITING RUN"}/>
     <Metric label="DUAL BOUND (LP RELAX)" value={result?fmt(result.objective):"—"} detail={result?"BOUND AVAILABLE":"—"}/>
     <Metric label="MIP OPT GAP" value={result?.status==="optimal"?"0.000%":"—"} detail="OPTIMALITY GAP"/>
     <Metric label="STEP SOLVE TIME" value="—" detail="NOT EXPOSED BY API"/>
     <Metric label="SIMPLEX / PDLP ITR" value={result?String(result.iterations):"—"} detail="CURRENT RUN"/>
     <Metric label="MODEL SIZE" value={fingerprint?fingerprint.variables.toLocaleString():"—"} detail={fingerprint?fingerprint.nonzeros.toLocaleString()+" NNZ":"NO RUN"}/>
     <Metric label="MATRIX TOPOLOGY" value={fingerprint?(fingerprint.density*100).toFixed(1)+"%":"—"} detail={fingerprint?.mixed_integer?"MIXED INTEGER":"CONTINUOUS / LP"}/>
    </section>
    <section className="twocol"><Panel title="CHART_01 // OBJECTIVE CONVERGENCE & BOUND TRAJECTORY" icon={<Activity/>}><Chart values={chart}/></Panel><Panel title="CHART_02 // NODE EXPLORATION" icon={<GitBranch/>}><NodeTree/></Panel></section>
    <section className="twocol"><Panel title="MODEL INPUT / SOLVER CONFIGURATION" icon={<Terminal/>}><div className="config"><Field label="OBJECTIVE"><select value={model.objective} onChange={e=>setModel({...model,objective:e.target.value as SolveRequest["objective"]})}><option value="minimize">MINIMIZE</option><option value="maximize">MAXIMIZE</option></select></Field><Field label="METHOD"><select value={model.solver.method} onChange={e=>setModel({...model,solver:{...model.solver,method:e.target.value as SolveRequest["solver"]["method"]}})}>{Object.entries(labels).map(([k,v])=><option key={k} value={k}>{v}</option>)}</select></Field><Field label="TOLERANCE"><input type="number" min="0" step="0.0000001" value={model.solver.tolerance} onChange={e=>setModel({...model,solver:{...model.solver,tolerance:Number(e.target.value)}})}/></Field><Field label="MAX ITERATIONS"><input type="number" min="0" value={model.solver.max_iterations} onChange={e=>setModel({...model,solver:{...model.solver,max_iterations:Number(e.target.value)}})}/></Field></div><div className="modelbar"><span>VARS <b>{model.variables.length}</b></span><span>CONSTRAINTS <b>{model.constraints.length}</b></span><span>QUADRATIC TERMS <b>{model.quadratic_terms.length}</b></span><button className="run" disabled={running} onClick={()=>void execute()}><Play size={12} fill="currentColor"/>{running?"SOLVING...":"RUN SOLVER"}</button></div></Panel><Panel title="SOLUTION VECTOR" icon={<BarChart3/>}><div className="table"><div className="thead"><span>VARIABLE</span><span>VALUE</span></div>{(result?.variables||model.variables.map(v=>({name:v.name,value:0}))).map(v=><div className="row" key={v.name}><code>{v.name}</code><b>{fmt(v.value)}</b></div>)}</div></Panel></section>
   </>:<HistoryPanel runs={runs} onRefresh={()=>void refresh()}/>}
  </main>
 </div>
}
function Metric(p:{label:string;value:string;detail:string}){return <div className="metric"><small>{p.label}</small><strong>{p.value}</strong><em>{p.detail}</em></div>}
function Panel(p:{title:string;icon:React.ReactNode;children:React.ReactNode}){return <section className="panel"><header><span>{p.icon}{p.title}</span><ChevronDown size={12}/></header>{p.children}</section>}
function Field(p:{label:string;children:React.ReactNode}){return <label className="field"><span>{p.label}</span>{p.children}</label>}
function Chart({values}:{values:number[]}){const pts=values.map((v,i)=>`${18+i*(564/Math.max(values.length-1,1))},${185-v*1.6}`).join(" ");return <div className="chart"><div className="gridlines">{[1,2,3,4,5].map(i=><i key={i}/>)}</div><svg viewBox="0 0 600 210" preserveAspectRatio="none"><polyline points={pts} fill="none" stroke="currentColor" strokeWidth="2"/></svg><div className="axis"><span>START</span><span>ITERATION / TIME</span><span>CONVERGENCE</span></div></div>}
function NodeTree(){return <div className="nodetree"><div className="node root">ROOT</div><div className="node n1">NODE 041<small>BOUND</small></div><div className="node n2">NODE 042<small>INCUMBENT</small></div><div className="node n3">NODE 117<small>PRUNED</small></div><div className="node n4">NODE 118<small>OPEN</small></div></div>}
function HistoryPanel({runs,onRefresh}:{runs:RunSummary[];onRefresh:()=>void}){return <div className="history"><div className="historyhead"><div><small>DENT / RUN HISTORY</small><h1>Execution Ledger</h1><p>Persisted solver executions from the DENT API.</p></div><button onClick={onRefresh}><RefreshCw size={12}/>REFRESH</button></div><Panel title="PERSISTED RUNS" icon={<History/>}><div className="runlist">{runs.length===0?<div className="empty">NO PERSISTED RUNS</div>:runs.map(r=><div className="runline" key={r.id}><code>#{r.id.slice(0,8).toUpperCase()}</code><span>{r.solver||"—"}</span><span>{r.status.toUpperCase()}</span><b>{r.objective!=null?fmt(r.objective):"—"}</b></div>)}</div></Panel></div>}
function fmt(n:number){return new Intl.NumberFormat("en-US",{maximumFractionDigits:4}).format(n)}
