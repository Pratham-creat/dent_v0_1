import type {RunResponse,RunSummary,SolveRequest,Telemetry} from "./types";
const API_URL=(import.meta.env.VITE_API_URL||"http://127.0.0.1:8000").replace(/\/$/,"");
async function request<T>(path:string,options?:RequestInit):Promise<T>{const response=await fetch(API_URL+path,{headers:{"Content-Type":"application/json",...(options?.headers||{})},...options});const body=await response.json().catch(()=>null);if(!response.ok)throw new Error(body?.detail||"Request failed with HTTP "+response.status);return body as T}
export const solveModel=(model:SolveRequest)=>request<RunResponse>("/api/v1/solve",{method:"POST",body:JSON.stringify(model)});
export const getRuns=()=>request<{runs:RunSummary[];limit:number;offset:number}>("/api/v1/runs?limit=20&offset=0");
export const getTelemetry=()=>request<Telemetry>("/api/v1/runs/telemetry");
export const getHealth=()=>request<{status:string;native_api:boolean}>("/health");