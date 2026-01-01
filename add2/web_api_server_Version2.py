#!/usr/bin/env python3
"""
web/api/server.py

Minimal Flask web API to serve maps and metrics (development only).
"""

from flask import Flask, jsonify, send_file, abort
import os

app = Flask(__name__)

@app.route("/v1/maps/<name>")
def get_map(name):
    path = os.path.join("maps", name + ".map.json")
    if not os.path.exists(path):
        abort(404)
    return send_file(path, mimetype="application/json")

@app.route("/v1/metrics")
def metrics():
    path = os.path.join("logs", "latest_metrics.json")
    if os.path.exists(path):
        return send_file(path, mimetype="application/json")
    return jsonify({"status":"no metrics"}), 204

@app.route("/health")
def health():
    return jsonify({"status":"ok"})

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080)