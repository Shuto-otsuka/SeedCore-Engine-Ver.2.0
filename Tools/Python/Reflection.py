import re
import os
import hashlib
import xml.etree.ElementTree as ET

STRUCT_START = re.compile(r'(struct|class)\s+(?:SEEDCORE_API\s+)?(\w+)')

CONDITION_PATTERN = re.compile(
    r'SC_REFLECTION_FIELD_CONDITION\((.+?)\)\s*'
)

FIELD_PATTERN = re.compile(
    r'SC_REFLECTION_FIELD\(\)\s*([\w:<>,\s]+)\s+(\w+)\s*(\[\d+\])?\s*(?:=[^;]*)?\s*;'
)

FIELD_EX_PATTERN = re.compile(
    r'SC_REFLECTION_FIELD_EX\(\s*"([^"]+)"\s*\)\s*([\w:<>,\s]+)\s+(\w+)\s*(\[\d+\])?\s*(?:=[^;]*)?\s*;'
)

CLAMPED_PATTERN = re.compile(
    r'SC_REFLECTION_CLAMPED\(\s*([^,]+)\s*,\s*([^)]+)\s*\)\s*([\w:<>,\s]+)\s+(\w+)\s*(\[\d+\])?\s*(?:=[^;]*)?\s*;'
)

CLAMPED_EX_PATTERN = re.compile(
    r'SC_REFLECTION_CLAMPED_EX\(\s*"([^"]+)"\s*,\s*([^,]+)\s*,\s*([^)]+)\s*\)\s*([\w:<>,\s]+)\s+(\w+)\s*(\[\d+\])?\s*(?:=[^;]*)?\s*;'
)

SERIALIZE_FIELD_PATTERN = re.compile(
    r'SC_SERIALIZE_FIELD\(\)\s*([\w:<>,\s]+)\s+(\w+)\s*(\[\d+\])?\s*(?:=[^;]*)?\s*;'
)

FIELD_MARKER_PATTERN = re.compile(
    r'SC_REFLECTION_FIELD\(\)|SC_REFLECTION_FIELD_EX\(|SC_REFLECTION_CLAMPED\(|SC_REFLECTION_CLAMPED_EX\(|SC_SERIALIZE_FIELD\b'
)

TYPE_MAP = {
    "Int": "AttributeType::Int",
    "int": "AttributeType::Int",
    "Uint": "AttributeType::Int",
    "unsigned int": "AttributeType::Int",

    "Int8": "AttributeType::Int",
    "int8_t": "AttributeType::Int",
    "Uint8": "AttributeType::Int", 
    "uint8_t": "AttributeType::Int",

    "Int16": "AttributeType::Int",
    "int16_t": "AttributeType::Int",
    "Uint16": "AttributeType::Int", 
    "uint16_t": "AttributeType::Int",

    "Int32": "AttributeType::Int",
    "int32_t": "AttributeType::Int",
    "Uint32": "AttributeType::Int", 
    "uint32_t": "AttributeType::Int",

    "Int64": "AttributeType::Int",
    "int64_t": "AttributeType::Int",
    "Uint64": "AttributeType::Int", 
    "uint64_t": "AttributeType::Int",

    "Short": "AttributeType::Int",
    "short": "AttributeType::Int",
    "Long": "AttributeType::Int",
    "long": "AttributeType::Int",

    "Float": "AttributeType::Float",
    "float": "AttributeType::Float",
    "Double": "AttributeType::Float",
    "double": "AttributeType::Float",

    "Bool": "AttributeType::Bool",
    "bool": "AttributeType::Bool",

    "Vector2": "AttributeType::Vector2",

    "Vector3": "AttributeType::Vector3",
    "Vector4": "AttributeType::Vector3",

    "String": "AttributeType::String",

    "Color": "AttributeType::Color",
}

def qualify_condition(condition, struct_name, enum_defs, enum_owners, own_enum_names=None):
    own_enum_names = own_enum_names or set()
    result = re.sub(r'\b(\w+_)\b', r'o.\1', condition)
    for enum_name in enum_defs:
        if enum_name in own_enum_names:
            owner = struct_name
        else:
            owner = enum_owners.get(enum_name)
        if owner:
            result = re.sub(r'\b' + enum_name + r'::', f'{owner}::{enum_name}::', result)
    return result

DYNAMIC_ARRAY_PREFIXES = ['std::vector', 'DynamicArray']
STATIC_ARRAY_PREFIXES = ['std::array', 'StaticArray']

ENUM_CLASS_PATTERN = re.compile(
    r'enum\s+class\s+(\w+)\s*\{([^}]*)\}'
)

def parse_enum_classes(content):
    enums = {}
    for match in ENUM_CLASS_PATTERN.finditer(content):
        enum_name = match.group(1)
        body = match.group(2)
        entries = []
        for entry in body.split(','):
            entry = entry.strip()
            if not entry:
                continue
            name = entry.split('=')[0].strip()
            if name:
                entries.append(name)
        if entries:
            enums[enum_name] = entries
    return enums

