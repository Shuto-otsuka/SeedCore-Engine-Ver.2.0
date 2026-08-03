"""
UserProject.vcxproj / UserProject.vcxproj.filters へ、新規作成したスクリプトの
.h/.cpp を登録する。Editor の「新規作成 > C++ スクリプト」機能から、ファイルを
実際に書き出した直後に呼び出される想定。

登録方式は Reflection.py/Payload.py の sync_vcxproj と同じ ElementTree ベース。
PrecompiledHeader は明示的に指定しない(Tutorial/ScTutorial.cpp と同じく既定の
Use のまま)。
"""
import sys
import os
import uuid
import xml.etree.ElementTree as ET

NS = 'http://schemas.microsoft.com/developer/msbuild/2003'


def ensure_filter_chain(filters_group, relative_directory):
    """
    relative_directory（例: "Script\\Player"、バックスラッシュ区切り）の各階層に
    対応する Filter 要素が無ければ作成する。既に存在するフィルタ名の集合は
    filters_group の子要素から都度読み直す(新規追加分も含めて重複を避けるため)。
    末端の階層のフィルタ名(= relative_directory そのもの)を返す。
    relative_directory が空文字列なら None を返す(フィルタ無し = ルート直下)。
    """
    if not relative_directory:
        return None

    existing = {
        filterElement.get('Include')
        for filterElement in filters_group.findall(f'{{{NS}}}Filter')
    }

    parts = relative_directory.split('\\')
    built = ''
    for part in parts:
        built = part if not built else built + '\\' + part
        if built not in existing:
            filterElement = ET.SubElement(filters_group, f'{{{NS}}}Filter')
            filterElement.set('Include', built)
            uniqueID = ET.SubElement(filterElement, f'{{{NS}}}UniqueIdentifier')
            uniqueID.text = '{' + str(uuid.uuid4()) + '}'
            existing.add(built)

    return built


def find_or_create_item_group(root, tag):
    """
    tag(例: 'ClCompile')の子要素を既に持つ ItemGroup があればそれを返す。
    無ければ root 直下に新しい ItemGroup を作って返す。
    """
    for itemGroup in root.iter(f'{{{NS}}}ItemGroup'):
        if itemGroup.find(f'{{{NS}}}{tag}') is not None:
            return itemGroup

    return ET.SubElement(root, f'{{{NS}}}ItemGroup')


def add_item(root, tag, include_path, filter_name):
    itemGroup = find_or_create_item_group(root, tag)
    element = ET.SubElement(itemGroup, f'{{{NS}}}{tag}')
    element.set('Include', include_path)
    if filter_name:
        filterRef = ET.SubElement(element, f'{{{NS}}}Filter')
        filterRef.text = filter_name
    return element


def main():
    if len(sys.argv) < 4:
        print('使用法: CreateScript.py <projectRoot> <relativeHeaderPath> <relativeCppPath>')
        return 1

    project_root = os.path.abspath(sys.argv[1])
    header_relative = sys.argv[2]
    cpp_relative = sys.argv[3]

    vcxproj_path = os.path.join(project_root, 'UserProject', 'UserProject.vcxproj')
    filters_path = vcxproj_path + '.filters'

    if not os.path.exists(vcxproj_path):
        print(f'UserProject.vcxproj が見つかりません: {vcxproj_path}')
        return 1

    ET.register_namespace('', NS)

    tree = ET.parse(vcxproj_path)
    root = tree.getroot()
    add_item(root, 'ClCompile', cpp_relative, None)
    add_item(root, 'ClInclude', header_relative, None)
    tree.write(vcxproj_path, xml_declaration=True, encoding='utf-8')
    print(f'vcxproj登録完了: {cpp_relative}, {header_relative}')

    if os.path.exists(filters_path):
        filtersTree = ET.parse(filters_path)
        filtersRoot = filtersTree.getroot()

        filterGroup = find_or_create_item_group(filtersRoot, 'Filter')
        directory = os.path.dirname(cpp_relative)
        filter_name = ensure_filter_chain(filterGroup, directory)

        add_item(filtersRoot, 'ClCompile', cpp_relative, filter_name)
        add_item(filtersRoot, 'ClInclude', header_relative, filter_name)
        filtersTree.write(filters_path, xml_declaration=True, encoding='utf-8')
        print(f'filters登録完了: {cpp_relative}, {header_relative}')

    return 0


if __name__ == '__main__':
    sys.exit(main())
