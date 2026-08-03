"""
UserProject.vcxproj / UserProject.vcxproj.filters の Script フォルダ分のエントリを、
実際に UserProject/Script 配下に存在する .h/.cpp と一致させる(足りないものを追加し、
既に消えているものを取り除く)。UserProject.vcxproj の PreBuildEvent から
Reflection.py/Payload.py と同じタイミングで、毎ビルド前に呼ばれる想定。

Editor の「新規 C++ スクリプト」機能(ContentsDrawerPanel)は作成時に
VisualStudioAutomation 経由(またはフォールバックとして CreateScript.py)で
即座に登録するが、それはVisual Studioが対象ソリューションを開いていない場合や、
エクスプローラーからの直接削除/リネームのように Editor を経由しない変更までは
拾えない。このスクリプトは毎ビルド前に実際のファイル一覧から作り直すことで、
どんな経路で Script フォルダの中身が変化しても最終的に整合させる安全網。

Script フォルダ以外(Tutorial、EntryPoint、Reflection、Payload等)のエントリは
一切変更しない。実際に変更が必要な場合のみファイルへ書き込む(スキャン結果が
現状と一致していれば何もしない) — Visual Studio がソリューションを開いたまま
ビルドしても、無用に「プロジェクトが外部で変更されました」と出さないため。
"""
import sys
import os
import uuid
import xml.etree.ElementTree as ET

NS = 'http://schemas.microsoft.com/developer/msbuild/2003'
SCRIPT_PREFIX = 'Script\\'


def scan_script_files(user_project_root):
    """
    UserProject/Script 配下に実在する .h/.cpp を、UserProject/ からの相対パス
    (バックスラッシュ区切り)で列挙する。
    """
    script_root = os.path.join(user_project_root, 'Script')
    headers = []
    cpps = []

    if not os.path.isdir(script_root):
        os.makedirs(script_root, exist_ok=True)
        return headers, cpps

    for root, _, files in os.walk(script_root):
        for file in files:
            full_path = os.path.join(root, file)
            relative = os.path.relpath(full_path, user_project_root)
            if file.endswith('.h'):
                headers.append(relative)
            elif file.endswith('.cpp'):
                cpps.append(relative)

    headers.sort()
    cpps.sort()
    return headers, cpps


def sync_project_items(root, tag, valid_paths):
    """
    root 内の tag(ClCompile/ClInclude)要素のうち Include が "Script\\" で
    始まるものの集合を valid_paths と比較する。既に一致していれば何もせず
    False を返す。異なっていれば、Script配下の既存エントリを全て削除して
    valid_paths の内容で作り直し、True を返す。
    """
    existing = {
        item.get('Include', '')
        for item in root.iter(f'{{{NS}}}{tag}')
        if item.get('Include', '').startswith(SCRIPT_PREFIX)
    }
    target = set(valid_paths)

    if existing == target:
        return False

    item_groups_with_tag = []
    for item_group in root.iter(f'{{{NS}}}ItemGroup'):
        for item in list(item_group.findall(f'{{{NS}}}{tag}')):
            if item.get('Include', '').startswith(SCRIPT_PREFIX):
                item_group.remove(item)
        if item_group.find(f'{{{NS}}}{tag}') is not None:
            item_groups_with_tag.append(item_group)

    target_group = item_groups_with_tag[0] if item_groups_with_tag else ET.SubElement(root, f'{{{NS}}}ItemGroup')

    for path in sorted(target):
        element = ET.SubElement(target_group, f'{{{NS}}}{tag}')
        element.set('Include', path)

    return True


