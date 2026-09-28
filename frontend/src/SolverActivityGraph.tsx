import type {SolveResult} from "./types";

type Props={
  running:boolean;
  result:SolveResult|undefined;
};

export default function SolverActivityGraph({running,result}:Props){
  const done=!running&&!!result;
  return <div className={running?"solver-activity running":"solver-activity"}>
    <svg viewBox="0 0 100 52" preserveAspectRatio="none" aria-label={running?"Solver activity animation":"Solver activity"}>
      <path className="activity-grid" d="M0 12H100M0 26H100M0 40H100M20 0V52M40 0V52M60 0V52M80 0V52"/>
      <path className="activity-track" d="M0 38 C10 38 12 15 22 26 S35 42 45 22 S58 12 68 28 S82 43 100 14"/>
      {running&&<path className="activity-sweep" d="M0 38 C10 38 12 15 22 26 S35 42 45 22 S58 12 68 28 S82 43 100 14"/>}
      <line className="activity-cursor" x1={running?"4":"100"} y1="4" x2={running?"4":"100"} y2="48"/>
      <circle className="activity-node" cx={running?"4":"100"} cy="38" r="2.2"/>
    </svg>
    <div className="activity-caption">
      <span>{running?"ENGINE ACTIVE":"SOLVE COMPLETE"}</span>
      <b>{running?"processing model · output pending":result?result.status+" · objective "+fmt(result.objective):"ready"}</b>
    </div>
    {running&&<div className="activity-note">visual activity only — no synthetic solver measurements</div>}
    {done&&<div className="activity-note">rendered from the returned DENT solve result</div>}
  </div>
}

function fmt(value:number){
  if(!Number.isFinite(value)) return "—";
  return Math.abs(value)>=1000?value.toLocaleString(undefined,{maximumFractionDigits:3}):value.toFixed(4).replace(/0+$/,"").replace(/\.$/,"");
}
