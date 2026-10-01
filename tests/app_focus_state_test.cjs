"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const root = path.resolve(__dirname, "..");

// Extract actual production handlers, including one-line QML statements.
// The test doubles record calls; they do not implement the C++ focus policy.
function braces(source) {
    const stack = [], pairs = [];
    let quote = "", comment = "";
    for (let i = 0; i < source.length; ++i) {
        const c = source[i], next = source[i + 1];
        if (comment === "line") { if (c === "\n") comment = ""; continue; }
        if (comment === "block") { if (c === "*" && next === "/") { comment = ""; ++i; } continue; }
        if (quote) { if (c === "\\") ++i; else if (c === quote) quote = ""; continue; }
        if (c === "/" && next === "/") { comment = "line"; ++i; continue; }
        if (c === "/" && next === "*") { comment = "block"; ++i; continue; }
        if (c === '"' || c === "'") { quote = c; continue; }
        if (c === "{") stack.push(i);
        else if (c === "}") { assert.ok(stack.length, "balanced production QML"); pairs.push([stack.pop(), i]); }
    }
    assert.equal(stack.length, 0, "balanced production QML");
    return pairs;
}
function objectAt(source, marker) {
    const position = source.indexOf(marker);
    assert.ok(position >= 0, "production object marker: " + marker);
    const enclosing = braces(source).filter(([start, end]) => start < position && position < end)
        .sort((a, b) => (a[1] - a[0]) - (b[1] - b[0]));
    assert.ok(enclosing.length, marker);
    return source.slice(enclosing[0][0] + 1, enclosing[0][1]);
}
function body(source, marker) {
    const position = source.indexOf(marker);
    assert.ok(position >= 0, "production handler: " + marker);
    let start = position + marker.length;
    while (/\s/.test(source[start] || "")) ++start;
    if (source[start] === "{") {
        const pair = braces(source).find(([begin]) => begin === start);
        assert.ok(pair, marker);
        return source.slice(start + 1, pair[1]);
    }
    let quote = "", depth = 0;
    for (let i = start; i < source.length; ++i) {
        const c = source[i];
        if (quote) { if (c === "\\") ++i; else if (c === quote) quote = ""; continue; }
        if (c === '"' || c === "'") { quote = c; continue; }
        if (c === "(" || c === "[") ++depth;
        else if (c === ")" || (c === "]" && depth)) --depth;
        else if (!depth && (c === "\n" || c === "\r" || c === "}" || c === "]")) return source.slice(start, i);
    }
    return source.slice(start);
}
function handler(source, marker) {
    return new Function("event", "with (this) { " + body(source, marker) + " }");
}
function visualChildren(source) {
    const pairs = braces(source);
    const controlTypes = new Set(["Container", "SegmentedControl", "TextArea", "TextField", "ListView", "CandidateStrip", "Label", "ImeToggle"]);
    return pairs.filter(([start, end]) => !pairs.some(([outerStart, outerEnd]) => outerStart < start && end < outerEnd))
        .map(([start, end]) => {
            const type = source.slice(0, start).match(/([A-Z][A-Za-z0-9_.]*)\s*$/);
            return { start, type: type ? type[1] : "", source: source.slice(start + 1, end) };
        }).filter(child => controlTypes.has(child.type)).sort((a, b) => a.start - b.start);
}
function binding(source, property) {
    return new Function("with (this) { return (" + body(source, property + ":").trim() + "); }");
}
const main = fs.readFileSync(path.join(root, "assets/main.qml"), "utf8");
const native = fs.readFileSync(path.join(root, "assets/NativeModulePage.qml"), "utf8");
const editorSource = objectAt(main, "id: editor");
const titleSource = objectAt(native, "id: title");
const bodySource = objectAt(native, "id: body");
const passwordSource = objectAt(native, "id: password");
const moduleSheetSource = objectAt(main, "id: moduleSheet");
const settingsSheetSource = objectAt(main, "id: settingsSheet");
const candidateSource = objectAt(main, "id: candidates");
const stripSource = fs.readFileSync(path.join(root, "assets/CandidateStrip.qml"), "utf8");
const ActionMenuVisualState = { Hidden: 0, AnimatingToVisibleFull: 1, VisibleFull: 2, VisibleCompact: 3, AnimatingToHidden: 4 };
let cases = 0;
function check(name, test) { test(); ++cases; console.log("PASS: " + name); }
function fixture() {
    const calls = [];
    const backend = new Proxy({ mode: "natural", imeEnabled: false, nativeModule: { ready: true }, startResult: true }, {
        get(target, name) {
            if (name in target) return target[name];
            return (...args) => { calls.push([name, ...args]); };
        }
    });
    backend.startNativeModule = () => { calls.push(["startNativeModule"]); return backend.startResult; };
    const context = { backend, calls, ActionMenuVisualState, sheetActive: false,
        sheetOpened: false, actionMenuVisualState: ActionMenuVisualState.Hidden,
        pendingIndex: -1, pendingGeneration: null,
        moduleSheet: { open() { calls.push(["moduleSheet.open"]); }, close() { calls.push(["moduleSheet.close"]); } },
        settingsSheet: { open() { calls.push(["settingsSheet.open"]); }, close() { calls.push(["settingsSheet.close"]); } } };
    context.page = context;
    context.syncMainScope = handler(main, "function syncMainScope()").bind(context);
    context.nativeModulePage = { backend, calls, ActionMenuVisualState, sheetOpened: false,
        actionMenuVisualState: ActionMenuVisualState.Hidden };
    context.nativeModulePage.syncNativeScope = handler(native, "function syncNativeScope()").bind(context.nativeModulePage);
    return context;
}

