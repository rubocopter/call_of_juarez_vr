"""Exact CoJ native tutorial *presentation*, pure byte transformations only.

The native GetText, GetTipText, release conditions and input handlers are left
intact. Only MenuHint.Show's two presentation calls opt into new HintBase methods.
Known EN resources use a semantic action label, never an install-time controller
guess. Unknown hints/resources/localizations execute original GetText unchanged.

Parent integration: patch_tutorial_classes(original_base, original_menu,
original_english_saloon_resource, original_english_tip_resource) -> (base, menu).
The caller owns archive hash
admission, transactional installation/restoration and opt-in. This module does
no filesystem I/O and introduces no bridge or native action delivery.
"""
from __future__ import annotations
import hashlib
import re

CODE_SHA256 = 'f9db47c166e03f23e37cbcdfd5344e4ad4c5c9134f35e8f6dcdf66db7e71ce12'
BASE_SHA256 = 'ec715bca0d4e87972b4b39cd6f881bb8f9e83464f2def228b26993012b632ec8'
MENU_SHA256 = '77b871680d7dab82d6b56b67fed71171678efbe961c8e568c508234be3cf7106'
RESOURCE_NAME = 'Data/SaloonTexts.scr'
RESOURCE_SHA256 = 'c44628375bd214f79af7dab7f6998a0dbf63ebb71602ccd5d7061a8fc72c8d65'
PRESENTATION_METHOD = 'cojvrTutorialPresentationText'
TIP_PRESENTATION_METHOD = 'cojvrTutorialPresentationTip'
TIP_RESOURCE_NAME = 'Data/TextsGenerated.scr'
TIP_RESOURCE_SHA256 = '4f435070dda3a111b1606b0c66aed4da6906acfdbfae55d3431c9ccb531e6d33'
TOKEN = '%KEY(_ACTION_TOGGLE_ALTERNATE)'
HINT_IDS = ('E03_H05_FastShootingA', 'E03_H05_FastShootingB')
NATIVE_ACTION = 20  # _ACTION_TOGGLE_ALTERNATE; this module never synthesizes it.


def map_template(hint_id: str, resource_key: str, locale: str,
                 template: str | None) -> str | None:
    """Bounded mapping API before native %KEY expansion; unknowns pass through.

    Only En is asset-qualified. Es (and X360/AX/BX resources) deliberately retain
    their native text until their exact original resource has been inspected.
    Dynamic binding names cannot be baked into this static transformer honestly.
    """
    if (hint_id not in HINT_IDS or resource_key != '&'+hint_id+'&' or locale != 'En'
            or not isinstance(template, str) or len(template) > 1024
            or template.count(TOKEN) != 1):
        return template
    return template.replace(TOKEN, 'Alternate fire')


def _check(data: bytes, expected: str, name: str) -> None:
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError('Unknown or already patched '+name+' SHA-256; no mutation')


def read_templates(resource: bytes) -> dict[str, str]:
    _check(resource, RESOURCE_SHA256, RESOURCE_NAME)
    result = {}
    for hint_id in HINT_IDS:
        pattern = rb'String\(\s*"'+hint_id.encode('ascii')+rb'",\s*"([^"\r\n]*)"\)'
        matches = re.findall(pattern, resource)
        if len(matches) != 1:
            raise ValueError('Missing or ambiguous tutorial resource '+hint_id)
        template = matches[0].decode('ascii')
        if map_template(hint_id, '&'+hint_id+'&', 'En', template) == template:
            raise ValueError('Missing exact tutorial action token '+hint_id)
        result[hint_id] = template
    return result


