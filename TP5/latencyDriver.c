#!/usr/bin/env python3

from http.server import BaseHTTPRequestHandler, HTTPServer
from collections import deque
from threading import Thread, Lock
import json
import time
import os

DEV_PATH = "/dev/tp5_signal"
HOST = "0.0.0.0"
PORT = 5000

POLL_INTERVAL_SECONDS = 0.050   # Python lee /dev cada 50 ms
DISPLAY_WINDOW_SECONDS = 5      # Escala fija visible del gráfico
MAX_HISTORY_SECONDS = 60        # Histórico guardado en memoria

MAX_HISTORY = int(MAX_HISTORY_SECONDS / POLL_INTERVAL_SECONDS)

history = deque(maxlen=MAX_HISTORY)
history_lock = Lock()
state_lock = Lock()

selected_channel = 0
sampling_enabled = True
start_time = time.monotonic()
running = True


HTML = r"""
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <title>TP5 - Latency Driver</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

    <style>
        :root {
            --bg: #0f172a;
            --panel: #111827;
            --panel-2: #1f2937;
            --text: #e5e7eb;
            --muted: #9ca3af;
            --accent: #38bdf8;
            --accent-2: #22c55e;
            --danger: #ef4444;
            --warning: #f59e0b;
            --border: #374151;
        }

        body {
            font-family: Arial, sans-serif;
            background: radial-gradient(circle at top, #1e293b 0%, #020617 60%);
            color: var(--text);
            margin: 0;
            padding: 30px;
        }

        .container {
            max-width: 1180px;
            margin: auto;
            background: rgba(17, 24, 39, 0.96);
            padding: 24px;
            border-radius: 16px;
            border: 1px solid var(--border);
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.45);
        }

        h1 {
            margin-top: 0;
            margin-bottom: 6px;
            font-size: 28px;
        }

        .subtitle {
            color: var(--muted);
            margin-bottom: 22px;
        }

        .buttons {
            display: flex;
            flex-wrap: wrap;
            gap: 10px;
            margin-bottom: 18px;
        }

        button {
            padding: 10px 18px;
            border: 1px solid transparent;
            border-radius: 10px;
            cursor: pointer;
            color: white;
            font-size: 15px;
            background: #2563eb;
        }

        button:hover {
            filter: brightness(1.15);
        }

        button.active {
            background: var(--accent-2);
        }

        button.stop {
            background: var(--danger);
        }

        button.resume {
            background: var(--warning);
        }

        .info {
            display: flex;
            flex-wrap: wrap;
            gap: 12px;
            margin: 16px 0 22px 0;
            font-size: 15px;
        }

        .badge {
            background: var(--panel-2);
            border: 1px solid var(--border);
            padding: 9px 12px;
            border-radius: 10px;
            color: var(--text);
        }

        .badge strong {
            color: var(--accent);
        }

        .chart-wrap {
            height: 480px;
            background: #020617;
            border: 1px solid var(--border);
            border-radius: 14px;
            padding: 16px;
        }

        canvas {
            width: 100%;
            height: 100%;
        }

        .note {
            margin-top: 16px;
            color: var(--muted);
            font-size: 14px;
            line-height: 1.45;
        }

        .error {
            color: #fca5a5;
            margin-top: 10px;
        }

        code {
            color: #93c5fd;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>TP5 - Latency Driver</h1>
        <div class="subtitle">
            Lectura de señales externas desde <code>/dev/tp5_signal</code> mediante un CDD.
        </div>

        <div class="buttons">
            <button id="btn0" onclick="changeChannel(0)">Canal 0 - GPIO17</button>
            <button id="btn1" onclick="changeChannel(1)">Canal 1 - GPIO27</button>
            <button id="samplingBtn" class="stop" onclick="toggleSampling()">Detener toma de muestras</button>
        </div>

        <div class="info">
            <div class="badge">
                <strong>Canal seleccionado:</strong>
                <span id="currentChannel">0</span>
            </div>

            <div class="badge">
                <strong>Último valor:</strong>
                <span id="lastValue">-</span>
            </div>

            <div class="badge">
                <strong>Estado:</strong>
                <span id="samplingState">Tomando muestras</span>
            </div>

            <div class="badge">
                <strong>Histórico:</strong>
                <span id="historyCount">0</span> muestras
            </div>

            <div class="badge">
                <strong>Ventana visible:</strong>
                <span id="windowSeconds">5</span> s
            </div>

            <div class="badge">
                <strong>Lectura Python:</strong>
                cada 50 ms
            </div>
        </div>

        <div class="chart-wrap">
            <canvas id="signalChart"></canvas>
        </div>

        <div class="note">
            El servidor Python lee el CDD cada 50 ms y guarda un histórico en memoria.
            El gráfico no intenta mostrar todo el histórico: usa una escala fija de 5 segundos,
            suficiente para visualizar señales cuadradas de período 1 s y 0.5 s.
            Al cambiar de canal, se escribe el nuevo canal en <code>/dev/tp5_signal</code> y se limpia el histórico.
        </div>

        <div id="errorBox" class="error"></div>
    </div>

    <script>
        let selectedChannel = 0;
        let samplingEnabled = true;
        let displayWindowSeconds = 5;

        const ctx = document.getElementById("signalChart").getContext("2d");

        const chart = new Chart(ctx, {
            type: "line",
            data: {
                datasets: [{
                    label: "Canal 0 - GPIO17",
                    data: [],
                    stepped: true,
                    tension: 0,
                    pointRadius: 0,
                    borderWidth: 2,
                    borderColor: "#38bdf8",
                    backgroundColor: "rgba(56, 189, 248, 0.18)"
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false,
                parsing: false,
                plugins: {
                    legend: {
                        display: true,
                        labels: {
                            color: "#e5e7eb"
                        }
                    },
                    tooltip: {
                        callbacks: {
                            label: function(context) {
                                return "Valor lógico: " + context.raw.y;
                            }
                        }
                    }
                },
                scales: {
                    x: {
                        type: "linear",
                        min: 0,
                        max: displayWindowSeconds,
                        title: {
                            display: true,
                            text: "Tiempo [s]",
                            color: "#e5e7eb"
                        },
                        ticks: {
                            color: "#9ca3af",
                            stepSize: 0.5
                        },
                        grid: {
                            color: "rgba(148, 163, 184, 0.18)"
                        }
                    },
                    y: {
                        min: -0.1,
                        max: 1.1,
                        title: {
                            display: true,
                            text: "Valor lógico",
                            color: "#e5e7eb"
                        },
                        ticks: {
                            color: "#9ca3af",
                            stepSize: 1
                        },
                        grid: {
                            color: "rgba(148, 163, 184, 0.18)"
                        }
                    }
                }
            }
        });

        function setActiveButton(channel) {
            document.getElementById("btn0").classList.remove("active");
            document.getElementById("btn1").classList.remove("active");

            if (channel === 0) {
                document.getElementById("btn0").classList.add("active");
            } else {
                document.getElementById("btn1").classList.add("active");
            }
        }

        function setSamplingButton(enabled) {
            const btn = document.getElementById("samplingBtn");

            samplingEnabled = enabled;

            if (enabled) {
                btn.innerText = "Detener toma de muestras";
                btn.classList.remove("resume");
                btn.classList.add("stop");
                document.getElementById("samplingState").innerText = "Tomando muestras";
            } else {
                btn.innerText = "Reanudar toma de muestras";
                btn.classList.remove("stop");
                btn.classList.add("resume");
                document.getElementById("samplingState").innerText = "Detenido";
            }
        }

        function resetChart(channel) {
            chart.data.datasets[0].data = [];

            chart.data.datasets[0].label =
                channel === 0
                    ? "Canal 0 - GPIO17"
                    : "Canal 1 - GPIO27";

            chart.update();

            document.getElementById("lastValue").innerText = "-";
            document.getElementById("historyCount").innerText = "0";
        }

        async function changeChannel(channel) {
            try {
                const response = await fetch("/api/channel", {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/json"
                    },
                    body: JSON.stringify({ channel: channel })
                });

                const data = await response.json();

                if (!response.ok) {
                    document.getElementById("errorBox").innerText = data.error || "Error al cambiar canal";
                    return;
                }

                selectedChannel = channel;

                document.getElementById("currentChannel").innerText = channel;
                document.getElementById("errorBox").innerText = "";

                setActiveButton(channel);
                resetChart(channel);

            } catch (error) {
                document.getElementById("errorBox").innerText = error;
            }
        }

        async function toggleSampling() {
            try {
                const response = await fetch("/api/sampling", {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/json"
                    },
                    body: JSON.stringify({ enabled: !samplingEnabled })
                });

                const data = await response.json();

                if (!response.ok) {
                    document.getElementById("errorBox").innerText = data.error || "Error cambiando estado de muestreo";
                    return;
                }

                setSamplingButton(data.sampling_enabled);
                document.getElementById("errorBox").innerText = "";

            } catch (error) {
                document.getElementById("errorBox").innerText = error;
            }
        }

        function buildFixedWindow(samples) {
            if (samples.length === 0) {
                return [];
            }

            const latestMs = samples[samples.length - 1].elapsed_ms;
            const windowMs = displayWindowSeconds * 1000;
            const windowStartMs = Math.max(0, latestMs - windowMs);

            return samples
                .filter(s => s.value !== null && s.elapsed_ms >= windowStartMs)
                .map(s => ({
                    x: (s.elapsed_ms - windowStartMs) / 1000,
                    y: s.value
                }));
        }

        async function refreshHistory() {
            try {
                const response = await fetch("/api/history");
                const data = await response.json();

                if (!response.ok) {
                    document.getElementById("errorBox").innerText = data.error || "Error leyendo histórico";
                    return;
                }

                displayWindowSeconds = data.display_window_seconds;
                document.getElementById("windowSeconds").innerText = displayWindowSeconds;
                chart.options.scales.x.max = displayWindowSeconds;

                const samples = data.history.filter(s => s.value !== null);
                const chartData = buildFixedWindow(samples);

                chart.data.datasets[0].data = chartData;
                chart.update();

                document.getElementById("historyCount").innerText = data.count;
                setSamplingButton(data.sampling_enabled);

                if (samples.length > 0) {
                    const last = samples[samples.length - 1];
                    document.getElementById("currentChannel").innerText = last.channel;
                    document.getElementById("lastValue").innerText = last.value;
                }

                document.getElementById("errorBox").innerText = "";

            } catch (error) {
                document.getElementById("errorBox").innerText = error;
            }
        }

        setActiveButton(0);
        changeChannel(0);

        /*
         * El servidor Python lee /dev cada 50 ms.
         * El navegador solo refresca el gráfico cada 100 ms.
         */
        setInterval(refreshHistory, 100);
    </script>
</body>
</html>
"""