def parse_array_info(type_str, bracket_str=None):
    """
    Returns (element_type, count_or_none, is_dynamic)
    count_or_none: int for static, None for dynamic
    Returns None if not an array type
    """
    type_str = type_str.strip()

    if bracket_str:
        count = int(bracket_str.strip('[]'))
        return (type_str, count, False)

    for prefix in DYNAMIC_ARRAY_PREFIXES:
        if type_str.startswith(prefix + '<'):
            depth = 0
            start = type_str.index('<')
            for i in range(start, len(type_str)):
                if type_str[i] == '<': depth += 1
                elif type_str[i] == '>': depth -= 1
                if depth == 0:
                    inner = type_str[start+1:i].strip()
                    return (inner, None, True)

    for prefix in STATIC_ARRAY_PREFIXES:
        if type_str.startswith(prefix + '<'):
            depth = 0
            start = type_str.index('<')
            for i in range(start, len(type_str)):
                if type_str[i] == '<': depth += 1
                elif type_str[i] == '>': depth -= 1
                if depth == 0:
                    inner = type_str[start+1:i].strip()
                    parts = inner.rsplit(',', 1)
                    elem_type = parts[0].strip()
                    count = int(parts[1].strip())
                    return (elem_type, count, False)

    return None


# ---------------------------
# コメント・文字列除去
# ---------------------------
def remove_comments_and_strings(code):
    result = []
    i = 0
    n = len(code)

    in_string = False
    in_char = False
    in_line_comment = False
    in_block_comment = False

    while i < n:
        c = code[i]
        next_c = code[i + 1] if i + 1 < n else ''

        if not in_string and not in_char and not in_block_comment:
            if c == '/' and next_c == '/':
                in_line_comment = True
                i += 2
                continue

        if not in_string and not in_char and not in_line_comment:
            if c == '/' and next_c == '*':
                in_block_comment = True
                i += 2
                continue

        if in_block_comment:
            if c == '*' and next_c == '/':
                in_block_comment = False
                i += 2
                continue
            i += 1
            continue

        if in_line_comment:
            if c == '\n':
                in_line_comment = False
                result.append(c)
            i += 1
            continue

        if not in_char and c == '"':
            if not in_string:
                prefix = ''.join(result)
                if prefix.rstrip().endswith('SC_REFLECTION_FIELD_EX(') or prefix.rstrip().endswith('SC_REFLECTION_CLAMPED_EX('):
                    end_quote = code.find('"', i + 1)
                    if end_quote != -1:
                        result.append(code[i:end_quote + 1])
                        i = end_quote + 1
                        continue
            in_string = not in_string
            result.append(' ')
            i += 1
            continue

        if not in_string and c == '\'':
            in_char = not in_char
            result.append(' ')
            i += 1
            continue

        result.append(c)
        i += 1

    return ''.join(result)


# ---------------------------
# {} ブロック抽出
# ---------------------------
def extract_block(content, start):
    depth = 0

    for i in range(start, len(content)):
        c = content[i]

        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return content[start:i]

    return None


