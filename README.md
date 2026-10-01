# HoneyDen — SSH/Telnet honeypot with live monitor

> ⚠️ **WARNING: run only in Docker or a dedicated VM, never on your main system.**
> A honeypot invites hostile traffic. Isolate it (separate VM/container, firewall,
> no access to your real data or LAN). The app never needs root: it listens on a
> high port, traffic from 22/23 is redirected externally via nftables (see below).

HoneyDen accepts inbound TCP connections, shows a fake `login:` / `Password:`
prompt, always answers `Access denied`, and stores everything in PostgreSQL.
The GUI shows a live attack table + logins-per-second chart and keeps a history
of past runs (table + chart per run, rendered with the same widgets).

## Layout

```
CMakeLists.txt
config.example.ini   # copy to config.ini, fill in your secrets (never committed)
sql/schema.sql
src/
  main.cpp
  ui/    MainMenu, MonitorWindow, AttackTableModel, AttackChart, HistoryPanel
  core/  Listener (QTcpServer), Session, EventQueue, GeoLookup, Config
  db/    DbWriter (own QThread), DbReader (own QThread)
```

Network (`Listener`/`Session`) and PostgreSQL (`DbWriter`/`DbReader`) run in
their own threads; the GUI only gets queued signals/slots. Client input is
untrusted: line length caps, idle timeouts, max simultaneous sessions, and
prepared SQL queries only (`prepare()` + `bindValue()`).

## Dependencies (Arch Linux)

```bash
sudo pacman -S base-devel cmake git qt5-base qt5-charts postgresql libmaxminddb
```

Qt modules used: `Widgets Network Sql (QPSQL) Charts`.

## PostgreSQL setup (Arch)

```bash
# 1. Initialize the cluster (once):
sudo -u postgres initdb -D /var/lib/postgres/data

# 2. Start the service:
sudo systemctl enable --now postgresql

# 3. Create the role and database (set YOUR OWN password, it stays local):
sudo -u postgres createuser -P honeyden
sudo -u postgres createdb -O honeyden honeyden

# 4. Create tables:
psql -h 127.0.0.1 -U honeyden -d honeyden -f sql/schema.sql
```

## GeoIP setup (optional)

1. Download `GeoLite2-City.mmdb` from MaxMind (free account + license key).
2. Put it anywhere, e.g. next to the binary, and set `geoip/path` in `config.ini`.
3. If the file is missing, the app still works — country/city stay empty.

## Config

```bash
cp config.example.ini config.ini
$EDITOR config.ini
```

```ini
[honeypot]
port=2222              # > 1024, no root needed
max_sessions=50
session_timeout_sec=60
max_line_length=256

[database]
host=127.0.0.1
port=5432
name=honeyden
user=honeyden
password=YOUR_PASSWORD_HERE   # never commit this file

[geoip]
path=GeoLite2-City.mmdb
```

## Build & run

```bash
cmake -B build -S .
cmake --build build
./build/honeyden
```

Click **«Запустить ханипот»** → live table + chart. **«Остановить сканирование»**
closes the listener, flushes the queue, stamps `runs.ended_at`, and opens the
**История** tab with past runs (click a run for its table + chart).

Smoke test (second terminal):

```bash
nc 127.0.0.1 2222
# type any login + password, expect: Access denied
psql -h 127.0.0.1 -U honeyden -d honeyden -c "SELECT username,password FROM auth_attempts ORDER BY id DESC LIMIT 5;"
```

## Redirecting ports 22/23 (outside the app, needs root)

```bash
# Example nftables: forward WAN SSH probes to the honeypot on 2222
sudo nft add table inet honeyden
sudo nft add chain inet honeyden pre '{ type nat hook prerouting priority -100; }'
sudo nft add rule inet honeyden pre tcp dport 22 redirect to :2222
```

## DB schema

`runs(id, started_at, ended_at)`,
`sessions(id, run_id, ip inet, port, country, city, started_at, ended_at)`,
`auth_attempts(id, session_id, username, password, ts)` + indexes on
`run_id` and `ts`. See `sql/schema.sql`.
