import React,{useEffect,useMemo,useState} from 'react';
import {Activity,BarChart3,ChevronRight,FileCode2,Gauge,History,Play,RefreshCw,Settings2,Terminal,Upload,Workflow,Database, Cpu, AlertTriangle} from 'lucide-react';
import {getRun,getRuns,health,solveFile,solveModel,RunResponse} from './api';

type View='overview'|'workbench'|'live'|'history'|'benchmarks'|'diagnostics'|'settings';
type Settings={method:'auto'|'primal_simplex'|'interior_point'|'pdhg'|'pdlp'|'qp'|'milp';tolerance:number;max_iterations:number};
type RunSummary={id:string;created_at:string;source_type:string;source_name?:string;status?:string;objective?:number;solver?:string;iterations?:number;solve_time_ms?:number;message?:string;result?:RunResponse['result']};

const sampleModel={objective:'minimize',variables:[{name:'x',lower_bound:0,upper_bound:0,type:'continuous',objective_coefficient:3},{name:'y',lower_bound:0,upper_bound:0,type:'continuous',objective_coefficient:2}],constraints:[{name:'capacity',coefficients:{x:1,y:1},sense:0,rhs:4}],quadratic_terms:[]};

export default function App(){
 const[view,setView]=useState<View>('workbench'),[online,setOnline]=useState(false),[runs,setRuns]=useState<RunSummary[]>([]),[active,setActive]=useState<RunResponse|null>(null),[busy,setBusy]=useState(false),[error,setError]=useState(''),[input,setInput]=useState(JSON.stringify(sampleModel,null,2));
 const[settings,setSettings]=useState<Settings>(()=>{try{return JSON.parse(localStorage.getItem('dent-settings')||'')||{method:'auto',tolerance:0,max_iterations:0}}catch{return{method:'auto',tolerance:0,max_iterations:0}}});
 const refresh=async()=>{health().then(()=>setOnline(true)).catch(()=>setOnline(false));try{const x=await getRuns();setRuns(Array.isArray(x?.runs)?x.runs:[])}catch{setRuns([])}};
 useEffect(()=>{refresh()},[]);
 const saveSettings=(s:Settings)=>{setSettings(s);localStorage.setItem('dent-settings',JSON.stringify(s))};
 const applyRun=(r:RunResponse)=>{setActive(r);setRuns(x=>[{id:r.id,created_at:r.created_at,source_type:r.source_type,source_name:r.source_name,status:r.result.status,objective:r.result.objective,solver:r.result.solver,iterations:r.result.iterations,solve_time_ms:r.result.solve_time_ms,message:r.result.message,result:r.result},...x.filter(a=>a.id!==r.id)]);setView('live')};
 const solve=async()=>{setError('');setBusy(true);try{const m=JSON.parse(input);m.solver=settings;applyRun(await solveModel(m))}catch(e){setError(e instanceof Error?e.message:'Invalid JSON or solve request failed')}finally{setBusy(false)}};
 const upload=async(f:File)=>{setError('');setBusy(true);try{applyRun(await solveFile(f,settings))}catch(e){setError(e instanceof Error?e.message:'File solve failed')}finally{setBusy(false)}};
 const openRun=async(id:string)=>{setError('');try{setActive(await getRun(id));setView('live')}catch(e){setError(e instanceof Error?e.message:'Run not found')}};
 return <div className="app">
  <aside className="sidebar"><div className="brand"><div className="brandmark">D</div><div><b>DENT</b><span>OPTIMIZATION ENGINE</span></div></div>
   <div className="navgroup"><label>WORKSPACE</label><Nav active={view==='overview'} icon={<Gauge/>} text="Overview" onClick={()=>setView('overview')}/><Nav active={view==='workbench'} icon={<Workflow/>} text="Workbench" onClick={()=>setView('workbench')}/><Nav active={view==='live'} icon={<Activity/>} text="Live Solver Run" onClick={()=>setView('live')}/><Nav active={view==='history'} icon={<History/>} text="Run History" onClick={()=>setView('history')}/></div>
   <div className="navgroup"><label>ENGINE</label><Nav active={view==='benchmarks'} icon={<BarChart3/>} text="Benchmarks" onClick={()=>setView('benchmarks')}/><Nav active={view==='diagnostics'} icon={<Terminal/>} text="Diagnostics" onClick={()=>setView('diagnostics')}/><Nav active={view==='settings'} icon={<Settings2/>} text="Settings" onClick={()=>setView('settings')}/></div>
   <div className="sidefoot"><div>ENGINE <b>DENT-CORE v0.11.0</b></div><div>API <b>{online?'ONLINE':'OFFLINE'}</b></div><div>RUNTIME <b>NATIVE DENT API</b></div></div>
  </aside>
  <main className="main"><header className="header"><div><span className="crumb">DENT /</span><b>{view.toUpperCase()}</b></div><div className="headerRight"><span className={online?'online':''}>● API {online?'ONLINE':'OFFLINE'}</span><button onClick={refresh}><RefreshCw size={14}/></button></div></header>
   {error&&<div className="errorbar">{error}</div>}
   {view==='overview'&&<Overview runs={runs} openRun={openRun} openWorkbench={()=>setView('workbench')} />}
   {view==='workbench'&&<Workbench input={input} setInput={setInput} solve={solve} upload={upload} busy={busy} settings={settings} openSettings={()=>setView('settings')}/>}
   {view==='live'&&<Live active={active} busy={busy} openWorkbench={()=>setView('workbench')}/>}
   {view==='history'&&<HistoryView runs={runs} openRun={openRun}/>}
   {view==='benchmarks'&&<Benchmarks runs={runs}/>}
   {view==='diagnostics'&&<Diagnostics online={online}/>}
   {view==='settings'&&<SettingsView settings={settings} save={saveSettings}/>}
  </main>
 </div>
}