# ---------------------------
# メイン処理
# ---------------------------
def process_file(file_path, project_root, global_enums, enum_headers, enum_owners=None, known_structs=None):
    known_structs = known_structs or {}
    if "reflection" in file_path:
        return None

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except Exception:
        return None

    enum_defs = global_enums

    content = remove_comments_and_strings(content)

    # -----------------------
    # キャッシュ
    # -----------------------
    enum_hash = hashlib.md5(str(sorted(enum_defs.items())).encode()).hexdigest()
    struct_hash = hashlib.md5(str(sorted(known_structs.items())).encode()).hexdigest()
    file_hash = hashlib.md5(
        (content + enum_hash + struct_hash).encode('utf-8', errors='ignore')
    ).hexdigest()

    cache = getattr(process_file, "cache", {})

    if file_path in cache and cache[file_path] == file_hash:
        return None

    cache[file_path] = file_hash
    process_file.cache = cache

    # -----------------------
    # 解析
    # -----------------------
    results = []
    struct_own_enums = {}

    for match in STRUCT_START.finditer(content):
        struct_name = match.group(2)

        brace_pos = content.find('{', match.end())
        if brace_pos == -1:
            continue

        body = extract_block(content, brace_pos)
        if not body:
            continue

        struct_own_enums[struct_name] = set(parse_enum_classes(body).keys())

        conditions = {}
        for cond_match in CONDITION_PATTERN.finditer(body):
            conditions[cond_match.end()] = cond_match.group(1).strip()

        fields = []
        for match in FIELD_PATTERN.finditer(body):
            fields.append((match.start(), match.group(1), match.group(2), match.group(2), match.group(3), None, None, False))
        for match in FIELD_EX_PATTERN.finditer(body):
            fields.append((match.start(), match.group(2), match.group(3), match.group(1), match.group(4), None, None, False))
        for match in CLAMPED_PATTERN.finditer(body):
            fields.append((match.start(), match.group(3), match.group(4), match.group(4), match.group(5), match.group(1).strip(), match.group(2).strip(), False))
        for match in CLAMPED_EX_PATTERN.finditer(body):
            fields.append((match.start(), match.group(4), match.group(5), match.group(1), match.group(6), match.group(2).strip(), match.group(3).strip(), False))
        for match in SERIALIZE_FIELD_PATTERN.finditer(body):
            fields.append((match.start(), match.group(1), match.group(2), match.group(2), match.group(3), None, None, True))
        fields.sort(key=lambda entry: entry[0])

        resolved_fields = []
        used_conditions = set()
        for pos, f_type, f_name, display_name, bracket, clamp_min, clamp_max, is_serialize_only in fields:
            condition = None
            for cond_end, cond_expr in conditions.items():
                if cond_end in used_conditions:
                    continue
                if cond_end <= pos and pos - cond_end < 200:
                    between = body[cond_end:pos].strip()
                    if between == '' or between.startswith('SC_REFLECTION'):
                        condition = cond_expr
                        used_conditions.add(cond_end)
                        break
            resolved_fields.append((f_type, f_name, display_name, bracket, condition, clamp_min, clamp_max, is_serialize_only))
        fields = resolved_fields
        if fields:
            results.append((struct_name, fields))

    if not results:
        return None

    # -----------------------
    # 出力生成
    # -----------------------
    header_name = os.path.basename(file_path)

    used_enum_headers = set()
    for struct_name, fields in results:
        for f_type, f_name, display_name, bracket, condition, clamp_min, clamp_max, is_serialize_only in fields:
            stripped = f_type.strip()
            if stripped in enum_defs and stripped in enum_headers:
                used_enum_headers.add(enum_headers[stripped])

    rel_path = os.path.relpath(file_path, start=project_root)
    normalized_path = rel_path.replace('\\', '/')

    includes = [normalized_path]
    for eh in sorted(used_enum_headers):
        if eh != normalized_path:
            includes.append(eh)

    force_lines = []
    for struct_name, fields in results:
        force_lines.append(f'extern "C" int _force_reflection_{struct_name} = 0;')

    lines = []
    lines.append(f'\t\t// ---- {normalized_path} ----')

    generated_enums = set()

    for struct_name, fields in results:
        lines.append(f'\t\tstruct Register_{struct_name}')
        lines.append('\t\t{')
        lines.append(f'\t\t\tRegister_{struct_name}()')
        lines.append('\t\t\t{')
        lines.append(
            f'\t\t\t\tReflectionRegistry::Register('
            f'String("{struct_name}"), '
            '[](void* ptr, DynamicArray<FieldInfo>& outInfo) {'
        )

        lines.append(f'\t\t\t\t\t{struct_name}& obj = *static_cast<{struct_name}*>(ptr);')

        for f_type, f_name, display_name, bracket, condition, clamp_min, clamp_max, is_serialize_only in fields:
            array_info = parse_array_info(f_type.strip(), bracket)
            stripped_type = f_type.strip()
            has_extras = condition or clamp_min is not None or is_serialize_only

            if array_info is None and stripped_type in enum_defs:
                lines.append(f'\t\t\t\t\t{{')
                lines.append(f'\t\t\t\t\t\tFieldInfo fi;')
                lines.append(f'\t\t\t\t\t\tfi.name_ = String("{display_name}");')
                lines.append(f'\t\t\t\t\t\tfi.offset_ = offsetof({struct_name}, {f_name});')
                lines.append(f'\t\t\t\t\t\tfi.type_ = AttributeType::Enum;')
                lines.append(f'\t\t\t\t\t\tfi.enum_.typeName_ = String("{stripped_type}");')
                if is_serialize_only:
                    lines.append(f'\t\t\t\t\t\tfi.editorVisible_ = false;')
                if condition:
                    lines.append(f'\t\t\t\t\t\tfi.enableIf_ = [](void* p) -> Bool {{ auto& o = *static_cast<{struct_name}*>(p); return {qualify_condition(condition, struct_name, enum_defs, enum_owners, struct_own_enums.get(struct_name))}; }};')
                if clamp_min is not None:
                    lines.append(f'\t\t\t\t\t\tfi.clampMin_ = {clamp_min};')
                    lines.append(f'\t\t\t\t\t\tfi.clampMax_ = {clamp_max};')
                lines.append(f'\t\t\t\t\t\toutInfo.push_back(std::move(fi));')
                lines.append(f'\t\t\t\t\t}}')

                if stripped_type not in generated_enums:
                    generated_enums.add(stripped_type)

            elif array_info is None and stripped_type in known_structs:
                lines.append(f'\t\t\t\t\t{{')
                lines.append(f'\t\t\t\t\t\tFieldInfo fi;')
                lines.append(f'\t\t\t\t\t\tfi.name_ = String("{display_name}");')
                lines.append(f'\t\t\t\t\t\tfi.offset_ = offsetof({struct_name}, {f_name});')
                lines.append(f'\t\t\t\t\t\tfi.type_ = AttributeType::Struct;')
                lines.append(f'\t\t\t\t\t\tfi.nestedTypeName_ = String("{stripped_type}");')
                if is_serialize_only:
                    lines.append(f'\t\t\t\t\t\tfi.editorVisible_ = false;')
                lines.append(f'\t\t\t\t\t\toutInfo.push_back(std::move(fi));')
                lines.append(f'\t\t\t\t\t}}')

            elif array_info is None:
                attr_type = TYPE_MAP.get(stripped_type, "AttributeType::Unknown")
                if has_extras:
                    lines.append(f'\t\t\t\t\t{{')
                    lines.append(f'\t\t\t\t\t\tFieldInfo fi;')
                    lines.append(f'\t\t\t\t\t\tfi.name_ = String("{display_name}");')
                    lines.append(f'\t\t\t\t\t\tfi.offset_ = offsetof({struct_name}, {f_name});')
                    lines.append(f'\t\t\t\t\t\tfi.type_ = {attr_type};')
                    if condition:
                        lines.append(f'\t\t\t\t\t\tfi.enableIf_ = [](void* p) -> Bool {{ auto& o = *static_cast<{struct_name}*>(p); return {qualify_condition(condition, struct_name, enum_defs, enum_owners, struct_own_enums.get(struct_name))}; }};')
                    if clamp_min is not None:
                        lines.append(f'\t\t\t\t\t\tfi.clampMin_ = {clamp_min};')
                        lines.append(f'\t\t\t\t\t\tfi.clampMax_ = {clamp_max};')
                    if is_serialize_only:
                        lines.append(f'\t\t\t\t\t\tfi.editorVisible_ = false;')
                    lines.append(f'\t\t\t\t\t\toutInfo.push_back(std::move(fi));')
                    lines.append(f'\t\t\t\t\t}}')
                else:
                    lines.append(
                        f'\t\t\t\t\toutInfo.push_back({{ '
                        f'String("{display_name}"), '
                        f'offsetof({struct_name}, {f_name}), '
                        f'{attr_type} '
                        f'}});'
                    )
            else:
                elem_type, count, is_dynamic = array_info
                attr_type = TYPE_MAP.get(elem_type, "AttributeType::Unknown")

                if is_dynamic and elem_type in known_structs:
                    lines.append(f'\t\t\t\t\t{{')
                    lines.append(f'\t\t\t\t\t\tauto& arr = obj.{f_name};')
                    lines.append(f'\t\t\t\t\t\tFieldInfo header;')
                    lines.append(f'\t\t\t\t\t\theader.name_ = String("{display_name}");')
                    lines.append(f'\t\t\t\t\t\theader.offset_ = 0;')
                    lines.append(f'\t\t\t\t\t\theader.type_ = AttributeType::Struct;')
                    lines.append(f'\t\t\t\t\t\theader.nestedTypeName_ = String("{elem_type}");')
                    lines.append(f'\t\t\t\t\t\theader.array_.size_ = arr.size();')
                    lines.append(f'\t\t\t\t\t\theader.array_.add_ = [&obj]() {{ obj.{f_name}.push_back({{}}); }};')
                    lines.append(f'\t\t\t\t\t\theader.array_.remove_ = [&obj](Size idx) {{ if (idx < obj.{f_name}.size()) obj.{f_name}.erase(obj.{f_name}.begin() + idx); }};')
                    if is_serialize_only:
                        lines.append(f'\t\t\t\t\t\theader.editorVisible_ = false;')
                    lines.append(f'\t\t\t\t\t\toutInfo.push_back(std::move(header));')
                    lines.append(f'\t\t\t\t\t\tfor (Size i = 0; i < arr.size(); ++i)')
                    lines.append(f'\t\t\t\t\t\t{{')
                    lines.append(f'\t\t\t\t\t\t\tFieldInfo elementInfo;')
                    lines.append(f'\t\t\t\t\t\t\telementInfo.offset_ = 0;')
                    lines.append(f'\t\t\t\t\t\t\telementInfo.type_ = AttributeType::Struct;')
                    lines.append(f'\t\t\t\t\t\t\telementInfo.directPtr_ = &arr[i];')
                    lines.append(f'\t\t\t\t\t\t\telementInfo.nestedTypeName_ = String("{elem_type}");')
                    if is_serialize_only:
                        lines.append(f'\t\t\t\t\t\t\telementInfo.editorVisible_ = false;')
                    lines.append(f'\t\t\t\t\t\t\toutInfo.push_back(std::move(elementInfo));')
                    lines.append(f'\t\t\t\t\t\t}}')
                    lines.append(f'\t\t\t\t\t}}')

                elif is_dynamic:
                    lines.append(f'\t\t\t\t\t{{')
                    lines.append(f'\t\t\t\t\t\tauto& arr = obj.{f_name};')
                    lines.append(f'\t\t\t\t\t\tFieldInfo header;')
                    lines.append(f'\t\t\t\t\t\theader.name_ = String("{display_name}");')
                    lines.append(f'\t\t\t\t\t\theader.offset_ = 0;')
                    lines.append(f'\t\t\t\t\t\theader.type_ = {attr_type};')
                    lines.append(f'\t\t\t\t\t\theader.array_.size_ = arr.size();')
                    lines.append(f'\t\t\t\t\t\theader.array_.add_ = [&obj]() {{ obj.{f_name}.push_back({{}}); }};')
                    lines.append(f'\t\t\t\t\t\theader.array_.remove_ = [&obj](Size idx) {{ if (idx < obj.{f_name}.size()) obj.{f_name}.erase(obj.{f_name}.begin() + idx); }};')
                    if is_serialize_only:
                        lines.append(f'\t\t\t\t\t\theader.editorVisible_ = false;')
                    lines.append(f'\t\t\t\t\t\toutInfo.push_back(std::move(header));')
                    lines.append(f'\t\t\t\t\t\tfor (Size i = 0; i < arr.size(); ++i)')
                    lines.append(f'\t\t\t\t\t\t{{')
                    if is_serialize_only:
                        lines.append(f'\t\t\t\t\t\t\tFieldInfo elementInfo{{ String("[" + std::to_string(i) + "]"), 0, {attr_type}, PayloadAssetType::None, &arr[i] }};')
                        lines.append(f'\t\t\t\t\t\t\telementInfo.editorVisible_ = false;')
                        lines.append(f'\t\t\t\t\t\t\toutInfo.push_back(std::move(elementInfo));')
                    else:
                        lines.append(
                            f'\t\t\t\t\t\t\toutInfo.push_back({{ '
                            f'String("[" + std::to_string(i) + "]"), 0, {attr_type}, '
                            f'PayloadAssetType::None, &arr[i] '
                            f'}});'
                        )
                    lines.append(f'\t\t\t\t\t\t}}')
                    lines.append(f'\t\t\t\t\t}}')
                else:
                    lines.append(f'\t\t\t\t\t{{')
                    lines.append(f'\t\t\t\t\t\tFieldInfo header;')
                    lines.append(f'\t\t\t\t\t\theader.name_ = String("{display_name}");')
                    lines.append(f'\t\t\t\t\t\theader.offset_ = offsetof({struct_name}, {f_name});')
                    lines.append(f'\t\t\t\t\t\theader.type_ = {attr_type};')
                    lines.append(f'\t\t\t\t\t\theader.array_.size_ = {count};')
                    if is_serialize_only:
                        lines.append(f'\t\t\t\t\t\theader.editorVisible_ = false;')
                    lines.append(f'\t\t\t\t\t\toutInfo.push_back(std::move(header));')
                    for i in range(count):
                        if is_serialize_only:
                            lines.append(f'\t\t\t\t\t\t{{ FieldInfo elem{{ String("[{i}]"), offsetof({struct_name}, {f_name}) + {i} * sizeof({elem_type}), {attr_type} }}; elem.editorVisible_ = false; outInfo.push_back(std::move(elem)); }}')
                        else:
                            lines.append(
                                f'\t\t\t\t\t\toutInfo.push_back({{ '
                                f'String("[{i}]"), offsetof({struct_name}, {f_name}) + {i} * sizeof({elem_type}), {attr_type} '
                                f'}});'
                            )
                    lines.append(f'\t\t\t\t\t}}')

        lines.append('\t\t\t\t});')
        lines.append('\t\t\t}')
        lines.append('\t\t};')
        lines.append(f'\t\tstatic Register_{struct_name} global_{struct_name}_register;')
        lines.append('')

    struct_names = [s for s, _ in results]
    return includes, force_lines, lines, struct_names, generated_enums


