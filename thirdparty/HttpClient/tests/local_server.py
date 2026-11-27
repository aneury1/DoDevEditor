import http.server, subprocess, sys, tempfile, threading, os
class Server(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        data=self.rfile.read(int(self.headers['Content-Length'])); self.send_response(201);self.send_header('X-Test-Reply',self.headers.get('X-Test',''));self.end_headers();self.wfile.write(data)
    def do_HEAD(self): self.send_response(200);self.end_headers()
    def do_GET(self):
        self.send_response(302 if self.path=='/redirect' else 404 if self.path=='/error' else 200)
        if self.path=='/redirect':self.send_header('Location','/error')
        self.end_headers()
        if self.path=='/huge':
            try:self.wfile.write(b'x'*(3*1024*1024))
            except (BrokenPipeError,ConnectionResetError):pass
    def log_message(self,*args):pass
with http.server.ThreadingHTTPServer(('127.0.0.1',0),Server) as server, tempfile.TemporaryDirectory() as tmp:
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    env=os.environ.copy();env['NO_PROXY']='127.0.0.1';env['no_proxy']='127.0.0.1'
    try:subprocess.run([sys.argv[1],f'http://127.0.0.1:{server.server_port}',tmp],env=env,check=True,timeout=25)
    finally:server.shutdown();thread.join()
