// FMOD Studio script (run by tools/make_audio.py through `fmodstudiocl -script`, which fills in OUT
// and EVENTS): writes the named events, and every object they reach through the relationships in
// FOLLOW, to OUT as JSON with every property resolved (defaults included, which the project's XML
// leaves out):
//   {"events": {"event:/path": id, ...},
//    "objects": {id: {"entity": "SingleSound", "props": {name: value}, "rels": {name: [id, ...]}}},
//    "parameterNames": {GameParameter id: its preset's name}}
// Read-only: it never saves the project.
var OUT = "%OUT%";
var EVENTS = %EVENTS%;

var FOLLOW = [
    "masterTrack", "groupTracks", "returnTracks", "mixerGroup", "effectChain", "effects", "modulators",
    "automationTracks", "automator", "automators", "automationCurves", "automationPoints", "parameter",
    "parameters", "preset", "modules", "sounds", "audioFile", "fadeInCurve", "fadeOutCurve", "startPoint",
    "endPoint", "timeline", "markers", "transitionTimeline", "destination", "triggerConditions",
    "automatableProperties", "mixerInput", "output",
];

// Shared objects whose relationships lead back into other events: kept, not followed (a project
// bus only up to the master).
var LEAVES = ["AudioFile", "GameParameter"];
var BUSES = ["MixerGroup", "MixerMaster", "MixerReturn"];

var objects = {};
function visit(o) {
    if (!o || objects[o.id]) return;
    var rec = {entity: o.entity, props: {}, rels: {}};
    objects[o.id] = rec;
    Object.keys(o.properties).forEach(function (k) {
        var v = o[k];
        if (typeof v === "number" || typeof v === "string" || typeof v === "boolean") rec.props[k] = v;
    });
    if (LEAVES.indexOf(o.entity) >= 0) return;
    (BUSES.indexOf(o.entity) >= 0 ? ["output"] : FOLLOW).forEach(function (k) {
        if (!o.relationships[k]) return;
        var ds = o.relationships[k].destinations;
        rec.rels[k] = ds.map(function (d) { return d.id; });
        ds.forEach(visit);
    });
}

var events = {};
EVENTS.forEach(function (path) {
    var ev = studio.project.lookup(path);
    if (!ev) throw new Error("no event " + path);
    events[path] = ev.id;
    visit(ev);
});

// A GameParameter's name is its preset's.
var parameterNames = {};
studio.project.model.ParameterPreset.findInstances().forEach(function (p) {
    if (p.parameter) parameterNames[p.parameter.id] = p.name;
});

var f = studio.system.getFile(OUT);
f.open(studio.system.openMode.WriteOnly);
f.writeText(JSON.stringify({events: events, objects: objects, parameterNames: parameterNames}));
f.close();
console.log("fmod_export: " + EVENTS.length + " events, " + Object.keys(objects).length + " objects");