# ---------------------------
# スキャン
# ---------------------------
BEGIN_MARKER = '// [REFLECTION_AUTO_BEGIN]'
END_MARKER = '// [REFLECTION_AUTO_END]'

def update_registry_cpp(registry_cpp_path, all_struct_names):
    with open(registry_cpp_path, 'r', encoding='utf-8', newline='') as f:
        content = f.read()

    pragma_lines = []
    for name in all_struct_names:
        pragma_lines.append(f'#pragma comment(linker, "/include:_force_reflection_{name}")')

    new_block = BEGIN_MARKER + '\r\n' + '\r\n'.join(pragma_lines) + '\r\n' + END_MARKER

    if BEGIN_MARKER in content and END_MARKER in content:
        start = content.index(BEGIN_MARKER)
        end = content.index(END_MARKER) + len(END_MARKER)
        updated = content[:start] + new_block + content[end:]
    else:
        include_end = content.rfind('#include')
        insert_pos = content.index('\r\n', include_end) + 2
        updated = content[:insert_pos] + '\r\n' + new_block + '\r\n' + content[insert_pos:]

    if updated != content:
        with open(registry_cpp_path, 'w', encoding='utf-8', newline='') as f:
            f.write(updated)
        print(f"更新完了: {os.path.basename(registry_cpp_path)}")


