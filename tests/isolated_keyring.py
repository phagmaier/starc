#!/usr/bin/env python3
"""Run a command with a private D-Bus and disposable native Secret Service.

No session-bus activation directories are configured. Neither a desktop daemon
nor the user's normal keyring can be contacted. Retain all generated evidence.
Usage: isolated_keyring.py EVIDENCE_DIR [--unavailable] -- COMMAND [ARG ...]
Set STARC_TEST_KEYRING_DAEMON to a locally installed gnome-keyring-daemon if needed.
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time


def main():
    split = sys.argv.index('--')
    directory = Path(sys.argv[1]).resolve()
    unavailable = '--unavailable' in sys.argv[2:split]
    command = sys.argv[split + 1:]
    directory.mkdir(parents=True, exist_ok=False)
    env = os.environ.copy()
    env.update(XDG_CONFIG_HOME=str(directory / 'config'),
               XDG_DATA_HOME=str(directory / 'data'),
               XDG_CACHE_HOME=str(directory / 'cache'),
               XDG_RUNTIME_DIR=str(directory / 'runtime'),
               STARC_PROFILE_DIR=str(directory / 'profile'),
               STARC_TEST_PARENT_NET=os.readlink('/proc/self/ns/net'),
               STARC_ISOLATED_KEYRING='1')
    for key in ('XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_CACHE_HOME', 'XDG_RUNTIME_DIR'):
        Path(env[key]).mkdir(mode=0o700)
    config = directory / 'bus.conf'
    # Deliberately no include, servicedir, or standard_session_servicedirs.
    config.write_text('''<busconfig>
 <type>session</type><listen>unix:tmpdir=/tmp</listen><auth>EXTERNAL</auth>
 <policy context="default">
  <allow own="*"/><allow send_destination="*"/><allow receive_sender="*"/>
 </policy>
</busconfig>''')
    bus_log = (directory / 'bus.log').open('w')
    bus = subprocess.Popen(['dbus-daemon', '--nofork', '--print-address=1',
                            '--config-file=' + str(config)],
                           stdout=subprocess.PIPE, stderr=bus_log, text=True)
    provider = None
    try:
        address = bus.stdout.readline().strip()
        assert address.startswith('unix:'), 'private bus did not publish its address'
        env['DBUS_SESSION_BUS_ADDRESS'] = address
        if not unavailable:
            daemon = os.environ.get('STARC_TEST_KEYRING_DAEMON') or shutil.which('gnome-keyring-daemon')
            assert daemon, 'Install gnome-keyring or set STARC_TEST_KEYRING_DAEMON'
            provider_log = (directory / 'provider.log').open('w')
            provider = subprocess.Popen([daemon, '--foreground', '--components=secrets', '--unlock',
                                         '--control-directory=' + str(directory / 'runtime')],
                                        env=env, stdin=subprocess.PIPE,
                                        stdout=provider_log, stderr=subprocess.STDOUT)
            provider.stdin.write(b'disposable-test-keyring-password')
            provider.stdin.close()
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                assert provider.poll() is None, 'private provider exited before readiness'
                owner = subprocess.run(['gdbus', 'call', '--session', '--dest', 'org.freedesktop.DBus',
                                        '--object-path', '/org/freedesktop/DBus', '--method',
                                        'org.freedesktop.DBus.NameHasOwner', 'org.freedesktop.secrets'],
                                       env=env, capture_output=True, text=True, check=True)
                if 'true' in owner.stdout:
                    break
                time.sleep(.05)  # readiness polling, not application input pacing
            else:
                raise RuntimeError('private Secret Service did not become ready')
        with (directory / 'command.log').open('w') as log:
            result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        (directory / 'result.txt').write_text(f'command_exit={result.returncode}\n'
                                              f'native_provider={not unavailable}\n')
        print(f'Isolated native evidence: {directory}; command exit: {result.returncode}', flush=True)
        return result.returncode
    finally:
        # These are only the exact private child processes this runner created.
        if provider is not None and provider.poll() is None:
            provider.terminate()
            provider.wait(timeout=10)
        bus.terminate()
        bus.wait(timeout=10)


if __name__ == '__main__':
    sys.exit(main())
