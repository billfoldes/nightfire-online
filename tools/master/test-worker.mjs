// worker.js under Node (18+), its Durable Object binding stood in for: node tools/master/test-worker.mjs
import assert from "node:assert/strict";
import worker, {Lobby} from "./worker.js";

const lobby = new Lobby({}, {});
const env = {LOBBY: {idFromName: name => name, get: () => lobby}};
const call = (method, path, ip, body) =>
  worker.fetch(new Request("https://list.example" + path, {method, body, headers: {"CF-Connecting-IP": ip}}), env)
    .then(async r => [r.status, await r.text()]);
const form = (o) => new URLSearchParams(o).toString();
const game = {port: 26565, version: 11, flags: 0, in: 1, wanted: 2, id: 7, name: "Bond\tJames\n", note: ""};

assert.deepEqual(await call("GET", "/list", "1.2.3.4"), [200, "NFM1\n"]);
assert.deepEqual(await call("POST", "/announce", "1.2.3.4", form(game)), [200, "ok\t1.2.3.4\n"]);
assert.deepEqual(await call("GET", "/list", "5.6.7.8"), [200, "NFM1\n1.2.3.4\t26565\t11\t0\t1\t2\t7\tBondJames\t\t\t\n"]);
// a game through a relay: its code and relay, and no address
assert.equal((await call("POST", "/announce", "9.9.9.9", form({...game, code: "NF-ABC12O", relay: "r.example"})))[0], 400);
assert.equal((await call("POST", "/announce", "9.9.9.9", form({...game, code: "NF-ABC234"})))[0], 400);
assert.equal((await call("POST", "/announce", "9.9.9.9", form({...game, code: "NF-ABC234", relay: "r.example:26581"})))[0], 200);
const relayed = (await call("GET", "/list", "5.6.7.8"))[1];
assert.ok(relayed.includes("-\t0\t11\t0\t1\t2\t7\tBondJames\t\tNF-ABC234\tr.example:26581\n") && !relayed.includes("9.9.9.9"));
// its code withheld: the relay alone, still no address
assert.equal((await call("POST", "/announce", "9.9.9.9", form({...game, flags: 24, code: "-"})))[0], 400);
assert.equal((await call("POST", "/announce", "9.9.9.9", form({...game, flags: 24, code: "-", relay: "r.example"})))[0], 200);
const hidden = (await call("GET", "/list", "5.6.7.8"))[1];
assert.ok(hidden.includes("-\t0\t11\t24\t1\t2\t7\tBondJames\t\t-\tr.example\n") && !hidden.includes("9.9.9.9"));
assert.deepEqual(await call("POST", "/remove", "9.9.9.9", form({port: 26565})), [200, "ok\n"]);
assert.equal((await call("POST", "/announce", "1.2.3.4", form({...game, in: "x"})))[0], 400);
for (let p = 1; p < 8; p++) assert.equal((await call("POST", "/announce", "1.2.3.4", form({...game, port: p})))[0], 200);
assert.equal((await call("POST", "/announce", "1.2.3.4", form({...game, port: 9})))[0], 429);
assert.deepEqual(await call("POST", "/remove", "5.6.7.8", form({port: 26565})), [200, "ok\n"]);
assert.equal((await call("GET", "/list", "1.2.3.4"))[1].split("\n").length, 10);  // another address removes nothing
assert.deepEqual(await call("POST", "/remove", "1.2.3.4", form({port: 26565})), [200, "ok\n"]);
assert.equal((await call("GET", "/list", "1.2.3.4"))[1].includes("\t26565\t"), false);
// the other servers: SERVERS' lines alone, comments and junk dropped
const named = {...env, SERVERS: "list https://a.example # the other list\nrelay 1.2.3.4:26581\njunk\n"};
const servers = await worker.fetch(new Request("https://list.example/servers"), named);
assert.equal(await servers.text(), "NFS1\nlist https://a.example\nrelay 1.2.3.4:26581\n");
assert.equal(await (await worker.fetch(new Request("https://list.example/servers"), env)).text(), "NFS1\n");
console.log("worker.js OK");
