"""Interactive disposable application driver; run inside isolated_keyring.py.

Adapted from the retained backup-retention Xvfb driver. Commands on stdin are
JSON; only fake credentials are supplied, and responses never include tokens.
Usage: github_credential_ui_driver.py NEW_EVIDENCE_DIR
"""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

repo = Path(os.environ['STARC_TEST_REPO']).resolve()
root = Path(sys.argv[1]).resolve()
root.mkdir(parents=True, exist_ok=False)
assert os.environ.get('STARC_ISOLATED_KEYRING') == '1'
# This runner is explicitly launched with unshare -Urn: enable its loopback so
# the app and mock server can communicate, while all external networking stays absent.
assert os.geteuid() == 0 and os.readlink('/proc/self/ns/net') != os.environ['STARC_TEST_PARENT_NET']
subprocess.run(['ip', 'link', 'set', 'lo', 'up'], check=True)
profile = root / 'profile'
config = profile / 'config' / 'Story Apps' / 'Story Architect.conf'
config.parent.mkdir(parents=True)
layout = (repo / '.build/evidence/backup-retention/ui/profile/config/Story Apps/Story Architect.conf').read_text()
start = layout.index('[application-view]')
end = layout.find('\n[', start + 1)
layout = layout[start:end if end != -1 else None]
initial = {
    'application/language': '1', 'application/save-backups': 'false',
    'github-sync/upload-on-exit': 'false',
}
if os.environ.get('STARC_UI_CONFIGURED') == '1':
    initial.update({'application/configured': 'true',
                    'github-sync/repository': 'test-owner/test-repository',
                    'github-sync/token': 'test-token'})
config.write_text('[General]\n' + '\n'.join(key.encode().hex() + '=' + value
                                          for key, value in initial.items()) + '\n\n' + layout)
xlog = (root / 'xvfb.log').open('w')
xvfb = subprocess.Popen(['Xvfb', '-displayfd', '1', '-screen', '0', '1500x950x24', '-nolisten', 'tcp'],
                        stdout=subprocess.PIPE, stderr=xlog, text=True)
display = ':' + xvfb.stdout.readline().strip()
env = os.environ.copy()
env.update(DISPLAY=display, QT_QPA_PLATFORM='xcb', STARC_PROFILE_DIR=str(profile))
env.pop('WAYLAND_DISPLAY', None)
server_log = (root / 'server.log').open('w')
server = subprocess.Popen(['python3', str(repo / 'tests/github_mock_server.py'), str(root / 'repository')],
                          env=env, stdout=server_log, stderr=subprocess.STDOUT)
deadline = time.monotonic() + 10
while time.monotonic() < deadline:
    lines = (root / 'server.log').read_text().splitlines()
    if lines and lines[0].startswith('PORT '):
        env['STARC_GITHUB_API_URL'] = 'http://127.0.0.1:' + lines[0].split()[1]
        break
    assert server.poll() is None, 'local mock server exited'
    time.sleep(.05)
else:
    raise RuntimeError('local server readiness timeout')
app = None
window = None


def run(*args):
    return subprocess.run(args, env=env, capture_output=True, text=True, check=True).stdout.strip()


def shot(name):
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-f', 'x11grab', '-video_size', '1500x950',
                    '-i', display, '-frames:v', '1', str(root / name)], env=env, check=True)


print(json.dumps({'display': display, 'xvfb_pid': xvfb.pid, 'evidence': str(root)}), flush=True)
for line in sys.stdin:
    try:
        op = json.loads(line)
        if op['op'] == 'start':
            assert app is None or app.poll() is not None
            log = (root / op.get('log', 'application.log')).open('w')
            args = [str(repo / 'build/dev.sh'), 'run', 'debug']
            if op.get('file'):
                args.append(str(root / op['file']))
            app = subprocess.Popen(args, env=env, stdout=log, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                assert app.poll() is None, f'application exited {app.returncode}'
                result = subprocess.run(['xdotool', 'search', '--onlyvisible', '--pid', str(app.pid)],
                                        env=env, capture_output=True, text=True)
                if result.returncode == 0 and result.stdout.strip():
                    window = result.stdout.split()[-1]
                    run('xdotool', 'windowfocus', '--sync', window)
                    break
                time.sleep(.05)
            else:
                raise RuntimeError('application window readiness timeout')
            answer = {'pid': app.pid, 'window': window}
        elif op['op'] == 'click':
            coordinates = [] if op.get('screen') else ['--window', window]
            run('xdotool', 'mousemove', *coordinates, str(op['x']), str(op['y']), 'click', '1')
            answer = {'clicked': [op['x'], op['y']]}
        elif op['op'] == 'geometry':
            answer = {'window': window, 'geometry': run('xdotool', 'getwindowgeometry', '--shell', window),
                      'visible_windows': run('xdotool', 'search', '--onlyvisible', '--pid', str(app.pid))}
        elif op['op'] == 'key':
            run('xdotool', 'key', '--clearmodifiers', *op['keys'])
            answer = {'keys': op['keys']}
        elif op['op'] == 'fill':
            values = {'repository': 'test-owner/test-repository', 'token': 'test-token',
                      'replacement': 'fake-ui-replacement-token', 'close': 'fake-ui-close-token'}
            run('xdotool', 'type', '--clearmodifiers', '--delay', '0', values[op['field']])
            answer = {'filled': op['field']}
        elif op['op'] == 'shot':
            shot(op['file'])
            answer = {'screenshot': str(root / op['file'])}
        elif op['op'] == 'quit':
            run('xdotool', 'key', '--clearmodifiers', 'ctrl+q')
            result = app.wait(timeout=30)
            with (root / 'exits.jsonl').open('a') as log:
                log.write(json.dumps({'log': op.get('label'), 'exit': result}) + '\n')
            answer = {'exit': result}
        elif op['op'] == 'stop':
            assert app is None or app.poll() is not None, 'Quit the application normally first'
            server.terminate(); server.wait(timeout=10)
            xvfb.terminate()
            answer = {'xvfb_exit': xvfb.wait(timeout=10)}
            print(json.dumps(answer), flush=True)
            break
        else:
            raise ValueError('unknown driver operation')
        print(json.dumps(answer), flush=True)
    except Exception as exc:
        print(json.dumps({'error': repr(exc)}), flush=True)
