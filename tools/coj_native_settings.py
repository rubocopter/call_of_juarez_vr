"""Hash-pinned CoJ 2006 native Options > Game augmentation (Java 1.4).

Pure byte transformations only; archive identity, staging, preference ownership
and game-thread/lifetime admission belong to the caller. No original game assets
are embedded. This adds no UI registration or native/JNI implementation.

Bridge: Show increments the instance serial and revokes readiness/request. Sync
initializes once per Show; OnOK publishes request=1 only for ready valid preferences.
The bridge consumes 1->2 before any effects; accepted completion requires 2 and
runs the retained original OnOK after clearing it. Back (including Cancel/Escape) revokes it. The caller must also reject stale
serials/modal owners before completion and keep JNI calls on the game thread.
Sync takes (cinematics, turning, holster guides, language), each bounded to 0/1.
An intentional conflict/recovery refresh clears ready before calling Sync again.
"""
from __future__ import annotations

import hashlib
import re

from inspect_java_bytecode import parse_class

MENU_CLASS_NAME = "MenuOptionsGame.class"
MENU_SCRIPT_NAME = "Data/Menu/scr/OptionsGame.scr"
MENU_CLASS_SHA256 = "2fe3aa28003aa2c3f9539071eefa040e4faedbdd6868fbd8c179ab0230ba6dd8"
MENU_SCRIPT_SHA256 = "995388edb69798167e318189598e451319d31c351b230455d737679608772b1d"
OWNER = "MenuOptionsGame"
SELECT = "UIOptionsControlSelect"
OPTION = SELECT + "$OptionInfo"
BRIDGE_FIELDS = (("cojvrSettingsReady", "Z"), ("cojvrSettingsRequest", "I"),
                 ("cojvrSettingsOpenSerial", "I"))
BRIDGE_METHODS = (("cojvrSyncSettings", "(IIII)V"), ("cojvrCinematicsValue", "()I"),
                  ("cojvrTurningValue", "()I"), ("cojvrHolsterGuidesValue", "()I"),
                  ("cojvrCompleteSettings", "(ZLjava/lang/String;)V"))


def _check(data: bytes, expected: str, name: str) -> None:
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError("Unknown or already patched " + name + " SHA-256; no mutation")


