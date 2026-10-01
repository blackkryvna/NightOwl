-- HoneyDen schema (PostgreSQL).
-- Runs: one row per honeypot launch. ended_at is set on clean stop.

CREATE TABLE IF NOT EXISTS runs (
    id         SERIAL PRIMARY KEY,
    started_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    ended_at   TIMESTAMPTZ
);

CREATE TABLE IF NOT EXISTS sessions (
    id         SERIAL PRIMARY KEY,
    run_id     INTEGER NOT NULL REFERENCES runs (id) ON DELETE CASCADE,
    ip         INET,
    port       INTEGER,
    country    TEXT,
    city       TEXT,
    started_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    ended_at   TIMESTAMPTZ
);
CREATE INDEX IF NOT EXISTS idx_sessions_run_id ON sessions (run_id);

CREATE TABLE IF NOT EXISTS auth_attempts (
    id         SERIAL PRIMARY KEY,
    session_id INTEGER NOT NULL REFERENCES sessions (id) ON DELETE CASCADE,
    username   TEXT,
    password   TEXT,
    ts         TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS idx_auth_session_id ON auth_attempts (session_id);
CREATE INDEX IF NOT EXISTS idx_auth_ts ON auth_attempts (ts);
