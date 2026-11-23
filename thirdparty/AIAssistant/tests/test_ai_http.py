#!/usr/bin/env python3
"""Loopback mock for Chat Completions, llama.cpp /v1/models and Bearer authentication."""
import http.server
import json
import subprocess
import sys
import threading

requests = []

class Handler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != '/v1/chat/completions':
            self.send_error(404)
            return
        length = int(self.headers.get('Content-Length', '0'))
        body = json.loads(self.rfile.read(length).decode('utf-8'))
        requests.append(('chat', body, self.headers.get('Authorization')))
        message = json.dumps({'choices': [{'message': {'role': 'assistant', 'content': 'mock response'}}]}).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(message)))
        self.end_headers()
        self.wfile.write(message)

    def do_GET(self):
        if self.path != '/v1/models':
            self.send_error(404)
            return
        requests.append(('models', None, self.headers.get('Authorization')))
        response = json.dumps({'object': 'list', 'data': [
            {'id': 'test-instruct', 'object': 'model'},
            {'id': 'test-code', 'object': 'model'}
        ]}).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(response)))
        self.end_headers()
        self.wfile.write(response)

    def log_message(self, *_args):
        pass

server = http.server.HTTPServer(('127.0.0.1', 0), Handler)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
try:
    endpoint = f'http://127.0.0.1:{server.server_address[1]}/v1/chat/completions'
    subprocess.run([sys.argv[1], endpoint], check=True, timeout=15)
finally:
    server.shutdown()
    thread.join(timeout=2)
assert len(requests) == 3, requests
assert requests[0][0] == 'chat'
assert requests[0][1]['model'] == 'test-model'
assert requests[0][1]['messages'][-1]['content'] == 'Hello from DoDevEditor'
assert requests[1][0] == 'models' and requests[1][2] is None
assert requests[2][0] == 'models' and requests[2][2] == 'Bearer safe-local-test-key'