def patch_menu_class(data: bytes) -> bytes:
    """Transform only the recognized original MenuOptionsGame.class, once."""
    _check(data, MENU_CLASS_SHA256, MENU_CLASS_NAME)
    # The archive patcher may import this module before defining its helpers.
    # Resolve existing edit utilities only when a transformation is requested.
    from coj_reload_wait_probe import ClassEdit
    from patch_coj_weapon_consumers import Code, u2

    e = ClassEdit(data)
    _, methods = parse_class(data)
    original = {(m.name, m.descriptor): m for m in methods}
    if any(m.name.startswith("cojvr") for m in methods):
        raise ValueError("Native settings already present")
    fields = {}
    for name, desc in BRIDGE_FIELDS:
        fields[name] = e.field(name, desc)
        # ClassEdit.field's probe default is public transient (0x81), not volatile.
        e.fields[-8:-6] = u2(0x0041)
    for name, desc in (("cojvrCinematicsControl", "L" + SELECT + ";"),
                       ("cojvrTurningControl", "L" + SELECT + ";"),
                       ("cojvrHolsterGuidesControl", "L" + SELECT + ";"),
                       ("cojvrStatusControl", "LUIStatic;")):
        fields[name] = e.field(name, desc)
        # Internal cached widgets are private; only bridge state is volatile/public.
        e.fields[-8:-6] = u2(0x0002)

    def method(name, desc="()V"):
        return e.ref(OWNER, name, desc)

    def string(value):
        # ClassEdit.refs uses the same display key for UTF8/String. Keep strings
        # in a separate cache so an existing UTF8 entry cannot become an ldc.
        if value not in strings:
            strings[value] = e.append(b"\x08" + u2(e.utf8(value)))
        return strings[value]

    strings = {}
    get_elem = e.ref("UIWindow", "GetElem", "(Ljava/lang/String;)LUIWindow;")
    selected = e.ref(SELECT, "GetSelected", "()L" + OPTION + ";")
    int_value = e.ref(OPTION, "intValue", "()I")
    set_selection = e.ref(SELECT, "SetSelectionByIntegerData", "(I)V")
    set_text = e.ref("UIStatic", "SetText", "(Ljava/lang/String;)V")
    set_label = e.ref(SELECT, "SetLabel", "(Ljava/lang/String;)V")
    set_tip = e.ref(SELECT, "SetToolTip", "(Ljava/lang/String;)V")
    cycle = e.ref(SELECT, "SetCycleOptions", "(Z)V")
    clear = e.ref(SELECT, "Clear", "()V")
    add = e.ref(SELECT, "AddOption", "(Ljava/lang/String;Ljava/lang/Object;)L" + OPTION + ";")
    integer_init = e.ref("java/lang/Integer", "<init>", "(I)V")

    def helper(name, desc, c, locals_):
        e.helper(name, desc, c.finish(), locals_)
        return method(name, desc)

    def put(c, name, value):
        c.emit(0x2a, value); c.ref(0xb5, fields[name])

    def revoke(c):
        put(c, "cojvrSettingsReady", 0x03)
        put(c, "cojvrSettingsRequest", 0x03)

    # Safe lookup: missing or wrong-typed new resources never reach checkcast.
    c = Code()
    revoke(c)
    for field, id_, type_ in (("cojvrCinematicsControl", "ID_VR_CINEMATICS", SELECT),
                               ("cojvrTurningControl", "ID_VR_TURNING", SELECT),
                               ("cojvrHolsterGuidesControl", "ID_VR_HOLSTER_GUIDES", SELECT),
                               ("cojvrStatusControl", "ID_VR_STATUS", "UIStatic")):
        put(c, field, 0x01)
        c.emit(0x2a); c.ref(0x13, string(id_)); c.ref(0xb6, get_elem); c.emit(0x4c)
        c.emit(0x2b); c.ref(0xc1, e.cls(type_)); c.branch(0x99, field)
        c.emit(0x2a, 0x2b); c.ref(0xc0, e.cls(type_)); c.ref(0xb5, fields[field])
        c.label(field)
    c.emit(0xb1)
    prepare = helper("cojvrPrepareSettings", "()V", c, 2)

    c = Code()
    c.emit(0x2b); c.branch(0xc6, "invalid")
    c.emit(0x2b); c.ref(0xb6, selected); c.emit(0x4d, 0x2c); c.branch(0xc6, "invalid")
    c.emit(0x2c); c.ref(0xb6, int_value); c.emit(0x3e, 0x1d); c.branch(0x9b, "invalid")
    c.emit(0x1d, 0x04); c.branch(0xa3, "invalid")
    c.emit(0x1d, 0xac)
    c.label("invalid"); c.emit(0x02, 0xac)
    value = helper("cojvrSettingsValue", "(L" + SELECT + ";)I", c, 4)
    for name, field in (("cojvrCinematicsValue", "cojvrCinematicsControl"),
                         ("cojvrTurningValue", "cojvrTurningControl"),
                         ("cojvrHolsterGuidesValue", "cojvrHolsterGuidesControl")):
        c = Code(); c.emit(0x2a, 0x2a); c.ref(0xb4, fields[field]); c.ref(0xb6, value); c.emit(0xac)
        helper(name, "()I", c, 1)

    c = Code()
    c.emit(0x2a); c.ref(0xb4, fields["cojvrStatusControl"]); c.branch(0xc6, "done")
    c.emit(0x2b); c.branch(0xc7, "text")
    c.ref(0x13, string("")); c.emit(0x4c)
    c.label("text")
    c.emit(0x2a); c.ref(0xb4, fields["cojvrStatusControl"]); c.emit(0x2b); c.ref(0xb6, set_text)
    c.label("done"); c.emit(0xb1)
    status = helper("cojvrSettingsStatus", "(Ljava/lang/String;)V", c, 2)

    # Parameters: control, label, option0, option1, tooltip, selected integer.
    row_desc = "(L" + SELECT + ";" + "Ljava/lang/String;" * 4 + "I)V"
    c = Code()
    c.emit(0x2b); c.ref(0xb6, clear)
    c.emit(0x2b, 0x2c); c.ref(0xb6, set_label)
    c.emit(0x2b, 0x19, 5); c.ref(0xb6, set_tip)
    c.emit(0x2b, 0x04); c.ref(0xb6, cycle)
    for local, constant in ((3, 0x03), (4, 0x04)):
        c.emit(0x2b, 0x19, local); c.ref(0xbb, e.cls("java/lang/Integer")); c.emit(0x59, constant)
        c.ref(0xb7, integer_init); c.ref(0xb6, add); c.emit(0x57)
    c.emit(0x2b, 0x15, 6); c.ref(0xb6, set_selection); c.emit(0xb1)
    row = helper("cojvrSettingsRow", row_desc, c, 7)

    c = Code()
    c.emit(0x2a); c.ref(0xb4, fields["cojvrSettingsReady"]); c.branch(0x9a, "done")
    put(c, "cojvrSettingsRequest", 0x03)
    c.emit(0x2a); c.ref(0x13, string("")); c.ref(0xb6, status)
    for load in ((0x1b,), (0x1c,), (0x1d,), (0x15, 4)):
        # cinema, turning, guides, locale are independently bounded integers.
        c.emit(*load); c.branch(0x9b, "done")
        c.emit(*load, 0x04); c.branch(0xa3, "done")
    for field in ("cojvrCinematicsControl", "cojvrTurningControl", "cojvrHolsterGuidesControl"):
        c.emit(0x2a); c.ref(0xb4, fields[field]); c.branch(0xc6, "done")
    for tag, field, selected_load, english, spanish in (
        ("cinema", "cojvrCinematicsControl", 0x1b,
         ("VR cinematics", "Theater", "Immersive", "Presentation of in-game cinematics in VR."),
         ("Cinemáticas RV", "Teatro", "Inmersivo", "Presentación de las cinemáticas del juego en RV.")),
        ("turning", "cojvrTurningControl", 0x1c,
         ("VR turning", "Smooth", "Snap", "Turn continuously or in fixed steps."),
         ("Giro RV", "Suave", "Por pasos", "Gira de forma continua o en pasos fijos.")),
        ("guides", "cojvrHolsterGuidesControl", 0x1d,
         ("VR Comfort: holster guides", "Off", "On", "Show colored hip holster areas."),
         ("Comodidad RV: guías de fundas", "No", "Sí", "Muestra zonas de color para las fundas de la cadera."))):
        c.emit(0x2a, 0x2a); c.ref(0xb4, fields[field])
        c.emit(0x15, 4); c.branch(0x9a, tag + "_es")
        for text in english: c.ref(0x13, string(text))
        c.branch(0xa7, tag + "_row")
        c.label(tag + "_es")
        for text in spanish: c.ref(0x13, string(text))
        c.label(tag + "_row"); c.emit(selected_load); c.ref(0xb6, row)
    # Publish readiness last, after all configured values pass the same getters
    # the bridge/OnOK use. A repeated sync cannot overwrite local edits.
    for getter in ("cojvrCinematicsValue", "cojvrTurningValue", "cojvrHolsterGuidesValue"):
        c.emit(0x2a); c.ref(0xb6, method(getter, "()I")); c.branch(0x9b, "done")
    put(c, "cojvrSettingsReady", 0x04)
    c.label("done"); c.emit(0xb1)
    helper("cojvrSyncSettings", "(IIII)V", c, 5)

    # Keep byte-identical original code in named helpers. Changes affect only
    # enumerated entry bodies; ClassEdit copies every unrelated member/attribute.
    def wrap(name, prefix=None, suffix=None, body=None):
        m = original[name, "()V"]
        if m.code is None or m.max_stack > 8:
            raise ValueError("Unexpected original menu method shape: " + name)
        original_name = "cojvrOriginal" + name
        e.helper(original_name, "()V", m.code, m.max_locals)
        c = Code()
        if body is not None:
            body(c, method(original_name))
        else:
            if prefix: prefix(c)
            c.emit(0x2a); c.ref(0xb6, method(original_name))
            if suffix: suffix(c)
            c.emit(0xb1)
        replacement = c.finish()
        e.changes[name, "()V"] = lambda old, replacement=replacement: replacement

    wrap("OnInit", suffix=lambda c: (c.emit(0x2a), c.ref(0xb6, prepare)))

    def show(c):
        revoke(c)
        c.emit(0x2a, 0x2a); c.ref(0xb4, fields["cojvrSettingsOpenSerial"])
        c.emit(0x04, 0x60); c.ref(0xb5, fields["cojvrSettingsOpenSerial"])
    wrap("Show", prefix=show)
    wrap("Back", prefix=revoke)

    def defaults(c):
        put(c, "cojvrSettingsRequest", 0x03)
        c.emit(0x2a); c.ref(0x13, string("")); c.ref(0xb6, status)
        for field in ("cojvrCinematicsControl", "cojvrTurningControl", "cojvrHolsterGuidesControl"):
            c.emit(0x2a); c.ref(0xb4, fields[field]); c.branch(0xc6, field)
            c.emit(0x2a); c.ref(0xb4, fields[field]); c.emit(0x03); c.ref(0xb6, set_selection)
            c.label(field)
    wrap("OnDefault", suffix=defaults)

    def ok(c, native):
        c.emit(0x2a); c.ref(0xb4, fields["cojvrSettingsReady"]); c.branch(0x99, "native")
        for getter in ("cojvrCinematicsValue", "cojvrTurningValue", "cojvrHolsterGuidesValue"):
            c.emit(0x2a); c.ref(0xb6, method(getter, "()I")); c.branch(0x9b, "native")
        # Preserve a queued/consumed token across duplicate native accept events.
        c.emit(0x2a); c.ref(0xb4, fields["cojvrSettingsRequest"]); c.branch(0x9a, "pending")
        put(c, "cojvrSettingsRequest", 0x04)
        c.label("pending"); c.emit(0xb1)
        c.label("native"); c.emit(0x2a); c.ref(0xb6, native); c.emit(0xb1)
    wrap("OnOK", body=ok)

    c = Code()
    c.emit(0x1b); c.branch(0x99, "rejected")
    # Only the bridge may consume 1->2 before preference/save/native effects.
    # Unconsumed/duplicate/cancelled completion is a no-op. Serial ownership for
    # close+reopen with another consumed request remains the bridge's contract.
    c.emit(0x2a); c.ref(0xb4, fields["cojvrSettingsRequest"]); c.emit(0x05); c.branch(0xa0, "done")
    c.emit(0x2a); c.ref(0xb4, fields["cojvrSettingsReady"]); c.branch(0x99, "done")
    put(c, "cojvrSettingsRequest", 0x03)
    c.emit(0x2a); c.ref(0xb6, method("cojvrOriginalOnOK")); c.branch(0xa7, "done")
    c.label("rejected"); put(c, "cojvrSettingsRequest", 0x03)
    c.emit(0x2a, 0x2c); c.ref(0xb6, status)
    c.label("done"); c.emit(0xb1)
    helper("cojvrCompleteSettings", "(ZLjava/lang/String;)V", c, 3)
    return e.finish()