def sync_filters_project_items(root, tag, valid_paths):
    """
    sync_project_items と同じ判定だが、filters ファイル用に各要素へ
    <Filter>要素(そのファイルが属するディレクトリ名、バックスラッシュ区切り)も
    書き込む。

    一致判定は Include パスの集合だけでなく (Include, Filter値) のペアで行う —
    パスは既に登録されているが <Filter> が抜けている/違う値になっている壊れた
    エントリ(例: Visual Studio の ProjectItems.AddFromFile() 経由で追加され、
    フィルタ未指定のままプロジェクトルート直下に置かれてしまったもの)も、
    パスの集合だけを見る比較では「一致している」と誤判定して見逃してしまうため。
    """
    existing = set()
    for item in root.iter(f'{{{NS}}}{tag}'):
        include = item.get('Include', '')
        if not include.startswith(SCRIPT_PREFIX):
            continue
        filterElement = item.find(f'{{{NS}}}Filter')
        filterText = filterElement.text if filterElement is not None else ''
        existing.add((include, filterText))

    target = {(path, os.path.dirname(path)) for path in valid_paths}

    if existing == target:
        return False

    item_groups_with_tag = []
    for item_group in root.iter(f'{{{NS}}}ItemGroup'):
        for item in list(item_group.findall(f'{{{NS}}}{tag}')):
            if item.get('Include', '').startswith(SCRIPT_PREFIX):
                item_group.remove(item)
        if item_group.find(f'{{{NS}}}{tag}') is not None:
            item_groups_with_tag.append(item_group)

    target_group = item_groups_with_tag[0] if item_groups_with_tag else ET.SubElement(root, f'{{{NS}}}ItemGroup')

    for path in sorted(valid_paths):
        element = ET.SubElement(target_group, f'{{{NS}}}{tag}')
        element.set('Include', path)
        filter_ref = ET.SubElement(element, f'{{{NS}}}Filter')
        filter_ref.text = os.path.dirname(path)

    return True


def sync_filter_definitions(root, valid_paths):
    """
    valid_paths(UserProject/からの相対パス)が属するディレクトリ階層ごとに
    <Filter>定義を揃える。既に無くなったディレクトリの Filter 定義(Script自身も
    含む)は削除し、まだ残っている/新たに必要になった階層だけを残す。
    """
    needed_chains = set()
    for path in valid_paths:
        directory = os.path.dirname(path)
        parts = directory.split('\\')
        built = ''
        for part in parts:
            built = part if not built else built + '\\' + part
            needed_chains.add(built)

    filter_group = None
    for item_group in root.iter(f'{{{NS}}}ItemGroup'):
        if item_group.find(f'{{{NS}}}Filter') is not None:
            filter_group = item_group
            break

    if filter_group is None:
        if not needed_chains:
            return False
        filter_group = ET.SubElement(root, f'{{{NS}}}ItemGroup')

    existing_script_filters = {
        filterElement.get('Include'): filterElement
        for filterElement in filter_group.findall(f'{{{NS}}}Filter')
        if filterElement.get('Include', '') == 'Script' or filterElement.get('Include', '').startswith(SCRIPT_PREFIX)
    }

    if set(existing_script_filters.keys()) == needed_chains:
        return False

    for name, element in existing_script_filters.items():
        if name not in needed_chains:
            filter_group.remove(element)

    for chain in sorted(needed_chains):
        if chain not in existing_script_filters:
            filterElement = ET.SubElement(filter_group, f'{{{NS}}}Filter')
            filterElement.set('Include', chain)
            uniqueID = ET.SubElement(filterElement, f'{{{NS}}}UniqueIdentifier')
            uniqueID.text = '{' + str(uuid.uuid4()) + '}'

    return True


def main():
    if len(sys.argv) < 2:
        print('使用法: SyncScript.py <projectRoot>')
        return 1

    project_root = os.path.abspath(sys.argv[1])
    user_project_root = os.path.join(project_root, 'UserProject')
    vcxproj_path = os.path.join(user_project_root, 'UserProject.vcxproj')
    filters_path = vcxproj_path + '.filters'

    if not os.path.exists(vcxproj_path):
        print(f'UserProject.vcxproj が見つかりません: {vcxproj_path}')
        return 1

    headers, cpps = scan_script_files(user_project_root)

    ET.register_namespace('', NS)

    tree = ET.parse(vcxproj_path)
    root = tree.getroot()
    changedCompile = sync_project_items(root, 'ClCompile', cpps)
    changedInclude = sync_project_items(root, 'ClInclude', headers)
    if changedCompile or changedInclude:
        tree.write(vcxproj_path, xml_declaration=True, encoding='utf-8')
        print(f'vcxproj同期完了: Script配下 {len(cpps)} cpp / {len(headers)} h')

    if os.path.exists(filters_path):
        filtersTree = ET.parse(filters_path)
        filtersRoot = filtersTree.getroot()

        changedFilterDefs = sync_filter_definitions(filtersRoot, headers + cpps)
        changedFilterCompile = sync_filters_project_items(filtersRoot, 'ClCompile', cpps)
        changedFilterInclude = sync_filters_project_items(filtersRoot, 'ClInclude', headers)

        if changedFilterDefs or changedFilterCompile or changedFilterInclude:
            filtersTree.write(filters_path, xml_declaration=True, encoding='utf-8')
            print('filters同期完了')

    return 0


if __name__ == '__main__':
    sys.exit(main())
