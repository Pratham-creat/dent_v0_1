from pathlib import Path
import os
import sqlite3


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DATABASE = ROOT / "backend" / "data" / "dent_runs.sqlite3"


def database_path():
    configured = os.getenv("DENT_DATABASE_PATH")
    return Path(configured) if configured else DEFAULT_DATABASE


def get_connection():
    path = database_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(path)
    connection.row_factory = sqlite3.Row
    return connection


def initialize_database():
    with get_connection() as connection:
        connection.execute(
            """
            CREATE TABLE IF NOT EXISTS runs (
                id TEXT PRIMARY KEY,
                created_at TEXT NOT NULL,
                source_type TEXT NOT NULL,
                source_name TEXT,
                status TEXT NOT NULL,
                objective REAL,
                solver TEXT,
                iterations INTEGER,
                solve_time_ms REAL,
                message TEXT,
                result_json TEXT NOT NULL
            )
            """
        )
        columns = {
            row["name"]
            for row in connection.execute("PRAGMA table_info(runs)").fetchall()
        }
        if "solve_time_ms" not in columns:
            connection.execute(
                "ALTER TABLE runs ADD COLUMN solve_time_ms REAL"
            )

        connection.execute(
            "CREATE INDEX IF NOT EXISTS idx_runs_created_at ON runs(created_at DESC)"
        )
        connection.commit()