def collect_all_enums(project_root):
    all_enums = {}
    enum_headers = {}
    enum_owners = {}
    for root, dirs, files in os.walk(project_root):
        prune_directories(dirs)
        for file in files:
            if not file.endswith(".h"):
                continue
            full_path = os.path.join(root, file)
            try:
                with open(full_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                cleaned = remove_comments_and_strings(content)
                rel = os.path.relpath(full_path, start=project_root).replace('\\', '/')

                for struct_match in STRUCT_START.finditer(cleaned):
                    owner_name = struct_match.group(2)
                    brace_pos = cleaned.find('{', struct_match.end())
                    if brace_pos == -1:
                        continue
                    body = extract_block(cleaned, brace_pos)
                    if not body:
                        continue
                    nested_enums = parse_enum_classes(body)
                    for enum_name, entries in nested_enums.items():
                        all_enums[enum_name] = entries
                        enum_headers[enum_name] = rel
                        enum_owners[enum_name] = owner_name

                global_enums = parse_enum_classes(cleaned)
                for enum_name, entries in global_enums.items():
                    if enum_name not in all_enums:
                        all_enums[enum_name] = entries
                        enum_headers[enum_name] = rel
                        enum_owners[enum_name] = None
            except Exception:
                continue
    return all_enums, enum_headers, enum_owners


def collect_all_reflectable_structs(project_root):
    """
    struct/class 名 -> その定義があるヘッダの相対パス。
    本体(直下のネストも含む)に SC_REFLECTION_FIELD 系か SC_SERIALIZE_FIELD が
    1つでもあれば「既知の構造体」とし、他フィールドの型としてこの名前が
    出てきたら nested_type_name_ で再帰させる対象になる。
    """
    known_structs = {}
    for root, dirs, files in os.walk(project_root):
        prune_directories(dirs)
        for file in files:
            if not file.endswith(".h"):
                continue
            if "reflection" in file:
                continue
            full_path = os.path.join(root, file)
            try:
                with open(full_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
            except Exception:
                continue

            cleaned = remove_comments_and_strings(content)
            rel = os.path.relpath(full_path, start=project_root).replace('\\', '/')

            for match in STRUCT_START.finditer(cleaned):
                struct_name = match.group(2)
                brace_pos = cleaned.find('{', match.end())
                if brace_pos == -1:
                    continue
                body = extract_block(cleaned, brace_pos)
                if not body:
                    continue
                if FIELD_MARKER_PATTERN.search(body):
                    known_structs[struct_name] = rel

    return known_structs


def is_userproject_file(full_path, project_root):
    """
    full_path が UserProject/ 配下にあるかどうかを判定する。
    UserProject で定義された型のリフレクションコードは UserProject.dll 側の
    出力ファイルへ、それ以外(エンジン側)は FoundationEngine 側の出力ファイル
    へ振り分けるために使う — UserProject.dll だけを再ビルドしてもリフレクション
    のフィールドオフセットが古いまま SeedCore.dll に取り残される事故を防ぐため。
    """
    rel = os.path.relpath(full_path, project_root)
    return rel.split(os.sep)[0].split('/')[0] == 'UserProject'


def build_reflection_source(includes, force_lines, body_lines, generated_enums, global_enums, enum_owners):
    """
    1モジュール分の includes/force_lines/body_lines/generated_enums から、
    Reflection.generated.cpp の完全なソースコード文字列を組み立てる。
    """
    body_lines = list(body_lines)

    for enum_name in sorted(generated_enums):
        entries = global_enums[enum_name]
        owner = enum_owners.get(enum_name) if enum_owners else None
        qualified = f'{owner}::{enum_name}' if owner else enum_name
        body_lines.append(f'\t\tstruct RegisterEnum_{enum_name}')
        body_lines.append('\t\t{')
        body_lines.append(f'\t\t\tRegisterEnum_{enum_name}()')
        body_lines.append('\t\t\t{')
        body_lines.append(f'\t\t\t\tEnumRegistry::Register(String("{enum_name}"), {{')
        for entry in entries:
            body_lines.append(f'\t\t\t\t\t{{ static_cast<Int>({qualified}::{entry}), String("{entry}") }},')
        body_lines.append('\t\t\t\t});')
        body_lines.append('\t\t\t}')
        body_lines.append('\t\t};')
        body_lines.append(f'\t\tstatic RegisterEnum_{enum_name} global_{enum_name}_enum_register;')
        body_lines.append('')

    lines = []
    lines.append('#include <FoundationEngine/Prelude.h>')
    lines.append('#include <FoundationEngine/ECS/ReflectionRegistry.h>')
    for include in sorted(set(includes)):
        lines.append(f'#include <{include}>')
    lines.append('')
    lines.extend(force_lines)
    lines.append('')
    lines.append('namespace SeedCore')
    lines.append('{')
    lines.append('\t namespace ScReflection')
    lines.append('\t {')
    lines.extend(body_lines)
    lines.append('\t}')
    lines.append('}')

    return '\r\n'.join(lines)


def write_generated_file(output_path, code, label):
    """
    code が output_path の現在の中身と異なる場合のみ書き込み、変化したかどうかを返す。
    """
    old_code = None
    if os.path.exists(output_path):
        with open(output_path, 'r', encoding='utf-8', newline='') as f:
            old_code = f.read()

    if old_code == code:
        return False

    with open(output_path, 'w', encoding='utf-8', newline='') as f:
        f.write(code)
    print(f"生成完了: {label}")
    return True


def run_auto_scan():
    """
    ヘッダーごとに新しい.reflection.cppを作る方式はやめて、既に vcxproj に
    登録済みの1ファイル(Reflection.generated.cpp)の中身だけを毎回まるごと
    書き換える。ファイルの追加/削除が発生しないので、PreBuildEvent内で
    Pythonが生成した内容がその回のビルドでそのままコンパイルされる
    (ファイルを新規追加すると、MSBuildは実行中のビルドで構成を再読み込み
    しないため、次のビルドまで反映されなかった)。

    UserProject/ 配下で定義された型は、FoundationEngine 側の集約ファイルへ
    混ぜず、UserProject/Reflection/Reflection.generated.cpp という別ファイルへ
    出力し UserProject.vcxproj 側でコンパイルする。UserProject.dll だけを
    再ビルドしてリフレクションフィールドを追加/削除/並べ替えても、
    そのオフセット情報は同じ UserProject.dll 内で更新される — SeedCore.dll
    (FoundationEngineの集約ファイル)側に古いオフセットが取り残されて
    ホットリロード時にメモリ破壊を起こす事故を防ぐ。
    """
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))

    foundation_output_dir = os.path.join(project_root, 'FoundationEngine', 'Reflection')
    os.makedirs(foundation_output_dir, exist_ok=True)
    foundation_output_path = os.path.join(foundation_output_dir, 'Reflection.generated.cpp')

    userproject_output_dir = os.path.join(project_root, 'UserProject', 'Reflection')
    os.makedirs(userproject_output_dir, exist_ok=True)
    userproject_output_path = os.path.join(userproject_output_dir, 'Reflection.generated.cpp')

    global_enums, enum_headers, enum_owners = collect_all_enums(project_root)
    known_structs = collect_all_reflectable_structs(project_root)

    buckets = {
        'foundation': {'struct_names': [], 'includes': [], 'force_lines': [], 'body_lines': [], 'generated_enums': set()},
        'userproject': {'struct_names': [], 'includes': [], 'force_lines': [], 'body_lines': [], 'generated_enums': set()},
    }

    for root, dirs, files in os.walk(project_root):
        prune_directories(dirs)
        for file in files:
            if not file.endswith(".h"):
                continue

            if "reflection" in file:
                continue

            full_path = os.path.join(root, file)
            result = process_file(full_path, project_root, global_enums, enum_headers, enum_owners, known_structs)

            if result:
                includes, force_lines, body_lines, struct_names, generated_enums = result
                bucket = buckets['userproject'] if is_userproject_file(full_path, project_root) else buckets['foundation']
                bucket['includes'].extend(includes)
                bucket['force_lines'].extend(force_lines)
                bucket['body_lines'].extend(body_lines)
                bucket['struct_names'].extend(struct_names)
                bucket['generated_enums'].update(generated_enums)

    foundation_bucket = buckets['foundation']
    userproject_bucket = buckets['userproject']

    foundation_code = build_reflection_source(
        foundation_bucket['includes'], foundation_bucket['force_lines'], foundation_bucket['body_lines'],
        foundation_bucket['generated_enums'], global_enums, enum_owners
    )
    userproject_code = build_reflection_source(
        userproject_bucket['includes'], userproject_bucket['force_lines'], userproject_bucket['body_lines'],
        userproject_bucket['generated_enums'], global_enums, enum_owners
    )

    foundation_changed = write_generated_file(foundation_output_path, foundation_code, 'Reflection.generated.cpp (FoundationEngine)')
    userproject_changed = write_generated_file(userproject_output_path, userproject_code, 'Reflection.generated.cpp (UserProject)')

    if foundation_changed:
        delete_stale_objs(project_root, 'FoundationEngine')
    if userproject_changed:
        delete_stale_objs(project_root, 'UserProject')

    registry_cpp = os.path.join(project_root, 'FoundationEngine', 'ECS', 'ReflectionRegistry.cpp')
    if os.path.exists(registry_cpp):
        update_registry_cpp(registry_cpp, foundation_bucket['struct_names'])


