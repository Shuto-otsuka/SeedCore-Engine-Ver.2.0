import sys
import os
import re
import struct
import shutil
import json
import hashlib
import subprocess
import concurrent.futures
import ctypes
from ctypes import wintypes

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)
import Payload

# .meta ファイルを持たない拡張子（ResourceCache::noMetaExtensions_ と同じ）。
NO_META_EXTENSIONS = {".h", ".cpp", ".hlsli", ".hlsl"}

# ゲームプレイコードが名前/パス/ID で Asset に辿り着く入口と、そのうち Asset を指す引数の位置。
# スクリプトから ResourceCache には直接触れないため、名前解決(ResourceCache::GetAssetID)を
# 経由する公開 API はこの3つだけ（Scene.h/.cpp, Prefab.h/.cpp 参照）。
# Scene::Change の第2引数はローディングシーン版ではパス/ID、フェード版では秒数なので、
# 解決できなかった場合も警告しない。
ASSET_ENTRY_ARGUMENTS = {
    "Scene::GetAsset": (0,),
    "Prefab::Spawn": (0,),
    "Scene::Change": (0, 1),
}
ASSET_ENTRY_CALL_PATTERN = re.compile(r'\b(Scene::GetAsset|Scene::Change|Prefab::Spawn)\s*\(')

# 引数が文字列リテラル1つ、またはそれを String(...) / String::intern(...) /
# std::filesystem::path(...) のような変換で包んだだけのものかを判定するパターン。
WRAPPED_STRING_LITERAL_PATTERN = re.compile(r'^(?:[A-Za-z_][\w:]*\s*[({]\s*)*(?:u8|L|u|U)?"((?:[^"\\]|\\.)*)"\s*(?:[)}]\s*)*$')

# Spawn(Uint32) / Change(Uint32) に Asset ID を数値リテラルで直接渡す呼び方。
INTEGER_LITERAL_PATTERN = re.compile(r'^(0[xX][0-9a-fA-F]+|\d+)[uUlL]*$')

# 引数が変数/メンバ/定数名だけのもの（a / a_ / this->a_ / obj.a / Ns::a）。
IDENTIFIER_ARGUMENT_PATTERN = re.compile(r'^(?:this\s*->\s*)?[A-Za-z_]\w*(?:\s*(?:::|\.|->)\s*[A-Za-z_]\w*)*$')

# 識別子が文字列リテラルで初期化/代入される箇所（name = "..." / name{"..."} / name("...")、
# 右辺が String(...) 等で包まれていてもよい）と、#define NAME "..."。
STRING_BINDING_PATTERN = re.compile(r'\b([A-Za-z_]\w*)\s*(?:=|\{|\()\s*(?:[A-Za-z_][\w:]*\s*[({]\s*)*(?:u8|L|u|U)?"((?:[^"\\]|\\.)*)"')
STRING_DEFINE_PATTERN = re.compile(r'#\s*define\s+([A-Za-z_]\w*)\s+(?:u8|L|u|U)?"((?:[^"\\]|\\.)*)"')

# Release の ShaderCompiler(GraphicsEngine/Shader/ShaderCompiler.cpp) と同じプロファイルとオプション。
# シェーダ種別はファイル名の接尾辞で決まり、*RT.hlsl のように接尾辞で決まらないものは
# 内容で判定する（[shader("...")] があれば DXR ライブラリ、[NumThreads] があれば CS）。
SHADER_PROFILE_BY_SUFFIX = {
    "VS": "vs_6_6",
    "HS": "hs_6_6",
    "DS": "ds_6_6",
    "GS": "gs_6_6",
    "PS": "ps_6_6",
    "AS": "as_6_6",
    "MS": "ms_6_6",
    "CS": "cs_6_6",
}
SHADER_COMPILE_OPTIONS = ["-Zi", "-Qstrip_debug", "-O3"]
SHADER_LIBRARY_PATTERN = re.compile(r'\[\s*shader\s*\(\s*"', re.IGNORECASE)
SHADER_NUMTHREADS_PATTERN = re.compile(r'\[\s*numthreads', re.IGNORECASE)

# CS のエントリポイント = [NumThreads(...)] の付いた関数（属性は大文字小文字を区別しない）。
# 1ファイルに複数のエントリを持つ CS（ブラー各パス、A-Trous 各パス等）もこれで全て拾う。
SHADER_COMPUTE_ENTRY_PATTERN = re.compile(r'\[\s*numthreads\s*\([^)]*\)\s*\]\s*(?:\[[^\]]*\]\s*)*void\s+([A-Za-z_]\w*)\s*\(', re.IGNORECASE)


def fnv1a(name):
    """
    FoundationEngine/Serialization/Binary/BinaryArchive.h の BinaryField と
    同じ FNV-1a。フィールド名からタグ付きバイナリのフィールドIDを求める。
    """
    h = 2166136261
    for byte in name.encode("utf-8"):
        h ^= byte
        h = (h * 16777619) & 0xFFFFFFFF
    return h