def write_channel(channel: int):
    if channel not in (0, 1):
        raise ValueError("El canal debe ser 0 o 1")

    with open(DEV_PATH, "w") as dev:
        dev.write(str(channel))


def read_driver():
    with open(DEV_PATH, "r") as dev:
        line = dev.readline().strip()

    parts = line.split(",")

    if len(parts) != 3:
        raise ValueError(f"Formato inválido desde {DEV_PATH}: {line}")

    driver_channel = int(parts[0])
    value = int(parts[1])
    driver_timestamp_ms = int(parts[2])

    now = time.monotonic()
    elapsed_ms = int((now - start_time) * 1000)

    return {
        "channel": driver_channel,
        "value": value,
        "driver_timestamp_ms": driver_timestamp_ms,
        "elapsed_ms": elapsed_ms,
        "elapsed_s": round(elapsed_ms / 1000, 3),
    }


def sampler_loop():
    global running

    while running:
        with state_lock:
            enabled = sampling_enabled

        if not enabled:
            time.sleep(POLL_INTERVAL_SECONDS)
            continue

        try:
            sample = read_driver()
        except Exception as e:
            with state_lock:
                channel = selected_channel

            sample = {
                "channel": channel,
                "value": None,
                "driver_timestamp_ms": None,
                "elapsed_ms": int((time.monotonic() - start_time) * 1000),
                "elapsed_s": round(time.monotonic() - start_time, 3),
                "error": str(e),
            }

        with history_lock:
            history.append(sample)

        time.sleep(POLL_INTERVAL_SECONDS)


