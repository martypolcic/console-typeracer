const assert = require('assert');

// Test 1: Default configuration values
delete process.env.PORT;
delete process.env.C_SERVER_HOST;
delete process.env.C_SERVER_PORT;
delete process.env.LOBBY_TIMEOUT_MS;
delete process.env.LOBBY_WAIT_TIMEOUT_MS;
delete process.env.TCP_COMMAND_TIMEOUT_MS;
delete process.env.TCP_KEEPALIVE_MS;
delete process.env.WS_HEARTBEAT_INTERVAL_MS;
delete process.env.MAX_LOBBY_CAPACITY;
delete process.env.FINISH_GRACE_TIMEOUT_SEC;
delete process.env.RACE_MAX_DURATION_SEC;

// Clear require cache to re-evaluate config module
delete require.cache[require.resolve('./config')];
const defaultConfig = require('./config');

assert.strictEqual(defaultConfig.HTTP_PORT, 8080);
assert.strictEqual(defaultConfig.C_SERVER_HOST, '127.0.0.1');
assert.strictEqual(defaultConfig.C_SERVER_PORT, 54897);
assert.strictEqual(defaultConfig.LOBBY_TIMEOUT_MS, 600000);
assert.strictEqual(defaultConfig.TCP_COMMAND_TIMEOUT_MS, 2500);
assert.strictEqual(defaultConfig.TCP_KEEPALIVE_MS, 15000);
assert.strictEqual(defaultConfig.WS_HEARTBEAT_INTERVAL_MS, 30000);
assert.strictEqual(defaultConfig.MAX_LOBBY_CAPACITY, 4);
assert.strictEqual(defaultConfig.FINISH_GRACE_TIMEOUT_SEC, 20);
assert.strictEqual(defaultConfig.RACE_MAX_DURATION_SEC, 60);

console.log('✓ Config defaults validated successfully');

// Test 2: Custom environment overrides
process.env.PORT = '9090';
process.env.C_SERVER_HOST = '10.0.0.5';
process.env.C_SERVER_PORT = '50000';
process.env.LOBBY_TIMEOUT_MS = '120000';
process.env.TCP_COMMAND_TIMEOUT_MS = '5000';
process.env.TCP_KEEPALIVE_MS = '30000';
process.env.WS_HEARTBEAT_INTERVAL_MS = '10000';
process.env.MAX_LOBBY_CAPACITY = '6';
process.env.FINISH_GRACE_TIMEOUT_SEC = '15';
process.env.RACE_MAX_DURATION_SEC = '45';

delete require.cache[require.resolve('./config')];
const customConfig = require('./config');

assert.strictEqual(customConfig.HTTP_PORT, 9090);
assert.strictEqual(customConfig.C_SERVER_HOST, '10.0.0.5');
assert.strictEqual(customConfig.C_SERVER_PORT, 50000);
assert.strictEqual(customConfig.LOBBY_TIMEOUT_MS, 120000);
assert.strictEqual(customConfig.TCP_COMMAND_TIMEOUT_MS, 5000);
assert.strictEqual(customConfig.TCP_KEEPALIVE_MS, 30000);
assert.strictEqual(customConfig.WS_HEARTBEAT_INTERVAL_MS, 10000);
assert.strictEqual(customConfig.MAX_LOBBY_CAPACITY, 6);
assert.strictEqual(customConfig.FINISH_GRACE_TIMEOUT_SEC, 15);
assert.strictEqual(customConfig.RACE_MAX_DURATION_SEC, 45);

console.log('✓ Custom environment overrides validated successfully');

// Test 3: Fallback on invalid non-numeric inputs
process.env.PORT = 'invalid';
process.env.LOBBY_TIMEOUT_MS = '-50';
delete require.cache[require.resolve('./config')];
const fallbackConfig = require('./config');

assert.strictEqual(fallbackConfig.HTTP_PORT, 8080);
assert.strictEqual(fallbackConfig.LOBBY_TIMEOUT_MS, 600000);

console.log('✓ Fallback on invalid non-numeric inputs validated successfully');

// Test 4: Backwards-compatible LOBBY_WAIT_TIMEOUT_MS fallback
delete process.env.LOBBY_TIMEOUT_MS;
process.env.LOBBY_WAIT_TIMEOUT_MS = '450000';
delete require.cache[require.resolve('./config')];
const legacyWaitConfig = require('./config');

assert.strictEqual(legacyWaitConfig.LOBBY_TIMEOUT_MS, 450000);
console.log('✓ Legacy LOBBY_WAIT_TIMEOUT_MS fallback validated successfully');

console.log('\n[SUCCESS] All connection ruleset config tests PASSED!\n');