def read_tagged_binary_fields(raw):
    """
    FoundationEngine/Serialization/Binary/BinaryArchive.h が書き出す、
    [fieldId:u32][size:u32][payload] の並びをスキャンし、
    {fieldId: payloadバイト列} を返す。
    """
    fields = {}
    offset = 0
    length = len(raw)
    while offset + 8 <= length:
        field_id, size = struct.unpack_from('<II', raw, offset)
        offset += 8
        if offset + size > length:
            break
        fields[field_id] = raw[offset:offset + size]
        offset += size
    return fields


GUID_FIELD_ID = fnv1a("guid")


def aes_gf_mul(a, b):
    result = 0
    while b:
        if b & 1:
            result ^= a
        a = ((a << 1) ^ (0x1B if a & 0x80 else 0)) & 0xFF
        b >>= 1
    return result


AES_SBOX = [0] * 256
_p = _q = 1
while True:
    _p = (_p ^ (_p << 1) ^ (0x1B if _p & 0x80 else 0)) & 0xFF
    _q = (_q ^ (_q << 1)) & 0xFF
    _q = (_q ^ (_q << 2)) & 0xFF
    _q = (_q ^ (_q << 4)) & 0xFF
    if _q & 0x80:
        _q ^= 0x09
    AES_SBOX[_p] = _q ^ ((_q << 1 | _q >> 7) & 0xFF) ^ ((_q << 2 | _q >> 6) & 0xFF) ^ ((_q << 3 | _q >> 5) & 0xFF) ^ ((_q << 4 | _q >> 4) & 0xFF) ^ 0x63
    if _p == 1:
        break
AES_SBOX[0] = 0x63

AES_INV_SBOX = [0] * 256
for _index, _value in enumerate(AES_SBOX):
    AES_INV_SBOX[_value] = _index

AES_MUL2 = [aes_gf_mul(_value, 2) for _value in range(256)]
AES_MUL3 = [aes_gf_mul(_value, 3) for _value in range(256)]
AES_MUL9 = [aes_gf_mul(_value, 9) for _value in range(256)]
AES_MUL11 = [aes_gf_mul(_value, 11) for _value in range(256)]
AES_MUL13 = [aes_gf_mul(_value, 13) for _value in range(256)]
AES_MUL14 = [aes_gf_mul(_value, 14) for _value in range(256)]


def read_encryption_key(project_root):
    with open(os.path.join(project_root, "FoundationEngine", "Prelude.h"), 'r', encoding='utf-8', errors='ignore') as f:
        match = re.search(r'#define\s+SC_ENCRYPTION_KEY_SEED\s+"([^"]+)"', f.read())
    if not match:
        return None
    return hashlib.sha256(match.group(1).encode("utf-8")).digest()


def aes256_expand_key(key):
    words = [list(key[4 * index:4 * index + 4]) for index in range(8)]
    rcon = 1
    for index in range(8, 60):
        temp = list(words[index - 1])
        if index % 8 == 0:
            temp = [AES_SBOX[b] for b in temp[1:] + temp[:1]]
            temp[0] ^= rcon
            rcon = aes_gf_mul(rcon, 2)
        elif index % 8 == 4:
            temp = [AES_SBOX[b] for b in temp]
        words.append([words[index - 8][j] ^ temp[j] for j in range(4)])
    return [sum(words[4 * r:4 * r + 4], []) for r in range(15)]


def aes256_cbc_encrypt(key, data):
    round_keys = aes256_expand_key(key)

    padding = 16 - len(data) % 16
    data = data + bytes([padding]) * padding

    iv = os.urandom(16)
    previous = list(iv)
    output = bytearray(iv)
    for offset in range(0, len(data), 16):
        state = [b ^ p ^ k for b, p, k in zip(data[offset:offset + 16], previous, round_keys[0])]
        for round_index in range(1, 15):
            state = [AES_SBOX[b] for b in state]
            state = [state[r + 4 * ((c + r) % 4)] for c in range(4) for r in range(4)]
            if round_index < 14:
                mixed = []
                for c in range(4):
                    a0, a1, a2, a3 = state[4 * c:4 * c + 4]
                    mixed += [
                        AES_MUL2[a0] ^ AES_MUL3[a1] ^ a2 ^ a3,
                        a0 ^ AES_MUL2[a1] ^ AES_MUL3[a2] ^ a3,
                        a0 ^ a1 ^ AES_MUL2[a2] ^ AES_MUL3[a3],
                        AES_MUL3[a0] ^ a1 ^ a2 ^ AES_MUL2[a3],
                    ]
                state = mixed
            state = [b ^ k for b, k in zip(state, round_keys[round_index])]
        output += bytes(state)
        previous = state

    return bytes(output)