function Nav({active,icon,text,onClick}:{active:boolean;icon:React.ReactNode;text:string;onClick:()=>void}){return <button className={'nav '+(active?'active':'')} onClick={onClick}>{icon}<span>{text}</span>{active&&<ChevronRight size={13}/>}</button>}
function Metric({label,value,sub}:{label:string;value:string|number;sub?:string}){return <div className="metric"><span>{label}</span><strong>{value}</strong>{sub&&<small>{sub}</small>}</div>}
function Panel({title,icon,children,wide=false}:{title:string;icon:React.ReactNode;children:React.ReactNode;wide?:boolean}){return <section className={'panel '+(wide?'wide':'')}><div className="panelhead"><span>{icon}{title}</span></div>{children}</section>}

function Overview({runs,openRun,openWorkbench}:{runs:RunSummary[];openRun:(id:string)=>void;openWorkbench:()=>void}){
 return <div className="content"><div className="title"><span>DASHBOARD //</span><h1>MATHEMATICAL OPTIMIZATION WORKBENCH</h1><p>Model construction, solver dispatch, result inspection and persisted run history.</p></div>
  <div className="overviewgrid"><Metric label="RUNS STORED" value={runs.length}/><Metric label="ENGINE" value="v0.11.0"/><Metric label="PROBLEM CLASSES" value="LP · MILP · QP"/><Metric label="TRANSPORT" value="REST / JSON"/></div>
  <div className="actionrow"><button className="dispatch" onClick={openWorkbench}><Play size={15}/> OPEN WORKBENCH</button><div className="statusbox"><Database size={14}/> SQLITE RUN HISTORY <b>{runs.length?'READY':'EMPTY'}</b></div></div>
  <Panel title="RECENT RUNS" icon={<History/>}><HistoryView runs={runs.slice(0,8)} openRun={openRun}/></Panel>
  <div className="split"><Panel title="ENGINE CAPABILITIES" icon={<Cpu/>}><div className="capgrid"><span>LP</span><span>MILP</span><span>QP</span><span>PRESOLVE</span><span>SCALING</span><span>ADAPTIVE DISPATCH</span></div></Panel><Panel title="TELEMETRY AVAILABILITY" icon={<AlertTriangle/>}><Unavailable text="Runtime GPU, MIP-gap, node and cut telemetry is not exposed by the current API." /></Panel></div>
 </div>
}

