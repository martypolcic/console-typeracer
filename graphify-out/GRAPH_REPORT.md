# Graph Report - console-typeracer  (2026-10-20)

## Corpus Check
- 30 files · ~27,984 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 3 file(s) not represented in the graph (top: (none) 3)

## Summary
- 329 nodes · 879 edges · 15 communities (12 shown, 3 thin omitted)
- Extraction: 84% EXTRACTED · 16% INFERRED · 0% AMBIGUOUS · INFERRED: 143 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `e06993ce`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- test_network.c
- gateway.js
- server.c
- cJSON_Delete
- cJSON
- cJSON.c
- main
- cJSON_New_Item
- cJSON_bool
- cJSON_ParseWithLengthOpts
- entrypoint.sh
- replace_item_in_object
- build.sh
- start_client.sh
- start_server.sh

## God Nodes (most connected - your core abstractions)
1. `cJSON_Delete()` - 35 edges
2. `cJSON_New_Item()` - 20 edges
3. `add_item_to_object()` - 19 edges
4. `coordinator_connection_worker()` - 19 edges
5. `main()` - 16 edges
6. `runGameHub()` - 15 edges
7. `print_value()` - 12 edges
8. `generateRoomSummary()` - 12 edges
9. `deserializeSocketsData()` - 12 edges
10. `deserializeTimes()` - 12 edges

## Surprising Connections (you probably didn't know these)
- `coordinator_thread()` --calls--> `startCoordinatorServer()`  [INFERRED]
  tests/test_network.c → server/server.c
- `coordinator_connection_worker()` --calls--> `cJSON_Delete()`  [INFERRED]
  server/server.c → cJSON-lib/cJSON.c
- `coordinator_connection_worker()` --calls--> `cJSON_PrintUnformatted()`  [INFERRED]
  server/server.c → cJSON-lib/cJSON.c
- `coordinator_connection_worker()` --calls--> `cJSON_AddItemToArray()`  [INFERRED]
  server/server.c → cJSON-lib/cJSON.c
- `generateRoomSummary()` --calls--> `cJSON_AddItemToArray()`  [INFERRED]
  server/server.c → cJSON-lib/cJSON.c

## Import Cycles
- None detected.

## Communities (15 total, 3 thin omitted)

### Community 0 - "test_network.c"
Cohesion: 0.08
Nodes (23): main(), freeSentenceCache(), stopCoordinatorServer(), parseSentence(), readFileLines(), validateLine(), connect_to_server(), passive_socket_destroy() (+15 more)

### Community 1 - "gateway.js"
Cohesion: 0.05
Nodes (33): config, config, crypto, generateRoomCode(), http, net, sendTcpCommand(), server (+25 more)

### Community 2 - "server.c"
Cohesion: 0.11
Nodes (29): event_queue_close(), event_queue_destroy(), event_queue_init(), event_queue_pop(), event_queue_push(), broadcastProgress(), coordinator_connection_worker(), coordinator_count_rooms_unlocked() (+21 more)

### Community 3 - "cJSON_Delete"
Cohesion: 0.15
Nodes (31): cJSON_AddBoolToObject(), cJSON_AddItemToObject(), cJSON_AddNumberToObject(), cJSON_AddObjectToObject(), cJSON_AddStringToObject(), cJSON_CreateArray(), cJSON_CreateDoubleArray(), cJSON_CreateFloatArray() (+23 more)

### Community 4 - "cJSON"
Cohesion: 0.11
Nodes (25): cJSON_DeleteItemFromArray(), cJSON_DeleteItemFromObject(), cJSON_DeleteItemFromObjectCaseSensitive(), cJSON_DetachItemFromArray(), cJSON_DetachItemFromObject(), cJSON_DetachItemFromObjectCaseSensitive(), cJSON_DetachItemViaPointer(), cJSON_GetNumberValue() (+17 more)

