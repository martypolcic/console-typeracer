const http = require('http');
const net = require('net');
const crypto = require('crypto');
const { StringDecoder } = require('string_decoder');
const { WebSocketServer, WebSocket } = require('ws');
const config = require('./config');

const {
  HTTP_PORT,
  C_SERVER_HOST,
  C_SERVER_PORT,
  LOBBY_TIMEOUT_MS,
  TCP_COMMAND_TIMEOUT_MS,
  TCP_KEEPALIVE_MS,
  WS_HEARTBEAT_INTERVAL_MS,
  MAX_LOBBY_CAPACITY
} = config;

/**
 * Send a single-shot ASCII command to the C server daemon via TCP
 */
function sendTcpCommand(cmd, timeoutMs = TCP_COMMAND_TIMEOUT_MS) {
  return new Promise((resolve, reject) => {
    const client = net.createConnection({ host: C_SERVER_HOST, port: C_SERVER_PORT }, () => {
      client.write(cmd.endsWith('\n') ? cmd : cmd + '\n');
    });

    const decoder = new StringDecoder('utf8');
    let data = '';
    client.on('data', chunk => {
      data += decoder.write(chunk);
    });

    client.on('end', () => {
      data += decoder.end();
      resolve(data.trim());
    });

    client.on('error', err => {
      reject(err);
    });

    client.setTimeout(timeoutMs, () => {
      client.destroy();
      reject(new Error('TCP command timeout'));
    });
  });
}

function generateRoomCode(len = 6) {
  const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
  const bytes = crypto.randomBytes(len);
  let res = '';
  for (let i = 0; i < len; i++) {
    res += chars[bytes[i] % chars.length];
  }
  return res;
}

