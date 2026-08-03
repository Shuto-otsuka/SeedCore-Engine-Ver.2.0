import re
import os
import hashlib
import xml.etree.ElementTree as ET

STRUCT_START = re.compile(r'(struct|class)\s+(?:SEEDCORE_API\s+)?(\w+)')

FIELD_PATTERN = re.compile(
    r'SC_PAYLOAD_FIELD\(\s*(\w+)\s*\)\s*([\w:<>\s]+)\s+(\w+)\s*(?:=[^;]*)?\s*;'
)

FIELD_EX_PATTERN = re.compile(
    r'SC_PAYLOAD_FIELD_EX\(\s*"([^"]+)"\s*,\s*(\w+)\s*\)\s*([\w:<>\s]+)\s+(\w+)\s*(?:=[^;]*)?\s*;'
)

CONDITION_PATTERN = re.compile(
    r'SC_REFLECTION_FIELD_CONDITION\((.+?)\)\s*'
)

ENUM_CLASS_PATTERN = re.compile(
    r'enum\s+class\s+(\w+)\s*\{([^}]*)\}'
)

DYNAMIC_ARRAY_PREFIXES = ['std::vector', 'DynamicArray']

def parse_array_info(type_str):
    """
    Payload fields are never fixed-size C arrays, so only DynamicArray/
    std::vector need detecting here. Returns the element type string, or
    None if type_str isn't a dynamic array.
    """
    type_str = type_str.strip()

    for prefix in DYNAMIC_ARRAY_PREFIXES:
        if type_str.startswith(prefix + '<'):
            depth = 0
            start = type_str.index('<')
            for i in range(start, len(type_str)):
                if type_str[i] == '<': depth += 1
                elif type_str[i] == '>': depth -= 1
                if depth == 0:
                    return type_str[start + 1:i].strip()

    return None

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
                if prefix.rstrip().endswith('SC_PAYLOAD_FIELD_EX('):
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
# Condition (enableIf_) 対応
# ---------------------------
def parse_enum_names(text):
    return [match.group(1) for match in ENUM_CLASS_PATTERN.finditer(text)]


def build_enum_owners(content):
    """ネストされた enum class を所有する struct/class 名にマップする。"""
    owners = {}
    for match in STRUCT_START.finditer(content):
        struct_name = match.group(2)
        brace_pos = content.find('{', match.end())
        if brace_pos == -1:
            continue
        body = extract_block(content, brace_pos)
        if not body:
            continue
        for enum_name in parse_enum_names(body):
            owners[enum_name] = struct_name
    return owners


def qualify_condition(condition, enum_owners):
    result = re.sub(r'\b(\w+_)\b', r'o.\1', condition)
    for enum_name, owner in enum_owners.items():
        if owner:
            result = re.sub(r'\b' + enum_name + r'::', f'{owner}::{enum_name}::', result)
    return result


# ---------------------------
# struct/field 解析（コード生成から独立した純粋関数）
# ---------------------------
def parse_payload_structs(content):
    """
    コメント・文字列除去済みの content から、SC_PAYLOAD_FIELD(_EX) が
    付与された struct/class ごとに [(f_type, f_name, display_name,
    asset_type, condition), ...] を集めて返す。
    [(struct_name, fields), ...] の形。process_file() のコード生成部分と
    collect_payload_field_names() の両方から使われる。
    """
    results = []

    enum_owners = build_enum_owners(content)

    for match in STRUCT_START.finditer(content):
        struct_name = match.group(2)

        brace_pos = content.find('{', match.end())
        if brace_pos == -1:
            continue

        body = extract_block(content, brace_pos)
        if not body:
            continue

        conditions = {}
        for cond_match in CONDITION_PATTERN.finditer(body):
            conditions[cond_match.end()] = cond_match.group(1).strip()

        fields = []
        for match in FIELD_PATTERN.finditer(body):
            fields.append((match.start(), match.group(2), match.group(3), match.group(3), match.group(1)))
        for match in FIELD_EX_PATTERN.finditer(body):
            fields.append((match.start(), match.group(3), match.group(4), match.group(1), match.group(2)))
        fields.sort(key=lambda entry: entry[0])

        resolved_fields = []
        used_conditions = set()
        for pos, f_type, f_name, display_name, asset_type in fields:
            condition = None
            for cond_end, cond_expr in conditions.items():
                if cond_end in used_conditions:
                    continue
                if cond_end <= pos and pos - cond_end < 200:
                    between = body[cond_end:pos].strip()
                    if between == '' or between.startswith('SC_'):
                        condition = cond_expr
                        used_conditions.add(cond_end)
                        break
            resolved_fields.append((f_type, f_name, display_name, asset_type, condition))
        fields = resolved_fields
        if fields:
            results.append((struct_name, fields))

    return results