SKIP_DIRECTORIES = {'External', 'Package', 'CompiledShaderObject', 'Build', 'Intermediate', '.git', '.vs'}


def prune_directories(dirs):
    """
    走査対象から外すディレクトリを dirs から取り除く。
    サードパーティ(External)やビルド生成物には SC_ アノテーションが
    存在しないため、読み込むだけ時間の無駄になる。os.walk は dirs を
    破壊的に書き換えるとその配下ごと走査をスキップするので、
    リポジトリの .h の約7割をここで除外できる。
    """
    dirs[:] = [directory for directory in dirs if directory not in SKIP_DIRECTORIES]


def delete_stale_objs(project_root, module_name):
    """
    生成ファイルが書き換わったときに、対応する obj を消しておく。
    生成ファイル自体もタイムスタンプが更新されるので MSBuild の通常の
    増分判定だけで再コンパイルされるが、念のための保険として残している。

    かつてはここで .tlog(MSBuild の増分ビルド情報)も消していた。これは
    ヘッダごとに .reflection.cpp を生成し、毎ビルドでファイルが増減して
    いた頃の名残で、当時はプロジェクト構成が変わるため増分情報を捨てる
    必要があった。現在は vcxproj 登録済みの1ファイルの中身を書き換える
    だけでファイルの増減が起きないので不要。むしろ消すと全ファイルが
    再コンパイル対象になり、巨大な Prelude.h の PCH まで作り直しになる。
    """
    import glob as globmod
    obj_root = os.path.join(project_root, 'Runtime', 'Intermediate', module_name)
    for obj in globmod.glob(os.path.join(obj_root, '**', 'Reflection.generated.obj'), recursive=True):
        try:
            os.remove(obj)
            print(f"obj削除: {obj}")
        except Exception:
            pass