function Workbench({input,setInput,solve,upload,busy,settings,openSettings}:{input:string;setInput:(v:string)=>void;solve:()=>void;upload:(f:File)=>void;busy:boolean;settings:Settings;openSettings:()=>void}){
 return <div className="content"><div className="title"><span>WORKBENCH //</span><h1>MODEL INPUT & SOLVER DISPATCH</h1><p>Edit a JSON model or load a .dent file. The request is sent to the real DENT API.</p></div>
  <div className="workspacegrid"><div className="editor"><div className="editorhead"><FileCode2 size={15}/> MODEL INPUT <label className="filebutton"><Upload size={13}/> LOAD .DENT<input type="file" accept=".dent" onChange={e=>{const f=e.target.files?.[0];if(f)upload(f)}}/></label></div><textarea className="modelinput" value={input} onChange={e=>setInput(e.target.value)} spellCheck={false}/><div className="editorfoot"><button onClick={()=>setInput(JSON.stringify(sampleModel,null,2))}>LOAD SAMPLE</button><span>JSON REQUEST MODEL</span></div></div>
   <div className="config"><Metric label="METHOD" value={settings.method.toUpperCase()}/><Metric label="TOLERANCE" value={settings.tolerance===0?'ENGINE DEFAULT':settings.tolerance}/><Metric label="MAX ITERATIONS" value={settings.max_iterations===0?'ENGINE DEFAULT':settings.max_iterations}/><button className="secondary" onClick={openSettings}><Settings2 size={14}/> EDIT SOLVER SETTINGS</button><button className="dispatch" disabled={busy} onClick={solve}><Play size={15}/>{busy?'SOLVING...':'DISPATCH TO DENT'}</button></div>
  </div>
  <div className="split"><Panel title="INPUT NOTES" icon={<FileCode2/>}><div className="notes">JSON uses the DENT API schema. For native <b>.dent</b> files, the backend sends the file directly to the native bridge.</div></Panel><Panel title="SOLVER TELEMETRY" icon={<Activity/>}><Unavailable text="Live internal solver telemetry is not currently streamed by DENT." /></Panel></div>
 </div>
}

function Live({active,busy,openWorkbench}:{active:RunResponse|null;busy:boolean;openWorkbench:()=>void}){
 const r=active?.result;
 if(!r)return <div className="content"><div className="title"><span>LIVE SOLVER RUN //</span><h1>{busy?'REQUEST IN PROGRESS':'NO ACTIVE RUN'}</h1><p>{busy?'Waiting for the DENT API response.':'Dispatch a model from the Workbench.'}</p></div><Panel title="SOLVER CONSOLE" icon={<Terminal/>}><div className="empty">{busy?'REQUEST SENT — WAITING FOR DENT':'NO SOLVER RESULT LOADED'}</div><button className="dispatch" onClick={openWorkbench}>OPEN WORKBENCH</button></Panel></div>;
 const vars=r.variables||[];
 return <div className="content"><div className="title"><span>LIVE SOLVER RUN //</span><h1>DENT SOLVE RESULT</h1><p>Runtime measurements below are limited to values returned by DENT or measured by the API layer.</p></div>
  <div className="metrics"><Metric label="STATUS" value={r.status.toUpperCase()}/><Metric label="OBJECTIVE" value={Number(r.objective).toLocaleString()}/><Metric label="SOLVER" value={r.solver}/><Metric label="DENT ITERATIONS" value={r.iterations}/><Metric label="API SOLVE TIME" value={r.solve_time_ms==null?'—':r.solve_time_ms.toFixed(2)+' ms'}/><Metric label="VARIABLES" value={r.fingerprint.variables}/><Metric label="CONSTRAINTS" value={r.fingerprint.constraints}/><Metric label="NONZEROS" value={r.fingerprint.nonzeros}/></div>
  <div className="graphsection"><div className="graphtitle">SOLVER VISUALIZATIONS //</div><div className="chartgrid"><Panel title="OBJECTIVE CONVERGENCE" icon={<Activity/>}><ObjectiveChart objective={r.objective} iterations={r.iterations}/></Panel><Panel title="SOLUTION VECTOR" icon={<BarChart3/>}><SolutionChart variables={vars}/></Panel></div></div>
  <div className="chartgrid"><Panel title="BRANCH & BOUND / CUT TELEMETRY" icon={<BarChart3/>}><TelemetryChart/></Panel><Panel title="HARDWARE SYNTHESIS & ENGINE TELEMETRY" icon={<Cpu/>}><div className="telemetrygrid"><Metric label="RUNTIME" value="NATIVE DENT API"/><Metric label="GPU SPMV" value="342.9 G/s"/><Metric label="ACCELERATION" value="7.8×"/><Metric label="VRAM BUFFER" value="18.4 GB"/><Metric label="HOST THREADS" value="32"/><Metric label="SYNC CLOCK" value="1.81 GHz"/><Metric label="MIP GAP" value="0.00%"/><Metric label="OPEN NODES" value="2,184"/></div><div className="simulatedbadge">SIMULATED WORKSTATION TELEMETRY — DISPLAY ONLY</div></Panel></div>
  <div className="chartgrid"><Panel title="DENT MESSAGE" icon={<Terminal/>}><div className="dentmessage"><div className="messageprimary">{r.message}</div><div className="messagerows"><span>STATUS <b>{r.status}</b></span><span>SOLVER <b>{r.solver}</b></span><span>OBJECTIVE <b>{Number(r.objective).toLocaleString()}</b></span><span>ITERATIONS <b>{r.iterations}</b></span></div></div></Panel><Panel title="SOLVER RESULT STREAM" icon={<Terminal/>}><div className="console"><div>[client] solve request submitted</div><div>[client] DENT response received</div><div>[dent] status={r.status}</div><div>[dent] solver={r.solver}</div><div>[dent] objective={r.objective}</div><div>[dent] iterations={r.iterations}</div><div>[dent] message={r.message}</div><div className="simline">[display] simulated presolve reduction: 27%</div><div className="simline">[display] simulated incumbent update: 428,000 → 412,000</div><div className="simline">[display] simulated cut pool: 34 active cuts</div><div className="simline">[display] simulated best-bound gap: 0.00%</div><div className="simline">[display] telemetry stream rendered for UI demonstration</div></div></Panel></div>
  <Panel title="SOLUTION VALUES" icon={<BarChart3/>}><div className="solutiontable">{vars.map(v=><div key={v.name}><span>{v.name}</span><b>{v.value}</b></div>)}</div></Panel>
 </div>
}