check("main page scope follows menu and Sheet state without unsupported Page visibility", () => {
    const view = fixture();
    handler(main, "onCreationCompleted:").call(view);
    assert.deepEqual(view.calls.pop(), ["setMainScopeActive", true]);
    assert.ok(!/\bvisible\b|\bonVisibleChanged\s*:/.test(body(main, "function syncMainScope()")));
    assert.ok(!main.includes("onVisibleChanged:"));
    for (const state of Object.values(ActionMenuVisualState)) {
        view.actionMenuVisualState = state;
        handler(main, "onActionMenuVisualStateChanged:").call(view);
        assert.deepEqual(view.calls.pop(), ["setMainScopeActive", state === ActionMenuVisualState.Hidden]);
    }
    view.sheetActive = true; view.syncMainScope();
    assert.deepEqual(view.calls.pop(), ["setMainScopeActive", false]);
});
check("main editor attaches and forwards genuine focus plus the untouched first key", () => {
    const view = fixture(), firstKey = { keycap: 110, key: 110, pressed: true };
    view.editor = { requestFocus() { view.calls.push(["editor.requestFocus"]); } };
    handler(editorSource, "onCreationCompleted:").call(view);
    assert.deepEqual(view.calls, [["attachEditor", view.editor], ["editor.requestFocus"]]);
    view.calls.length = 0;
    view.focused = true; handler(editorSource, "onFocusedChanged:").call(view);
    view.focused = false; handler(editorSource, "onFocusedChanged:").call(view);
    handler(editorSource, "onKeyEvent:").call(view, firstKey);
    assert.deepEqual(view.calls, [["editorFocusChanged", true], ["editorFocusChanged", false], ["handleKey", firstKey]]);
});
check("settings scope closes before Sheet opens and close never forces editor focus", () => {
    const view = fixture();
    handler(objectAt(main, 'title: "设置"'), "onTriggered:").call(view);
    assert.equal(view.sheetActive, true);
    assert.deepEqual(view.calls, [["setMainScopeActive", false], ["settingsSheet.open"]]);
    view.calls.length = 0;
    handler(settingsSheetSource, "onClosed:").call(view);
    assert.equal(view.sheetActive, false);
    assert.deepEqual(view.calls, [["setMainScopeActive", true]]);
});
check("native Sheet opening holds main scope and rolls back a failed start", () => {
    const action = handler(objectAt(main, 'title: "原生模块"'), "onTriggered:");
    const success = fixture(); action.call(success);
    assert.equal(success.sheetActive, true);
    assert.deepEqual(success.calls, [["setMainScopeActive", false], ["startNativeModule"], ["moduleSheet.open"]]);
    assert.equal(success.nativeModulePage.sheetOpened, false, "native input waits for real opened event");
    const failure = fixture(); failure.backend.startResult = false; action.call(failure);
    assert.equal(failure.sheetActive, false);
    assert.deepEqual(failure.calls, [["setMainScopeActive", false], ["startNativeModule"], ["setMainScopeActive", true]]);
});
check("native scope requires Sheet opened and hidden menu without unsupported Page visibility", () => {
    const view = fixture(), page = view.nativeModulePage;
    handler(native, "onCreationCompleted:").call(page);
    assert.deepEqual(view.calls.pop(), ["setNativeScopeActive", false]);
    handler(moduleSheetSource, "onOpened:").call(view);
    assert.equal(page.sheetOpened, true);
    assert.deepEqual(view.calls.pop(), ["setNativeScopeActive", true]);
    assert.ok(!/\bvisible\b|\bonVisibleChanged\s*:/.test(body(native, "function syncNativeScope()")));
    assert.ok(!native.includes("onVisibleChanged:"));
    for (const state of Object.values(ActionMenuVisualState)) {
        page.actionMenuVisualState = state;
        handler(native, "onActionMenuVisualStateChanged:").call(page);
        assert.deepEqual(view.calls.pop(), ["setNativeScopeActive", state === ActionMenuVisualState.Hidden]);
    }
});
check("native Sheet close revokes native input before returning main scope", () => {
    const view = fixture(); view.sheetActive = true; view.nativeModulePage.sheetOpened = true;
    handler(moduleSheetSource, "onClosed:").call(view);
    assert.equal(view.nativeModulePage.sheetOpened, false); assert.equal(view.sheetActive, false);
    assert.deepEqual(view.calls, [["setNativeScopeActive", false], ["stopNativeModule"], ["setMainScopeActive", true]]);
});
check("native finished revokes its scope before the closing animation", () => {
    const view = fixture(); view.nativeModulePage.sheetOpened = true;
    handler(objectAt(main, "id: nativeModulePage"), "onFinished:").call(view);
    assert.equal(view.nativeModulePage.sheetOpened, false);
    assert.deepEqual(view.calls, [["setNativeScopeActive", false], ["moduleSheet.close"]]);
});
check("native title-bar dismissal revokes scope before emitting finished", () => {
    const view = fixture(); view.finished = () => view.calls.push(["finished"]);
    handler(objectAt(native, 'title: "关闭"'), "onTriggered:").call(view);
    assert.deepEqual(view.calls, [["setNativeScopeActive", false], ["finished"]]);
});
check("native title and body register but password only reports focus", () => {
    const view = fixture();
    view.title = { id: "title" }; view.body = { id: "body" }; view.password = { id: "password" };
    for (const [source, field] of [[titleSource, view.title], [bodySource, view.body]]) {
        handler(source, "onCreationCompleted:").call(view);
        assert.deepEqual(view.calls.pop(), ["registerNativeEditor", field]);
        view.focused = true; handler(source, "onFocusedChanged:").call(view);
        assert.deepEqual(view.calls.pop(), ["nativeEditorFocusChanged", field, true]);
        view.focused = false; handler(source, "onFocusedChanged:").call(view);
        assert.deepEqual(view.calls.pop(), ["nativeEditorFocusChanged", field, false]);
        const event = { keycap: 110, pressed: true };
        handler(source, "onKeyEvent:").call(view, event);
        assert.deepEqual(view.calls.pop(), ["handleNativeKey", field, event]);
    }
    assert.ok(!passwordSource.includes("registerNativeEditor") && !passwordSource.includes("registerEditor"));
    view.focused = true; handler(passwordSource, "onFocusedChanged:").call(view);
    assert.deepEqual(view.calls.pop(), ["nativeEditorFocusChanged", view.password, true]);
});
check("manual toggles call the host pause gates without direct enabled writes", () => {
    const view = fixture();
    handler(objectAt(main, "id: imeToggle"), "onToggleRequested:").call(view);
    handler(native, "onToggleRequested:").call(view);
    assert.deepEqual(view.calls, [["toggleIme"], ["toggleNativeIme"]]);
    assert.ok(!native.includes("ime.enabled = !ime.enabled"));
});
check("native submit keeps title to body navigation isolated", () => {
    const view = fixture(); view.title = {}; view.body = { requestFocus() { view.calls.push(["body.requestFocus"]); } };
    view.editor = view.title; handler(native, "onSubmitRequested:").call(view);
    assert.deepEqual(view.calls, [["body.requestFocus"]]);
    view.calls.length = 0; view.editor = view.body; handler(native, "onSubmitRequested:").call(view);
    assert.deepEqual(view.calls, []);
});
check("candidate clicks preserve the actual row generation for C++ stale-click rejection", () => {
    const view = fixture(); view.indexPath = [3];
    view.dataModel = { data(indexPath) { assert.equal(indexPath, view.indexPath); return { generation: 19 }; } };
    const click = handler(candidateSource, "onTriggered:"); click.call(view);
    assert.deepEqual(view.calls, [["chooseCandidateAt", 3, 19]]);
    view.calls.length = 0; view.dataModel.data = () => null; click.call(view);
    assert.deepEqual(view.calls, [], "missing model row cannot manufacture a ticket");
});
check("candidate touch keeps the old generation across a model rebuild", () => {
    const view = fixture(); view.indexPath = [3];
    const item = { candidateIndex: 3, ListItem: { view }, ListItemData: { generation: 19 } };
    const touch = handler(objectAt(main, "id: candidateItem"), "onTouch:");
    const down = { isDown: () => true, isCancel: () => false,
        accept() { throw new Error("candidate touch must leave ListView gesture ownership intact"); } };
    touch.call(item, down);
    assert.equal(view.pendingIndex, 3); assert.equal(view.pendingGeneration, 19);
    view.dataModel = { data() { throw new Error("touch snapshot must not read a newer row"); } };
    item.ListItemData.generation = 20;
    handler(candidateSource, "onTriggered:").call(view);
    assert.deepEqual(view.calls, [["chooseCandidateAt", 3, 19]], "C++ receives the generation seen at touch down");
    assert.equal(view.pendingIndex, -1); assert.equal(view.pendingGeneration, null);
});
check("candidate mismatched trigger and touch cancellation discard pending snapshots", () => {
    const view = fixture(); view.indexPath = [4];
    const item = { candidateIndex: 3, ListItem: { view }, ListItemData: { generation: 19 } };
    const touch = handler(objectAt(main, "id: candidateItem"), "onTouch:");
    const event = kind => ({ isDown: () => kind === "down", isCancel: () => kind === "cancel" });
    touch.call(item, event("down"));
    view.dataModel = { data() { throw new Error("mismatched old gesture cannot fall through to current row"); } };
    handler(candidateSource, "onTriggered:").call(view);
    assert.deepEqual(view.calls, []); assert.equal(view.pendingIndex, -1); assert.equal(view.pendingGeneration, null);
    touch.call(item, event("down")); touch.call(item, event("move")); touch.call(item, event("up"));
    assert.equal(view.pendingGeneration, 19, "normal ListView gesture keeps its original generation until trigger");
    touch.call(item, event("cancel"));
    assert.equal(view.pendingIndex, -1); assert.equal(view.pendingGeneration, null);
});
check("shared candidate auxiliary trigger forwards the current complete ticket", () => {
    const calls = [], view = { pendingIndex: -1, pendingTicket: null, indexPath: [2],
        ime: { chooseCandidate(...ticket) { calls.push(ticket); } },
        dataModel: { data() { return { session: 11, revision: 12, document: 13, index: 2 }; } } };
    const trigger = handler(stripSource, "onTriggered:"); trigger.call(view);
    assert.deepEqual(calls, [[11, 12, 13, 2]]);
    view.dataModel.data = () => null; trigger.call(view);
    assert.equal(calls.length, 1, "missing model row does not invent a session ticket");
});
check("shared candidate touch copies the original full ticket across model and field changes", () => {
    const calls = [], view = { pendingIndex: -1, pendingTicket: null, indexPath: [2],
        ime: { chooseCandidate(...ticket) { calls.push(ticket); } },
        dataModel: { data() { throw new Error("an old gesture must not read the new editor's candidate row"); } } };
    const original = { session: 11, revision: 12, document: 13, index: 2 };
    const item = { candidateIndex: 2, ListItem: { view }, ListItemData: original };
    const touch = handler(objectAt(stripSource, "id: item"), "onTouch:");
    touch.call(item, { isDown: () => true, isCancel: () => false,
        accept() { throw new Error("candidate touch must leave ListView gesture handling intact"); } });
    assert.notEqual(view.pendingTicket, original, "ticket is a value snapshot, not a mutable model row");
    Object.assign(original, { session: 21, revision: 22, document: 23, index: 5 });
    assert.deepEqual(view.pendingTicket, { session: 11, revision: 12, document: 13, index: 2 });
    handler(stripSource, "onTriggered:").call(view);
    assert.deepEqual(calls, [[11, 12, 13, 2]], "the C++ core receives the stale ticket to reject, never the replacement field's ticket");
    assert.equal(view.pendingIndex, -1); assert.equal(view.pendingTicket, null);
});
check("shared candidate wrong-row trigger and cancelled gesture clear the snapshot", () => {
    const calls = [], view = { pendingIndex: -1, pendingTicket: null, indexPath: [3],
        ime: { chooseCandidate(...ticket) { calls.push(ticket); } },
        dataModel: { data() { throw new Error("wrong-row gesture cannot fall back to a current candidate"); } } };
    const item = { candidateIndex: 2, ListItem: { view },
        ListItemData: { session: 11, revision: 12, document: 13, index: 2 } };
    const touch = handler(objectAt(stripSource, "id: item"), "onTouch:");
    const event = kind => ({ isDown: () => kind === "down", isCancel: () => kind === "cancel" });
    touch.call(item, event("down")); handler(stripSource, "onTriggered:").call(view);
    assert.deepEqual(calls, []); assert.equal(view.pendingIndex, -1); assert.equal(view.pendingTicket, null);
    touch.call(item, event("down")); touch.call(item, event("move")); touch.call(item, event("up"));
    assert.deepEqual(view.pendingTicket, { session: 11, revision: 12, document: 13, index: 2 });
    touch.call(item, event("cancel"));
    assert.equal(view.pendingIndex, -1); assert.equal(view.pendingTicket, null);
});
check("BBIME pages contain no custom Sym entry, Dialog or panel connection", () => {
    for (const source of [main, native]) {
        assert.ok(!/\bDialog\s*\{|NativeSymbolPanel\s*\{|\bcycleSymbols\s*\(|\bchooseSymbol\s*\(|\bonSymbolsChanged\s*:/.test(source));
        assert.ok(!/text\s*:\s*["']Sym["']/.test(source));
    }
});
check("main candidates occupy the last content row after the footer with a flexible editor", () => {
    const content = objectAt(main, "id: content"), children = visualChildren(content);
    assert.equal(binding(content, "bottomPadding").call({}), 0);
    assert.match(content, /layout:\s*StackLayout\s*\{\s*orientation:\s*LayoutOrientation\.TopToBottom/);
    assert.match(editorSource, /layoutProperties:\s*StackLayoutProperties\s*\{\s*spaceQuota:\s*1\s*\}/);
    assert.ok(children.length >= 3);
    assert.equal(children.at(-1).type, "ListView");
    assert.ok(children.at(-1).source.includes("id: candidates"));
    assert.ok(children.at(-2).source.includes('backend.recordLayout("footer"'), "footer is above the final candidate dock");
    assert.match(candidateSource, /verticalAlignment:\s*VerticalAlignment\.Bottom/);
    for (const property of ["preferredHeight", "minHeight", "maxHeight"])
        assert.equal(binding(candidateSource, property).call({}), 72);
    for (const property of ["topMargin", "bottomMargin"])
        assert.equal(binding(candidateSource, property).call({}), 0);
    for (const imeEnabled of [true, false]) {
        const view = { backend: { imeEnabled } };
        assert.equal(binding(candidateSource, "visible").call(view), imeEnabled);
        assert.equal(binding(candidateSource, "enabled").call(view), imeEnabled);
    }
});
check("native candidate dock is the final row and hides while paused", () => {
    const content = objectAt(native, "id: nativeContent"), children = visualChildren(content);
    assert.equal(binding(content, "bottomPadding").call({}), 0);
    assert.match(content, /layout:\s*StackLayout\s*\{\s*orientation:\s*LayoutOrientation\.TopToBottom/);
    assert.match(bodySource, /layoutProperties:\s*StackLayoutProperties\s*\{\s*spaceQuota:\s*1\s*\}/);
    assert.equal(children.at(-1).type, "CandidateStrip");
    assert.match(stripSource, /verticalAlignment:\s*VerticalAlignment\.Bottom/);
    for (const property of ["preferredHeight", "minHeight", "maxHeight"])
        assert.equal(binding(stripSource, property).call({}), 72);
    for (const property of ["topMargin", "bottomMargin"])
        assert.equal(binding(stripSource, property).call({}), 0);
    assert.equal(Boolean(binding(stripSource, "visible").call({ ime: null })), false);
    for (const enabled of [true, false]) {
        assert.equal(binding(stripSource, "visible").call({ ime: { enabled } }), enabled);
        assert.equal(binding(stripSource, "enabled").call({ ime: { enabled } }), enabled);
    }
});
console.log("PASS: " + cases + " BBIME production QML focus/Sheet/key/candidate/dock cases (test doubles and declarative structure; SDK focus delivery, rendering, physical keys and C++ key handling not exercised)");