// HTTP Server for REST API
const server = http.createServer(async (req, res) => {
  // CORS Headers
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');

  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  const parsedUrl = new URL(req.url, 'http://' + (req.headers.host || 'localhost'));
  const pathname = parsedUrl.pathname;

  // Helper to send JSON response
  const sendJson = (statusCode, data) => {
    res.writeHead(statusCode, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(data));
  };

  try {
    // 1. GET /api/health
    if (req.method === 'GET' && pathname === '/api/health') {
      try {
        const infoRaw = await sendTcpCommand('INFO\n');
        const info = JSON.parse(infoRaw);
        sendJson(200, {
          status: 'healthy',
          uptime: process.uptime(),
          engine: 'online',
          lobbies: info.rooms ? info.rooms.length : 0,
          rooms: info.rooms || []
        });
      } catch (err) {
        sendJson(503, {
          status: 'degraded',
          engine: 'offline',
          uptime: process.uptime(),
          error: err.message
        });
      }
      return;
    }

    // 2. POST /api/lobbies
    if (req.method === 'POST' && pathname === '/api/lobbies') {
      let bodyStr = '';
      req.on('data', chunk => {
        bodyStr += chunk;
        if (bodyStr.length > 4096) {
          sendJson(413, { error: 'Payload Too Large' });
          req.destroy();
          return;
        }
      });
      req.on('end', async () => {
        if (res.writableEnded) return;
        let maxPlayers = 2;
        let customCode = null;
        if (bodyStr) {
          try {
            const body = JSON.parse(bodyStr);
            if (body.maxPlayers && Number.isInteger(body.maxPlayers)) {
              maxPlayers = Math.max(1, Math.min(MAX_LOBBY_CAPACITY, body.maxPlayers));
            }
            if (body.code && typeof body.code === 'string') {
              customCode = body.code.trim().toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 15);
            }
          } catch (_) {}
        }

        const roomCode = customCode || generateRoomCode(6);
        try {
          const respRaw = await sendTcpCommand(`CREATE ${roomCode} ${maxPlayers}\n`);
          if (respRaw.startsWith('ERROR:')) {
            sendJson(503, { error: respRaw.trim(), lobbyCode: roomCode });
            return;
          }
          let resp;
          try {
            resp = JSON.parse(respRaw);
          } catch (_) {
            sendJson(502, { error: 'Invalid response from game engine', details: respRaw });
            return;
          }
          if (!resp || resp.status !== 'ok') {
            const statusCode = (resp && resp.message === 'room_already_exists') ? 409 : 503;
            sendJson(statusCode, { error: (resp && resp.message) || 'room_creation_failed', lobbyCode: roomCode });
            return;
          }
          sendJson(201, {
            lobbyCode: roomCode,
            joinUrl: `/join/${roomCode}`,
            maxPlayers,
            status: 'created'
          });
        } catch (err) {
          sendJson(500, { error: 'Failed to create room in game engine', details: err.message });
        }
      });
      return;
    }

    // 3. GET /api/lobbies
    if (req.method === 'GET' && pathname === '/api/lobbies') {
      try {
        const infoRaw = await sendTcpCommand('INFO\n');
        const info = JSON.parse(infoRaw);
        sendJson(200, info);
      } catch (err) {
        sendJson(500, { error: 'Failed to query lobbies', details: err.message });
      }
      return;
    }

    // 4. GET /api/lobbies/:code/summary
    const summaryMatch = pathname.match(/^\/api\/lobbies\/([^/]+)\/summary$/);
    if (req.method === 'GET' && summaryMatch) {
      const roomCode = summaryMatch[1].toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 15);
      if (!roomCode) {
        sendJson(400, { error: 'Invalid room code' });
        return;
      }
      try {
        const summaryRaw = await sendTcpCommand(`SUMMARY ${roomCode}\n`);
        const summary = JSON.parse(summaryRaw);
        if (summary.error) {
          sendJson(404, summary);
        } else {
          sendJson(200, summary);
        }
      } catch (err) {
        sendJson(500, { error: 'Failed to query room summary', details: err.message });
      }
      return;
    }

    // 5. GET /api/lobbies/:code
    const roomMatch = pathname.match(/^\/api\/lobbies\/([^/]+)$/);
    if (req.method === 'GET' && roomMatch) {
      const roomCode = roomMatch[1].toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 15);
      if (!roomCode) {
        sendJson(400, { error: 'Invalid room code' });
        return;
      }
      try {
        const infoRaw = await sendTcpCommand('INFO\n');
        const info = JSON.parse(infoRaw);
        const room = (info.rooms || []).find(r => r.code === roomCode);
        if (room) {
          sendJson(200, room);
        } else {
          sendJson(404, { error: 'Lobby not found', code: roomCode });
        }
      } catch (err) {
        sendJson(500, { error: 'Failed to query room', details: err.message });
      }
      return;
    }

    // Default 404
    sendJson(404, { error: 'Not Found', path: pathname });
  } catch (err) {
    sendJson(500, { error: 'Internal Server Error', message: err.message });
  }
});

// WebSocket Server
const wss = new WebSocketServer({ noServer: true });

server.on('upgrade', (request, socket, head) => {
  let parsedUrl;
  try {
    parsedUrl = new URL(request.url, 'http://' + (request.headers.host || 'localhost'));
  } catch (err) {
    socket.write('HTTP/1.1 400 Bad Request\r\n\r\n');
    socket.destroy();
    return;
  }
  const match = parsedUrl.pathname.match(/^\/ws\/lobbies\/([^/]+)$/);

  if (!match) {
    socket.write('HTTP/1.1 404 Not Found\r\n\r\n');
    socket.destroy();
    return;
  }

  const roomCode = match[1].toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 15);
  if (!roomCode) {
    socket.write('HTTP/1.1 400 Bad Request\r\n\r\n');
    socket.destroy();
    return;
  }
  const playerName = (parsedUrl.searchParams.get('player') || `Web_${Math.floor(1000 + Math.random() * 9000)}`).replace(/[^a-zA-Z0-9_-]/g, '').slice(0, 20);

  wss.handleUpgrade(request, socket, head, (ws) => {
    wss.emit('connection', ws, request, { roomCode, playerName });
  });
});