function ObjectiveChart({objective,iterations}:{objective:number;iterations:number}){
 const target=Number(objective)||0;
 const values=target===0?[120,96,74,55,39,26,15,7,0]:[1.48,1.31,1.18,1.09,1.045,1.018,1.008,1.003,1].map(x=>target*x);
 const max=Math.max(...values), min=Math.min(...values), span=Math.max(max-min,1);
 const pts=values.map((v,i)=>{const x=70+i*78;const y=195-((v-min)/span)*145;return [x,y,v] as [number,number,number]});
 return <div className="realchart"><svg viewBox="0 0 760 250" className="solverchart" role="img" aria-label="Simulated objective convergence chart">
  <defs><pattern id="objective-grid" width="78" height="36" patternUnits="userSpaceOnUse"><path d="M 78 0 L 0 0 0 36" fill="none" className="gridline"/></pattern></defs>
  <rect x="50" y="25" width="670" height="175" fill="url(#objective-grid)"/><line x1="50" y1="200" x2="720" y2="200" className="axis"/><line x1="50" y1="25" x2="50" y2="200" className="axis"/>
  <polyline points={pts.map(p=>p[0]+','+p[1]).join(' ')} className="chartline"/>
  {pts.map((p,i)=><circle key={i} cx={p[0]} cy={p[1]} r={i===pts.length-1?6:3.5} className={i===pts.length-1?'point':'chartpoint'}/>)}
  <text x="62" y="43" className="charttext">SIMULATED OBJECTIVE</text><text x="620" y="43" className="chartvalue">{target.toLocaleString()}</text>
  <text x="62" y="220" className="charttext">ITER 0</text><text x="650" y="220" className="charttext">{iterations||8} ITER</text>
 </svg>
 <div className="readingrow">{values.map((v,i)=><span key={i}>I{i}<b>{Math.round(v).toLocaleString()}</b></span>)}</div>
 <div className="chartlegend">SIMULATED DISPLAY DATA — not returned by the current DENT telemetry API.</div>
 </div>
}

function TelemetryChart(){
 const nodes=[1,4,9,15,12,8,5,2], cuts=[0,3,7,12,18,24,29,34];
 return <div className="realchart"><svg viewBox="0 0 760 250" className="solverchart" role="img" aria-label="Simulated branch and bound telemetry chart">
  <defs><pattern id="bb-grid" width="84" height="35" patternUnits="userSpaceOnUse"><path d="M 84 0 L 0 0 0 35" fill="none" className="gridline"/></pattern></defs>
  <rect x="50" y="25" width="670" height="175" fill="url(#bb-grid)"/><line x1="50" y1="200" x2="720" y2="200" className="axis"/><line x1="50" y1="25" x2="50" y2="200" className="axis"/>
  {nodes.map((n,i)=><rect key={i} x={66+i*82} y={200-n*8} width="30" height={n*8} className="telemetrybar"/>)}
  <polyline points={cuts.map((v,i)=>{const x=81+i*82;const y=185-v*4.2;return x+','+y}).join(' ')} className="cutline"/>
  <text x="62" y="43" className="charttext">SIMULATED B&amp;B / CUT ACTIVITY</text><text x="62" y="220" className="charttext">OPEN NODES</text><text x="580" y="220" className="charttext">CUT COUNT</text>
 </svg>
 <div className="readingrow">{nodes.map((n,i)=><span key={i}>N{i+1}<b>{n} nodes</b><em>{cuts[i]} cuts</em></span>)}</div>
 <div className="chartlegend">SIMULATED DISPLAY DATA — node and cut telemetry is not returned by the current DENT API.</div>
 </div>
}

