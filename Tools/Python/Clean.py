import os
import shutil

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))


def clean_registry_pragmas(registry_cpp, begin_marker, end_marker):
    if not os.path.exists(registry_cpp):
        return

    with open(registry_cpp, 'r', encoding='utf-8', newline='') as f:
        content = f.read()

    if begin_marker not in content or end_marker not in content:
        return

    start = content.index(begin_marker)
    end = content.index(end_marker) + len(end_marker)
    updated = content[:start] + begin_marker + '\r\n' + end_marker + content[end:]

    if updated != content:
        with open(registry_cpp, 'w', encoding='utf-8', newline='') as f:
            f.write(updated)
        print(f'pragma クリア: {os.path.basename(registry_cpp)}')


def clean_folder_contents(folder_path):
    if not os.path.exists(folder_path):
        return

    for entry in os.listdir(folder_path):
        full = os.path.join(folder_path, entry)
        if os.path.isdir(full):
            shutil.rmtree(full)
        else:
            os.remove(full)
        print(f'削除: {full}')


def main():
    user_project = os.path.join(PROJECT_ROOT, 'UserProject')
    clean_folder_contents(os.path.join(user_project, 'Script'))

    for name in ['Audio', 'Config', 'Effect', 'Font', 'Image', 'Model', 'Movie', 'Prefab', 'Scene', 'Skymap']:
        clean_folder_contents(os.path.join(user_project, 'Assets', name))

    reflection_cpp = os.path.join(PROJECT_ROOT, 'FoundationEngine', 'ECS', 'ReflectionRegistry.cpp')
    payload_cpp = os.path.join(PROJECT_ROOT, 'FoundationEngine', 'ECS', 'PayloadRegistry.cpp')

    clean_registry_pragmas(reflection_cpp, '// [REFLECTION_AUTO_BEGIN]', '// [REFLECTION_AUTO_END]')
    clean_registry_pragmas(payload_cpp, '// [PAYLOAD_AUTO_BEGIN]', '// [PAYLOAD_AUTO_END]')

    print('Clean.py 完了')


if __name__ == '__main__':
    main()
