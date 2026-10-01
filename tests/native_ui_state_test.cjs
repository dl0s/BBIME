"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const root = path.resolve(__dirname, "..");
function block(source, marker) {
    const start = source.indexOf(marker);
    assert.ok(start >= 0, marker);
    const begin = source.indexOf("{", start);
    let depth = 1, quote = "", comment = "";
    for (let i = begin + 1; i < source.length; ++i) {
        const c = source[i], next = source[i + 1];
        if (comment === "line") { if (c === "\n") comment = ""; continue; }
        if (comment === "block") { if (c === "*" && next === "/") { comment = ""; ++i; } continue; }
        if (quote) { if (c === "\\") ++i; else if (c === quote) quote = ""; continue; }
        if (c === "/" && next === "/") { comment = "line"; ++i; continue; }
        if (c === "/" && next === "*") { comment = "block"; ++i; continue; }
        if (c === '"' || c === "'") { quote = c; continue; }
        if (c === "{") ++depth;
        else if (c === "}" && --depth === 0) return source.slice(begin + 1, i);
    }
    throw new Error("Unclosed " + marker);
}
function handler(source, marker) {
    return new Function("event", "with (this) { " + block(source, marker) + " }");
}
const panelSource = fs.readFileSync(path.join(root, "assets/NativeSymbolPanel.qml"), "utf8");
function panel() {
    const view = { animationPhase: 0, syncingGroup: false, groups: { selectedIndex: 0 }, opens: 0, closes: 0, restored: 0,
        grid: { requestFocus() {} }, open() { ++this.opens; }, close() { ++this.closes; } };
    view.ime = { symbolsVisible: false, symbolGroup: 0, symbolPanelActive: false,
        setSymbolPanelActive(value) { this.symbolPanelActive = value; view.reconcile(); },
        closeSymbols() { this.symbolsVisible = false; view.reconcile(); },
        restoreFocus() { ++view.restored; } };
    view.reconcile = handler(panelSource, "function reconcile()").bind(view);
    view.openedEvent = handler(panelSource, "onOpened:").bind(view);
    view.closedEvent = handler(panelSource, "onClosed:").bind(view);
    return view;
}
let cases = 0;
{
    const view = panel();
    view.ime.symbolsVisible = true; view.reconcile(); view.reconcile();
    assert.equal(view.opens, 1); assert.equal(view.ime.symbolPanelActive, true);
    view.ime.symbolsVisible = false; view.reconcile();
    assert.equal(view.closes, 0, "closing during opening waits for SDK completion");
    view.openedEvent(); assert.equal(view.closes, 1);
    view.closedEvent(); assert.equal(view.animationPhase, 0);
    assert.equal(view.ime.symbolPanelActive, false); assert.equal(view.restored, 1);
    ++cases;
}
{
    const view = panel();
    view.ime.symbolsVisible = true; view.reconcile(); view.openedEvent();
    view.ime.closeSymbols(); assert.equal(view.animationPhase, 3);
    view.ime.symbolsVisible = true; view.reconcile();
    view.closedEvent();
    assert.equal(view.ime.symbolsVisible, true, "old close cannot cancel newer intent");
    assert.equal(view.opens, 2); assert.equal(view.restored, 0);
    view.openedEvent(); assert.equal(view.animationPhase, 2);
    ++cases;
}
{
    const view = panel();
    view.ime.symbolsVisible = true; view.reconcile(); view.openedEvent();
    view.closedEvent();
    assert.equal(view.ime.symbolsVisible, false, "external dismissal cancels symbols");
    assert.equal(view.opens, 1); assert.equal(view.restored, 1);
    ++cases;
}
{
    const view = panel();
    for (let i = 0; i < 100; ++i) {
        view.ime.symbolsVisible = true; view.reconcile(); view.openedEvent();
        view.ime.closeSymbols(); view.closedEvent();
    }
    assert.equal(view.opens, 100); assert.equal(view.closes, 100);
    assert.equal(view.restored, 100); assert.equal(view.ime.symbolPanelActive, false);
    ++cases;
}
const toggleSource = fs.readFileSync(path.join(root, "assets/ImeToggle.qml"), "utf8");
const onTouch = handler(toggleSource, "onTouch:");
function touch(kind, x = 20, y = 20) {
    return { localX: x, localY: y, isDown: () => kind === "down", isMove: () => kind === "move",
        isUp: () => kind === "up", isCancel: () => kind === "cancel" };
}
{
    const view = { enabled: true, touchPressed: false, count: 0, clicked() { ++this.count; } };
    onTouch.call(view, touch("down")); onTouch.call(view, touch("up"));
    assert.equal(view.count, 1);
    onTouch.call(view, touch("down")); onTouch.call(view, touch("move", 90)); onTouch.call(view, touch("up"));
    onTouch.call(view, touch("down")); onTouch.call(view, touch("cancel")); onTouch.call(view, touch("up"));
    view.enabled = false; onTouch.call(view, touch("down")); onTouch.call(view, touch("up"));
    assert.equal(view.count, 1, "drag, cancellation and disabled touches cannot activate");
    ++cases;
}
console.log("PASS: " + cases + " production QML animation/touch cases (SDK rendering and physical gestures not exercised)");
