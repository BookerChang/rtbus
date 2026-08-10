const vscode = require('vscode');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');

const SDK_VERSION = '1.0.0';
const BUILDER_IMAGE = `localhost/rtbus-zephyr:arm-${SDK_VERSION}`;
const DOCKERFILE = 'docker/Dockerfile.embedded-arm';
const TERMINAL_PROFILE_ID = 'rtduo.builderShell';
const TERMINAL_PROFILE_TITLE = 'RTDuo Builder Shell';
const BOARDS = [
  'rak4631',
  'rak3172',
  'rak3172f',
  'rak3172p',
  'rak3172t',
  'rak11720',
  'rak4200'
];

let output;

function activate(context) {
  output = vscode.window.createOutputChannel('RTDuo');

  context.subscriptions.push(
    vscode.commands.registerCommand('rtduo.buildBuilderImage', buildBuilderImage),
    vscode.commands.registerCommand('rtduo.compileArduinoExample', compileArduinoExample),
    vscode.commands.registerCommand('rtduo.openBuilderShell', openBuilderShell),
    vscode.window.registerTerminalProfileProvider(TERMINAL_PROFILE_ID, {
      provideTerminalProfile: provideBuilderShellProfile
    }),
    output
  );

  const root = workspaceRoot();
  if (root && isRTDuoWorkspace(root)) {
    output.show(true);
    log(`RTDuo workspace detected: ${root}`);
  }
}

function deactivate() {}

function workspaceRoot() {
  const folders = vscode.workspace.workspaceFolders;
  if (!folders || folders.length === 0) {
    return undefined;
  }
  return folders[0].uri.fsPath;
}

function isRTDuoWorkspace(root) {
  return fs.existsSync(path.join(root, DOCKERFILE)) &&
    fs.existsSync(path.join(root, 'Makefile')) &&
    fs.existsSync(path.join(root, 'platform.txt')) &&
    fs.existsSync(path.join(root, 'boards.txt')) &&
    fs.existsSync(path.join(root, 'libraries', 'RTDuo'));
}

function requireWorkspace() {
  const root = workspaceRoot();
  if (!root) {
    throw new Error('Open an RTDuo workspace before running this command.');
  }
  if (!isRTDuoWorkspace(root)) {
    throw new Error('This workspace does not look like an RTDuo Arduino package.');
  }
  return root;
}

async function buildBuilderImage() {
  const root = requireWorkspace();

  output.show(true);
  log('Building RTDuo builder image');
  await runMake(root, ['docker.build']);
  log(`[OK] Builder image ready: ${BUILDER_IMAGE}`);
}

async function compileArduinoExample() {
  const root = requireWorkspace();
  const board = await vscode.window.showQuickPick(BOARDS, {
    title: 'RTDuo Board Profile',
    placeHolder: 'Select board profile'
  });
  if (!board) {
    return;
  }

  output.show(true);
  log(`Compiling RTDuo Arduino example: ${board}`);
  await runMake(root, ['arduino.compile', `BOARD_PROFILE=${board}`]);
  log(`[OK] Arduino example compile complete: ${board}`);
}

async function openBuilderShell() {
  const terminal = vscode.window.createTerminal(await builderShellTerminalOptions());
  terminal.show();
}

async function provideBuilderShellProfile() {
  return new vscode.TerminalProfile(await builderShellTerminalOptions());
}

async function builderShellTerminalOptions() {
  const root = requireWorkspace();
  const cli = await findContainerCli();

  output.show(true);
  log(`[OK] Container CLI: ${cli.command}`);
  await ensureBuilderImage(root);

  return {
    name: TERMINAL_PROFILE_TITLE,
    shellPath: cli.command,
    shellArgs: [
      'run', '-it', '--user', 'root', '--rm',
      '-v', `${root}:/workdir`,
      '-w', '/workdir',
      BUILDER_IMAGE,
      'bash'
    ],
    cwd: root,
    env: cli.env
  };
}

