import type {SolveRequest} from "./types";

export const sampleMILP:SolveRequest={
 objective:"maximize",
 variables:[
  {name:"BatchA",lower_bound:0,upper_bound:0,type:"integer",objective_coefficient:31},
  {name:"BatchB",lower_bound:0,upper_bound:0,type:"integer",objective_coefficient:27},
  {name:"BatchC",lower_bound:0,upper_bound:0,type:"integer",objective_coefficient:34},
  {name:"BatchD",lower_bound:0,upper_bound:0,type:"integer",objective_coefficient:43}
 ],
 constraints:[
  {name:"capacity",sense:0,rhs:120,coefficients:{BatchA:3,BatchB:2,BatchC:4,BatchD:1}},
  {name:"labor",sense:0,rhs:180,coefficients:{BatchA:2,BatchB:4,BatchC:3,BatchD:5}},
  {name:"minimum_a",sense:2,rhs:5,coefficients:{BatchA:1}}
 ],
 quadratic_terms:[],
 solver:{method:"milp",tolerance:0,max_iterations:0}
};

export const sampleQP:SolveRequest={
 objective:"minimize",
 variables:[
  {name:"x",lower_bound:0,upper_bound:0,type:"continuous",objective_coefficient:-4},
  {name:"y",lower_bound:0,upper_bound:0,type:"continuous",objective_coefficient:-6}
 ],
 constraints:[
  {name:"budget",sense:0,rhs:5,coefficients:{x:1,y:1}}
 ],
 quadratic_terms:[
  {row:"x",column:"x",coefficient:2},
  {row:"y",column:"y",coefficient:2}
 ],
 solver:{method:"qp",tolerance:0,max_iterations:0}
};

export const sampleModel=sampleMILP;
