// Pyrite64 node-definition API: the globals node scripts use.

var _REGISTRY = {};
var _TYPES = {};        // value-pin data types, by id
var _CONVERSIONS = {};  // "from>to" -> conversion template ("{}" = source expr)

function icon(n) { return (globalThis.__ICONS && globalThis.__ICONS[n]) || ""; } // MDI glyph by name
function crc32(s) { return __crc32(s); }               // matches Utils::Hash::crc32
function lambda(body) { return "[&]{ " + body + " }()"; }

// --- value types -------------------------------------------------------------
function valueType(id, o) {
  o = o || {};
  _TYPES[id] = {
    id: id,
    name: o.name || id,
    cType: o.cType || "int",
    default: ("default" in o) ? ("" + o.default) : "0",
    color: o.color || [0xFF, 0x99, 0x55],
    size: ("size" in o) ? o.size : 4,
  };
}
// Implicit conversion 'from'->'to'; "{}" in tmpl is the source expression.
function convert(from, to, tmpl) { _CONVERSIONS[from + ">" + to] = tmpl; }

function typeColor(id) {
  var t = _TYPES[id];
  return t ? t.color : [0xFF, 0x99, 0x55];
}

// The generated C++ type for a value type id.
function typeCType(id) {
  var t = _TYPES[id];
  return t ? t.cType : "int";
}

// Named node header colors (use as e.g. color: Color.orange).
var Color = {
  orange: [0xFF, 0x99, 0x55],
  green:  [90, 191, 93],
  red:    [191, 90, 93],
  grey:   [0x88, 0x88, 0x88],
  white:  [0xEE, 0xEE, 0xEE],
};

// --- pins --------------------------------------------------------------------
// Value pins carry a data type (default "f32"); logic pins are untyped.
function logicIn(name)        { return { value: false, name: name || "" }; }
function valueIn(name, type, def) { return { value: true, name: name || "", type: type || "f32", default: def || 0 }; }
function logicOut(name)       { return { value: false, name: name || "" }; }
function valueOut(type, name) { return { value: true,  name: name || "", type: type || "f32" }; }

// --- property descriptors ----------------------------------------------------
function _prop(type, o) {
  o = o || {};
  return {
    type: type,
    label: o.label || "",
    width: (o.width != null) ? o.width : 80,
    default: ("default" in o) ? o.default : null,
    hideIfInputConnected: ("hideIfInputConnected" in o) ? o.hideIfInputConnected : null,
    compact: ("compact" in o) ? !!o.compact : false,
    _view: null,
  };
}
function U16(o)   { return _prop("u16", o); }
function U32(o)   { return _prop("u32", o); }
function Int(o)   { return _prop("i32", o); }
function Float(o) { return _prop("f32", o); }
function Bool(o)  { var p = _prop("bool", o); p._view = function (raw) { return !!raw; }; return p; }
function Str(o)   { var p = _prop("string", o); if (p.default === null) p.default = ""; return p; }
function Scene(o) { o = o || {}; var p = _prop("sceneref", o); if (p.default === null) p.default = 0; if (o.width == null) p.width = 110; return p; }

function Enum(o) {
  var p = _prop("enum", o);
  p.values = o.values;
  p.icons = o.icons || null;
  if (p.default === null) p.default = 0;
  p._display = p.icons ? p.icons.map(icon) : p.values.slice();
  p._view = function (raw) {
    var i = (raw == null) ? (p.default | 0) : (raw | 0);
    if (i < 0) i = 0;
    if (i >= p.values.length) i = p.values.length - 1;
    return { index: i, value: p.values[i], icon: p.icons ? icon(p.icons[i]) : p.values[i] };
  };
  return p;
}

// A numeric literal passes through, anything else becomes a hashed string token.
function numberOrHash(v) {
  var s = "" + v;
  if (/^-?\d+$/.test(s)) return s;
  return '"' + s + '"_hash';
}

// --- registration ------------------------------------------------------------
function node(def) {
  def.inputs = def.inputs || [];
  def.outputs = def.outputs || [];
  def.props = def.props || {};
  _REGISTRY[def.id] = def;
}

// --- editor entry points (called from C++) -----------------------------------
function __reset() { _REGISTRY = {}; }
function __resetTypes() { _TYPES = {}; _CONVERSIONS = {}; }

function __describeTypes() {
  var types = [];
  for (var id in _TYPES) types.push(_TYPES[id]);
  var convs = [];
  for (var k in _CONVERSIONS) {
    var sep = k.indexOf(">");
    convs.push({ from: k.substr(0, sep), to: k.substr(sep + 1), tmpl: _CONVERSIONS[k] });
  }
  return { types: types, conversions: convs };
}

function __describe() {
  var out = [];
  for (var id in _REGISTRY) {
    var d = _REGISTRY[id];
    var props = [];
    for (var key in d.props) {
      var p = d.props[key];
      var pd = { key: key, label: p.label, type: p.type, width: p.width };
      if (p.default != null) pd.default = p.default;
      if (p.hideIfInputConnected != null) pd.hideIfInputConnected = p.hideIfInputConnected;
      if (p.compact) pd.compact = true;
      if (p.type === "enum") pd.enum = p._display;
      props.push(pd);
    }
    out.push({
      id: d.id, name: d.name, color: d.color || [90, 191, 93],
      entry: !!d.entry, hidden: !!d.hidden, category: d.category || "",
      rounding: (d.rounding != null) ? d.rounding : null,
      title: (d.title != null) ? d.title : null,
      inputs: d.inputs, outputs: d.outputs, props: props,
      hasBuild: typeof d.build === "function",
      hasValue: typeof d.value === "function",
    });
  }
  return out;
}

function __makeView(d, props, resVar) {
  var v = { res: function () { return resVar; } };
  for (var key in d.props) {
    var p = d.props[key];
    var raw = (props && (key in props)) ? props[key] : null;
    v[key] = p._view ? p._view(raw) : ((raw != null) ? raw : p.default);
  }
  return v;
}
function __invoke_build(id, props, ctx, resVar) {
  _REGISTRY[id].build(__makeView(_REGISTRY[id], props, resVar), ctx);
}
function __invoke_value(id, props, ctx, resVar) {
  return _REGISTRY[id].value(__makeView(_REGISTRY[id], props, resVar), ctx);
}