### Community 5 - "cJSON.c"
Cohesion: 0.11
Nodes (11): cJSON_AddFalseToObject(), cJSON_AddNullToObject(), cJSON_CreateFalse(), cJSON_CreateNull(), cJSON_InitHooks(), cJSON_Minify(), minify_string(), parse_hex4() (+3 more)

### Community 6 - "main"
Cohesion: 0.18
Nodes (18): appendChar(), deleteChar(), gameDestroy(), gameLoop(), getRemainingChars(), menu(), showProgress(), showResults() (+10 more)

### Community 7 - "cJSON_New_Item"
Cohesion: 0.14
Nodes (21): add_item_to_array(), add_item_to_object(), cast_away_const(), cJSON_AddArrayToObject(), cJSON_AddItemReferenceToArray(), cJSON_AddItemReferenceToObject(), cJSON_AddItemToArray(), cJSON_AddItemToObjectCS() (+13 more)

### Community 8 - "cJSON_bool"
Cohesion: 0.40
Nodes (12): cJSON_PrintBuffered(), cJSON_PrintPreallocated(), compare_double(), ensure(), print(), print_array(), print_number(), print_object() (+4 more)

### Community 9 - "cJSON_ParseWithLengthOpts"
Cohesion: 0.30
Nodes (11): buffer_skip_whitespace(), cJSON_ParseWithLength(), cJSON_ParseWithLengthOpts(), cJSON_ParseWithOpts(), get_decimal_point(), parse_array(), parse_number(), parse_object() (+3 more)

### Community 10 - "entrypoint.sh"
Cohesion: 0.20
Nodes (10): ENGINE_HANDSHAKE_TIMEOUT_SEC, FINISH_GRACE_TIMEOUT_SEC, LOBBY_TIMEOUT_MS, MAX_LOBBY_CAPACITY, RACE_MAX_DURATION_SEC, entrypoint.sh script, shutdown(), TCP_COMMAND_TIMEOUT_MS (+2 more)

### Community 11 - "replace_item_in_object"
Cohesion: 0.25
Nodes (8): case_insensitive_strcmp(), cJSON_Compare(), cJSON_free(), cJSON_ReplaceItemInObject(), cJSON_ReplaceItemInObjectCaseSensitive(), cJSON_SetValuestring(), get_object_item(), replace_item_in_object()

## Knowledge Gaps
- **36 isolated node(s):** `build.sh script`, `LOBBY_TIMEOUT_MS`, `TCP_COMMAND_TIMEOUT_MS`, `TCP_KEEPALIVE_MS`, `WS_HEARTBEAT_INTERVAL_MS` (+31 more)
  These have ≤1 connection - possible missing edges. (Counts symbols only; 70 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **3 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `coordinator_connection_worker()` connect `server.c` to `test_network.c`, `cJSON_Delete`, `cJSON_New_Item`?**
  _High betweenness centrality (0.056) - this node is a cross-community bridge._
- **Are the 8 inferred relationships involving `cJSON_Delete()` (e.g. with `coordinator_connection_worker()` and `generateRoomSummary()`) actually correct?**
  _`cJSON_Delete()` has 8 INFERRED edges - model-reasoned connections that need verification._
- **What connects `build.sh script`, `LOBBY_TIMEOUT_MS`, `TCP_COMMAND_TIMEOUT_MS` to the rest of the system?**
  _36 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `test_network.c` be split into smaller, more focused modules?**
  _Cohesion score 0.07868852459016394 - nodes in this community are weakly interconnected._
- **Why does `cJSON_Delete()` connect `cJSON_Delete` to `server.c`, `cJSON`, `cJSON.c`, `cJSON_New_Item`, `cJSON_ParseWithLengthOpts`?**
  _High betweenness centrality (0.049) - this node is a cross-community bridge._
- **Should `gateway.js` be split into smaller, more focused modules?**
  _Cohesion score 0.04734299516908213 - nodes in this community are weakly interconnected._
- **Why does `main()` connect `main` to `test_network.c`, `server.c`, `cJSON_Delete`?**
  _High betweenness centrality (0.047) - this node is a cross-community bridge._