def _block(data: bytes, kind: bytes, id_: bytes) -> tuple[int, int]:
    """Find one measured leaf block, including its matching native close comment."""
    pattern = (rb"(?m)^ +" + kind + rb'\([^\n]*"' + id_ +
               rb'"[^\n]*\)[\r\n ]+\{[^{}]*\}// \[[^\]]+\] ' + id_ + rb"(?=[\r\n])")
    matches = list(re.finditer(pattern, data))
    if len(matches) != 1:
        raise ValueError("Unexpected native script block: " + id_.decode("ascii"))
    return matches[0].span()


def patch_menu_script(data: bytes) -> bytes:
    """Extend the measured native layout, keeping original controls/styles intact."""
    _check(data, MENU_SCRIPT_SHA256, MENU_SCRIPT_NAME)
    start, end = _block(data, b"MenuButton", b"ID_SUBTITLES")
    template = data[start:end]
    # Clone at transformation time: no original resource text is redistributed.
    rows = []
    for id_, y in ((b"ID_VR_CINEMATICS", b"0.30681818"),
                   (b"ID_VR_TURNING", b"0.37954545"),
                   (b"ID_VR_HOLSTER_GUIDES", b"0.45227273")):
        row = template.replace(b"ID_SUBTITLES", id_)
        if row.count(b"LogicPos(0.04411765,0.23409091)") != 1:
            raise ValueError("Unexpected native row geometry")
        rows.append(row.replace(b"0.23409091", y))
    # Existing rows use a .053333 screen-space pitch: positions reference the
    # .73333335-high ID_FRAME while sizes are screen-relative. Status follows
    # the seventh row. Grow only the decorative list frame; existing tooltip,
    # buttons and main frame keep their original placement.
    newline = b"\r\n"  # generated additions; original bytes/newlines are retained
    status_lines = (
        '        Static("UIStatic","ID_VR_STATUS","Specific")',
        '        {',
        '            Text("")',
        '            TextAlpha(1.0)',
        '            TextColor(1.0,1.0,1.0)',
        '            Font("MenuSmall")',
        '            FontScale(0.5)',
        '            TextPosFlags(273)',
        '            CanFocus(0)',
        '            Mouse(0)',
        '            Keyboard(0)',
        '            Joy(0)',
        '            Visible(1)',
        '            ParentPositioningX(0)',
        '            ParentPositioningY(0)',
        '            PreviousElemPositioningX(0)',
        '            PreviousElemPositioningY(0)',
        '            PreviousElemSizingX(0)',
        '            PreviousElemSizingY(0)',
        '            OtherElemAsParentPositioningX("ID_FRAME")',
        '            OtherElemAsParentPositioningY("ID_FRAME")',
        '            OtherElemAsParentSizingX("ID_FRAME")',
        '            OtherElemAsParentSizingY("ID_FRAME")',
        '            ParentAlignX(4)',
        '            ParentAlignY(-1)',
        '            ParentSizingX(0)',
        '            ParentSizingY(0)',
        '            TextureAlpha(0.0)',
        '            LogicSize(0.465,0.06)',
        '            LogicPos(0.04411765,0.525)',
        '        }// [UIStatic] ID_VR_STATUS',
    )
    additions = newline.join(rows + [newline.join(s.encode("ascii") for s in status_lines)])
    list_start, list_end = _block(data, b"Window", b"ID_LIST_FRAME")
    frame = data[list_start:list_end]
    if frame.count(b"LogicSize(0.5125,0.29166666)") != 1:
        raise ValueError("Unexpected native list frame geometry")
    frame = frame.replace(b"LogicSize(0.5125,0.29166666)", b"LogicSize(0.5125,0.49833333)")
    return data[:list_start] + additions + newline + frame + data[list_end:]
