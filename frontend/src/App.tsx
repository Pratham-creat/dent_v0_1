import {useEffect,useMemo,useState} from "react";
import type {ReactNode} from "react";
import {Activity,BarChart3,ChevronDown,CirclePause,FileText,Gauge,GitBranch,History,Play,Plus,RefreshCw,Server,Square,Terminal,Trash2,Zap} from "lucide-react";
import {getHealth,getRuns,solveModel} from "./api";
import {sampleMILP,sampleModel,sampleQP} from "./sampleModel";
import type {RunResponse,RunSummary,SolveRequest} from "./types";

const labels:Record<string,string>={auto:"AUTO",primal_simplex:"PRIMAL SIMPLEX",interior_point:"INTERIOR POINT",pdhg:"PDHG",pdlp:"PDLP",qp:"QP",milp:"MILP"};

export default function App(){
 const [model,setModel]=useState<SolveRequest>(sampleModel);
 const [run,setRun]=useState<RunResponse|null>(null);
 const [runs,setRuns]=useState<RunSummary[]>([]);
 const [online,setOnline]=useState(false);
 const [running,setRunning]=useState(false);
 const [tab,setTab]=useState<"run"|"history"|"model"|"benchmark"|"telemetry">("run");
 const [error,setError]=useState("");
 const refresh=async()=>{try{const [h,r]=await Promise.all([getHealth(),getRuns()]);setOnline(h.native_api);setRuns(r.runs)}catch{setOnline(false)}};
 useEffect(()=>{void refresh()},[]);
 const execute=async()=>{setRunning(true);setError("");try{const r=await solveModel(model);setRun(r);await refresh()}catch(e){setError(e instanceof Error?e.message:"Solver request failed.")}finally{setRunning(false)}};
 const result=run?.result;
 const fingerprint=result?.fingerprint;
 const chart=useMemo(()=>{const values=runs.map(r=>r.objective).filter((v):v is number=>typeof v==="number").slice(0,10).reverse();if(values.length<2)return[28,34,31,45,41,54,49,68];const min=Math.min(...values),max=Math.max(...values);return values.map(v=>max===min?50:15+((v-min)/(max-min))*70)},[runs]);
 const setMethod=(method:SolveRequest["solver"]["method"])=>{
  if(method==="qp")setModel(sampleQP);
  else if(method==="milp")setModel(sampleMILP);
  else setModel({...model,solver:{...model.solver,method}});
  setRun(null);setError("");
 };
 return <div className="shell">
  <header className="topbar"><div className="brand"><div className="brandmark">D</div><div><b>DENT</b><small>OPTIMIZATION ENGINE</small></div></div><div className="topmeta"><i className={online?"online":""}/>{online?"NATIVE API ONLINE":"NATIVE API OFFLINE"}<span>/</span>ENGINE 0.11.0</div><button className="icon" onClick={()=>void refresh()}><RefreshCw size={14}/></button></header>
  <aside className="sidebar">
   <button className={tab==="run"?"nav active":"nav"} onClick={()=>setTab("run")}><Activity/>LIVE RUN</button>
   <button className={tab==="history"?"nav active":"nav"} onClick={()=>setTab("history")}><History/>RUN HISTORY</button>
   <div className="sidegroup"><label>WORKSPACE</label><button className="nav"><FileText/>MODEL INPUT</button><button className="nav"><Gauge/>BENCHMARK</button><button className="nav"><BarChart3/>TELEMETRY</button></div>
   <div className="sidefoot"><span><Server/> REST / JSON</span><span><Terminal/> C++20 CORE</span><span><Zap/> CPU / GPU READY</span></div>
  </aside>
  <main className="main">
   {tab==="run"?<LiveRunView model={model} setModel={setModel} run={run} setRun={setRun} running={running} online={online} error={error} setError={setError} execute={execute} runs={runs}/>:tab==="history"?<HistoryPanel runs={runs} onRefresh={()=>void refresh()}/>:tab==="model"?<WorkspaceModelView model={model} setModel={setModel}/>:tab==="benchmark"?<BenchmarkView model={model}/>:<TelemetryView runs={runs}/>} 
  </main>
 </div>
}

