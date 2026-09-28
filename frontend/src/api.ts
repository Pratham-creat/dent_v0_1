export const API_URL=import.meta.env.VITE_API_URL||'http://127.0.0.1:8000';
export type SolveResult={status:string;objective:number;solver:string;iterations:number;message:string;solve_time_ms?:number;variables:{name:string;value:number}[];fingerprint:{variables:number;constraints:number;nonzeros:number;density:number;mixed_integer:boolean;quadratic:boolean};configuration?:{method:string;tolerance:number;max_iterations:number}};
export type RunResponse={id:string;created_at:string;source_type:string;source_name?:string;result:SolveResult};
async function readError(r:Response){const b=await r.json().catch(()=>({}));throw new Error(b.detail||b.message||'Request failed')}
export async function health(){const r=await fetch(API_URL+'/health');if(!r.ok)await readError(r);return r.json()}
export async function solveModel(model:any):Promise<RunResponse>{const r=await fetch(API_URL+'/api/v1/solve',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(model)});if(!r.ok)await readError(r);return r.json()}
export async function solveFile(file:File,o:{method:string;tolerance:number;max_iterations:number}):Promise<RunResponse>{const q=new URLSearchParams({method:o.method,tolerance:String(o.tolerance),max_iterations:String(o.max_iterations)});const f=new FormData();f.append('file',file);const r=await fetch(API_URL+'/api/v1/solve-file?'+q,{method:'POST',body:f});if(!r.ok)await readError(r);return r.json()}
export async function getRuns(){const r=await fetch(API_URL+'/api/v1/runs?limit=50');if(!r.ok)await readError(r);return r.json()}
export async function getRun(id:string):Promise<RunResponse>{const r=await fetch(API_URL+'/api/v1/runs/'+encodeURIComponent(id));if(!r.ok)await readError(r);return r.json()}
