import type {SolveRequest} from "./types";

export interface TestCase {
  id: string;
  name: string;
  kind: "LP" | "MILP" | "QP";
  description: string;
  expected: string;
  model: SolveRequest;
}

export const testCases: TestCase[] = [
  {
    id: "lp-production",
    name: "production planning",
    kind: "LP",
    description: "Maximize profit from two products under one capacity constraint.",
    expected: "optimal · objective 15",
    model: {
      objective: "maximize",
      variables: [
        {name: "x", lower_bound: 0, upper_bound: 0, type: "continuous", objective_coefficient: 3},
        {name: "y", lower_bound: 0, upper_bound: 0, type: "continuous", objective_coefficient: 2}
      ],
      constraints: [
        {name: "capacity", sense: 0, rhs: 10, coefficients: {x: 2, y: 1}}
      ],
      quadratic_terms: [],
      solver: {method: "primal_simplex", tolerance: 0, max_iterations: 0}
    }
  },
  {
    id: "lp-transport",
    name: "minimum allocation",
    kind: "LP",
    description: "Minimize allocation cost while meeting a minimum requirement.",
    expected: "optimal · objective 10",
    model: {
      objective: "minimize",
      variables: [
        {name: "a", lower_bound: 0, upper_bound: 0, type: "continuous", objective_coefficient: 1},
        {name: "b", lower_bound: 0, upper_bound: 0, type: "continuous", objective_coefficient: 1}
      ],
      constraints: [
        {name: "requirement", sense: 2, rhs: 10, coefficients: {a: 1, b: 1}}
      ],
      quadratic_terms: [],
      solver: {method: "primal_simplex", tolerance: 0, max_iterations: 0}
    }
  },
  {
    id: "milp-production",
    name: "integer production",
    kind: "MILP",
    description: "Integer production plan with capacity, labor, and minimum-production constraints.",
    expected: "optimal · objective 2533",
    model: {
      objective: "maximize",
      variables: [
        {name: "BatchA", lower_bound: 0, upper_bound: 0, type: "integer", objective_coefficient: 31},
        {name: "BatchB", lower_bound: 0, upper_bound: 0, type: "integer", objective_coefficient: 27},
        {name: "BatchC", lower_bound: 0, upper_bound: 0, type: "integer", objective_coefficient: 34},
        {name: "BatchD", lower_bound: 0, upper_bound: 0, type: "integer", objective_coefficient: 43}
      ],
      constraints: [
        {name: "capacity", sense: 0, rhs: 120, coefficients: {BatchA: 3, BatchB: 2, BatchC: 4, BatchD: 1}},
        {name: "labor", sense: 0, rhs: 180, coefficients: {BatchA: 2, BatchB: 4, BatchC: 3, BatchD: 5}},
        {name: "minimum_a", sense: 2, rhs: 5, coefficients: {BatchA: 1}}
      ],
      quadratic_terms: [],
      solver: {method: "milp", tolerance: 0, max_iterations: 0}
    }
  },
  {
    id: "qp-small",
    name: "small quadratic program",
    kind: "QP",
    description: "Convex quadratic objective with a linear budget constraint.",
    expected: "optimal · objective about -6.5",
    model: {
      objective: "minimize",
      variables: [
        {name: "x", lower_bound: 0, upper_bound: 0, type: "continuous", objective_coefficient: -4},
        {name: "y", lower_bound: 0, upper_bound: 0, type: "continuous", objective_coefficient: -6}
      ],
      constraints: [
        {name: "budget", sense: 0, rhs: 5, coefficients: {x: 1, y: 1}}
      ],
      quadratic_terms: [
        {row: "x", column: "x", coefficient: 2},
        {row: "y", column: "y", coefficient: 2}
      ],
      solver: {method: "qp", tolerance: 0, max_iterations: 0}
    }
  },
  {
    id: "infeasible",
    name: "infeasible constraints",
    kind: "LP",
    description: "Deliberately contradictory constraints; use this to test failure handling.",
    expected: "infeasible",
    model: {
      objective: "minimize",
      variables: [
        {name: "x", lower_bound: 0, upper_bound: 0, type: "continuous", objective_coefficient: 1},
        {name: "y", lower_bound: 0, upper_bound: 0, type: "continuous", objective_coefficient: 1}
      ],
      constraints: [
        {name: "upper_limit", sense: 0, rhs: 5, coefficients: {x: 1, y: 1}},
        {name: "lower_requirement", sense: 2, rhs: 10, coefficients: {x: 1, y: 1}}
      ],
      quadratic_terms: [],
      solver: {method: "auto", tolerance: 0, max_iterations: 0}
    }
  }
];