def aes256_cbc_decrypt(key, data):
    if len(data) < 32 or (len(data) - 16) % 16 != 0:
        return None

    round_keys = aes256_expand_key(key)

    previous = list(data[:16])
    output = bytearray()
    for offset in range(16, len(data), 16):
        block = list(data[offset:offset + 16])
        state = [b ^ k for b, k in zip(block, round_keys[14])]
        for round_index in range(13, -1, -1):
            state = [state[r + 4 * ((c - r) % 4)] for c in range(4) for r in range(4)]
            state = [AES_INV_SBOX[b] for b in state]
            state = [b ^ k for b, k in zip(state, round_keys[round_index])]
            if round_index > 0:
                mixed = []
                for c in range(4):
                    a0, a1, a2, a3 = state[4 * c:4 * c + 4]
                    mixed += [
                        AES_MUL14[a0] ^ AES_MUL11[a1] ^ AES_MUL13[a2] ^ AES_MUL9[a3],
                        AES_MUL9[a0] ^ AES_MUL14[a1] ^ AES_MUL11[a2] ^ AES_MUL13[a3],
                        AES_MUL13[a0] ^ AES_MUL9[a1] ^ AES_MUL14[a2] ^ AES_MUL11[a3],
                        AES_MUL11[a0] ^ AES_MUL13[a1] ^ AES_MUL9[a2] ^ AES_MUL14[a3],
                    ]
                state = mixed
        output += bytes(s ^ v for s, v in zip(state, previous))
        previous = block

    padding = output[-1]
    if padding < 1 or padding > 16 or output[-padding:] != bytes([padding]) * padding:
        return None
    return bytes(output[:-padding])


def build_guid_map(project_root, user_project_root):
    """
    UserProject 以下を走査し、.meta を持つ全ファイルについて
    guid -> 実ファイルパス のマップと、
    プロジェクトルート相対パス（スラッシュ区切り） -> guid のマップを構築する。
    .meta は AssetMeta::Serialize がタグ付きバイナリ形式(BinaryArchive.h参照)
    で書き出し、BinaryOutputArchive::Write が [IV 16バイト][AES-256-CBC 暗号文]
    として保存している(鍵は SC_ENCRYPTION_KEY_SEED の SHA-256)。
    aes256_cbc_decrypt で復号してから、read_tagged_binary_fields で
    フィールドIDから "guid" の4バイトを引く。
    後者は ResourceCache::GetAssetID() が asset.path_（プロジェクトルート
    相対、フォワードスラッシュ）に対して完全一致で引く仕組みと同じ形。
    """
    guid_to_path = {}
    relpath_to_guid = {}

    key = read_encryption_key(project_root)
    if key is None:
        print("警告: FoundationEngine/Prelude.h から SC_ENCRYPTION_KEY_SEED を読み取れませんでした — .meta を復号できないため、参照 Asset を解決できません")
        return guid_to_path, relpath_to_guid

    for root, _, files in os.walk(user_project_root):
        for file in files:
            if file.endswith(".meta"):
                continue

            ext = os.path.splitext(file)[1].lower()
            if ext in NO_META_EXTENSIONS:
                continue

            full_path = os.path.join(root, file)
            meta_path = full_path + ".meta"
            if not os.path.exists(meta_path):
                continue

            try:
                with open(meta_path, 'rb') as f:
                    raw = aes256_cbc_decrypt(key, f.read())
                if raw is None:
                    print(f"警告: .meta の復号に失敗しました: {meta_path}")
                    continue
                guid_bytes = read_tagged_binary_fields(raw).get(GUID_FIELD_ID)
                if guid_bytes is None or len(guid_bytes) != 4:
                    continue
                guid = struct.unpack('<I', guid_bytes)[0]
            except Exception:
                continue

            guid_to_path[guid] = full_path

            rel_path = os.path.relpath(full_path, project_root).replace('\\', '/')
            relpath_to_guid[rel_path] = guid

    return guid_to_path, relpath_to_guid


def strip_comments(content):
    """
    C++ の // 行コメント / * ブロックコメントだけを除去する
    （文字列/文字リテラルの中身はそのまま残す）。コメント中に書かれた
    Asset パス文字列が Scene::GetAsset(...) 呼び出しとして誤検出される
    のを防ぐための前処理。行番号がずれないよう、コメントも改行だけは
    残して除去する。
    """
    result = []
    i = 0
    n = len(content)

    in_string = False
    in_char = False
    in_line_comment = False
    in_block_comment = False

    while i < n:
        c = content[i]
        next_c = content[i + 1] if i + 1 < n else ''

        if in_line_comment:
            if c == '\n':
                in_line_comment = False
                result.append(c)
            i += 1
            continue

        if in_block_comment:
            if c == '*' and next_c == '/':
                in_block_comment = False
                i += 2
                continue
            if c == '\n':
                result.append(c)
            i += 1
            continue

        if in_string:
            result.append(c)
            if c == '\\' and i + 1 < n:
                result.append(next_c)
                i += 2
                continue
            if c == '"':
                in_string = False
            i += 1
            continue

        if in_char:
            result.append(c)
            if c == '\\' and i + 1 < n:
                result.append(next_c)
                i += 2
                continue
            if c == "'":
                in_char = False
            i += 1
            continue

        if c == '/' and next_c == '/':
            in_line_comment = True
            i += 2
            continue

        if c == '/' and next_c == '*':
            in_block_comment = True
            i += 2
            continue

        if c == '"':
            in_string = True
            result.append(c)
            i += 1
            continue

        if c == "'":
            in_char = True
            result.append(c)
            i += 1
            continue

        result.append(c)
        i += 1

    return ''.join(result)


def build_filename_index(relpath_to_guid):
    filename_to_guids = {}
    for rel_path, guid in relpath_to_guid.items():
        filename_to_guids.setdefault(os.path.basename(rel_path), set()).add(guid)
    return filename_to_guids


