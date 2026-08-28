import sys
import os
import re
import struct
import shutil
import json

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)
import Payload

# .meta ファイルを持たない拡張子（ResourceCache::noMetaExtensions_ と同じ）。
NO_META_EXTENSIONS = {".h", ".cpp", ".hlsli", ".hlsl"}

# Scene/Prefab は丸ごとコピー済みなので、参照 Asset コピーの対象からは除く。
SKIP_ASSET_COPY_EXTENSIONS = {".scene", ".prefab"}

# SeedCore::Scene::GetAsset(path) / SeedCore::Prefab::Spawn(name, ...) の呼び出しと、
# その第1引数を抜き出す簡易パターン。ゲームプレイコードが名前文字列で Asset を
# 動的ロードする入口はこの2つ（Scene.h/.cpp, Prefab.h/.cpp 参照）。
# 第1引数を , か ) まで読むので Prefab::Spawn の第2引数(parent)は無視される。
# 引数に入れ子の括弧がある呼び方（関数呼び出しの合成など）は想定しない。
GET_ASSET_CALL_PATTERN = re.compile(r'(?:Scene::GetAsset|Prefab::Spawn)\s*\(\s*([^,()]*?)\s*[,)]')

# 引数全体がちょうど1つの文字列リテラルであることを確認するパターン（エスケープされた " も1文字として飲み込む）。
STRING_LITERAL_FULL_PATTERN = re.compile(r'^"((?:[^"\\]|\\.)*)"$')


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


def build_guid_map(project_root, user_project_root):
    """
    UserProject 以下を走査し、.meta を持つ全ファイルについて
    guid -> 実ファイルパス のマップと、
    プロジェクトルート相対パス（スラッシュ区切り） -> guid のマップを構築する。
    .meta は AssetMeta::Serialize がタグ付きバイナリ形式(BinaryArchive.h参照)
    で書き出しているので、read_tagged_binary_fields でフィールドIDから
    "guid" の4バイトを引く。
    後者は ResourceCache::GetAssetID() が asset.path_（プロジェクトルート
    相対、フォワードスラッシュ）に対して完全一致で引く仕組みと同じ形。
    """
    guid_to_path = {}
    relpath_to_guid = {}

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
                    raw = f.read()
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


def scan_source_for_asset_references(user_project_root, relpath_to_guid):
    """
    UserProject 以下の .cpp/.h を走査し、Scene::GetAsset(...) と
    Prefab::Spawn(...) の呼び出しを見つける。ゲームプレイコードが名前文字列で
    Asset を動的ロードする入口はこの2つ（FoundationEngine/Resource/Scene.h/.cpp,
    Prefab.h/.cpp 参照）。引数はまずプロジェクトルート相対パスとして完全一致で
    引き、外れたらファイル名だけで引く（ResourceCache::GetAssetID と同じ挙動。
    同名ファイルが複数あれば最初に見つかったものを使う）。引数が文字列
    リテラルでない場合は、静的に解決できない旨を警告する。
    """
    # ファイル名 -> guid のフォールバック索引（同名複数なら最初の1つ）。
    filename_to_guid = {}
    for rel_path, guid in relpath_to_guid.items():
        filename_to_guid.setdefault(os.path.basename(rel_path), guid)

    referenced_ids = set()

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

            rel_source_path = os.path.relpath(full_path, user_project_root)

            for match in GET_ASSET_CALL_PATTERN.finditer(content):
                arg = match.group(1).strip()
                line_number = content.count('\n', 0, match.start()) + 1

                literal_match = STRING_LITERAL_FULL_PATTERN.match(arg)
                if not literal_match:
                    print(f"警告: Scene::GetAsset()/Prefab::Spawn() に文字列リテラル以外の引数が渡されています ({rel_source_path}:{line_number}) — RuntimePackager はこの Asset を自動検出できません")
                    continue

                literal = literal_match.group(1)
                literal = literal.replace('\\\\', '\\').replace('\\', '/')

                asset_id = relpath_to_guid.get(literal) or filename_to_guid.get(os.path.basename(literal))
                if asset_id:
                    referenced_ids.add(asset_id)
                else:
                    print(f"警告: Scene::GetAsset()/Prefab::Spawn() の引数 \"{literal}\" が既知の Asset パスと一致しません ({rel_source_path}:{line_number})")

    return referenced_ids