function LiveRunView({model,setModel,run,setRun,running,online,error,setError,execute,runs}:{model:SolveRequest;setModel:(m:SolveRequest)=>void;run:RunResponse|null;setRun:(r:RunResponse|null)=>void;running:boolean;online:boolean;error:string;setError:(s:string)=>void;execute:()=>Promise<void>;runs:RunSummary[]}){
 const result=run?.result; const fingerprint=result?.fingerprint;
 return <>
  <section className="command"><div className="commandmeta"><span>JOB_ID: <b>{run?"#"+run.id.slice(0,8).toUpperCase():"#NEW-RUN"}</b></span><em>/</em><span>MODEL: <b>optimization_model.json</b></span><strong>[{model.objective.toUpperCase()} · {labels[model.solver.method]}]</strong><em>/</em><span>STATUS: <mark className={running?"running":""}>{running?"● RUNNING":result?.status?.toUpperCase()||"● IDLE"}</mark></span></div><div className="actions"><button disabled={!running}><CirclePause/>PAUSE SOLVER</button><button disabled={!running}><Square/>ABORT &amp; DUMP</button><button><Activity/>STREAM TELEMETRY</button></div></section>
  {error&&<div className="error">{error}</div>}
  <section className="metrics"><Metric label="ENGINE_STATE" value={running?"RUNNING":result?.status?.toUpperCase()||"IDLE"} detail={running?"SOLVER ACTIVE":"READY"}/><Metric label="PRIMAL INCUMBENT (MIN)" value={result?fmt(result.objective):"—"} detail={result?"BEST FEASIBLE SOL":"AWAITING RUN"}/><Metric label="DUAL BOUND (LP RELAX)" value={result?fmt(result.objective):"—"} detail={result?"BOUND AVAILABLE":"—"}/><Metric label="MIP OPT GAP" value={result?.status==="optimal"?"0.000%":"—"} detail="OPTIMALITY GAP"/><Metric label="STEP SOLVE TIME" value={result?.solve_time_ms!=null?result.solve_time_ms.toFixed(2)+" ms":"—"} detail={result?"MEASURED BY API":"AWAITING RUN"}/><Metric label="SIMPLEX / PDLP ITR" value={result?String(result.iterations):"—"} detail="CURRENT RUN"/><Metric label="MODEL SIZE" value={fingerprint?fingerprint.variables.toLocaleString():"—"} detail={fingerprint?fingerprint.nonzeros.toLocaleString()+" NNZ":"NO RUN"}/><Metric label="MATRIX TOPOLOGY" value={fingerprint?(fingerprint.density*100).toFixed(1)+"%":"—"} detail={fingerprint?.mixed_integer?"MIXED INTEGER":"CONTINUOUS / LP"}/></section>
  <section className="twocol"><Panel title="CHART_01 // OBJECTIVE CONVERGENCE & BOUND TRAJECTORY" icon={<Activity/>}><Chart values={runs.map(r=>r.objective??0).slice(0,10).reverse()}/></Panel><Panel title="CHART_02 // NODE EXPLORATION" icon={<GitBranch/>}><NodeTree/></Panel></section>
  <section className="twocol"><Panel title="MODEL INPUT / SOLVER CONFIGURATION" icon={<Terminal/>}><div className="config"><Field label="OBJECTIVE"><select value={model.objective} onChange={e=>{setModel({...model,objective:e.target.value as SolveRequest["objective"]});setRun(null)}}><option value="minimize">MINIMIZE</option><option value="maximize">MAXIMIZE</option></select></Field><Field label="METHOD"><select value={model.solver.method} onChange={e=>{const method=e.target.value as SolveRequest["solver"]["method"];setModel(method==="qp"?sampleQP:method==="milp"?sampleMILP:{...model,solver:{...model.solver,method}});setRun(null)}}>{Object.entries(labels).map(([k,v])=><option key={k} value={k}>{v}</option>)}</select></Field><Field label="TOLERANCE"><input type="number" min="0" step="0.0000001" value={model.solver.tolerance} onChange={e=>setModel({...model,solver:{...model.solver,tolerance:Number(e.target.value)}})}/></Field><Field label="MAX ITERATIONS"><input type="number" min="0" value={model.solver.max_iterations} onChange={e=>setModel({...model,solver:{...model.solver,max_iterations:Number(e.target.value)}})}/></Field></div><ModelEditor model={model} setModel={setModel} disabled={running}/><div className="modelbar"><span>VARS <b>{model.variables.length}</b></span><span>CONSTRAINTS <b>{model.constraints.length}</b></span><span>QUADRATIC TERMS <b>{model.quadratic_terms.length}</b></span><button className="run" disabled={running} onClick={()=>void execute()}><Play size={12} fill="currentColor"/>{running?"SOLVING...":"RUN SOLVER"}</button></div></Panel><Panel title="SOLUTION VECTOR" icon={<BarChart3/>}><div className="table"><div className="thead"><span>VARIABLE</span><span>VALUE</span></div>{(result?.variables||model.variables.map(v=>({name:v.name,value:0}))).map(v=><div className="row" key={v.name}><code>{v.name}</code><b>{fmt(v.value)}</b></div>)}</div></Panel></section>
 </>
}
function WorkspaceModelView({model,setModel}:{model:SolveRequest;setModel:(m:SolveRequest)=>void}){return <div className="workspace-page"><div className="page-head"><small>DENT / WORKSPACE</small><h1>Model Input</h1><p>Build and inspect the optimization model before execution.</p></div><Panel title="MODEL DEFINITION" icon={<FileText/>}><ModelEditor model={model} setModel={setModel} disabled={false}/></Panel></div>}
function BenchmarkView({model}:{model:SolveRequest}){const methods=Object.keys(labels);return <div className="workspace-page"><div className="page-head"><small>DENT / ANALYSIS</small><h1>Benchmark</h1><p>Compare the configured model across available DENT solver methods.</p></div><Panel title="BENCHMARK MATRIX" icon={<Gauge/>}><div className="benchmark-grid">{methods.map(m=><div className="bench-card" key={m}><span>{labels[m]}</span><b>READY</b><small>{m==="qp"&&!model.quadratic_terms.length?"REQUIRES QUADRATIC TERMS":m==="milp"&&!model.variables.some(v=>v.type!=="continuous")?"CONTINUOUS MODEL":"AVAILABLE"}</small></div>)}</div></Panel></div>}
function TelemetryView({runs}:{runs:RunSummary[]}){const total=runs.length,avg=total?runs.reduce((a,r)=>a+(r.solve_time_ms??0),0)/total:0;return <div className="workspace-page"><div className="page-head"><small>DENT / OBSERVABILITY</small><h1>Telemetry</h1><p>Execution telemetry currently available from persisted API runs.</p></div><section className="metrics telemetry-metrics"><Metric label="TOTAL RUNS" value={String(total)} detail="PERSISTED EXECUTIONS"/><Metric label="AVERAGE SOLVE TIME" value={total?avg.toFixed(2)+" ms":"—"} detail="API MEASUREMENT"/><Metric label="OPTIMAL RUNS" value={String(runs.filter(r=>r.status==="optimal").length)} detail="PERSISTED STATUS"/><Metric label="SOLVER TYPES" value={String(new Set(runs.map(r=>r.solver).filter(Boolean)).size)} detail="OBSERVED"/></section><Panel title="TELEMETRY RUN STREAM" icon={<Activity/>}><div className="runlist">{runs.length?runs.map(r=><div className="runline" key={r.id}><code>#{r.id.slice(0,8).toUpperCase()}</code><span>{r.solver||"—"}</span><span>{r.status.toUpperCase()}</span><b>{r.solve_time_ms!=null?r.solve_time_ms.toFixed(2)+" ms":"—"}</b></div>):<div className="empty">NO TELEMETRY RUNS</div>}</div></Panel></div>}

function ModelEditor({model,setModel,disabled}:{model:SolveRequest;setModel:(model:SolveRequest)=>void;disabled:boolean}){
 const updateVariable=(index:number,key:keyof SolveRequest["variables"][number],value:string)=>{
  const variables=model.variables.map((v,i)=>i===index?{...v,[key]:key==="name"||key==="type"?value:Number(value)}:v);
  setModel({...model,variables});
 };
 const addVariable=()=>{const name="x"+(model.variables.length+1);const variables=[...model.variables,{name,lower_bound:0,upper_bound:0,type:"continuous" as const,objective_coefficient:0}];const constraints=model.constraints.map(c=>({...c,coefficients:{...c.coefficients,[name]:0}}));setModel({...model,variables,constraints});};
 const removeVariable=(index:number)=>{if(model.variables.length<=1)return;const name=model.variables[index].name;const variables=model.variables.filter((_,i)=>i!==index);const constraints=model.constraints.map(c=>{const coefficients={...c.coefficients};delete coefficients[name];return {...c,coefficients}});const quadratic_terms=model.quadratic_terms.filter(q=>q.row!==name&&q.column!==name);setModel({...model,variables,constraints,quadratic_terms});};
 const addConstraint=()=>{const coefficients=Object.fromEntries(model.variables.map(v=>[v.name,0]));setModel({...model,constraints:[...model.constraints,{name:"constraint"+(model.constraints.length+1),sense:0,rhs:0,coefficients}]});};
 const removeConstraint=(index:number)=>{if(model.constraints.length<=1)return;setModel({...model,constraints:model.constraints.filter((_,i)=>i!==index)});};
 const updateConstraint=(index:number,key:"name"|"sense"|"rhs",value:string)=>setModel({...model,constraints:model.constraints.map((c,i)=>i===index?{...c,[key]:key==="name"?value:Number(value)}:c)});
 const updateCoefficient=(ci:number,name:string,value:string)=>setModel({...model,constraints:model.constraints.map((c,i)=>i===ci?{...c,coefficients:{...c.coefficients,[name]:Number(value)}}:c)});
 const addQuadratic=()=>{const a=model.variables[0]?.name||"x";const b=model.variables[1]?.name||a;setModel({...model,quadratic_terms:[...model.quadratic_terms,{row:a,column:b,coefficient:0}]});};
 const removeQuadratic=(index:number)=>setModel({...model,quadratic_terms:model.quadratic_terms.filter((_,i)=>i!==index)});
 const updateQuadratic=(index:number,key:"row"|"column"|"coefficient",value:string)=>setModel({...model,quadratic_terms:model.quadratic_terms.map((q,i)=>i===index?{...q,[key]:key==="coefficient"?Number(value):value}:q)});
 return <div className="model-editor">
  <div className="editor-heading"><span>MODEL DEFINITION</span><small>ENTER VARIABLES, CONSTRAINTS AND QUADRATIC TERMS</small></div>
  <EditorSection title="VARIABLES" count={model.variables.length} onAdd={addVariable} disabled={disabled}>
   {model.variables.map((v,i)=><div className="editor-row variable-row" key={i}>
    <input value={v.name} disabled={disabled} aria-label="Variable name" onChange={e=>updateVariable(i,"name",e.target.value)}/>
    <select value={v.type} disabled={disabled} onChange={e=>updateVariable(i,"type",e.target.value)}><option value="continuous">CONTINUOUS</option><option value="integer">INTEGER</option><option value="binary">BINARY</option></select>
    <input type="number" value={v.lower_bound} disabled={disabled} aria-label="Lower bound" onChange={e=>updateVariable(i,"lower_bound",e.target.value)}/>
    <input type="number" value={v.upper_bound} disabled={disabled} aria-label="Upper bound" onChange={e=>updateVariable(i,"upper_bound",e.target.value)}/>
    <input type="number" value={v.objective_coefficient} disabled={disabled} aria-label="Objective coefficient" onChange={e=>updateVariable(i,"objective_coefficient",e.target.value)}/>
    <button className="icon danger" disabled={disabled||model.variables.length<=1} onClick={()=>removeVariable(i)}><Trash2 size={12}/></button>
   </div>)}
   <div className="editor-labels"><span>NAME</span><span>TYPE</span><span>LOWER</span><span>UPPER</span><span>OBJ COEFF</span><span/></div>
  </EditorSection>
  <EditorSection title="CONSTRAINTS" count={model.constraints.length} onAdd={addConstraint} disabled={disabled}>
   {model.constraints.map((c,i)=><div className="constraint-card" key={i}>
    <div className="editor-row"><input value={c.name} disabled={disabled} onChange={e=>updateConstraint(i,"name",e.target.value)}/><select value={c.sense} disabled={disabled} onChange={e=>updateConstraint(i,"sense",e.target.value)}><option value={0}>&lt;=</option><option value={1}>=</option><option value={2}>&gt;=</option></select><input type="number" value={c.rhs} disabled={disabled} onChange={e=>updateConstraint(i,"rhs",e.target.value)}/><button className="icon danger" disabled={disabled||model.constraints.length<=1} onClick={()=>removeConstraint(i)}><Trash2 size={12}/></button></div>
    <div className="coefficient-grid">{model.variables.map(v=><label key={v.name}><span>{v.name}</span><input type="number" value={c.coefficients[v.name]??0} disabled={disabled} onChange={e=>updateCoefficient(i,v.name,e.target.value)}/></label>)}</div>
   </div>)}
  </EditorSection>
  <EditorSection title="QUADRATIC TERMS" count={model.quadratic_terms.length} onAdd={addQuadratic} disabled={disabled}>
   {model.quadratic_terms.length===0&&<div className="editor-empty">No quadratic terms. Add terms here to create a QP objective.</div>}
   {model.quadratic_terms.map((q,i)=><div className="editor-row quadratic-row" key={i}><select value={q.row} disabled={disabled} onChange={e=>updateQuadratic(i,"row",e.target.value)}>{model.variables.map(v=><option key={v.name}>{v.name}</option>)}</select><select value={q.column} disabled={disabled} onChange={e=>updateQuadratic(i,"column",e.target.value)}>{model.variables.map(v=><option key={v.name}>{v.name}</option>)}</select><input type="number" value={q.coefficient} disabled={disabled} onChange={e=>updateQuadratic(i,"coefficient",e.target.value)}/><button className="icon danger" disabled={disabled} onClick={()=>removeQuadratic(i)}><Trash2 size={12}/></button></div>)}
   <div className="editor-labels"><span>ROW</span><span>COLUMN</span><span>COEFFICIENT</span><span/></div>
  </EditorSection>
 </div>;
}

function EditorSection({title,count,onAdd,disabled,children}:{title:string;count:number;onAdd:()=>void;disabled:boolean;children:ReactNode}){return <div className="editor-section"><header><span>{title} <b>{count}</b></span><button className="add" disabled={disabled} onClick={onAdd}><Plus size={12}/>ADD</button></header>{children}</div>}

function Metric(p:{label:string;value:string;detail:string}){return <div className="metric"><small>{p.label}</small><strong>{p.value}</strong><em>{p.detail}</em></div>}
function Panel(p:{title:string;icon:ReactNode;children:ReactNode}){return <section className="panel"><header><span>{p.icon}{p.title}</span><ChevronDown size={12}/></header>{p.children}</section>}
function Field(p:{label:string;children:ReactNode}){return <label className="field"><span>{p.label}</span>{p.children}</label>}
function Chart({values}:{values:number[]}){const pts=values.map((v,i)=>`${18+i*(564/Math.max(values.length-1,1))},${185-v*1.6}`).join(" ");return <div className="chart"><div className="gridlines">{[1,2,3,4,5].map(i=><i key={i}/>)}</div><svg viewBox="0 0 600 210" preserveAspectRatio="none"><polyline points={pts} fill="none" stroke="currentColor" strokeWidth="2"/></svg><div className="axis"><span>START</span><span>ITERATION / TIME</span><span>CONVERGENCE</span></div></div>}
function NodeTree(){return <div className="nodetree"><div className="node root">ROOT</div><div className="node n1">NODE 041<small>BOUND</small></div><div className="node n2">NODE 042<small>INCUMBENT</small></div><div className="node n3">NODE 117<small>PRUNED</small></div><div className="node n4">NODE 118<small>OPEN</small></div></div>}
function HistoryPanel({runs,onRefresh}:{runs:RunSummary[];onRefresh:()=>void}){return <div className="history"><div className="historyhead"><div><small>DENT / RUN HISTORY</small><h1>Execution Ledger</h1><p>Persisted solver executions from the DENT API.</p></div><button onClick={onRefresh}><RefreshCw size={12}/>REFRESH</button></div><Panel title="PERSISTED RUNS" icon={<History/>}><div className="runlist">{runs.length===0?<div className="empty">NO PERSISTED RUNS</div>:runs.map(r=><div className="runline" key={r.id}><code>#{r.id.slice(0,8).toUpperCase()}</code><span>{r.solver||"—"}</span><span>{r.status.toUpperCase()}</span><b>{r.objective!=null?fmt(r.objective):"—"}</b></div>)}</div></Panel></div>}
function fmt(n:number){return new Intl.NumberFormat("en-US",{maximumFractionDigits:4}).format(n)}
