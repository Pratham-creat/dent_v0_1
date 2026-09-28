export const API_URL = import.meta.env.VITE_API_URL || 'http://127.0.0.1:8000';

export type SolveResult = {
  status: string; objective: number; solver: string; iterations: number;
  message: string; solve_time_ms?: number; variables: {name:string; value:number}[];
  fingerprint: {variables:number; constraints:number; nonzeros:number; density:number; mixed_integer:boolean; quadratic:boolean};
  configuration?: {method:string; tolerance:number; max_iterations:number};
};
export type RunResponse = {id:string; created_at:string; source_type:string; source_name?:string; result:SolveResult};

export async function health(){const r=await fetch(API_URL+'/health'); if(!r.ok) throw new Error('Backend unavailable'); return r.json();}
export async function solveModel(model:any):Promise<RunResponse>{
  const r=await fetch(API_URL+'/api/v1/solve',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(model)});
  const body=await r.json(); if(!r.ok) throw new Error(body.detail || 'Solve request failed'); return body;
}
export async function getRuns(){const r=await fetch(API_URL+'/api/v1/runs?limit=50'); if(!r.ok) throw new Error('History unavailable'); return r.json();}
export async function getRun(id:string):Promise<RunResponse>{const r=await fetch(API_URL+'/api/v1/runs/'+encodeURIComponent(id)); if(!r.ok) throw new Error('Run not found'); return r.json();}

export async function benchmarkModel(request:any){const r=await fetch(API_URL+'/api/v1/benchmark',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(request)});const body=await r.json();if(!r.ok)throw new Error(body.detail||'Benchmark request failed');return body;}
