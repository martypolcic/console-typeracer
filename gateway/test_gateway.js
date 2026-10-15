const { spawn } = require('child_process');
const http = require('http');
const path = require('path');
const { WebSocket } = require('ws');

const C_PORT = 54903;
const HTTP_PORT = 18081;

function fetchJson(url, options = {}) {
  return new Promise((resolve, reject) => {
    const req = http.request(url, options, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        try {
          resolve({ status: res.statusCode, body: JSON.parse(data) });
        } catch (e) {
          resolve({ status: res.statusCode, body: data });
        }
      });
    });
    req.on('error', reject);
    if (options.body) {
      req.write(typeof options.body === 'string' ? options.body : JSON.stringify(options.body));
    }
    req.end();
  });
}

async function run() {
  console.log('--- Starting Integration Test: C Engine Daemon + Gateway ---');

  const serverBin = path.join(__dirname, '..', 'build', 'server', 'server');
  const inputFile = path.join(__dirname, '..', 'input.txt');

  // Spawn C daemon
  const engineProc = spawn(serverBin, ['--daemon', String(C_PORT), inputFile, '2'], {
    stdio: 'inherit'
  });

  // Spawn Gateway
  const gatewayProc = spawn(process.execPath, [path.join(__dirname, 'gateway.js')], {
    env: {
      ...process.env,
      PORT: String(HTTP_PORT),
      C_SERVER_PORT: String(C_PORT),
      C_SERVER_HOST: '127.0.0.1'
    },
    stdio: 'inherit'
  });

  const cleanup = () => {
    engineProc.kill('SIGTERM');
    gatewayProc.kill('SIGTERM');
  };

  try {
    // Wait for services to start
    await new Promise(r => setTimeout(r, 600));

    // 1. Health check
    console.log('Testing GET /api/health...');
    const health = await fetchJson(`http://127.0.0.1:${HTTP_PORT}/api/health`);
    console.log('Health response:', health.body);
    if (health.status !== 200 || health.body.status !== 'healthy') {
      throw new Error(`Health check failed: ${JSON.stringify(health)}`);
    }

    // 2. Create Lobby
    console.log('Testing POST /api/lobbies...');
    const createRes = await fetchJson(`http://127.0.0.1:${HTTP_PORT}/api/lobbies`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ code: 'TESTROOM', maxPlayers: 2 })
    });
    console.log('Create lobby response:', createRes.body);
    if (createRes.status !== 201 || createRes.body.lobbyCode !== 'TESTROOM') {
      throw new Error(`Create lobby failed: ${JSON.stringify(createRes)}`);
    }
    const lobbyCode = createRes.body.lobbyCode;

    // 2b. Test creating duplicate active lobby returns 409 Conflict
    console.log('Testing duplicate lobby creation (expecting 409)...');
    const dupRes = await fetchJson(`http://127.0.0.1:${HTTP_PORT}/api/lobbies`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ code: 'TESTROOM', maxPlayers: 2 })
    });
    console.log('Duplicate lobby response:', dupRes.body);
    if (dupRes.status !== 409) {
      throw new Error(`Expected 409 Conflict on duplicate room creation, got: ${dupRes.status}`);
    }

    // 3. Connect 2 WebSocket clients
    console.log(`Connecting 2 WebSocket clients to lobby ${lobbyCode}...`);
    const p1Promise = new Promise((resolve, reject) => {
      const ws1 = new WebSocket(`ws://127.0.0.1:${HTTP_PORT}/ws/lobbies/${lobbyCode}?player=PlayerOne`);
      let lobbyStateReceived = false;
      let resultsReceived = false;

      ws1.on('message', data => {
        const msg = JSON.parse(data.toString());
        if (msg.type === 'LOBBY_STATE') {
          lobbyStateReceived = true;
          const wordCount = msg.sentence ? msg.sentence.trim().split(/\s+/).length : 1;
          for (let i = 0; i < wordCount - 1; i++) {
            setTimeout(() => ws1.send(JSON.stringify({ type: 'WORD_DONE' })), 10 * (i + 1));
          }
          setTimeout(() => ws1.send(JSON.stringify({ type: 'SENTENCE_FINISHED' })), 10 * wordCount + 30);
        } else if (msg.type === 'RACE_RESULTS') {
          resultsReceived = true;
          ws1.close();
          resolve(msg);
        }
      });
      ws1.on('error', reject);
    });

    const p2Promise = new Promise((resolve, reject) => {
      const ws2 = new WebSocket(`ws://127.0.0.1:${HTTP_PORT}/ws/lobbies/${lobbyCode}?player=PlayerTwo`);
      let lobbyStateReceived = false;
      let resultsReceived = false;

      ws2.on('message', data => {
        const msg = JSON.parse(data.toString());
        if (msg.type === 'LOBBY_STATE') {
          lobbyStateReceived = true;
          const wordCount = msg.sentence ? msg.sentence.trim().split(/\s+/).length : 1;
          for (let i = 0; i < wordCount - 1; i++) {
            setTimeout(() => ws2.send(JSON.stringify({ type: 'WORD_DONE' })), 12 * (i + 1));
          }
          setTimeout(() => ws2.send(JSON.stringify({ type: 'SENTENCE_FINISHED' })), 12 * wordCount + 40);
        } else if (msg.type === 'RACE_RESULTS') {
          resultsReceived = true;
          ws2.close();
          resolve(msg);
        }
      });
      ws2.on('error', reject);
    });

    const [res1, res2] = await Promise.all([p1Promise, p2Promise]);
    console.log('Both WS clients completed race!');
    console.log('Client 1 results:', res1);
    console.log('Client 2 results:', res2);

    // 4. Query Summary
    await new Promise(r => setTimeout(r, 200));
    console.log(`Querying GET /api/lobbies/${lobbyCode}/summary...`);
    const summaryRes = await fetchJson(`http://127.0.0.1:${HTTP_PORT}/api/lobbies/${lobbyCode}/summary`);
    console.log('Summary response:', summaryRes.body);
    if (summaryRes.status !== 200 || summaryRes.body.roomCode !== lobbyCode) {
      throw new Error(`Summary check failed: ${JSON.stringify(summaryRes)}`);
    }

    // 5. Oversized payload test (413 Payload Too Large)
    console.log('Testing oversized POST /api/lobbies payload (> 4096 bytes)...');
    const largeBody = JSON.stringify({ padding: 'A'.repeat(5000) });
    const largeRes = await fetchJson(`http://127.0.0.1:${HTTP_PORT}/api/lobbies`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: largeBody
    });
    console.log('Oversized payload response status:', largeRes.status);
    if (largeRes.status !== 413) {
      throw new Error(`Expected 413 on oversized payload, got ${largeRes.status}`);
    }

    console.log('\n[SUCCESS] All REST and WebSocket Gateway integration tests PASSED!\n');
    cleanup();
    process.exit(0);
  } catch (err) {
    console.error('\n[FAILURE] Integration test failed:', err);
    cleanup();
    process.exit(1);
  }
}

run();