def resolve_asset_name(name, relpath_to_guid, filename_to_guids):
    """
    ResourceCache::GetAssetID と同じ規則で name を Asset ID の集合へ解決する:
    まずプロジェクトルート相対パスとして完全一致、外れたらファイル名だけで一致。
    同名ファイルが複数ある場合、エンジンは最初に見つかった1つを使うが、
    どれになるかは静的に決まらないため全候補を返す（パッケージには全て入れる）。
    """
    name = name.replace('\\\\', '\\').replace('\\', '/')
    exact = relpath_to_guid.get(name)
    if exact:
        return {exact}
    return set(filename_to_guids.get(os.path.basename(name), ()))


def split_call_arguments(content, open_index):
    arguments = []
    current = []
    depth = 0
    index = open_index + 1
    length = len(content)

    while index < length:
        c = content[index]

        if c == '"' or c == "'":
            current.append(c)
            index += 1
            while index < length and content[index] != c:
                if content[index] == '\\' and index + 1 < length:
                    current.append(content[index])
                    index += 1
                current.append(content[index])
                index += 1
            if index < length:
                current.append(content[index])
            index += 1
            continue

        if c in '([{':
            depth += 1
        elif c in ')]}':
            if depth == 0:
                arguments.append(''.join(current).strip())
                return arguments
            depth -= 1
        elif c == ',' and depth == 0:
            arguments.append(''.join(current).strip())
            current = []
            index += 1
            continue

        current.append(c)
        index += 1

    return None