def patch_tutorial_classes(base: bytes, menu: bytes, resource: bytes,
                          tip_resource: bytes) -> tuple[bytes, bytes]:
    """Hash-check ALL inputs before returning any transformed output."""
    _check(base, BASE_SHA256, 'HintBase.class')
    _check(menu, MENU_SHA256, 'MenuHint.class')
    templates = read_templates(resource)
    _check(tip_resource, TIP_RESOURCE_SHA256, TIP_RESOURCE_NAME)
    tips = re.findall(rb'String\(\s*"MenuHint_PressKey",\s*"([^"\r\n]*)"\)', tip_resource)
    if tips != [b'Press %s to continue']:
        raise ValueError('Unexpected native continuation template')
    from coj_reload_wait_probe import ClassEdit
    from patch_coj_weapon_consumers import Code, u2

    e = ClassEdit(base)
    strings = {}
    def string(value):
        if value not in strings:
            strings[value] = e.append(b'\x08'+u2(e.utf8(value)))
        return strings[value]
    equals = e.ref('java/lang/String', 'equals', '(Ljava/lang/Object;)Z')
    get_text = e.ref('HintBase', 'GetText', '()Ljava/lang/String;')
    get_localized = e.ref('Text', 'Get', '(Ljava/lang/String;)Ljava/lang/String;')
    hint_id_field = e.ref('HintBase', 'm_sId', 'Ljava/lang/String;', 9)
    resource_field = e.ref('HintBase', 'm_sText', 'Ljava/lang/String;', 9)
    c = Code()
    for index, (hint_id, template) in enumerate(templates.items()):
        # Constant.equals(nullable value), never nullableValue.equals(...).
        c.ref(0x13, string(hint_id)); c.emit(0x2a); c.ref(0xb4, hint_id_field)
        c.ref(0xb6, equals); c.branch(0x99, 'next'+str(index))
        c.ref(0x13, string('&'+hint_id+'&')); c.emit(0x2a); c.ref(0xb4, resource_field)
        c.ref(0xb6, equals); c.branch(0x99, 'fallback')
        c.emit(0x2a); c.ref(0xb4, resource_field); c.ref(0xb8, get_localized); c.emit(0x4c)
        c.ref(0x13, string(template)); c.emit(0x2b); c.ref(0xb6, equals)
        c.branch(0x99, 'fallback')
        c.ref(0x13, string(map_template(hint_id, '&'+hint_id+'&', 'En', template))); c.emit(0xb0)
        c.label('next'+str(index))
    c.label('fallback'); c.emit(0x2a); c.ref(0xb6, get_text); c.emit(0xb0)
    e.helper(PRESENTATION_METHOD, '()Ljava/lang/String;', c.finish(), 2)

    # Retain the native tip read/check first, including CheckAssignedKeys. Only
    # its displayed string is replaced; the release condition still owns input.
    # Savegame or script replacement can change conditions: inspect the current
    # object graph afresh, not merely the originally inspected Hints.scr.
    c = Code()
    c.emit(0x2a); c.ref(0xb6, e.ref('HintBase', 'GetTipText', '()Ljava/lang/String;')); c.emit(0x4c)
    hint_id = HINT_IDS[0]
    for literal, field in ((hint_id, hint_id_field), ('&'+hint_id+'&', resource_field)):
        c.ref(0x13, string(literal)); c.emit(0x2a); c.ref(0xb4, field)
        c.ref(0xb6, equals); c.branch(0x99, 'tip_fallback')
    c.ref(0x13, string(templates[hint_id])); c.emit(0x2a); c.ref(0xb4, resource_field)
    c.ref(0xb8, get_localized); c.ref(0xb6, equals); c.branch(0x99, 'tip_fallback')
    c.emit(0x2a); c.ref(0xb4, e.ref('HintBase', 'm_cReleaseConditions', 'LHintReleaseConditions;', 9))
    c.emit(0x4d, 0x2c); c.branch(0xc6, 'tip_fallback')
    c.emit(0x2c); c.ref(0xb6, e.ref('HintReleaseConditions', 'GetSize', '()I'))
    c.emit(0x04); c.branch(0xa0, 'tip_fallback')
    c.emit(0x2c, 0x03); c.ref(0xb6, e.ref('HintReleaseConditions', 'GetConditionAt', '(I)LHintReleaseCondition;'))
    c.emit(0x4e, 0x2d); c.branch(0xc6, 'tip_fallback')
    c.emit(0x2d); c.ref(0xb6, e.ref('HintReleaseCondition', 'GetSize', '()I'))
    c.emit(0x04); c.branch(0xa0, 'tip_fallback')
    c.emit(0x2d, 0x03); c.ref(0xb6, e.ref('HintReleaseCondition', 'GetActionAt', '(I)LHintReleaseAction;'))
    c.emit(0x4d, 0x2c); c.branch(0xc6, 'tip_fallback')
    c.emit(0x2c); c.ref(0xb6, e.ref('HintReleaseAction', 'GetActionId', '()I'))
    c.emit(0x10, NATIVE_ACTION); c.branch(0xa0, 'tip_fallback')
    c.ref(0x13, string(tips[0].decode('ascii'))); c.ref(0x13, string('&MenuHint_PressKey&'))
    c.ref(0xb8, get_localized); c.ref(0xb6, equals); c.branch(0x99, 'tip_fallback')
    c.ref(0x13, string("Press 'Alternate fire' to continue")); c.emit(0xb0)
    c.label('tip_fallback'); c.emit(0x2b, 0xb0)
    e.helper(TIP_PRESENTATION_METHOD, '()Ljava/lang/String;', c.finish(), 4)
    patched_base = e.finish()

    e = ClassEdit(menu)
    original_ref = e.ref('Hint', 'GetText', '()Ljava/lang/String;')
    presentation_ref = e.ref('HintBase', PRESENTATION_METHOD, '()Ljava/lang/String;')
    original_tip_ref = e.ref('Hint', 'GetTipText', '()Ljava/lang/String;')
    presentation_tip_ref = e.ref('HintBase', TIP_PRESENTATION_METHOD, '()Ljava/lang/String;')
    def replace_call(code):
        expected = b'\xb6'+u2(original_ref)
        # Exact original Show call offset. Same length: no relocation, branch,
        # exception, event, delay or input/permission rewrite.
        if code[83:86] != expected or code.count(expected) != 1:
            raise ValueError('Unexpected MenuHint.Show presentation call')
        expected_tip = b'\xb6'+u2(original_tip_ref)
        if code[94:97] != expected_tip or code.count(expected_tip) != 1:
            raise ValueError('Unexpected MenuHint.Show continuation call')
        return (code[:83]+b'\xb6'+u2(presentation_ref)+code[86:94]+
                b'\xb6'+u2(presentation_tip_ref)+code[97:])
    e.changes[('Show', '(Ljava/lang/Object;)V')] = replace_call
    return patched_base, e.finish()