wss.on('connection', (ws, req, { roomCode, playerName }) => {
  console.log(`[WS] Client connected for room: ${roomCode}, player: ${playerName}`);

  // Create TCP connection to C server daemon
  const tcpClient = net.createConnection({ host: C_SERVER_HOST, port: C_SERVER_PORT }, () => {
    console.log(`[TCP] Connected to engine for ${playerName} in ${roomCode}`);
    if (TCP_KEEPALIVE_MS > 0) {
      tcpClient.setKeepAlive(true, TCP_KEEPALIVE_MS);
    }
    // Send JOIN handshake
    tcpClient.write(`JOIN ${roomCode} ${playerName}\n`);
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({
        type: 'LOBBY_WAITING',
        roomCode,
        playerName,
        message: 'Connected to engine. Waiting for players to join...'
      }));
    }
  });

  // Setup WebSocket ping/pong heartbeat to prevent proxy idle dropouts
  let heartbeatTimer = null;
  if (WS_HEARTBEAT_INTERVAL_MS > 0) {
    heartbeatTimer = setInterval(() => {
      if (ws.readyState === WebSocket.OPEN) {
        ws.ping();
      } else {
        clearInterval(heartbeatTimer);
        heartbeatTimer = null;
      }
    }, WS_HEARTBEAT_INTERVAL_MS);
  }

  const cleanupHeartbeat = () => {
    if (heartbeatTimer) {
      clearInterval(heartbeatTimer);
      heartbeatTimer = null;
    }
  };

  // Wait in lobby for opponents with configurable timeout
  tcpClient.setTimeout(LOBBY_TIMEOUT_MS);
  tcpClient.on('timeout', () => {
    if (!hasStarted) {
      console.warn(`[TCP] Lobby wait timed out waiting for opponents (${LOBBY_TIMEOUT_MS}ms): ${playerName} in ${roomCode}`);
      if (ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'ERROR', message: 'Lobby timed out waiting for opponents' }));
        ws.close(1011, 'Lobby timed out');
      }
      cleanupHeartbeat();
      tcpClient.destroy();
    } else {
      tcpClient.setTimeout(0);
    }
  });

  const decoder = new StringDecoder('utf8');
  let rawBuffer = '';
  let hasStarted = false;

  tcpClient.on('data', chunk => {
    rawBuffer += decoder.write(chunk);

    // Only scan for handshake rejection strings before initial JSON object is seen
    if (!hasStarted && rawBuffer.indexOf('{') === -1) {
      if (rawBuffer.includes('ERROR:')) {
        const errLine = rawBuffer.split('\n').find(l => l.includes('ERROR:')) || 'ERROR: Server error';
        const cleanMsg = errLine.replace(/^ERROR:\s*/, '').trim();
        if (ws.readyState === WebSocket.OPEN) {
          ws.send(JSON.stringify({
            type: 'ERROR',
            message: cleanMsg
          }));
          ws.close(1013, cleanMsg.slice(0, 120));
        }
        tcpClient.destroy();
        return;
      }

      if (rawBuffer.includes('ROOM_FULL_OR_ACTIVE')) {
        if (ws.readyState === WebSocket.OPEN) {
          ws.send(JSON.stringify({
            type: 'ERROR',
            message: 'Room is full or already active'
          }));
          ws.close(1008, 'Room is full or already active');
        }
        tcpClient.destroy();
        return;
      }

      if (rawBuffer.includes('SERVER_ERROR')) {
        if (ws.readyState === WebSocket.OPEN) {
          ws.send(JSON.stringify({
            type: 'ERROR',
            message: 'Server error joining room'
          }));
          ws.close(1011, 'Server error');
        }
        tcpClient.destroy();
        return;
      }
    }

    // Parse balanced JSON objects from incoming TCP stream
    while (true) {
      const startIdx = rawBuffer.indexOf('{');
      if (startIdx === -1) {
        if (rawBuffer.length > 65536) {
          rawBuffer = rawBuffer.slice(-4096);
        }
        break;
      }

      // Track brace depth to find full JSON object
      let depth = 0;
      let inString = false;
      let escape = false;
      let endIdx = -1;

      for (let i = startIdx; i < rawBuffer.length; i++) {
        const c = rawBuffer[i];
        if (escape) {
          escape = false;
          continue;
        }
        if (c === '\\') {
          escape = true;
          continue;
        }
        if (c === '"') {
          inString = !inString;
          continue;
        }
        if (!inString) {
          if (c === '{') depth++;
          else if (c === '}') {
            depth--;
            if (depth === 0) {
              endIdx = i;
              break;
            }
          }
        }
      }

      if (endIdx === -1) {
        // Incomplete JSON object in buffer, wait for more data
        if (rawBuffer.length > 65536) {
          rawBuffer = rawBuffer.slice(startIdx);
        }
        break;
      }

      const jsonStr = rawBuffer.slice(startIdx, endIdx + 1);
      rawBuffer = rawBuffer.slice(endIdx + 1);

      try {
        const parsed = JSON.parse(jsonStr);
        hasStarted = true;
        tcpClient.setTimeout(0);

        if (parsed.sentence !== undefined) {
          // Initial SocketsData
          if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({
              type: 'LOBBY_STATE',
              sentence: parsed.sentence,
              names: parsed.names || [],
              playerIndex: parsed.playerIndex,
              playerCount: parsed.playerCount,
              roomCode
            }));
          }
        } else if (parsed.progress !== undefined) {
          // RefreshingData broadcast
          if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({
              type: 'PROGRESS_UPDATE',
              progress: parsed.progress
            }));
          }
        } else if (parsed.times !== undefined) {
          // Times results
          if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({
              type: 'RACE_RESULTS',
              times: parsed.times
            }));
          }
        }
      } catch (e) {
        console.error('[TCP Parser] Failed to parse JSON chunk:', jsonStr, e);
      }
    }
  });

  tcpClient.on('error', err => {
    console.error(`[TCP Error] ${err.message}`);
    cleanupHeartbeat();
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: 'ERROR', message: 'Engine connection error' }));
      ws.close();
    }
  });

  tcpClient.on('end', () => {
    console.log(`[TCP] Engine closed connection for ${playerName}`);
    cleanupHeartbeat();
    if (ws.readyState === WebSocket.OPEN) {
      ws.close();
    }
  });

  // Handle messages from browser WebSocket
  ws.on('message', data => {
    try {
      const msgStr = data.toString('utf8');
      let msgType = msgStr;
      try {
        const parsed = JSON.parse(msgStr);
        if (parsed.type) msgType = parsed.type;
      } catch (_) {}

      if (msgType === 'WORD_DONE' || msgType === '3') {
        tcpClient.write('3');
      } else if (msgType === 'SENTENCE_FINISHED' || msgType === '2') {
        // Send '2' followed by dummy trailing self-reported time
        tcpClient.write('2');
        const dummyTime = '0.00\0';
        tcpClient.write(dummyTime);
      }
    } catch (err) {
      console.error('[WS Message Error]', err);
    }
  });

  ws.on('close', () => {
    console.log(`[WS] Client closed connection for ${playerName}`);
    cleanupHeartbeat();
    tcpClient.destroy();
  });

  ws.on('error', err => {
    console.error(`[WS Error] ${err.message}`);
    cleanupHeartbeat();
    tcpClient.destroy();
  });
});

server.listen(HTTP_PORT, () => {
  console.log(`TypeRacer Dual-Transport Gateway listening on port ${HTTP_PORT}`);
  console.log(`Forwarding game TCP sessions to ${C_SERVER_HOST}:${C_SERVER_PORT}`);
  console.log(`[Gateway] Active Connection Ruleset:`);
  console.log(`  - Lobby Wait Timeout : ${LOBBY_TIMEOUT_MS} ms (${(LOBBY_TIMEOUT_MS / 1000).toFixed(1)}s)`);
  console.log(`  - TCP Command Timeout: ${TCP_COMMAND_TIMEOUT_MS} ms`);
  console.log(`  - TCP KeepAlive      : ${TCP_KEEPALIVE_MS} ms`);
  console.log(`  - WS Heartbeat       : ${WS_HEARTBEAT_INTERVAL_MS} ms`);
  console.log(`  - Max Lobby Capacity : ${MAX_LOBBY_CAPACITY}`);
});