def scan_source_for_asset_references(user_project_root, relpath_to_guid, filename_to_guids, guid_to_path):
    """
    UserProject 以下の .cpp/.h を走査し、ASSET_ENTRY_ARGUMENTS の入口
    (Scene::GetAsset / Scene::Change / Prefab::Spawn) に渡される Asset 引数を
    静的に解決する。引数は括弧の対応を取って切り出し、次の形を解決できる:
      - 文字列リテラル（String(...) 等の変換で包まれていてもよい）
      - 文字列リテラルで初期化/代入される変数・メンバ・定数、#define 定数
        （UserProject 全体から同名の束縛を集め、その全ての値を候補にする）
      - Asset ID の数値リテラル（Spawn(Uint32) / Change(Uint32) 版）
    インスペクタで設定される String フィールドの値は、ここではなく
    collect_referenced_asset_ids がシーン/プレハブの保存値から拾う。
    どれにも当てはまらない式や、解決先の Asset が無い引数は警告する。
    """
    sources = []
    bindings = {}

    for root, _, files in os.walk(user_project_root):
        for file in files:
            if not (file.endswith(".cpp") or file.endswith(".h")):
                continue

            full_path = os.path.join(root, file)
            try:
                with open(full_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
            except Exception:
                continue

            content = strip_comments(content)
            sources.append((os.path.relpath(full_path, user_project_root), content))

            for pattern in (STRING_BINDING_PATTERN, STRING_DEFINE_PATTERN):
                for name, literal in pattern.findall(content):
                    bindings.setdefault(name, set()).add(literal)

    referenced_ids = set()

    for rel_source_path, content in sources:
        for match in ASSET_ENTRY_CALL_PATTERN.finditer(content):
            entry = match.group(1)
            line_number = content.count('\n', 0, match.start()) + 1

            arguments = split_call_arguments(content, match.end() - 1)
            if arguments is None:
                continue

            for argument_index in ASSET_ENTRY_ARGUMENTS[entry]:
                if argument_index >= len(arguments) or not arguments[argument_index]:
                    continue

                argument = arguments[argument_index]
                warn = argument_index == 0

                literal_match = WRAPPED_STRING_LITERAL_PATTERN.match(argument)
                integer_match = INTEGER_LITERAL_PATTERN.match(argument)

                if literal_match:
                    names = {literal_match.group(1)}
                elif integer_match:
                    text = integer_match.group(1)
                    asset_id = (int(text, 16) if text[:2].lower() == "0x" else int(text, 10)) & 0xFFFFFFFF
                    if asset_id in guid_to_path:
                        referenced_ids.add(asset_id)
                    elif warn:
                        print(f"警告: {entry}() に渡された Asset ID {text} が既知の Asset と一致しません ({rel_source_path}:{line_number})")
                    continue
                elif IDENTIFIER_ARGUMENT_PATTERN.match(argument):
                    names = bindings.get(re.split(r'::|\.|->', argument)[-1].strip(), set())
                    if not names:
                        if warn:
                            print(f"警告: {entry}() の引数 {argument} の値を静的に解決できません ({rel_source_path}:{line_number}) — インスペクタで設定される値ならシーン/プレハブの保存値から拾います")
                        continue
                else:
                    if warn:
                        print(f"警告: {entry}() の引数 {argument} は静的に解決できない式です ({rel_source_path}:{line_number}) — RuntimePackager はこの Asset を自動検出できません")
                    continue

                resolved = set()
                for name in names:
                    resolved |= resolve_asset_name(name, relpath_to_guid, filename_to_guids)

                if resolved:
                    referenced_ids |= resolved
                elif warn:
                    print(f"警告: {entry}() の引数 {argument} が既知の Asset パスと一致しません ({rel_source_path}:{line_number})")

    return referenced_ids


def collect_referenced_asset_ids(scene_paths, payload_map, guid_to_path, relpath_to_guid, filename_to_guids, seed_ids):
    """
    scene_paths の各 .scene と、seed_ids（コードから参照された Asset）のうち
    .scene/.prefab であるものを起点に、参照されている Asset ID を集める。
    各ドキュメントからは次を拾う:
      - payload_map（コンポーネント名 -> payload フィールド表示名の集合）に
        載っているフィールドの Asset ID
      - ネストされたプレハブ (nestedPrefabAssetID) と派生元プレハブ (basePrefabAssetID)
      - インスペクタで設定された String フィールドのうち、既知 Asset の
        パス/ファイル名に一致する値（スクリプトが Scene::Change / Prefab::Spawn /
        Scene::GetAsset へ渡す名前をインスペクタで持つ使い方）
    集めた ID のうち .scene/.prefab のものは、Spawner 経由でもコード経由でも
    同様に中身まで辿り、新たな参照が出なくなるまで繰り返す。
    Asset ID は Uint32 だが、シーン JSON の "int" は符号付きで保存されるため
    0xFFFFFFFF でマスクして戻す。
    """
    referenced_ids = set(seed_ids)
    walked_paths = set()

    def add_id(value):
        asset_id = value & 0xFFFFFFFF
        if asset_id:
            referenced_ids.add(asset_id)

    def walk_fields(fields, payload_names):
        for field in fields:
            string_value = field.get("string", "")
            if string_value and '.' in os.path.basename(string_value):
                referenced_ids.update(resolve_asset_name(string_value, relpath_to_guid, filename_to_guids))

            if field.get("name") in payload_names:
                if field.get("is_array"):
                    for child in field.get("children", []):
                        add_id(child.get("int", 0))
                else:
                    add_id(field.get("int", 0))

            walk_fields(field.get("children", []), ())

    def walk_document(path):
        if path in walked_paths:
            return
        walked_paths.add(path)

        try:
            with open(path, 'r', encoding='utf-8') as f:
                data = json.load(f)
        except Exception:
            print(f"シーン/プレハブの読み込みに失敗しました: {path}")
            return

        for node in data.get("nodes", []):
            add_id(node.get("nestedPrefabAssetID", 0))
            for comp in node.get("components", []):
                walk_fields(comp.get("fields", []), payload_map.get(comp.get("component"), ()))

        add_id(data.get("basePrefabAssetID", 0))

    for scene_path in scene_paths:
        walk_document(scene_path)

    while True:
        pending = [guid_to_path[asset_id] for asset_id in referenced_ids if asset_id in guid_to_path and os.path.splitext(guid_to_path[asset_id])[1].lower() in (".scene", ".prefab") and guid_to_path[asset_id] not in walked_paths]
        if not pending:
            break
        for path in pending:
            walk_document(path)

    return referenced_ids


def binary_record(name, payload):
    return struct.pack('<II', fnv1a(name), len(payload)) + payload


def precompile_shaders(project_root, output_dir):
    """
    GraphicsEngine 以下の全 .hlsl を、Release の ShaderCompiler と同じ
    プロファイル/オプションで External/DXC/dxc.exe によりコンパイルし、
    エンジンが読むキャッシュ形式
    (CompiledShaderObject/Application/<ファイル名>.dx.cso) で output_dir へ書き出す。
    キャッシュは BinaryOutputArchive の "entries" フィールドに
    std::unordered_map<String, DynamicArray<Uint8>>（キー "プロファイル:エントリ"、
    DXR ライブラリは "lib_6_6:main"）を入れ、AES-256-CBC で暗号化したもの。
    実行時の機能レベル分岐(MS/VS、RT の有無)に関係なく全シェーダを含めるため、
    どの GPU でもソース無しで動く。1つでも失敗したら None を返す。
    """
    dxc_path = os.path.join(project_root, "External", "DXC", "dxc.exe")
    if not os.path.exists(dxc_path):
        print(f"dxc.exe が見つかりません: {dxc_path}")
        return None

    key = read_encryption_key(project_root)
    if key is None:
        print("FoundationEngine/Prelude.h から SC_ENCRYPTION_KEY_SEED を読み取れませんでした — シェーダキャッシュを暗号化できません")
        return None

    jobs = []
    for root, _, files in os.walk(os.path.join(project_root, "GraphicsEngine")):
        for file in files:
            if not file.lower().endswith(".hlsl"):
                continue

            path = os.path.join(root, file)
            with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                source = strip_comments(f.read())

            profile = SHADER_PROFILE_BY_SUFFIX.get(file[:-5][-2:])
            if SHADER_LIBRARY_PATTERN.search(source):
                jobs.append((path, "lib_6_6", "main"))
            elif profile == "cs_6_6" or (profile is None and SHADER_NUMTHREADS_PATTERN.search(source)):
                for entry in SHADER_COMPUTE_ENTRY_PATTERN.findall(source) or ["main"]:
                    jobs.append((path, "cs_6_6", entry))
            elif profile:
                jobs.append((path, profile, "main"))
            else:
                print(f"警告: シェーダ種別を判定できないため事前コンパイルしません: {os.path.relpath(path, project_root)}")

    temp_dir = os.path.join(output_dir, "_ShaderTemp")
    os.makedirs(temp_dir, exist_ok=True)

    def compile_job(job_index):
        path, profile, entry = jobs[job_index]
        object_path = os.path.join(temp_dir, f"{job_index}.cso")
        arguments = [dxc_path, "-T", profile] + ([] if profile == "lib_6_6" else ["-E", entry]) + SHADER_COMPILE_OPTIONS + [path, "-Fo", object_path]
        result = subprocess.run(arguments, capture_output=True)
        if result.returncode != 0 or not os.path.exists(object_path):
            return job_index, None, result.stderr.decode("utf-8", "replace") + result.stdout.decode("utf-8", "replace")
        with open(object_path, 'rb') as f:
            data = f.read()
        os.remove(object_path)
        return job_index, data, ""

    entries_by_file = {}
    failed = False
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as executor:
        for job_index, data, message in executor.map(compile_job, range(len(jobs))):
            path, profile, entry = jobs[job_index]
            if data is None:
                print(f"シェーダのコンパイルに失敗しました: {os.path.relpath(path, project_root)} ({profile}:{entry})\n{message}")
                failed = True
                continue
            entries_by_file.setdefault(path, {})[f"{profile}:{entry}"] = data

    shutil.rmtree(temp_dir, ignore_errors=True)
    if failed:
        return None

    shader_dir = os.path.join(output_dir, "CompiledShaderObject", "Application")
    if os.path.exists(shader_dir):
        shutil.rmtree(shader_dir)
    os.makedirs(shader_dir)

    stems = {}
    for path in entries_by_file:
        stems.setdefault(os.path.basename(path)[:-5].lower(), []).append(os.path.relpath(path, project_root))
    for paths in stems.values():
        if len(paths) > 1:
            print(f"同名のシェーダが複数あり、キャッシュファイル名が衝突します: {', '.join(paths)}")
            return None

    for path, entries in entries_by_file.items():
        map_payload = struct.pack('<I', len(entries))
        for cache_key, data in entries.items():
            entry_body = binary_record("key", cache_key.encode("utf-8")) + binary_record("value", struct.pack('<I', len(data)) + data)
            map_payload += struct.pack('<I', len(entry_body)) + entry_body

        stem = os.path.basename(path)[:-5]
        with open(os.path.join(shader_dir, f"{stem}.dx.cso"), 'wb') as f:
            f.write(aes256_cbc_encrypt(key, binary_record("entries", map_payload)))

    return len(jobs)


def read_executable_name(project_root):
    """
    GameConfig.scg の "executableName"（Editor の ConfigPanel「ゲーム設定」で
    入力した、プレイヤーが起動するランチャー exe の名前）を、Windows の
    ファイル名として使える形にして返す。使えない文字と制御文字を除き、
    末尾のドット/空白と、入力されていれば ".exe" を落とす。空、または
    予約デバイス名(CON, NUL, COM1 など)になった場合は "Launcher" を返す。
    """
    name = ""
    config_path = os.path.join(project_root, "UserProject", "Assets", "Config", "GameConfig.scg")
    key = read_encryption_key(project_root)
    if key is not None and os.path.exists(config_path):
        with open(config_path, 'rb') as f:
            body = aes256_cbc_decrypt(key, f.read())
        payload = read_tagged_binary_fields(body).get(fnv1a("executableName")) if body else None
        if payload:
            name = payload.decode("utf-8", "replace")

    name = re.sub(r'[\\/:*?"<>|\x00-\x1f]', '', name).strip()
    if name.lower().endswith(".exe"):
        name = name[:-4]
    name = name.rstrip(". ")

    reserved = {"con", "prn", "aux", "nul"} | {f"com{index}" for index in range(1, 10)} | {f"lpt{index}" for index in range(1, 10)}
    if not name or name.split(".")[0].lower() in reserved:
        return "Launcher"
    return name


def read_application_icon(project_root):
    """
    Editor の ConfigPanel「アイコン設定」で選ばれたアイコン（IconConfig が
    UserProject/Assets/Config/IconBindings.scg の "icon" フィールドに .ico を
    まるごと保存したもの）を返す。未指定ならエンジン既定の
    Runtime/Logo/SeedCore.ico を返す。
    """
    bindings_path = os.path.join(project_root, "UserProject", "Assets", "Config", "IconBindings.scg")
    key = read_encryption_key(project_root)
    if key is not None and os.path.exists(bindings_path):
        with open(bindings_path, 'rb') as f:
            body = aes256_cbc_decrypt(key, f.read())
        payload = read_tagged_binary_fields(body).get(fnv1a("icon")) if body else None
        if payload and len(payload) >= 4:
            count = struct.unpack_from('<I', payload, 0)[0]
            if count > 0 and len(payload) == 4 + count:
                return payload[4:], "IconBindings.scg"

    with open(os.path.join(project_root, "Runtime", "Logo", "SeedCore.ico"), 'rb') as f:
        return f.read(), "デフォルト (Runtime/Logo/SeedCore.ico)"


def embed_application_icon(exe_path, icon):
    """
    .ico の各画像を RT_ICON (ID 1..N) として、それらをまとめる
    RT_GROUP_ICON (ID 1) と一緒に exe へ書き込む。エクスプローラーは最小 ID の
    グループアイコンを exe のアイコンとして表示し、Runtime の Window は
    LoadImage(自身の exe, ID 1) で同じものをウィンドウ/タスクバーに使う。
    .ico 内の ICONDIRENTRY (16 バイト、末尾が画像オフセット) を、リソース用の
    GRPICONDIRENTRY (14 バイト、末尾が RT_ICON の ID) に詰め替える。
    """
    reserved, icon_type, count = struct.unpack_from('<HHH', icon, 0)
    if reserved != 0 or icon_type != 1 or count == 0 or len(icon) < 6 + count * 16:
        print(f"アイコンの形式が不正です: {exe_path}")
        return False

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.BeginUpdateResourceW.argtypes = [wintypes.LPCWSTR, wintypes.BOOL]
    kernel32.BeginUpdateResourceW.restype = wintypes.HANDLE
    kernel32.UpdateResourceW.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.LPVOID, wintypes.WORD, wintypes.LPVOID, wintypes.DWORD]
    kernel32.UpdateResourceW.restype = wintypes.BOOL
    kernel32.EndUpdateResourceW.argtypes = [wintypes.HANDLE, wintypes.BOOL]
    kernel32.EndUpdateResourceW.restype = wintypes.BOOL

    rt_icon = 3
    rt_group_icon = 14
    language_neutral = 0

    handle = kernel32.BeginUpdateResourceW(exe_path, False)
    if not handle:
        print(f"exe のリソース更新を開始できませんでした ({ctypes.get_last_error()}): {exe_path}")
        return False

    group = struct.pack('<HHH', 0, 1, count)
    for image_index in range(count):
        width, height, color_count, entry_reserved, planes, bit_count, size, offset = struct.unpack_from('<BBBBHHII', icon, 6 + image_index * 16)
        image = ctypes.create_string_buffer(icon[offset:offset + size], size)
        if not kernel32.UpdateResourceW(handle, ctypes.c_void_p(rt_icon), ctypes.c_void_p(image_index + 1), language_neutral, image, size):
            print(f"アイコン画像を書き込めませんでした ({ctypes.get_last_error()}): {exe_path}")
            kernel32.EndUpdateResourceW(handle, True)
            return False
        group += struct.pack('<BBBBHHIH', width, height, color_count, entry_reserved, planes, bit_count, size, image_index + 1)

    group_buffer = ctypes.create_string_buffer(group, len(group))
    if not kernel32.UpdateResourceW(handle, ctypes.c_void_p(rt_group_icon), ctypes.c_void_p(1), language_neutral, group_buffer, len(group)):
        print(f"グループアイコンを書き込めませんでした ({ctypes.get_last_error()}): {exe_path}")
        kernel32.EndUpdateResourceW(handle, True)
        return False

    if not kernel32.EndUpdateResourceW(handle, False):
        print(f"exe のリソース更新を確定できませんでした ({ctypes.get_last_error()}): {exe_path}")
        return False

    return True