async function ensureBuilderImage(root) {
  const cli = await findContainerCli();
  const exists = await commandSucceeds(cli.command, ['image', 'inspect', BUILDER_IMAGE], {
    cwd: root,
    env: cli.env
  });
  if (exists) {
    return;
  }

  const choice = await vscode.window.showWarningMessage(
    `Missing builder image: ${BUILDER_IMAGE}`,
    'Build Image'
  );
  if (choice !== 'Build Image') {
    throw new Error(`Missing builder image: ${BUILDER_IMAGE}`);
  }
  await buildBuilderImage();
}

async function runMake(root, args) {
  await runCommand('make', args, { cwd: root, env: process.env });
}

async function findContainerCli() {
  const env = { ...process.env };
  const fromEnv = process.env.DOCKER_EXE;
  if (fromEnv && fs.existsSync(fromEnv)) {
    env.PATH = `${path.dirname(fromEnv)}${path.delimiter}${env.PATH || ''}`;
    return { command: fromEnv, env };
  }

  const docker = await resolveCommandPath(process.platform === 'win32' ? 'docker.exe' : 'docker');
  if (docker) {
    env.PATH = `${path.dirname(docker)}${path.delimiter}${env.PATH || ''}`;
    return { command: docker, env };
  }

  if (process.platform !== 'win32') {
    const podman = await resolveCommandPath('podman');
    if (podman) {
      env.PATH = `${path.dirname(podman)}${path.delimiter}${env.PATH || ''}`;
      return { command: podman, env };
    }
  }

  if (process.platform === 'win32') {
    const dockerDesktop = 'C:\\Program Files\\Docker\\Docker\\resources\\bin\\docker.exe';
    if (fs.existsSync(dockerDesktop)) {
      env.PATH = `${path.dirname(dockerDesktop)}${path.delimiter}${env.PATH || ''}`;
      return { command: dockerDesktop, env };
    }
  }

  throw new Error('Container CLI not found. Install Docker Desktop, install Podman, or set DOCKER_EXE.');
}

function resolveCommandPath(command) {
  return new Promise((resolve) => {
    const child = process.platform === 'win32'
      ? cp.spawn('where', [command], { shell: false })
      : cp.spawn('sh', ['-lc', `command -v ${shellQuote(command)}`], { shell: false });
    let stdout = '';
    child.stdout.on('data', (chunk) => {
      stdout += chunk.toString();
    });
    child.on('close', (code) => {
      if (code !== 0) {
        resolve(undefined);
        return;
      }
      const first = stdout.split(/\r?\n/).map((line) => line.trim()).find(Boolean);
      resolve(first);
    });
    child.on('error', () => resolve(undefined));
  });
}

async function commandSucceeds(command, args, options) {
  try {
    await runCommand(command, args, { ...options, quiet: true });
    return true;
  } catch (_) {
    return false;
  }
}

function runCommand(command, args, options = {}) {
  return new Promise((resolve, reject) => {
    const child = cp.spawn(command, args, {
      cwd: options.cwd,
      env: options.env,
      shell: false
    });

    child.stdout.on('data', (chunk) => {
      if (!options.quiet) {
        output.append(chunk.toString());
      }
    });
    child.stderr.on('data', (chunk) => {
      if (!options.quiet) {
        output.append(chunk.toString());
      }
    });
    child.on('error', reject);
    child.on('close', (code) => {
      if (code === 0) {
        resolve();
      } else {
        reject(new Error(`${command} ${args.join(' ')} failed with exit code ${code}`));
      }
    });
  });
}

function shellQuote(value) {
  const text = String(value);
  if (/^[A-Za-z0-9_./:=@+-]+$/.test(text)) {
    return text;
  }
  return `"${text.replace(/"/g, '\\"')}"`;
}

function log(message) {
  output.appendLine(message);
}

module.exports = {
  activate,
  deactivate
};