def collect_referenced_asset_ids(scene_paths, payload_map, guid_to_path):
    """
    scene_paths の各 .scene を読み込み、payload_map（コンポーネント名 ->
    payload フィールド表示名の集合）を手がかりに、参照されている Asset ID
    を再帰的に（ネストされた/派生元プレハブも辿って）集める。
    """
    referenced_ids = set()
    visited_prefabs = set()

    def walk_nodes(nodes):
        for node in nodes:
            nested_id = node.get("nestedPrefabAssetID", 0)
            if nested_id:
                referenced_ids.add(nested_id)
                walk_prefab(nested_id)
                continue

            for comp in node.get("components", []):
                comp_name = comp.get("component")
                payload_names = payload_map.get(comp_name)
                if not payload_names:
                    continue

                for field in comp.get("fields", []):
                    field_name = field.get("name")
                    if field_name not in payload_names:
                        continue

                    if field.get("is_array"):
                        for child in field.get("children", []):
                            asset_id = child.get("int", 0)
                            if asset_id:
                                referenced_ids.add(asset_id)
                    else:
                        asset_id = field.get("int", 0)
                        if asset_id:
                            referenced_ids.add(asset_id)

    def walk_prefab(prefab_id):
        if prefab_id in visited_prefabs:
            return
        visited_prefabs.add(prefab_id)

        path = guid_to_path.get(prefab_id)
        if not path:
            return

        try:
            with open(path, 'r', encoding='utf-8') as f:
                data = json.load(f)
        except Exception:
            return

        walk_nodes(data.get("nodes", []))

        base_id = data.get("basePrefabAssetID", 0)
        if base_id:
            referenced_ids.add(base_id)
            walk_prefab(base_id)

    for scene_path in scene_paths:
        try:
            with open(scene_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
        except Exception:
            print(f"シーンの読み込みに失敗しました: {scene_path}")
            continue

        walk_nodes(data.get("nodes", []))

    return referenced_ids


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

    # --- Launcher.exe: パッケージルートに置く、プレイヤーが起動する薄い exe ---
    launcher_path = os.path.join(build_dir, "Launcher.exe")
    if not os.path.exists(launcher_path):
        print(f"Launcher.exe が見つかりません: {launcher_path}")
        return 1
    shutil.copy2(launcher_path, os.path.join(output_dir, "Launcher.exe"))
    print(f"Launcher コピー完了: {os.path.join(output_dir, 'Launcher.exe')}")

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

    # --- CompiledShaderObject/Application だけコピー（Develop はエディタ専用） ---
    shader_src = os.path.join(project_root, "CompiledShaderObject", "Application")
    shader_dst = os.path.join(output_dir, "CompiledShaderObject", "Application")
    if os.path.isdir(shader_src):
        if os.path.exists(shader_dst):
            shutil.rmtree(shader_dst)
        shutil.copytree(shader_src, shader_dst)
        print(f"CompiledShaderObject/Application コピー完了: {shader_dst}")

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

    # --- 参照されている Asset のみコピー ---
    guid_to_path, relpath_to_guid = build_guid_map(project_root, user_project_root)
    payload_map = Payload.collect_payload_field_names(project_root)
    referenced_ids = collect_referenced_asset_ids(scene_paths, payload_map, guid_to_path)

    code_referenced_ids = scan_source_for_asset_references(user_project_root, relpath_to_guid)
    print(f"コードから動的参照している Asset を検出: {len(code_referenced_ids)} 件")
    referenced_ids |= code_referenced_ids

    copied_count = 0
    for asset_id in referenced_ids:
        asset_path = guid_to_path.get(asset_id)
        if not asset_path:
            continue

        ext = os.path.splitext(asset_path)[1].lower()
        if ext in SKIP_ASSET_COPY_EXTENSIONS:
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