def copy_preserving_relative(src_path, src_root, dst_root):
    rel = os.path.relpath(src_path, src_root)
    dst_path = os.path.join(dst_root, rel)
    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    shutil.copy2(src_path, dst_path)


def main():
    if len(sys.argv) < 3:
        print("使用法: RuntimePackager.py <projectRoot> <outputDir>")
        return 1

    project_root = os.path.abspath(sys.argv[1])
    output_dir = os.path.abspath(sys.argv[2])
    user_project_root = os.path.join(project_root, "UserProject")

    os.makedirs(output_dir, exist_ok=True)

    build_dir = os.path.join(project_root, "Runtime", "Build", "x64", "Release")

    # --- Launcher.exe: パッケージルートに置く、プレイヤーが起動する薄い exe。GameConfig の実行ファイル名で置く ---
    launcher_path = os.path.join(build_dir, "Launcher.exe")
    if not os.path.exists(launcher_path):
        print(f"Launcher.exe が見つかりません: {launcher_path}")
        return 1
    launcher_output_path = os.path.join(output_dir, f"{read_executable_name(project_root)}.exe")
    shutil.copy2(launcher_path, launcher_output_path)
    print(f"Launcher コピー完了: {launcher_output_path}")

    # --- Plugins: Runtime.exe + エンジン / サードパーティ DLL + UserProject.dll(プラグイン) ---
    # ランチャーが Plugins\Runtime.exe を作業ディレクトリ Plugins\ で起動するので、
    # SeedCore.dll 等が隣で解決され、PluginHost は UserProject.dll をこのフォルダから拾う。
    exe_path = os.path.join(build_dir, "Runtime.exe")
    if not os.path.exists(exe_path):
        print(f"Runtime.exe が見つかりません: {exe_path}")
        return 1

    plugins_dir = os.path.join(output_dir, "Plugins")
    os.makedirs(plugins_dir, exist_ok=True)
    shutil.copy2(exe_path, os.path.join(plugins_dir, "Runtime.exe"))
    for file in os.listdir(build_dir):
        if not file.lower().endswith(".dll"):
            continue
        # PluginModule のシャドウコピー（Foo_<数字>.dll）は出荷しない。
        if re.search(r'_\d+\.dll$', file, re.IGNORECASE):
            continue
        shutil.copy2(os.path.join(build_dir, file), os.path.join(plugins_dir, file))

    if not os.path.exists(os.path.join(plugins_dir, "UserProject.dll")):
        print(f"UserProject.dll が見つかりません: {os.path.join(build_dir, 'UserProject.dll')}")
        return 1
    print(f"Plugins コピー完了: {plugins_dir}")

    # --- アプリケーションアイコン: プレイヤーが起動するランチャー exe と、実行中にタスクバーへ出る Runtime.exe の両方へ埋め込む ---
    icon, icon_source = read_application_icon(project_root)
    for exe in (launcher_output_path, os.path.join(plugins_dir, "Runtime.exe")):
        if not embed_application_icon(exe, icon):
            return 1
    print(f"アイコン埋め込み完了: {icon_source}")

    # --- スプラッシュのロゴ: Runtime.exe は Plugins\ から ../Runtime/Logo/*.logo を読むので、パッケージ直下の Runtime\Logo へ置く ---
    # SeedCore.ico は exe へ埋め込み済みなので同梱しない。
    logo_dir = os.path.join(project_root, "Runtime", "Logo")
    logo_count = 0
    if os.path.isdir(logo_dir):
        for file in os.listdir(logo_dir):
            if not file.lower().endswith(".logo"):
                continue
            copy_preserving_relative(os.path.join(logo_dir, file), project_root, output_dir)
            logo_count += 1
    if logo_count == 0:
        print(f"ロゴ (.logo) が見つかりません: {logo_dir}")
        return 1
    print(f"ロゴ コピー完了 ({logo_count} 件)")

    # --- DXC はビルド出力の版に関係なく External/DXC の版を同梱する（事前コンパイルと同じ版） ---
    for file in ("dxcompiler.dll", "dxil.dll"):
        dxc_dll = os.path.join(project_root, "External", "DXC", file)
        if not os.path.exists(dxc_dll):
            print(f"{file} が見つかりません: {dxc_dll}")
            return 1
        shutil.copy2(dxc_dll, os.path.join(plugins_dir, file))

    # --- シェーダは毎回全て事前コンパイルし、ソース(.hlsl)無しで動くキャッシュとして同梱する ---
    shader_count = precompile_shaders(project_root, output_dir)
    if shader_count is None:
        print("シェーダの事前コンパイルに失敗したため、パッケージを中断しました")
        return 1
    print(f"シェーダ事前コンパイル完了 ({shader_count} 件)")

    # --- Scene / Prefab は丸ごとコピー（フィルタしない） ---
    scene_paths = []
    for sub_dir in ("Scene", "Prefab"):
        src_dir = os.path.join(user_project_root, "Assets", sub_dir)
        if not os.path.isdir(src_dir):
            continue
        for root, _, files in os.walk(src_dir):
            for file in files:
                src_path = os.path.join(root, file)
                copy_preserving_relative(src_path, user_project_root, os.path.join(output_dir, "UserProject"))
                if sub_dir == "Scene" and file.endswith(".scene"):
                    scene_paths.append(src_path)
    print(f"Scene/Prefab コピー完了 ({len(scene_paths)} 件のシーン)")

    # --- Config の .scg（GameConfig / AudioBindings / InputBindings / LayerBindings 等）はそのままコピー ---
    # IconBindings.scg は書き出し時に exe へ埋め込むための入力で、ゲームは exe のリソースから読むので同梱しない。
    config_count = 0
    config_dir = os.path.join(user_project_root, "Assets", "Config")
    if os.path.isdir(config_dir):
        for root, _, files in os.walk(config_dir):
            for file in files:
                if not file.lower().endswith(".scg") or file.lower() == "iconbindings.scg":
                    continue
                copy_preserving_relative(os.path.join(root, file), user_project_root, os.path.join(output_dir, "UserProject"))
                config_count += 1
    print(f"Config コピー完了 ({config_count} 件)")

    # --- 参照されている Asset のみコピー ---
    guid_to_path, relpath_to_guid = build_guid_map(project_root, user_project_root)
    filename_to_guids = build_filename_index(relpath_to_guid)
    payload_map = Payload.collect_payload_field_names(project_root)

    code_referenced_ids = scan_source_for_asset_references(user_project_root, relpath_to_guid, filename_to_guids, guid_to_path)
    print(f"コードから動的参照している Asset を検出: {len(code_referenced_ids)} 件")

    referenced_ids = collect_referenced_asset_ids(scene_paths, payload_map, guid_to_path, relpath_to_guid, filename_to_guids, code_referenced_ids)

    copied_count = 0
    for asset_id in referenced_ids:
        asset_path = guid_to_path.get(asset_id)
        if not asset_path:
            continue

        copy_preserving_relative(asset_path, user_project_root, os.path.join(output_dir, "UserProject"))

        meta_path = asset_path + ".meta"
        if os.path.exists(meta_path):
            copy_preserving_relative(meta_path, user_project_root, os.path.join(output_dir, "UserProject"))

        copied_count += 1

    print(f"参照 Asset コピー完了 ({copied_count} 件)")
    print(f"パッケージ完了: {output_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
