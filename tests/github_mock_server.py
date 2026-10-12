#!/usr/bin/env python3
"""Local stand-in for the parts of the GitHub REST API used by the story sync.

It keeps the files of one repository in a directory and follows the documented
behaviour of the repository contents endpoints: a file can be replaced only when
the request names the sha of the stored version.

Usage: github_mock_server.py STORAGE_DIR [--port N] [--token T] [--public]
The chosen port is printed as "PORT <n>" on the first line of the output.
"""

import argparse
import base64
import datetime
import hashlib
import json
import pathlib
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


def blob_sha(content: bytes) -> str:
    return hashlib.sha1(b"blob %d\0" % len(content) + content).hexdigest()


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, format, *args):
        print("%s %s" % (self.command, self.path), flush=True)

    def reply(self, status, payload, content_type="application/json"):
        body = payload if isinstance(payload, bytes) else json.dumps(payload).encode()
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def route(self):
        """Return (kind, file name, query) for the request path, or None."""
        url = urllib.parse.urlsplit(self.path)
        parts = [urllib.parse.unquote(part) for part in url.path.split("/") if part]
        query = urllib.parse.parse_qs(url.query)
        if len(parts) < 3 or parts[0] != "repos":
            return None
        rest = parts[3:]
        if parts[1:3] == ["echo", "denied"]:
            return ("credential-error", None, query)
        if not rest:
            return ("repository", None, query)
        if rest[0] == "contents":
            return ("contents", "/".join(rest[1:]), query)
        if rest == ["commits"]:
            return ("commits", query.get("path", [""])[0], query)
        return None

    def authorized(self):
        if self.headers.get("Authorization") == "Bearer " + self.server.token:
            return True
        self.reply(401, {"message": "Bad credentials"})
        return False

    def do_GET(self):
        if not self.authorized():
            return
        route = self.route()
        if route is None:
            self.reply(404, {"message": "Not Found"})
            return
        kind, name, _ = route
        storage = self.server.storage

        if kind == "credential-error":
            self.reply(403, {"message": "Refused credential " + self.server.token})
        elif kind == "repository":
            self.reply(200, {"private": not self.server.public, "permissions": {"push": True}})
        elif kind == "contents" and not name:
            files = sorted(path for path in storage.iterdir() if path.is_file())
            if not files:
                self.reply(404, {"message": "This repository is empty."})
                return
            self.reply(200, [
                {"type": "file", "name": path.name, "path": path.name,
                 "sha": blob_sha(path.read_bytes()), "size": path.stat().st_size}
                for path in files
            ])
        elif kind == "contents":
            path = storage / name
            if not path.is_file():
                self.reply(404, {"message": "Not Found"})
            elif "raw" in self.headers.get("Accept", ""):
                self.reply(200, path.read_bytes(), "application/vnd.github.raw+json")
            else:
                content = path.read_bytes()
                self.reply(200, {"type": "file", "name": name, "sha": blob_sha(content),
                                 "size": len(content), "encoding": "base64",
                                 "content": base64.b64encode(content).decode()})
        else:
            commits = self.server.commits.get(name, [])
            self.reply(200, commits[-1:])

    def do_PUT(self):
        if not self.authorized():
            return
        route = self.route()
        if route is None or route[0] != "contents" or not route[1]:
            self.reply(404, {"message": "Not Found"})
            return
        name = route[1]
        request = json.loads(self.rfile.read(int(self.headers.get("Content-Length", "0"))))
        path = self.server.storage / name
        sha = request.get("sha")

        if path.is_file():
            current = blob_sha(path.read_bytes())
            if sha is None:
                self.reply(422, {"message": "Invalid request.\n\n\"sha\" wasn't supplied."})
                return
            if sha != current:
                self.reply(409, {"message": "%s does not match %s" % (name, sha)})
                return
        elif sha is not None:
            self.reply(404, {"message": "Not Found"})
            return

        existed = path.is_file()
        content = base64.b64decode(request["content"])
        path.write_bytes(content)
        commit = {"commit": {
            "message": request["message"],
            "committer": {"date": datetime.datetime.now(datetime.timezone.utc)
                          .strftime("%Y-%m-%dT%H:%M:%SZ")},
        }}
        self.server.commits.setdefault(name, []).append(commit)
        self.reply(200 if existed else 201,
                   {"content": {"name": name, "path": name, "sha": blob_sha(content)},
                    "commit": commit["commit"]})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("storage")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--token", default="test-token")
    parser.add_argument("--public", action="store_true")
    arguments = parser.parse_args()

    server = ThreadingHTTPServer(("127.0.0.1", arguments.port), Handler)
    server.storage = pathlib.Path(arguments.storage)
    server.storage.mkdir(parents=True, exist_ok=True)
    server.token = arguments.token
    server.public = arguments.public
    server.commits = {}
    print("PORT %d" % server.server_address[1], flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
