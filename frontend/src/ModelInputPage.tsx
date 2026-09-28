import {useMemo, useState} from "react";
import {Check, FileUp, Play, RotateCcw} from "lucide-react";
import type {SolveRequest} from "./types";
import type {TestCase} from "./testCases";
import {testCases} from "./testCases";

type Mode="library"|"json"|"file";

export default function ModelInputPage({
  model,
  setModel,
  running,
  onRun,
  onFileRun,
  onBack,
}:{
  model:SolveRequest;
  setModel:(model:SolveRequest)=>void;
  running:boolean;
  onRun:(model:SolveRequest)=>Promise<void>;
  onFileRun:(file:File)=>Promise<void>;
  onBack:()=>void;
}){
  const [mode,setMode]=useState<Mode>("library");
  const [json,setJson]=useState(JSON.stringify(model,null,2));
  const [selected,setSelected]=useState(testCases[0].id);
  const [file,setFile]=useState<File|null>(null);
  const [message,setMessage]=useState("");
  const [jsonError,setJsonError]=useState("");

  const current=useMemo(()=>testCases.find(t=>t.id===selected)??testCases[0],[selected]);

  const loadCase=(test:TestCase)=>{
    setSelected(test.id);
    setModel(test.model);
    setJson(JSON.stringify(test.model,null,2));
    setJsonError("");
    setMessage("Loaded "+test.name+".");
  };

  const loadJson=()=>{
    try{
      const parsed=JSON.parse(json) as SolveRequest;
      if(!parsed || typeof parsed!=="object") throw new Error("JSON root must be an object.");
      setModel(parsed);
      setJson(JSON.stringify(parsed,null,2));
      setJsonError("");
      setMessage("JSON model loaded into DENT.");
    }catch(error){
      setJsonError(error instanceof Error?error.message:"Invalid JSON.");
      setMessage("");
    }
  };

  const resetJson=()=>{
    setJson(JSON.stringify(model,null,2));
    setJsonError("");
    setMessage("Editor reset to the current model.");
  };

  return <div className="input-page">
    <div className="input-head">
      <button className="back-link" onClick={onBack}>← workbench</button>
      <div>
        <small>DENT / MODEL INPUT</small>
        <h1>model input</h1>
        <p>Load a test case, paste a JSON model, or upload a native .dent model. All three paths use the same solver API.</p>
      </div>
      <div className="input-actions">
        <button className="secondary" onClick={resetJson}><RotateCcw size={12}/> reset editor</button>
        <button className="primary" disabled={running} onClick={()=>void onRun(model)}><Play size={12} fill="currentColor"/> run current model</button>
      </div>
    </div>

    <div className="input-tabs">
      <button className={mode==="library"?"active":""} onClick={()=>setMode("library")}>test library</button>
      <button className={mode==="json"?"active":""} onClick={()=>setMode("json")}>JSON editor</button>
      <button className={mode==="file"?"active":""} onClick={()=>setMode("file")}>.dent file</button>
    </div>

    {mode==="library"&&<div className="input-library">
      <section className="input-list panel">
        <header><span>built-in test cases</span><b>{testCases.length} cases</b></header>
        {testCases.map(test=><button className={selected===test.id?"test-card active":"test-card"} key={test.id} onClick={()=>loadCase(test)}>
          <span className="test-kind">{test.kind}</span>
          <span className="test-name">{test.name}</span>
          <small>{test.description}</small>
          <em>expected: {test.expected}</em>
        </button>)}
      </section>
      <section className="panel input-preview">
        <header><span>selected model</span><b>{current.kind}</b></header>
        <div className="preview-meta">
          <div><small>objective</small><strong>{current.model.objective}</strong></div>
          <div><small>variables</small><strong>{current.model.variables.length}</strong></div>
          <div><small>constraints</small><strong>{current.model.constraints.length}</strong></div>
          <div><small>solver</small><strong>{current.model.solver.method}</strong></div>
        </div>
        <pre>{JSON.stringify(current.model,null,2)}</pre>
        <div className="input-preview-actions">
          <button className="secondary" onClick={()=>{setModel(current.model);setJson(JSON.stringify(current.model,null,2));setMessage("Loaded "+current.name+".")}}><Check size={12}/> load into editor</button>
          <button className="primary" disabled={running} onClick={()=>void onRun(current.model)}><Play size={12} fill="currentColor"/> run test case</button>
        </div>
      </section>
    </div>}

    {mode==="json"&&<section className="panel json-editor-panel">
      <header><span>JSON model</span><b>POST /api/v1/solve</b></header>
      <textarea spellCheck={false} value={json} onChange={e=>{setJson(e.target.value);setJsonError("");setMessage("")}}/>
      {jsonError&&<div className="input-error">{jsonError}</div>}
      <div className="json-actions">
        <button className="secondary" onClick={resetJson}>reset</button>
        <button className="primary" onClick={loadJson}><Check size={12}/> load JSON</button>
        <button className="primary" disabled={running} onClick={()=>void onRun(model)}><Play size={12} fill="currentColor"/> run loaded model</button>
      </div>
    </section>}

    {mode==="file"&&<section className="panel file-panel">
      <header><span>native DENT file input</span><b>POST /api/v1/solve-file</b></header>
      <label className="dropzone">
        <FileUp size={22}/>
        <strong>{file?file.name:"choose a .dent model file"}</strong>
        <small>Only .dent files are accepted by the current backend.</small>
        <input type="file" accept=".dent" onChange={e=>setFile(e.target.files?.[0]??null)}/>
      </label>
      <div className="file-actions">
        <button className="primary" disabled={!file||running} onClick={()=>file&&void onFileRun(file)}><Play size={12} fill="currentColor"/> solve file</button>
      </div>
    </section>}

    {message&&<div className="input-message">{message}</div>}
  </div>;
}