NS = 'http://schemas.microsoft.com/developer/msbuild/2003'

FILTER_NAME = 'Reflection'

def sync_vcxproj(vcxproj_path, reflection_cpps):
    ET.register_namespace('', NS)
    tree = ET.parse(vcxproj_path)
    root = tree.getroot()

    proj_dir = os.path.dirname(vcxproj_path)

    existing = set()
    for item in root.iter(f'{{{NS}}}ClCompile'):
        inc = item.get('Include', '')
        existing.add(os.path.normpath(inc).lower())

    changed = False

    for cpp_path in reflection_cpps:
        rel = os.path.relpath(cpp_path, proj_dir)
        if os.path.normpath(rel).lower() in existing:
            continue

        item_groups = [ig for ig in root.iter(f'{{{NS}}}ItemGroup') if ig.find(f'{{{NS}}}ClCompile') is not None]
        if not item_groups:
            continue
        ig = item_groups[0]

        elem = ET.SubElement(ig, f'{{{NS}}}ClCompile')
        elem.set('Include', rel)
        pch_debug = ET.SubElement(elem, f'{{{NS}}}PrecompiledHeader')
        pch_debug.set('Condition', "'$(Configuration)|$(Platform)'=='Debug|x64'")
        pch_debug.text = 'NotUsing'
        pch_release = ET.SubElement(elem, f'{{{NS}}}PrecompiledHeader')
        pch_release.set('Condition', "'$(Configuration)|$(Platform)'=='Release|x64'")
        pch_release.text = 'NotUsing'

        print(f"vcxproj追加: {rel}")
        changed = True

    if changed:
        tree.write(vcxproj_path, xml_declaration=True, encoding='utf-8')

    filters_path = vcxproj_path + '.filters'
    if os.path.exists(filters_path):
        sync_filters(filters_path, reflection_cpps, proj_dir)