def collect_payload_field_names(project_root):
    """
    project_root 以下の全 .h を走査し、payload フィールドを持つ struct/class
    ごとの表示名一覧を {struct_name: set(display_name)} で返す。
    run_auto_scan() と同じ走査対象（"payload" を含むファイル名は除外）。
    RuntimePackager.py がシーン/プレハブの参照 Asset を解決するために使う。
    """
    field_names = {}

    for root, dirs, files in os.walk(project_root):
        prune_directories(dirs)
        for file in files:
            if not file.endswith(".h"):
                continue
            if "payload" in file.lower():
                continue

            full_path = os.path.join(root, file)
            try:
                with open(full_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
            except Exception:
                continue

            content = remove_comments_and_strings(content)

            for struct_name, fields in parse_payload_structs(content):
                names = field_names.setdefault(struct_name, set())
                for _f_type, _f_name, display_name, _asset_type, _condition in fields:
                    names.add(display_name)

    return field_names


# ---------------------------
# メイン処理
# ---------------------------
def process_file(file_path, project_root):
    if "payload" in file_path.lower():
        return None

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except Exception:
        return None

    content = remove_comments_and_strings(content)

    # -----------------------
    # キャッシュ
    # -----------------------
    file_hash = hashlib.md5(
        content.encode('utf-8', errors='ignore')
    ).hexdigest()

    cache = getattr(process_file, "cache", {})

    if file_path in cache and cache[file_path] == file_hash:
        return None

    cache[file_path] = file_hash
    process_file.cache = cache

    # -----------------------
    # 解析
    # -----------------------
    enum_owners = build_enum_owners(content)
    results = parse_payload_structs(content)

    if not results:
        return None

    # -----------------------
    # 出力生成
    # -----------------------
    rel_path = os.path.relpath(file_path, start=project_root)
    normalized_path = rel_path.replace('\\', '/')

    includes = [normalized_path]

    force_lines = []
    for struct_name, fields in results:
        force_lines.append(f'extern "C" int _force_payload_{struct_name} = 0;')

    lines = []
    lines.append(f'\t\t// ---- {normalized_path} ----')

    for struct_name, fields in results:
        lines.append(f'\t\tstruct Register_{struct_name}')
        lines.append('\t\t{')
        lines.append(f'\t\t\tRegister_{struct_name}()')
        lines.append('\t\t\t{')
        lines.append(
            f'\t\t\t\tPayloadRegistry::Register('
            f'String("{struct_name}"), '
            '[](void* ptr, DynamicArray<FieldInfo>& outInfo) {'
        )

        lines.append(f'\t\t\t\t\t{struct_name}& obj = *static_cast<{struct_name}*>(ptr);')

        for f_type, f_name, display_name, asset_type, condition in fields:
            elem_type = parse_array_info(f_type)

            if elem_type is not None:
                attr_type = TYPE_MAP.get(elem_type.strip(), "AttributeType::Unknown")
                lines.append('\t\t\t\t\t{')
                lines.append(f'\t\t\t\t\t\tauto& arr = obj.{f_name};')
                lines.append('\t\t\t\t\t\tFieldInfo header;')
                lines.append(f'\t\t\t\t\t\theader.name_ = String("{display_name}");')
                lines.append('\t\t\t\t\t\theader.offset_ = 0;')
                lines.append(f'\t\t\t\t\t\theader.type_ = {attr_type};')
                lines.append(f'\t\t\t\t\t\theader.assetType_ = PayloadAssetType::{asset_type};')
                lines.append('\t\t\t\t\t\theader.array_.size_ = arr.size();')
                lines.append(f'\t\t\t\t\t\theader.array_.add_ = [&obj]() {{ obj.{f_name}.push_back({{}}); }};')
                lines.append(f'\t\t\t\t\t\theader.array_.remove_ = [&obj](Size idx) {{ if (idx < obj.{f_name}.size()) obj.{f_name}.erase(obj.{f_name}.begin() + idx); }};')
                lines.append(f'\t\t\t\t\t\theader.array_.lastPtr_ = [&obj]() -> void* {{ return &obj.{f_name}.back(); }};')
                lines.append('\t\t\t\t\t\toutInfo.push_back(std::move(header));')
                lines.append('\t\t\t\t\t\tfor (Size i = 0; i < arr.size(); ++i)')
                lines.append('\t\t\t\t\t\t{')
                lines.append(
                    f'\t\t\t\t\t\t\toutInfo.push_back({{ '
                    f'String("[" + std::to_string(i) + "]"), 0, {attr_type}, '
                    f'PayloadAssetType::{asset_type}, &arr[i] '
                    f'}});'
                )
                lines.append('\t\t\t\t\t\t}')
                lines.append('\t\t\t\t\t}')
                continue

            attr_type = TYPE_MAP.get(f_type.strip(), "AttributeType::Unknown")
            if condition:
                lines.append('\t\t\t\t\t{')
                lines.append('\t\t\t\t\t\tFieldInfo fi;')
                lines.append(f'\t\t\t\t\t\tfi.name_ = String("{display_name}");')
                lines.append(f'\t\t\t\t\t\tfi.offset_ = offsetof({struct_name}, {f_name});')
                lines.append(f'\t\t\t\t\t\tfi.type_ = {attr_type};')
                lines.append(f'\t\t\t\t\t\tfi.assetType_ = PayloadAssetType::{asset_type};')
                lines.append(f'\t\t\t\t\t\tfi.enableIf_ = [](void* p) -> Bool {{ auto& o = *static_cast<{struct_name}*>(p); return {qualify_condition(condition, enum_owners)}; }};')
                lines.append('\t\t\t\t\t\toutInfo.push_back(std::move(fi));')
                lines.append('\t\t\t\t\t}')
            else:
                lines.append(
                    f'\t\t\t\t\toutInfo.push_back({{ '
                    f'String("{display_name}"), '
                    f'offsetof({struct_name}, {f_name}), '
                    f'{attr_type}, '
                    f'PayloadAssetType::{asset_type} '
                    f'}});'
                )

        lines.append('\t\t\t\t});')
        lines.append('\t\t\t}')
        lines.append('\t\t};')
        lines.append(f'\t\tstatic Register_{struct_name} global_{struct_name}_register;')
        lines.append('')

    struct_names = [s for s, _ in results]
    return includes, force_lines, lines, struct_names


# ---------------------------
# スキャン
# ---------------------------
BEGIN_MARKER = '// [PAYLOAD_AUTO_BEGIN]'
END_MARKER = '// [PAYLOAD_AUTO_END]'

def update_registry_cpp(registry_cpp_path, all_struct_names):
    with open(registry_cpp_path, 'r', encoding='utf-8', newline='') as f:
        content = f.read()

    pragma_lines = []
    for name in all_struct_names:
        pragma_lines.append(f'#pragma comment(linker, "/include:_force_payload_{name}")')

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


def is_userproject_file(full_path, project_root):
    """
    full_path が UserProject/ 配下にあるかどうかを判定する。
    UserProject で定義された型の Payload コードは UserProject.dll 側の
    出力ファイルへ、それ以外(エンジン側)は FoundationEngine 側の出力ファイル
    へ振り分けるために使う — UserProject.dll だけを再ビルドしてもPayload
    フィールドのオフセットが古いまま SeedCore.dll に取り残される事故を防ぐ。
    """
    rel = os.path.relpath(full_path, project_root)
    return rel.split(os.sep)[0].split('/')[0] == 'UserProject'


def build_payload_source(includes, force_lines, body_lines):
    lines = []
    lines.append('#include <FoundationEngine/Prelude.h>')
    lines.append('#include <FoundationEngine/ECS/PayloadRegistry.h>')
    for include in sorted(set(includes)):
        lines.append(f'#include <{include}>')
    lines.append('')
    lines.extend(force_lines)
    lines.append('')
    lines.append('namespace SeedCore')
    lines.append('{')
    lines.append('\t namespace ScPayload')
    lines.append('\t {')
    lines.extend(body_lines)
    lines.append('\t}')
    lines.append('}')

    return '\r\n'.join(lines)


def write_generated_file(output_path, code, label):
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
    ヘッダーごとに新しい.payload.cppを作る方式はやめて、既にvcxprojに登録
    済みの1ファイル(Payload.generated.cpp)の中身だけを毎回まるごと書き換える。
    理由はReflection.pyと同じ(PreBuildEvent中の新規ファイル追加は同じ
    ビルドに反映されない)。

    UserProject/ 配下で定義された型は、FoundationEngine 側の集約ファイルへ
    混ぜず、UserProject/Payload/Payload.generated.cpp という別ファイルへ
    出力し UserProject.vcxproj 側でコンパイルする(Reflection.py と同じ理由)。
    """
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))

    foundation_output_dir = os.path.join(project_root, 'FoundationEngine', 'Payload')
    os.makedirs(foundation_output_dir, exist_ok=True)
    foundation_output_path = os.path.join(foundation_output_dir, 'Payload.generated.cpp')

    userproject_output_dir = os.path.join(project_root, 'UserProject', 'Payload')
    os.makedirs(userproject_output_dir, exist_ok=True)
    userproject_output_path = os.path.join(userproject_output_dir, 'Payload.generated.cpp')

    buckets = {
        'foundation': {'struct_names': [], 'includes': [], 'force_lines': [], 'body_lines': []},
        'userproject': {'struct_names': [], 'includes': [], 'force_lines': [], 'body_lines': []},
    }

    for root, dirs, files in os.walk(project_root):
        prune_directories(dirs)
        for file in files:
            if not file.endswith(".h"):
                continue

            if "payload" in file.lower():
                continue

            full_path = os.path.join(root, file)
            result = process_file(full_path, project_root)

            if result:
                includes, force_lines, body_lines, struct_names = result
                bucket = buckets['userproject'] if is_userproject_file(full_path, project_root) else buckets['foundation']
                bucket['includes'].extend(includes)
                bucket['force_lines'].extend(force_lines)
                bucket['body_lines'].extend(body_lines)
                bucket['struct_names'].extend(struct_names)

    foundation_bucket = buckets['foundation']
    userproject_bucket = buckets['userproject']

    foundation_code = build_payload_source(foundation_bucket['includes'], foundation_bucket['force_lines'], foundation_bucket['body_lines'])
    userproject_code = build_payload_source(userproject_bucket['includes'], userproject_bucket['force_lines'], userproject_bucket['body_lines'])

    foundation_changed = write_generated_file(foundation_output_path, foundation_code, 'Payload.generated.cpp (FoundationEngine)')
    userproject_changed = write_generated_file(userproject_output_path, userproject_code, 'Payload.generated.cpp (UserProject)')

    if foundation_changed:
        delete_stale_objs(project_root, 'FoundationEngine')
    if userproject_changed:
        delete_stale_objs(project_root, 'UserProject')

    registry_cpp = os.path.join(project_root, 'FoundationEngine', 'ECS', 'PayloadRegistry.cpp')
    if os.path.exists(registry_cpp):
        update_registry_cpp(registry_cpp, foundation_bucket['struct_names'])


SKIP_DIRECTORIES = {'External', 'Package', 'CompiledShaderObject', 'Build', 'Intermediate', '.git', '.vs'}


def prune_directories(dirs):
    """
    走査対象から外すディレクトリを dirs から取り除く
    (詳細は Reflection.py の同名関数のコメントを参照)。
    """
    dirs[:] = [directory for directory in dirs if directory not in SKIP_DIRECTORIES]


def delete_stale_objs(project_root, module_name):
    """
    生成ファイルが書き換わったときに、対応する obj を消しておく。
    以前ここで行っていた .tlog 削除は、ヘッダごとに .payload.cpp を
    生成していた頃の名残なので廃止した
    (詳細は Reflection.py の同名関数のコメントを参照)。
    """
    import glob as globmod
    obj_root = os.path.join(project_root, 'Runtime', 'Intermediate', module_name)
    for obj in globmod.glob(os.path.join(obj_root, '**', 'Payload.generated.obj'), recursive=True):
        try:
            os.remove(obj)
            print(f"obj削除: {obj}")
        except Exception:
            pass


NS = 'http://schemas.microsoft.com/developer/msbuild/2003'

FILTER_NAME = 'Payload'

def sync_vcxproj(vcxproj_path, payload_cpps):
    ET.register_namespace('', NS)
    tree = ET.parse(vcxproj_path)
    root = tree.getroot()

    proj_dir = os.path.dirname(vcxproj_path)

    existing = set()
    for item in root.iter(f'{{{NS}}}ClCompile'):
        inc = item.get('Include', '')
        existing.add(os.path.normpath(inc).lower())

    added = False

    for cpp_path in payload_cpps:
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
        added = True

    if added:
        tree.write(vcxproj_path, xml_declaration=True, encoding='utf-8')

    filters_path = vcxproj_path + '.filters'
    if os.path.exists(filters_path):
        sync_filters(filters_path, payload_cpps, proj_dir)


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
            uid.text = '{a1b2c3d4-payl-0000-0000-000000000001}'

    existing = set()
    for item in root.iter(f'{{{NS}}}ClCompile'):
        inc = item.get('Include', '')
        existing.add(os.path.normpath(inc).lower())

    added = False

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
        added = True

    if added:
        tree.write(filters_path, xml_declaration=True, encoding='utf-8')



if __name__ == "__main__":
    run_auto_scan()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))

    output_dir = os.path.join(project_root, 'FoundationEngine', 'Payload')
    foundation_vcxproj = os.path.join(project_root, 'FoundationEngine', 'FoundationEngine.vcxproj')

    payload_cpps = [os.path.join(output_dir, 'Payload.generated.cpp')]

    if os.path.exists(foundation_vcxproj):
        sync_vcxproj(foundation_vcxproj, payload_cpps)

    userproject_output_dir = os.path.join(project_root, 'UserProject', 'Payload')
    userproject_vcxproj = os.path.join(project_root, 'UserProject', 'UserProject.vcxproj')

    userproject_payload_cpps = [os.path.join(userproject_output_dir, 'Payload.generated.cpp')]

    if os.path.exists(userproject_vcxproj):
        sync_vcxproj(userproject_vcxproj, userproject_payload_cpps)