class Handler(BaseHTTPRequestHandler):
    def send_json(self, data, status=200):
        payload = json.dumps(data).encode("utf-8")

        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def send_html(self, html, status=200):
        payload = html.encode("utf-8")

        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def do_GET(self):
        if self.path == "/":
            self.send_html(HTML)
            return

        if self.path == "/api/history":
            with history_lock:
                data = list(history)

            with state_lock:
                current_channel = selected_channel
                enabled = sampling_enabled

            self.send_json({
                "history": data,
                "count": len(data),
                "selected_channel": current_channel,
                "sampling_enabled": enabled,
                "poll_interval_ms": int(POLL_INTERVAL_SECONDS * 1000),
                "display_window_seconds": DISPLAY_WINDOW_SECONDS,
                "max_history_seconds": MAX_HISTORY_SECONDS,
            })
            return

        self.send_json({"error": "Not found"}, status=404)

    def do_POST(self):
        if self.path == "/api/channel":
            self.handle_channel_change()
            return

        if self.path == "/api/sampling":
            self.handle_sampling_change()
            return

        self.send_json({"error": "Not found"}, status=404)

    def handle_channel_change(self):
        global selected_channel

        try:
            content_length = int(self.headers.get("Content-Length", "0"))
            body = self.rfile.read(content_length).decode("utf-8")
            data = json.loads(body) if body else {}

            channel = int(data.get("channel", -1))

            write_channel(channel)

            with state_lock:
                selected_channel = channel

            with history_lock:
                history.clear()

            self.send_json({
                "ok": True,
                "channel": channel
            })

        except Exception as e:
            self.send_json({
                "ok": False,
                "error": str(e)
            }, status=400)

    def handle_sampling_change(self):
        global sampling_enabled

        try:
            content_length = int(self.headers.get("Content-Length", "0"))
            body = self.rfile.read(content_length).decode("utf-8")
            data = json.loads(body) if body else {}

            enabled = bool(data.get("enabled", True))

            with state_lock:
                sampling_enabled = enabled

            self.send_json({
                "ok": True,
                "sampling_enabled": enabled
            })

        except Exception as e:
            self.send_json({
                "ok": False,
                "error": str(e)
            }, status=400)

    def log_message(self, format, *args):
        return


if __name__ == "__main__":
    print("TP5 latency web server")
    print(f"Device: {DEV_PATH}")

    if not os.path.exists(DEV_PATH):
        print(f"ERROR: no existe {DEV_PATH}")
        print("Cargá el driver primero, por ejemplo:")
        print("  sudo insmod latencyDriver.ko")
        print("  sudo chmod 666 /dev/tp5_signal")
        exit(1)

    sampler = Thread(target=sampler_loop, daemon=True)
    sampler.start()

    print(f"Servidor escuchando en http://{HOST}:{PORT}")
    print("Desde tu PC abrí: http://192.168.1.135:5000")
    print("CTRL + C para cerrar.")

    server = HTTPServer((HOST, PORT), Handler)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        running = False
        print("\nCerrando servidor...")
        server.server_close()