def sync_filters(filters_path, cpp_files, proj_dir):
    ET.register_namespace('', NS)
    tree = ET.parse(filters_path)
    root = tree.getroot()

    filter_exists = False
    for f in root.iter(f'{{{NS}}}Filter'):
        if f.get('Include', '') == FILTER_NAME:
            filter_exists = True
            break

    if not filter_exists:
        filter_groups = [ig for ig in root.iter(f'{{{NS}}}ItemGroup') if ig.find(f'{{{NS}}}Filter') is not None]
        if filter_groups:
            fg = filter_groups[0]
            filt = ET.SubElement(fg, f'{{{NS}}}Filter')
            filt.set('Include', FILTER_NAME)
            uid = ET.SubElement(filt, f'{{{NS}}}UniqueIdentifier')
            uid.text = '{a1b2c3d4-refl-0000-0000-000000000001}'

    existing = set()
    for item in root.iter(f'{{{NS}}}ClCompile'):
        inc = item.get('Include', '')
        existing.add(os.path.normpath(inc).lower())

    changed = False

    for cpp_path in cpp_files:
        rel = os.path.relpath(cpp_path, proj_dir)
        if os.path.normpath(rel).lower() in existing:
            continue

        compile_groups = [ig for ig in root.iter(f'{{{NS}}}ItemGroup') if ig.find(f'{{{NS}}}ClCompile') is not None]
        if not compile_groups:
            continue
        ig = compile_groups[0]

        elem = ET.SubElement(ig, f'{{{NS}}}ClCompile')
        elem.set('Include', rel)
        filt_elem = ET.SubElement(elem, f'{{{NS}}}Filter')
        filt_elem.text = FILTER_NAME

        print(f"filters追加: {rel} -> {FILTER_NAME}")
        changed = True

    if changed:
        tree.write(filters_path, xml_declaration=True, encoding='utf-8')



if __name__ == "__main__":
    run_auto_scan()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))

    output_dir = os.path.join(project_root, 'FoundationEngine', 'Reflection')
    foundation_vcxproj = os.path.join(project_root, 'FoundationEngine', 'FoundationEngine.vcxproj')

    reflection_cpps = [os.path.join(output_dir, 'Reflection.generated.cpp')]

    if os.path.exists(foundation_vcxproj):
        sync_vcxproj(foundation_vcxproj, reflection_cpps)

    userproject_output_dir = os.path.join(project_root, 'UserProject', 'Reflection')
    userproject_vcxproj = os.path.join(project_root, 'UserProject', 'UserProject.vcxproj')

    userproject_reflection_cpps = [os.path.join(userproject_output_dir, 'Reflection.generated.cpp')]

    if os.path.exists(userproject_vcxproj):
        sync_vcxproj(userproject_vcxproj, userproject_reflection_cpps)