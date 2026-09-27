import json

from storage.database import get_connection, initialize_database


class RunRepository:
    def __init__(self):
        initialize_database()

    def save(self, run):
        with get_connection() as connection:
            connection.execute(
                """
                INSERT INTO runs (
                    id, created_at, source_type, source_name, status,
                    objective, solver, iterations, solve_time_ms, message, result_json
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    run["id"],
                    run["created_at"],
                    run["source_type"],
                    run.get("source_name"),
                    run["result"].get("status", "unknown"),
                    run["result"].get("objective"),
                    run["result"].get("solver"),
                    run["result"].get("iterations"),
                    run["result"].get("solve_time_ms"),
                    run["result"].get("message"),
                    json.dumps(run["result"]),
                ),
            )
            connection.commit()
        return run

    def list(self, limit=50, offset=0):
        with get_connection() as connection:
            rows = connection.execute(
                """
                SELECT id, created_at, source_type, source_name, status,
                       objective, solver, iterations, solve_time_ms, message
                FROM runs
                ORDER BY created_at DESC
                LIMIT ? OFFSET ?
                """,
                (limit, offset),
            ).fetchall()
        return [dict(row) for row in rows]

    def get(self, run_id):
        with get_connection() as connection:
            row = connection.execute(
                "SELECT * FROM runs WHERE id = ?", (run_id,)
            ).fetchone()

        if not row:
            return None

        result = dict(row)
        result["result"] = json.loads(result.pop("result_json"))
        return result

    def delete(self, run_id):
        with get_connection() as connection:
            cursor = connection.execute(
                "DELETE FROM runs WHERE id = ?", (run_id,)
            )
            connection.commit()
        return cursor.rowcount > 0

    def clear(self):
        with get_connection() as connection:
            cursor = connection.execute("DELETE FROM runs")
            connection.commit()
        return cursor.rowcount


    def telemetry(self):
        with get_connection() as connection:
            summary = connection.execute(
                """
                SELECT
                    COUNT(*) AS total_runs,
                    SUM(CASE WHEN status = 'optimal' THEN 1 ELSE 0 END) AS optimal_runs,
                    AVG(solve_time_ms) AS average_solve_time_ms,
                    MIN(solve_time_ms) AS minimum_solve_time_ms,
                    MAX(solve_time_ms) AS maximum_solve_time_ms,
                    AVG(iterations) AS average_iterations,
                    SUM(CASE WHEN source_type = 'json' THEN 1 ELSE 0 END) AS json_runs,
                    SUM(CASE WHEN source_type = 'file' THEN 1 ELSE 0 END) AS file_runs
                FROM runs
                """
            ).fetchone()

            solvers = connection.execute(
                """
                SELECT solver, COUNT(*) AS runs, AVG(solve_time_ms) AS average_solve_time_ms,
                       AVG(iterations) AS average_iterations
                FROM runs
                WHERE solver IS NOT NULL
                GROUP BY solver
                ORDER BY runs DESC, solver ASC
                """
            ).fetchall()

        return {
            "summary": dict(summary),
            "by_solver": [dict(row) for row in solvers],
        }