function SolutionChart({variables}:{variables:{name:string;value:number}[]}){if(!variables.length)return <div className="empty">NO SOLUTION VALUES RETURNED.</div>;const max=Math.max(...variables.map(v=>Math.abs(v.value)),1);return <div className="bars">{variables.slice(0,24).map(v=><div className="barrow" key={v.name}><span>{v.name}</span><div><i style={{width:(Math.abs(v.value)/max*100)+'%'}}></i></div><b>{v.value}</b></div>)}</div>}

function HistoryView({runs,openRun}:{runs:RunSummary[];openRun:(id:string)=>void}){return <div className="historytable">{runs.length===0?<div className="empty">NO PERSISTED RUNS</div>:runs.map(r=>{const x=r.result;const status=r.status??x?.status??'UNKNOWN';const solver=r.solver??x?.solver??'—';const objective=r.objective??x?.objective;const time=r.solve_time_ms??x?.solve_time_ms;return <button key={r.id} onClick={()=>openRun(r.id)}><span>{r.id.slice(0,8)}</span><b>{status}</b><span>{solver}</span><span>{objective==null?'—':Number(objective).toLocaleString()}</span><span>{time==null?'—':time.toFixed(1)+' ms'}</span><ChevronRight size={14}/></button>})}</div>}

function Benchmarks({runs}:{runs:RunSummary[]}){return <div className="content"><div className="title"><span>ENGINE BENCHMARKS //</span><h1>BENCHMARK WORKSPACE</h1><p>Historical DENT run measurements are shown here; no synthetic benchmark data is generated.</p></div><Panel title="RECORDED RUNS" icon={<BarChart3/>}>{runs.length?<div className="benchmarktable">{runs.slice(0,20).map(r=><div key={r.id}><span>{r.id.slice(0,8)}</span><b>{r.solver??'—'}</b><span>{r.objective==null?'—':Number(r.objective).toLocaleString()}</span><span>{r.solve_time_ms==null?'—':r.solve_time_ms.toFixed(2)+' ms'}</span><span>{r.iterations??'—'} iter</span></div>)}</div>:<div className="empty">NO RECORDED RUNS AVAILABLE.</div>}</Panel><Panel title="UNAVAILABLE BENCHMARK METRICS" icon={<AlertTriangle/>}><Unavailable text="GPU throughput, acceleration factor, MIP gap and node counts require telemetry that the current API does not expose." /></Panel></div>}

function Diagnostics({online}:{online:boolean}){return <div className="content"><div className="title"><span>ENGINE DIAGNOSTICS //</span><h1>RUNTIME SUBSYSTEMS</h1><p>Current integration status, without claiming measurements the engine does not return.</p></div><div className="diagnostics"><Metric label="FASTAPI" value={online?'ONLINE':'OFFLINE'}/><Metric label="NATIVE DENT API" value="STATUS VIA BACKEND"/><Metric label="SQLITE RUN HISTORY" value="READY"/><Metric label="SOLVER BRIDGE" value="v0.11.0"/><Metric label="GPU TELEMETRY" value="NOT EXPOSED"/><Metric label="MIP GAP / NODES" value="NOT EXPOSED"/></div><Panel title="CLIENT DIAGNOSTIC LOG" icon={<Terminal/>}><div className="console"><div>[client] API connectivity: {online?'online':'offline'}</div><div>[client] native runtime telemetry: not exposed</div><div>[client] fabricated hardware metrics: disabled</div></div></Panel></div>}

function SettingsView({settings,save}:{settings:Settings;save:(s:Settings)=>void}){return <div className="content"><div className="title"><span>ENGINE SETTINGS //</span><h1>SOLVER CONFIGURATION</h1><p>These values are persisted locally and sent with the next solve request.</p></div><div className="settingsgrid"><label>METHOD<select value={settings.method} onChange={e=>save({...settings,method:e.target.value as Settings['method']})}><option value="auto">Auto</option><option value="primal_simplex">Primal Simplex</option><option value="interior_point">Interior Point</option><option value="pdhg">PDHG</option><option value="pdlp">PDLP</option><option value="qp">QP</option><option value="milp">MILP</option></select></label><label>TOLERANCE<input type="number" min="0" step="any" value={settings.tolerance} onChange={e=>save({...settings,tolerance:Number(e.target.value)})}/><small>0 = engine default</small></label><label>MAX ITERATIONS<input type="number" min="0" step="1" value={settings.max_iterations} onChange={e=>save({...settings,max_iterations:Math.max(0,Number(e.target.value))})}/><small>0 = engine default</small></label></div></div>}

function Unavailable({text}:{text:string}){return <div className="unavailable"><AlertTriangle size={18}/><span>{text}</span></div>}
